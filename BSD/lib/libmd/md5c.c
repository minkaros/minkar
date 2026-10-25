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

/* MD5, from the algorithm's description in RFC 1321. */

#include <string.h>

#include "md5.h"

static const uint32_t T[64] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

static const unsigned char S[64] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
};

#define	ROTL(x, n)	(((x) << (n)) | ((x) >> (32 - (n))))

static void
md5_block(uint32_t state[4], const unsigned char block[64])
{
	uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
	uint32_t m[16], f, t;
	int i, g;

	for (i = 0; i < 16; i++)
		m[i] = (uint32_t)block[4 * i] | (uint32_t)block[4 * i + 1] << 8 |
		    (uint32_t)block[4 * i + 2] << 16 | (uint32_t)block[4 * i + 3] << 24;
	for (i = 0; i < 64; i++) {
		if (i < 16) {
			f = (b & c) | (~b & d);
			g = i;
		} else if (i < 32) {
			f = (d & b) | (~d & c);
			g = (5 * i + 1) % 16;
		} else if (i < 48) {
			f = b ^ c ^ d;
			g = (3 * i + 5) % 16;
		} else {
			f = c ^ (b | ~d);
			g = (7 * i) % 16;
		}
		t = d;
		d = c;
		c = b;
		b = b + ROTL(a + f + T[i] + m[g], S[i]);
		a = t;
	}
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

void
MD5Init(MD5_CTX *ctx)
{
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xefcdab89;
	ctx->state[2] = 0x98badcfe;
	ctx->state[3] = 0x10325476;
	ctx->count[0] = ctx->count[1] = 0;
}

void
MD5Update(MD5_CTX *ctx, const void *in, unsigned int len)
{
	const unsigned char *p = in;
	unsigned int used = (ctx->count[0] >> 3) & 63;
	uint32_t bits = (uint32_t)len << 3;

	if ((ctx->count[0] += bits) < bits)
		ctx->count[1]++;
	ctx->count[1] += (uint32_t)(len >> 29);
	while (len > 0) {
		unsigned int n = 64 - used < len ? 64 - used : len;

		memcpy(ctx->buffer + used, p, n);
		used += n;
		p += n;
		len -= n;
		if (used == 64) {
			md5_block(ctx->state, ctx->buffer);
			used = 0;
		}
	}
}

void
MD5Final(unsigned char digest[MD5_DIGEST_LENGTH], MD5_CTX *ctx)
{
	unsigned char lenbytes[8], pad[64] = { 0x80 };
	unsigned int used = (ctx->count[0] >> 3) & 63;
	int i;

	for (i = 0; i < 4; i++) {
		lenbytes[i] = (unsigned char)(ctx->count[0] >> (8 * i));
		lenbytes[4 + i] = (unsigned char)(ctx->count[1] >> (8 * i));
	}
	MD5Update(ctx, pad, used < 56 ? 56 - used : 120 - used);
	MD5Update(ctx, lenbytes, 8);
	for (i = 0; i < 16; i++)
		digest[i] = (unsigned char)(ctx->state[i / 4] >> (8 * (i % 4)));
	memset(ctx, 0, sizeof(*ctx));
}
