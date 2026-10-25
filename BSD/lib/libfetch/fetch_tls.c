/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * TLS for libfetch on Mbed TLS. See fetch_tls.h for the environment it
 * reads. The socket is blocking, as libfetch leaves it.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include "fetch.h"
#include "common.h"
#include "fetch_tls.h"

/* The system's CA bundle, used when SSL_CA_CERT_{FILE,PATH} are unset. */
#define	DEFAULT_CA_FILE	"/etc/ssl/cert.pem"
#define	DEFAULT_CA_PATH	"/etc/ssl/certs"

struct fetch_tls {
	mbedtls_net_context	 net;
	mbedtls_ssl_context	 ssl;
	mbedtls_ssl_config	 conf;
	mbedtls_entropy_context	 entropy;
	mbedtls_ctr_drbg_context drbg;
	mbedtls_x509_crt	 ca;
	mbedtls_x509_crl	 crl;
	mbedtls_x509_crt	 cert;
	mbedtls_pk_context	 key;
	int			 verify_hostname;
};

static void
tls_error(const char *what, int ret)
{
	char msg[160];

	mbedtls_strerror(ret, msg, sizeof(msg));
	fprintf(stderr, "%s: %s (-0x%04x)\n", what, msg, (unsigned)-ret);
}

static int
is_file(const char *path)
{
	struct stat sb;

	return (stat(path, &sb) == 0 && S_ISREG(sb.st_mode));
}

static int
is_dir(const char *path)
{
	struct stat sb;

	return (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode));
}

/*
 * Called for each certificate of the server's chain. It decides nothing
 * except that a name mismatch is accepted under SSL_NO_VERIFY_HOSTNAME.
 */
static int
verify_cb(void *arg, mbedtls_x509_crt *crt, int depth, uint32_t *flags)
{
	struct fetch_tls *tls = arg;

	(void)crt;
	if (depth == 0 && !tls->verify_hostname)
		*flags &= ~(uint32_t)MBEDTLS_X509_BADCERT_CN_MISMATCH;
	return (0);
}

/* Load the CA certificates and CRL; 0 on failure. */
static int
setup_peer_verification(struct fetch_tls *tls, int verbose)
{
	const char *ca_file, *ca_path, *crl_file;
	mbedtls_x509_crl *crl = NULL;
	int loaded = 0, ret;

	if (getenv("SSL_NO_VERIFY_PEER") != NULL) {
		mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
		if (verbose)
			fetch_info("Peer verification disabled");
		return (1);
	}
	ca_file = getenv("SSL_CA_CERT_FILE");
	ca_path = getenv("SSL_CA_CERT_PATH");
	if (ca_file == NULL && ca_path == NULL) {
		if (is_file(DEFAULT_CA_FILE))
			ca_file = DEFAULT_CA_FILE;
		else if (is_dir(DEFAULT_CA_PATH))
			ca_path = DEFAULT_CA_PATH;
	}
	if (verbose) {
		fetch_info("Peer verification enabled");
		if (ca_file != NULL)
			fetch_info("Using CA cert file: %s", ca_file);
		if (ca_path != NULL)
			fetch_info("Using CA cert path: %s", ca_path);
	}
	/* A bundle may hold a few certificates Mbed TLS can't parse. */
	if (ca_file != NULL) {
		ret = mbedtls_x509_crt_parse_file(&tls->ca, ca_file);
		if (ret < 0) {
			tls_error(ca_file, ret);
			return (0);
		}
		loaded = 1;
	}
	if (ca_path != NULL) {
		ret = mbedtls_x509_crt_parse_path(&tls->ca, ca_path);
		if (ret < 0) {
			tls_error(ca_path, ret);
			return (0);
		}
		loaded = 1;
	}
	if (!loaded || tls->ca.raw.len == 0) {
		fprintf(stderr, "No CA certificates: install %s, or set "
		    "SSL_CA_CERT_FILE, SSL_CA_CERT_PATH or SSL_NO_VERIFY_PEER\n",
		    DEFAULT_CA_FILE);
		return (0);
	}
	if ((crl_file = getenv("SSL_CRL_FILE")) != NULL) {
		if (verbose)
			fetch_info("Using CRL file: %s", crl_file);
		if ((ret = mbedtls_x509_crl_parse_file(&tls->crl,
		    crl_file)) != 0) {
			fprintf(stderr, "Could not load CRL file %s\n",
			    crl_file);
			return (0);
		}
		crl = &tls->crl;
	}
	mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&tls->conf, &tls->ca, crl);
	mbedtls_ssl_conf_verify(&tls->conf, verify_cb, tls);
	return (1);
}

/* Load the client certificate and key, if any; 0 on failure. */
static int
setup_client_certificate(struct fetch_tls *tls, int verbose)
{
	const char *cert_file, *key_file;
	int ret;

	if ((cert_file = getenv("SSL_CLIENT_CERT_FILE")) == NULL)
		return (1);
	key_file = getenv("SSL_CLIENT_KEY_FILE") != NULL ?
	    getenv("SSL_CLIENT_KEY_FILE") : cert_file;
	if (verbose) {
		fetch_info("Using client cert file: %s", cert_file);
		fetch_info("Using client key file: %s", key_file);
	}
	if ((ret = mbedtls_x509_crt_parse_file(&tls->cert, cert_file)) != 0) {
		fprintf(stderr, "Could not load client certificate %s\n",
		    cert_file);
		return (0);
	}
	if ((ret = mbedtls_pk_parse_keyfile(&tls->key, key_file, NULL,
	    mbedtls_ctr_drbg_random, &tls->drbg)) != 0) {
		fprintf(stderr, "Could not load client key %s\n", key_file);
		return (0);
	}
	if ((ret = mbedtls_ssl_conf_own_cert(&tls->conf, &tls->cert,
	    &tls->key)) != 0) {
		tls_error("client certificate", ret);
		return (0);
	}
	return (1);
}

struct fetch_tls *
fetch_tls_connect(int sd, const struct url *URL, int verbose)
{
	static const char pers[] = "libfetch";
	const mbedtls_x509_crt *peer;
	struct fetch_tls *tls;
	psa_status_t status;
	uint32_t flags;
	char buf[512];
	int ret;

	/* TLS 1.3 goes through PSA crypto, which must be started first. */
	if ((status = psa_crypto_init()) != PSA_SUCCESS) {
		fprintf(stderr, "PSA crypto initialization failed (%d)\n",
		    (int)status);
		return (NULL);
	}
	if ((tls = calloc(1, sizeof(*tls))) == NULL) {
		fetch_syserr();
		return (NULL);
	}
	mbedtls_net_init(&tls->net);
	mbedtls_ssl_init(&tls->ssl);
	mbedtls_ssl_config_init(&tls->conf);
	mbedtls_entropy_init(&tls->entropy);
	mbedtls_ctr_drbg_init(&tls->drbg);
	mbedtls_x509_crt_init(&tls->ca);
	mbedtls_x509_crl_init(&tls->crl);
	mbedtls_x509_crt_init(&tls->cert);
	mbedtls_pk_init(&tls->key);
	tls->verify_hostname = getenv("SSL_NO_VERIFY_HOSTNAME") == NULL;

	if ((ret = mbedtls_ctr_drbg_seed(&tls->drbg, mbedtls_entropy_func,
	    &tls->entropy, (const unsigned char *)pers, sizeof(pers))) != 0) {
		tls_error("random number generator", ret);
		goto fail;
	}
	if ((ret = mbedtls_ssl_config_defaults(&tls->conf,
	    MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
	    MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		tls_error("TLS configuration", ret);
		goto fail;
	}
	mbedtls_ssl_conf_rng(&tls->conf, mbedtls_ctr_drbg_random, &tls->drbg);
	if (getenv("SSL_NO_TLS1_2") != NULL)
		mbedtls_ssl_conf_min_tls_version(&tls->conf,
		    MBEDTLS_SSL_VERSION_TLS1_3);
	if (!setup_peer_verification(tls, verbose) ||
	    !setup_client_certificate(tls, verbose))
		goto fail;

	if ((ret = mbedtls_ssl_setup(&tls->ssl, &tls->conf)) != 0) {
		tls_error("TLS context creation", ret);
		goto fail;
	}
	/* Server name indication, and the name the certificate must carry. */
	if ((ret = mbedtls_ssl_set_hostname(&tls->ssl, URL->host)) != 0) {
		fprintf(stderr, "TLS server name indication failed for "
		    "host %s\n", URL->host);
		goto fail;
	}
	tls->net.fd = sd;
	mbedtls_ssl_set_bio(&tls->ssl, &tls->net, mbedtls_net_send,
	    mbedtls_net_recv, NULL);

	if (verbose && tls->verify_hostname)
		fetch_info("Verify hostname");
	while ((ret = mbedtls_ssl_handshake(&tls->ssl)) != 0) {
		if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
		    ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			continue;
		if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
			flags = mbedtls_ssl_get_verify_result(&tls->ssl);
			mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ",
			    flags);
			fprintf(stderr, "Certificate verification failed "
			    "for %s:\n%s", URL->host, buf);
		} else
			tls_error("TLS handshake", ret);
		goto fail;
	}

	if ((peer = mbedtls_ssl_get_peer_cert(&tls->ssl)) == NULL) {
		fprintf(stderr, "No server SSL certificate\n");
		goto fail;
	}
	if (verbose) {
		fetch_info("%s connection established using %s",
		    mbedtls_ssl_get_version(&tls->ssl),
		    mbedtls_ssl_get_ciphersuite(&tls->ssl));
		if (mbedtls_x509_dn_gets(buf, sizeof(buf), &peer->subject) > 0)
			fetch_info("Certificate subject: %s", buf);
		if (mbedtls_x509_dn_gets(buf, sizeof(buf), &peer->issuer) > 0)
			fetch_info("Certificate issuer: %s", buf);
	}
	return (tls);

fail:
	/* The socket belongs to the connection, which closes it. */
	tls->net.fd = -1;
	fetch_tls_close(tls);
	return (NULL);
}

ssize_t
fetch_tls_read(struct fetch_tls *tls, char *buf, size_t len)
{
	int ret;

	for (;;) {
		ret = mbedtls_ssl_read(&tls->ssl, (unsigned char *)buf, len);
		if (ret >= 0)
			return (ret);
		switch (ret) {
		case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
			return (0);
		case MBEDTLS_ERR_SSL_WANT_READ:
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			return (FETCH_TLS_WAIT);
		case MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET:
			continue;	/* TLS 1.3 post-handshake message */
		default:
			tls_error("TLS read", ret);
			errno = EIO;
			return (FETCH_TLS_ERROR);
		}
	}
}

ssize_t
fetch_tls_write(struct fetch_tls *tls, const void *buf, size_t len)
{
	int ret;

	for (;;) {
		ret = mbedtls_ssl_write(&tls->ssl, buf, len);
		if (ret >= 0)
			return (ret);
		if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
		    ret == MBEDTLS_ERR_SSL_WANT_WRITE)
			continue;
		tls_error("TLS write", ret);
		errno = EIO;
		return (-1);
	}
}

void
fetch_tls_close(struct fetch_tls *tls)
{

	if (tls == NULL)
		return;
	if (tls->net.fd != -1)
		(void)mbedtls_ssl_close_notify(&tls->ssl);
	mbedtls_ssl_free(&tls->ssl);
	mbedtls_ssl_config_free(&tls->conf);
	mbedtls_ctr_drbg_free(&tls->drbg);
	mbedtls_entropy_free(&tls->entropy);
	mbedtls_x509_crt_free(&tls->ca);
	mbedtls_x509_crl_free(&tls->crl);
	mbedtls_x509_crt_free(&tls->cert);
	mbedtls_pk_free(&tls->key);
	/* libfetch closes the socket itself. */
	free(tls);
}
