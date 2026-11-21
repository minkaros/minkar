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

/* MD5's compression function, from RFC 1321. */

#include <stddef.h>
#include <stdint.h>
#include <corecrypto/ccmd5.h>
#include "digest.h"

static const uint32_t T[64] = {
	0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU,
	0xf57c0fafU, 0x4787c62aU, 0xa8304613U, 0xfd469501U,
	0x698098d8U, 0x8b44f7afU, 0xffff5bb1U, 0x895cd7beU,
	0x6b901122U, 0xfd987193U, 0xa679438eU, 0x49b40821U,
	0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU,
	0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U,
	0x21e1cde6U, 0xc33707d6U, 0xf4d50d87U, 0x455a14edU,
	0xa9e3e905U, 0xfcefa3f8U, 0x676f02d9U, 0x8d2a4c8aU,
	0xfffa3942U, 0x8771f681U, 0x6d9d6122U, 0xfde5380cU,
	0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
	0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U,
	0xd9d4d039U, 0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U,
	0xf4292244U, 0x432aff97U, 0xab9423a7U, 0xfc93a039U,
	0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU, 0x85845dd1U,
	0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
	0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U,
};

static const uint8_t S[4][4] = {
	{ 7, 12, 17, 22 }, { 5, 9, 14, 20 }, { 4, 11, 16, 23 }, { 6, 10, 15, 21 },
};

static inline uint32_t
rotl(uint32_t x, unsigned n)
{
	return (x << n) | (x >> (32 - n));
}

static void
md5_compress(ccdigest_state_t state, size_t nblocks, const void *data)
{
	uint32_t *h = ccdigest_u32(state);
	const uint8_t *p = data;

	while (nblocks--) {
		uint32_t m[16], a = h[0], b = h[1], c = h[2], d = h[3];

		for (int i = 0; i < 16; i++)
			m[i] = (uint32_t)p[4 * i] | (uint32_t)p[4 * i + 1] << 8 |
			    (uint32_t)p[4 * i + 2] << 16 | (uint32_t)p[4 * i + 3] << 24;
		for (int i = 0; i < 64; i++) {
			uint32_t f;
			int g;

			switch (i / 16) {
			case 0: f = (b & c) | (~b & d); g = i; break;
			case 1: f = (d & b) | (~d & c); g = (5 * i + 1) % 16; break;
			case 2: f = b ^ c ^ d; g = (3 * i + 5) % 16; break;
			default: f = c ^ (b | ~d); g = (7 * i) % 16; break;
			}
			f = a + f + T[i] + m[g];
			a = d;
			d = c;
			c = b;
			b = b + rotl(f, S[i / 16][i % 4]);
		}
		h[0] += a;
		h[1] += b;
		h[2] += c;
		h[3] += d;
		p += CCMD5_BLOCK_SIZE;
	}
}

static const uint32_t md5_initial_state[4] = {
	0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U,
};

const struct ccdigest_info pdcmd5_di = {
	.output_size = CCMD5_OUTPUT_SIZE,
	.state_size = CCMD5_STATE_SIZE,
	.block_size = CCMD5_BLOCK_SIZE,
	.oid_size = 10,
	.oid = (const unsigned char *)CC_DIGEST_OID_MD5,
	.initial_state = md5_initial_state,
	.compress = md5_compress,
	.final = mcc_digest_final_le,
};
