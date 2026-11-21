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

/* SHA-1's compression function, from FIPS 180-4, section 6.1. */

#include <stddef.h>
#include <stdint.h>
#include <corecrypto/ccsha1.h>
#include "digest.h"

static inline uint32_t
rotl(uint32_t x, unsigned n)
{
	return (x << n) | (x >> (32 - n));
}

static void
sha1_compress(ccdigest_state_t state, size_t nblocks, const void *data)
{
	uint32_t *h = ccdigest_u32(state);
	const uint8_t *p = data;

	while (nblocks--) {
		uint32_t w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];

		for (int t = 0; t < 16; t++)
			w[t] = (uint32_t)p[4 * t] << 24 | (uint32_t)p[4 * t + 1] << 16 |
			    (uint32_t)p[4 * t + 2] << 8 | (uint32_t)p[4 * t + 3];
		for (int t = 16; t < 80; t++)
			w[t] = rotl(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
		for (int t = 0; t < 80; t++) {
			uint32_t f, k;

			if (t < 20) {
				f = (b & c) | (~b & d);
				k = 0x5a827999U;
			} else if (t < 40) {
				f = b ^ c ^ d;
				k = 0x6ed9eba1U;
			} else if (t < 60) {
				f = (b & c) | (b & d) | (c & d);
				k = 0x8f1bbcdcU;
			} else {
				f = b ^ c ^ d;
				k = 0xca62c1d6U;
			}
			uint32_t tmp = rotl(a, 5) + f + e + k + w[t];
			e = d;
			d = c;
			c = rotl(b, 30);
			b = a;
			a = tmp;
		}
		h[0] += a;
		h[1] += b;
		h[2] += c;
		h[3] += d;
		h[4] += e;
		p += CCSHA1_BLOCK_SIZE;
	}
}

static const uint32_t sha1_initial_state[5] = {
	0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U, 0xc3d2e1f0U,
};

const struct ccdigest_info ccsha1_ltc_di = {
	.output_size = CCSHA1_OUTPUT_SIZE,
	.state_size = CCSHA1_STATE_SIZE,
	.block_size = CCSHA1_BLOCK_SIZE,
	.oid_size = ccoid_sha1_len,
	.oid = (const unsigned char *)CC_DIGEST_OID_SHA1,
	.initial_state = sha1_initial_state,
	.compress = sha1_compress,
	.final = mcc_digest_final_be,
};

const struct ccdigest_info ccsha1_eay_di = {
	.output_size = CCSHA1_OUTPUT_SIZE,
	.state_size = CCSHA1_STATE_SIZE,
	.block_size = CCSHA1_BLOCK_SIZE,
	.oid_size = ccoid_sha1_len,
	.oid = (const unsigned char *)CC_DIGEST_OID_SHA1,
	.initial_state = sha1_initial_state,
	.compress = sha1_compress,
	.final = mcc_digest_final_be,
};

const struct ccdigest_info *
ccsha1_di(void)
{
	return &ccsha1_ltc_di;
}
