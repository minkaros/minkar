/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * CCRandomGenerateBytes (<CommonCrypto/CommonRandom.h>) and PBKDF2
 * (<CommonCrypto/CommonKeyDerivation.h>, RFC 8018 section 5.2) over the
 * CCHmac functions.
 */

#include <CommonCrypto/CommonCryptoError.h>
#include <CommonCrypto/CommonRandom.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/random.h>
#include <unistd.h>

#include "digest_internal.h"

CCRNGStatus
CCRandomGenerateBytes(void *bytes, size_t count)
{
	uint8_t *p = bytes;

	if (bytes == NULL && count > 0)
		return kCCParamError;
	while (count > 0) {
		size_t n = count > 256 ? 256 : count;	/* getentropy's limit */

		if (getentropy(p, n) != 0)
			return kCCRNGFailure;
		p += n;
		count -= n;
	}
	return kCCSuccess;
}

static int
prf_hmac(CCPseudoRandomAlgorithm prf, CCHmacAlgorithm *alg, size_t *len)
{
	switch (prf) {
	case kCCPRFHmacAlgSHA1:   *alg = kCCHmacAlgSHA1;   *len = 20; return 0;
	case kCCPRFHmacAlgSHA224: *alg = kCCHmacAlgSHA224; *len = 28; return 0;
	case kCCPRFHmacAlgSHA256: *alg = kCCHmacAlgSHA256; *len = 32; return 0;
	case kCCPRFHmacAlgSHA384: *alg = kCCHmacAlgSHA384; *len = 48; return 0;
	case kCCPRFHmacAlgSHA512: *alg = kCCHmacAlgSHA512; *len = 64; return 0;
	default:		  return -1;
	}
}

int
CCKeyDerivationPBKDF(CCPBKDFAlgorithm algorithm, const char *password,
    size_t passwordLen, const uint8_t *salt, size_t saltLen,
    CCPseudoRandomAlgorithm prf, unsigned rounds, uint8_t *derivedKey,
    size_t derivedKeyLen)
{
	CCHmacAlgorithm alg;
	CCHmacContext base, ctx;
	uint8_t U[CC_MAX_DIGEST_LENGTH], T[CC_MAX_DIGEST_LENGTH], be[4];
	size_t hlen, off, n, k;
	uint32_t block;
	unsigned r;

	if (algorithm != kCCPBKDF2 || prf_hmac(prf, &alg, &hlen) != 0 ||
	    rounds == 0 || derivedKey == NULL || derivedKeyLen == 0 ||
	    (password == NULL && passwordLen > 0) || (salt == NULL && saltLen > 0))
		return kCCParamError;

	/* The keyed HMAC state, copied for every use. */
	CCHmacInit(&base, alg, password, passwordLen);
	for (block = 1, off = 0; off < derivedKeyLen; block++, off += n) {
		be[0] = (uint8_t)(block >> 24);
		be[1] = (uint8_t)(block >> 16);
		be[2] = (uint8_t)(block >> 8);
		be[3] = (uint8_t)block;
		ctx = base;
		CCHmacUpdate(&ctx, salt, saltLen);
		CCHmacUpdate(&ctx, be, 4);
		CCHmacFinal(&ctx, U);
		memcpy(T, U, hlen);
		for (r = 1; r < rounds; r++) {
			ctx = base;
			CCHmacUpdate(&ctx, U, hlen);
			CCHmacFinal(&ctx, U);
			for (k = 0; k < hlen; k++)
				T[k] ^= U[k];
		}
		n = derivedKeyLen - off < hlen ? derivedKeyLen - off : hlen;
		memcpy(derivedKey + off, T, n);
	}
	memset(&base, 0, sizeof(base));
	memset(&ctx, 0, sizeof(ctx));
	memset(U, 0, sizeof(U));
	memset(T, 0, sizeof(T));
	return kCCSuccess;
}

static double
now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

/* Rounds that make one derivation take about msec milliseconds here. */
unsigned
CCCalibratePBKDF(CCPBKDFAlgorithm algorithm, size_t passwordLen, size_t saltLen,
    CCPseudoRandomAlgorithm prf, size_t derivedKeyLen, uint32_t msec)
{
	char password[128];
	uint8_t salt[128], key[256];
	unsigned trial = 1000;
	double start, elapsed, rounds;

	if (algorithm != kCCPBKDF2 || passwordLen > sizeof(password) ||
	    saltLen > sizeof(salt) || derivedKeyLen == 0 ||
	    derivedKeyLen > sizeof(key) || msec == 0)
		return (unsigned)-1;
	memset(password, 'x', sizeof(password));
	memset(salt, 's', sizeof(salt));
	start = now_ms();
	if (CCKeyDerivationPBKDF(algorithm, password, passwordLen, salt, saltLen,
	    prf, trial, key, derivedKeyLen) != kCCSuccess)
		return (unsigned)-1;
	elapsed = now_ms() - start;
	if (elapsed <= 0)
		elapsed = 0.001;
	rounds = trial * (double)msec / elapsed;
	if (rounds < 1)
		rounds = 1;
	if (rounds > 0xffffffffU)
		rounds = 0xffffffffU;
	return (unsigned)rounds;
}
