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
 * HMAC (RFC 2104) over the CommonCrypto digests: <CommonCrypto/CommonHMAC.h>.
 *
 * CCHmacContext holds the algorithm, the inner digest's context and the
 * key block K0; CCHmacFinal finishes the inner digest and computes the
 * outer one, H((K0 ^ opad) || inner).
 */

#include <CommonCrypto/CommonHMAC.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "digest_internal.h"

struct hmac_state {
	CCHmacAlgorithm		algorithm;
	unsigned char		key[CC_MAX_BLOCK_BYTES];	/* K0 */
	alignas(8) unsigned char inner[sizeof(CC_SHA512_CTX)];
};

_Static_assert(sizeof(struct hmac_state) <= sizeof(CCHmacContext),
    "HMAC state does not fit in CCHmacContext");
_Static_assert(sizeof(CC_SHA512_CTX) >= sizeof(CC_SHA256_CTX) &&
    sizeof(CC_SHA512_CTX) >= sizeof(CC_SHA1_CTX) &&
    sizeof(CC_SHA512_CTX) >= sizeof(CC_MD5_CTX),
    "SHA-512's context is not the largest");

static const struct cc_digest_info *
digest_for(CCHmacAlgorithm algorithm)
{
	switch (algorithm) {
	case kCCHmacAlgSHA1:	return &_cc_digest_sha1;
	case kCCHmacAlgMD5:	return &_cc_digest_md5;
	case kCCHmacAlgSHA256:	return &_cc_digest_sha256;
	case kCCHmacAlgSHA384:	return &_cc_digest_sha384;
	case kCCHmacAlgSHA512:	return &_cc_digest_sha512;
	case kCCHmacAlgSHA224:	return &_cc_digest_sha224;
	default:		return NULL;
	}
}

static struct hmac_state *
state(CCHmacContext *ctx)
{
	return (struct hmac_state *)(void *)ctx->ctx;
}

void
CCHmacInit(CCHmacContext *ctx, CCHmacAlgorithm algorithm, const void *key,
    size_t keyLength)
{
	struct hmac_state *s = state(ctx);
	const struct cc_digest_info *d = digest_for(algorithm);
	unsigned char ipad[CC_MAX_BLOCK_BYTES];
	size_t i;

	memset(ctx, 0, sizeof(*ctx));
	s->algorithm = algorithm;
	if (d == NULL)
		return;		/* CCHmacUpdate and CCHmacFinal do nothing */

	/* K0: the key, hashed first if longer than a block, zero padded. */
	if (keyLength > d->block_bytes) {
		d->init(s->inner);
		_cc_digest_update_sized(d, s->inner, key, keyLength);
		d->final(s->key, s->inner);
	} else if (keyLength > 0) {
		memcpy(s->key, key, keyLength);
	}

	for (i = 0; i < d->block_bytes; i++)
		ipad[i] = s->key[i] ^ 0x36;
	d->init(s->inner);
	d->update(s->inner, ipad, (CC_LONG)d->block_bytes);
	memset(ipad, 0, sizeof(ipad));
}

void
CCHmacUpdate(CCHmacContext *ctx, const void *data, size_t dataLength)
{
	struct hmac_state *s = state(ctx);
	const struct cc_digest_info *d = digest_for(s->algorithm);

	if (d != NULL)
		_cc_digest_update_sized(d, s->inner, data, dataLength);
}

void
CCHmacFinal(CCHmacContext *ctx, void *macOut)
{
	struct hmac_state *s = state(ctx);
	const struct cc_digest_info *d = digest_for(s->algorithm);
	alignas(8) unsigned char outer[sizeof(CC_SHA512_CTX)];
	unsigned char opad[CC_MAX_BLOCK_BYTES], ihash[CC_MAX_DIGEST_LENGTH];
	size_t i;

	if (d == NULL)
		return;
	d->final(ihash, s->inner);
	for (i = 0; i < d->block_bytes; i++)
		opad[i] = s->key[i] ^ 0x5c;
	d->init(outer);
	d->update(outer, opad, (CC_LONG)d->block_bytes);
	d->update(outer, ihash, (CC_LONG)d->digest_length);
	d->final(macOut, outer);

	memset(opad, 0, sizeof(opad));
	memset(ihash, 0, sizeof(ihash));
	memset(ctx, 0, sizeof(*ctx));
}

void
CCHmac(CCHmacAlgorithm algorithm, const void *key, size_t keyLength,
    const void *data, size_t dataLength, void *macOut)
{
	CCHmacContext ctx;

	CCHmacInit(&ctx, algorithm, key, keyLength);
	CCHmacUpdate(&ctx, data, dataLength);
	CCHmacFinal(&ctx, macOut);
}
