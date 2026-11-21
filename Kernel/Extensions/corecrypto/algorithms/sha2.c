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
 * SHA-256, SHA-384 and SHA-512's compression functions, from FIPS 180-4,
 * sections 6.2 and 6.4. The constants are the fractional parts of the
 * square and cube roots of the first primes (section 4.2.2, 4.2.3, 5.3),
 * computed for Minkar from that definition.
 */

#include <stddef.h>
#include <stdint.h>
#include <corecrypto/ccsha2.h>
#include "digest.h"

static const uint32_t K256[64] = {
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
	0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
	0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
	0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
	0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
	0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
	0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
	0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
	0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
	0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
	0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
	0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
	0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
	0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
	0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

static const uint64_t K512[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
	0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
	0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
	0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
	0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
	0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
	0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
	0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
	0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
	0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
	0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
	0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
	0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
	0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

static inline uint32_t
ror32(uint32_t x, unsigned n)
{
	return (x >> n) | (x << (32 - n));
}

static inline uint64_t
ror64(uint64_t x, unsigned n)
{
	return (x >> n) | (x << (64 - n));
}

static void
sha256_compress(ccdigest_state_t state, size_t nblocks, const void *data)
{
	uint32_t *h = ccdigest_u32(state);
	const uint8_t *p = data;

	while (nblocks--) {
		uint32_t w[64], v[8];

		for (int t = 0; t < 16; t++)
			w[t] = (uint32_t)p[4 * t] << 24 | (uint32_t)p[4 * t + 1] << 16 |
			    (uint32_t)p[4 * t + 2] << 8 | (uint32_t)p[4 * t + 3];
		for (int t = 16; t < 64; t++) {
			uint32_t s0 = ror32(w[t - 15], 7) ^ ror32(w[t - 15], 18) ^ (w[t - 15] >> 3);
			uint32_t s1 = ror32(w[t - 2], 17) ^ ror32(w[t - 2], 19) ^ (w[t - 2] >> 10);
			w[t] = w[t - 16] + s0 + w[t - 7] + s1;
		}
		for (int i = 0; i < 8; i++)
			v[i] = h[i];
		for (int t = 0; t < 64; t++) {
			uint32_t S1 = ror32(v[4], 6) ^ ror32(v[4], 11) ^ ror32(v[4], 25);
			uint32_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
			uint32_t t1 = v[7] + S1 + ch + K256[t] + w[t];
			uint32_t S0 = ror32(v[0], 2) ^ ror32(v[0], 13) ^ ror32(v[0], 22);
			uint32_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
			uint32_t t2 = S0 + maj;

			v[7] = v[6];
			v[6] = v[5];
			v[5] = v[4];
			v[4] = v[3] + t1;
			v[3] = v[2];
			v[2] = v[1];
			v[1] = v[0];
			v[0] = t1 + t2;
		}
		for (int i = 0; i < 8; i++)
			h[i] += v[i];
		p += CCSHA256_BLOCK_SIZE;
	}
}

static void
sha512_compress(ccdigest_state_t state, size_t nblocks, const void *data)
{
	uint64_t *h = ccdigest_u64(state);
	const uint8_t *p = data;

	while (nblocks--) {
		uint64_t w[80], v[8];

		for (int t = 0; t < 16; t++) {
			w[t] = 0;
			for (int j = 0; j < 8; j++)
				w[t] = w[t] << 8 | p[8 * t + j];
		}
		for (int t = 16; t < 80; t++) {
			uint64_t s0 = ror64(w[t - 15], 1) ^ ror64(w[t - 15], 8) ^ (w[t - 15] >> 7);
			uint64_t s1 = ror64(w[t - 2], 19) ^ ror64(w[t - 2], 61) ^ (w[t - 2] >> 6);
			w[t] = w[t - 16] + s0 + w[t - 7] + s1;
		}
		for (int i = 0; i < 8; i++)
			v[i] = h[i];
		for (int t = 0; t < 80; t++) {
			uint64_t S1 = ror64(v[4], 14) ^ ror64(v[4], 18) ^ ror64(v[4], 41);
			uint64_t ch = (v[4] & v[5]) ^ (~v[4] & v[6]);
			uint64_t t1 = v[7] + S1 + ch + K512[t] + w[t];
			uint64_t S0 = ror64(v[0], 28) ^ ror64(v[0], 34) ^ ror64(v[0], 39);
			uint64_t maj = (v[0] & v[1]) ^ (v[0] & v[2]) ^ (v[1] & v[2]);
			uint64_t t2 = S0 + maj;

			v[7] = v[6];
			v[6] = v[5];
			v[5] = v[4];
			v[4] = v[3] + t1;
			v[3] = v[2];
			v[2] = v[1];
			v[1] = v[0];
			v[0] = t1 + t2;
		}
		for (int i = 0; i < 8; i++)
			h[i] += v[i];
		p += CCSHA512_BLOCK_SIZE;
	}
}

static const uint32_t sha256_initial_state[8] = {
	0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
	0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
};

static const uint64_t sha384_initial_state[8] = {
	0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL,
	0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
	0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL,
	0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL,
};

static const uint64_t sha512_initial_state[8] = {
	0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
	0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
	0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
	0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
};

const struct ccdigest_info ccsha256_ltc_di = {
	.output_size = CCSHA256_OUTPUT_SIZE,
	.state_size = CCSHA256_STATE_SIZE,
	.block_size = CCSHA256_BLOCK_SIZE,
	.oid_size = ccoid_sha256_len,
	.oid = (const unsigned char *)CC_DIGEST_OID_SHA256,
	.initial_state = sha256_initial_state,
	.compress = sha256_compress,
	.final = mcc_digest_final_be,
};

const struct ccdigest_info ccsha384_ltc_di = {
	.output_size = 48,
	.state_size = CCSHA512_STATE_SIZE,
	.block_size = CCSHA512_BLOCK_SIZE,
	.oid_size = ccoid_sha384_len,
	.oid = (const unsigned char *)CC_DIGEST_OID_SHA384,
	.initial_state = sha384_initial_state,
	.compress = sha512_compress,
	.final = mcc_digest_final_be,
};

const struct ccdigest_info ccsha512_ltc_di = {
	.output_size = CCSHA512_OUTPUT_SIZE,
	.state_size = CCSHA512_STATE_SIZE,
	.block_size = CCSHA512_BLOCK_SIZE,
	.oid_size = ccoid_sha512_len,
	.oid = (const unsigned char *)CC_DIGEST_OID_SHA512,
	.initial_state = sha512_initial_state,
	.compress = sha512_compress,
	.final = mcc_digest_final_be,
};

const struct ccdigest_info *
ccsha256_di(void)
{
	return &ccsha256_ltc_di;
}

const struct ccdigest_info *
ccsha384_di(void)
{
	return &ccsha384_ltc_di;
}

const struct ccdigest_info *
ccsha512_di(void)
{
	return &ccsha512_ltc_di;
}
