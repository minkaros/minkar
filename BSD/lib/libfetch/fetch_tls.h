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
 * libfetch's TLS on Mbed TLS (Minkar), in place of OpenSSL. It reads the
 * same environment as FreeBSD's: SSL_NO_VERIFY_PEER, SSL_NO_VERIFY_HOSTNAME,
 * SSL_CA_CERT_FILE, SSL_CA_CERT_PATH, SSL_CRL_FILE, SSL_CLIENT_CERT_FILE,
 * SSL_CLIENT_KEY_FILE and SSL_NO_TLS1_2 (TLS 1.3 only). Mbed TLS has no
 * TLS 1.0 or 1.1, so SSL_NO_TLS1 and SSL_NO_TLS1_1 change nothing.
 */

#ifndef FETCH_TLS_H
#define FETCH_TLS_H

#include <sys/types.h>

struct fetch_tls;
struct url;

/* fetch_tls_read() results besides a byte count. */
#define	FETCH_TLS_WAIT		-2
#define	FETCH_TLS_ERROR		-1

/* Start TLS on the connected socket sd; NULL (and a message) on failure. */
struct fetch_tls *fetch_tls_connect(int sd, const struct url *URL,
    int verbose);
ssize_t	fetch_tls_read(struct fetch_tls *tls, char *buf, size_t len);
ssize_t	fetch_tls_write(struct fetch_tls *tls, const void *buf, size_t len);
void	fetch_tls_close(struct fetch_tls *tls);

#endif /* FETCH_TLS_H */
