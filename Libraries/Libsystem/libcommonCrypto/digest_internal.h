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

/* The digests, as a table HMAC can drive (digest.c, hmac.c). */

#ifndef _CC_DIGEST_INTERNAL_H_
#define _CC_DIGEST_INTERNAL_H_

#include <CommonCrypto/CommonDigest.h>
#include <stddef.h>

#define CC_MAX_DIGEST_LENGTH	CC_SHA512_DIGEST_LENGTH
#define CC_MAX_BLOCK_BYTES	CC_SHA512_BLOCK_BYTES

struct cc_digest_info {
	int	(*init)(void *ctx);
	int	(*update)(void *ctx, const void *data, CC_LONG len);
	int	(*final)(unsigned char *md, void *ctx);
	size_t	ctx_size;
	size_t	digest_length;
	size_t	block_bytes;
};

extern const struct cc_digest_info _cc_digest_md5, _cc_digest_sha1,
    _cc_digest_sha224, _cc_digest_sha256, _cc_digest_sha384,
    _cc_digest_sha512;

/* Update with a size_t length, in CC_LONG pieces. */
void _cc_digest_update_sized(const struct cc_digest_info *d, void *ctx,
    const void *data, size_t len);

#endif /* !_CC_DIGEST_INTERNAL_H_ */
