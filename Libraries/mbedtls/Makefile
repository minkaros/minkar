# Mbed TLS 3.6.7 (LTS), the TLS of curl and fetch. Apache-2.0 (LICENSE;
# dual Apache-2.0 OR GPL-2.0-or-later upstream, taken as Apache-2.0;
# approved for Minkar 2026-10-24). Unmodified include/ and library/ of
# the release tarball (sha256 a7e8bcbe...12d11f6), without 3rdparty/
# (Everest, p256-m: off in the default configuration).
# One static library with crypto, X.509 and TLS, in the SDK's /usr/local:
# macOS has no Mbed TLS, so it is not part of the public SDK.

SUBDIR =
MK_WERROR = no
.PATH: ${.CURDIR}/library
LIB = mbedtls
SRCS = \
	aes.c aesce.c aesni.c aria.c asn1parse.c asn1write.c base64.c  \
	bignum.c bignum_core.c bignum_mod.c bignum_mod_raw.c  \
	block_cipher.c camellia.c ccm.c chacha20.c chachapoly.c cipher.c  \
	cipher_wrap.c cmac.c constant_time.c ctr_drbg.c debug.c des.c  \
	dhm.c ecdh.c ecdsa.c ecjpake.c ecp.c ecp_curves.c  \
	ecp_curves_new.c entropy.c entropy_poll.c error.c gcm.c hkdf.c  \
	hmac_drbg.c lmots.c lms.c md5.c md.c memory_buffer_alloc.c  \
	mps_reader.c mps_trace.c net_sockets.c nist_kw.c oid.c padlock.c  \
	pem.c pk.c pkcs12.c pkcs5.c pkcs7.c pk_ecc.c pkparse.c pk_wrap.c  \
	pkwrite.c platform.c platform_util.c poly1305.c psa_crypto_aead.c  \
	psa_crypto.c psa_crypto_cipher.c psa_crypto_client.c  \
	psa_crypto_driver_wrappers_no_static.c psa_crypto_ecp.c  \
	psa_crypto_ffdh.c psa_crypto_hash.c psa_crypto_mac.c  \
	psa_crypto_pake.c psa_crypto_random.c psa_crypto_rsa.c  \
	psa_crypto_se.c psa_crypto_slot_management.c psa_crypto_storage.c  \
	psa_its_file.c psa_util.c ripemd160.c rsa_alt_helpers.c rsa.c  \
	sha1.c sha256.c sha3.c sha512.c ssl_cache.c ssl_ciphersuites.c  \
	ssl_client.c ssl_cookie.c ssl_debug_helpers_generated.c ssl_msg.c  \
	ssl_ticket.c ssl_tls12_client.c ssl_tls12_server.c  \
	ssl_tls13_client.c ssl_tls13_generic.c ssl_tls13_keys.c  \
	ssl_tls13_server.c ssl_tls.c threading.c timing.c version.c  \
	version_features.c x509.c x509_create.c x509_crl.c x509_crt.c  \
	x509_csr.c x509write.c x509write_crt.c x509write_csr.c  

CFLAGS = --sysroot=${RAVYN_SDKROOT} -mmacos-version-min=${MACOS_VERSION_MIN} \
	-I${.CURDIR}/include -I${.CURDIR}/library -fPIC -O2 \
	-Wno-nullability-completeness

all: lib${LIB}.a
	mkdir -p ${RAVYN_SDKROOT}/usr/local/lib ${RAVYN_SDKROOT}/usr/local/include
	cp -f ${.OBJDIR}/lib${LIB}.a ${RAVYN_SDKROOT}/usr/local/lib/
	rm -rf ${RAVYN_SDKROOT}/usr/local/include/mbedtls ${RAVYN_SDKROOT}/usr/local/include/psa
	cp -R ${.CURDIR}/include/mbedtls ${.CURDIR}/include/psa ${RAVYN_SDKROOT}/usr/local/include/

.include <bsd.lib.mk>
