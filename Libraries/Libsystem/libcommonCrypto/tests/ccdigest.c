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
 * Host test driver: ccdigest ALG [KEYHEX] < data
 * Prints the digest (or, with a key, the HMAC) of stdin in hex. Data is
 * fed in chunks of varying size to exercise the partial-block paths.
 */

#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char data[1 << 20];

static void
feed(void (*fn)(void *, const unsigned char *, size_t), void *ctx, size_t len)
{
	size_t off = 0, step = 1;

	while (off < len) {
		size_t n = step < len - off ? step : len - off;
		fn(ctx, data + off, n);
		off += n;
		step = step * 3 % 131 + 1;
	}
}

#define WRAP(alg, T) \
	static void up_##alg(void *c, const unsigned char *p, size_t n) \
	{ CC_##alg##_Update((T *)c, p, (CC_LONG)n); }
WRAP(MD5, CC_MD5_CTX)
WRAP(SHA1, CC_SHA1_CTX)
WRAP(SHA224, CC_SHA256_CTX)
WRAP(SHA256, CC_SHA256_CTX)
WRAP(SHA384, CC_SHA512_CTX)
WRAP(SHA512, CC_SHA512_CTX)

static void
up_hmac(void *c, const unsigned char *p, size_t n)
{
	CCHmacUpdate(c, p, n);
}

int
main(int argc, char **argv)
{
	unsigned char md[64], oneshot[64], key[512];
	size_t len = fread(data, 1, sizeof(data), stdin), mdlen = 0, keylen = 0, i;
	const char *alg = argc > 1 ? argv[1] : "";

	if (argc > 2) {
		for (i = 0; argv[2][2 * i] && i < sizeof(key); i++)
			sscanf(argv[2] + 2 * i, "%2hhx", &key[i]);
		keylen = i;
		CCHmacAlgorithm a;
		if (!strcmp(alg, "md5")) { a = kCCHmacAlgMD5; mdlen = 16; }
		else if (!strcmp(alg, "sha1")) { a = kCCHmacAlgSHA1; mdlen = 20; }
		else if (!strcmp(alg, "sha224")) { a = kCCHmacAlgSHA224; mdlen = 28; }
		else if (!strcmp(alg, "sha256")) { a = kCCHmacAlgSHA256; mdlen = 32; }
		else if (!strcmp(alg, "sha384")) { a = kCCHmacAlgSHA384; mdlen = 48; }
		else if (!strcmp(alg, "sha512")) { a = kCCHmacAlgSHA512; mdlen = 64; }
		else return 2;
		CCHmacContext ctx;
		CCHmacInit(&ctx, a, key, keylen);
		feed(up_hmac, &ctx, len);
		CCHmacFinal(&ctx, md);
		CCHmac(a, key, keylen, data, len, oneshot);
	}
#define DO(name, A, T, L) \
	else if (!strcmp(alg, name)) { T c; CC_##A##_Init(&c); \
		feed(up_##A, &c, len); CC_##A##_Final(md, &c); \
		CC_##A(data, (CC_LONG)len, oneshot); mdlen = L; }
	DO("md5", MD5, CC_MD5_CTX, 16)
	DO("sha1", SHA1, CC_SHA1_CTX, 20)
	DO("sha224", SHA224, CC_SHA256_CTX, 28)
	DO("sha256", SHA256, CC_SHA256_CTX, 32)
	DO("sha384", SHA384, CC_SHA512_CTX, 48)
	DO("sha512", SHA512, CC_SHA512_CTX, 64)
	else
		return 2;
	if (memcmp(md, oneshot, mdlen) != 0) {
		fprintf(stderr, "chunked and one-shot results differ\n");
		return 1;
	}
	for (i = 0; i < mdlen; i++)
		printf("%02x", md[i]);
	printf("\n");
	return 0;
}
