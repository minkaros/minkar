/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
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
 * CommonDigestSPI: CCDigest, the digests by algorithm number (CCDigestInit,
 * CCDigestCreate, CCDigestUpdate, CCDigestFinal...), over the CC_* digests
 * of digest.c. MD2, MD4 and RIPEMD-160 are not there: kCCUnimplemented.
 */

#include <stdlib.h>
#include <string.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptoError.h>
#include <CommonCrypto/CommonDigestSPI.h>

struct digest_ctx {
	CCDigestAlgorithm alg;
	union {
		CC_MD5_CTX md5;
		CC_SHA1_CTX sha1;
		CC_SHA256_CTX sha256;
		CC_SHA512_CTX sha512;
	} u;
};

_Static_assert(sizeof(struct digest_ctx) <= sizeof(CCDigestCtx),
    "a digest context must fit in CCDigestCtx");

#define CTX(ref) ((struct digest_ctx *)(void *)(ref)->context)

size_t
CCDigestGetOutputSize(CCDigestAlgorithm algorithm)
{
	switch (algorithm) {
	case kCCDigestMD5:	return CC_MD5_DIGEST_LENGTH;
	case kCCDigestSHA1:	return CC_SHA1_DIGEST_LENGTH;
	case kCCDigestSHA224:	return CC_SHA224_DIGEST_LENGTH;
	case kCCDigestSHA256:	return CC_SHA256_DIGEST_LENGTH;
	case kCCDigestSHA384:	return CC_SHA384_DIGEST_LENGTH;
	case kCCDigestSHA512:	return CC_SHA512_DIGEST_LENGTH;
	default:		return 0;
	}
}

size_t
CCDigestGetBlockSize(CCDigestAlgorithm algorithm)
{
	switch (algorithm) {
	case kCCDigestMD5:	return CC_MD5_BLOCK_BYTES;
	case kCCDigestSHA1:	return CC_SHA1_BLOCK_BYTES;
	case kCCDigestSHA224:	return CC_SHA224_BLOCK_BYTES;
	case kCCDigestSHA256:	return CC_SHA256_BLOCK_BYTES;
	case kCCDigestSHA384:	return CC_SHA384_BLOCK_BYTES;
	case kCCDigestSHA512:	return CC_SHA512_BLOCK_BYTES;
	default:		return 0;
	}
}

int
CCDigestInit(CCDigestAlgorithm algorithm, CCDigestRef ref)
{
	struct digest_ctx *c;

	if (ref == NULL)
		return kCCParamError;
	c = CTX(ref);
	memset(ref, 0, sizeof(*ref));
	c->alg = algorithm;
	switch (algorithm) {
	case kCCDigestMD5:	CC_MD5_Init(&c->u.md5); break;
	case kCCDigestSHA1:	CC_SHA1_Init(&c->u.sha1); break;
	case kCCDigestSHA224:	CC_SHA224_Init(&c->u.sha256); break;
	case kCCDigestSHA256:	CC_SHA256_Init(&c->u.sha256); break;
	case kCCDigestSHA384:	CC_SHA384_Init(&c->u.sha512); break;
	case kCCDigestSHA512:	CC_SHA512_Init(&c->u.sha512); break;
	default:		return kCCUnimplemented;
	}
	return kCCSuccess;
}

int
CCDigestUpdate(CCDigestRef ref, const void *data, size_t length)
{
	struct digest_ctx *c;

	if (ref == NULL || (data == NULL && length != 0))
		return kCCParamError;
	c = CTX(ref);
	while (length > 0) {
		CC_LONG n = length > 0x40000000 ? 0x40000000 : (CC_LONG)length;

		switch (c->alg) {
		case kCCDigestMD5:	CC_MD5_Update(&c->u.md5, data, n); break;
		case kCCDigestSHA1:	CC_SHA1_Update(&c->u.sha1, data, n); break;
		case kCCDigestSHA224:	CC_SHA224_Update(&c->u.sha256, data, n); break;
		case kCCDigestSHA256:	CC_SHA256_Update(&c->u.sha256, data, n); break;
		case kCCDigestSHA384:	CC_SHA384_Update(&c->u.sha512, data, n); break;
		case kCCDigestSHA512:	CC_SHA512_Update(&c->u.sha512, data, n); break;
		default:		return kCCUnimplemented;
		}
		data = (const uint8_t *)data + n;
		length -= n;
	}
	return kCCSuccess;
}

int
CCDigestFinal(CCDigestRef ref, uint8_t *output)
{
	struct digest_ctx *c;

	if (ref == NULL || output == NULL)
		return kCCParamError;
	c = CTX(ref);
	switch (c->alg) {
	case kCCDigestMD5:	CC_MD5_Final(output, &c->u.md5); break;
	case kCCDigestSHA1:	CC_SHA1_Final(output, &c->u.sha1); break;
	case kCCDigestSHA224:	CC_SHA224_Final(output, &c->u.sha256); break;
	case kCCDigestSHA256:	CC_SHA256_Final(output, &c->u.sha256); break;
	case kCCDigestSHA384:	CC_SHA384_Final(output, &c->u.sha512); break;
	case kCCDigestSHA512:	CC_SHA512_Final(output, &c->u.sha512); break;
	default:		return kCCUnimplemented;
	}
	return kCCSuccess;
}

int
CCDigest(CCDigestAlgorithm algorithm, const uint8_t *data, size_t length,
    uint8_t *output)
{
	CCDigestCtx ctx;
	int r;

	if ((r = CCDigestInit(algorithm, &ctx)) != kCCSuccess ||
	    (r = CCDigestUpdate(&ctx, data, length)) != kCCSuccess)
		return r;
	r = CCDigestFinal(&ctx, output);
	memset(&ctx, 0, sizeof(ctx));
	return r;
}

CCDigestRef
CCDigestCreate(CCDigestAlgorithm alg)
{
	CCDigestRef ref = malloc(sizeof(*ref));

	if (ref == NULL)
		return NULL;
	if (CCDigestInit(alg, ref) != kCCSuccess) {
		free(ref);
		return NULL;
	}
	return ref;
}

void
CCDigestReset(CCDigestRef ref)
{
	if (ref != NULL)
		(void)CCDigestInit(CTX(ref)->alg, ref);
}

void
CCDigestDestroy(CCDigestRef ref)
{
	if (ref == NULL)
		return;
	memset(ref, 0, sizeof(*ref));
	free(ref);
}

size_t
CCDigestGetOutputSizeFromRef(CCDigestRef ref)
{
	return ref ? CCDigestGetOutputSize(CTX(ref)->alg) : 0;
}

size_t
CCDigestGetBlockSizeFromRef(CCDigestRef ref)
{
	return ref ? CCDigestGetBlockSize(CTX(ref)->alg) : 0;
}

/* The reference's alias. */
size_t
CCDigestOutputSize(CCDigestRef ref)
{
	return CCDigestGetOutputSizeFromRef(ref);
}

size_t
CCDigestBlockSize(CCDigestRef ref)
{
	return CCDigestGetBlockSizeFromRef(ref);
}
