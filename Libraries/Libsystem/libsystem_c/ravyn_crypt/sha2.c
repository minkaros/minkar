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
 * SHA-256 and SHA-512 (FIPS 180-4) for crypt(3)'s "$5$" and "$6$" hashes,
 * with the libmd interface the SHA-crypt code expects. Internal to libc:
 * sha256.h and sha512.h rename the functions and hide them.
 */

#include <stdint.h>
#include <string.h>

#include "sha256.h"
#include "sha512.h"

/* SHA-256 */

static const uint32_t K256[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
	0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
	0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
	0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
	0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
	0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static inline uint32_t
ror32(uint32_t x, int n)
{
	return (x >> n) | (x << (32 - n));
}

static inline uint32_t
load_be32(const unsigned char *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void
store_be32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)(v >> 24);
	p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);
	p[3] = (unsigned char)v;
}

static void
sha256_block(SHA256_CTX *ctx, const unsigned char *block)
{
	uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = load_be32(block + 4 * i);
	for (; i < 64; i++) {
		uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
		uint32_t s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
	e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
	for (i = 0; i < 64; i++) {
		t1 = h + (ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25)) +
		    ((e & f) ^ (~e & g)) + K256[i] + w[i];
		t2 = (ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22)) +
		    ((a & b) ^ (a & c) ^ (b & c));
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}
	ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
	ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void
SHA256_Init(SHA256_CTX *ctx)
{
	static const uint32_t init[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
	};

	memcpy(ctx->state, init, sizeof(init));
	ctx->count = 0;
}

void
SHA256_Update(SHA256_CTX *ctx, const void *data, size_t len)
{
	const unsigned char *p = data;
	size_t used = (size_t)(ctx->count % SHA256_BLOCK_LENGTH);

	ctx->count += len;
	if (used != 0) {
		size_t room = SHA256_BLOCK_LENGTH - used;
		if (len < room) {
			memcpy(ctx->buf + used, p, len);
			return;
		}
		memcpy(ctx->buf + used, p, room);
		sha256_block(ctx, ctx->buf);
		p += room;
		len -= room;
	}
	for (; len >= SHA256_BLOCK_LENGTH; p += SHA256_BLOCK_LENGTH, len -= SHA256_BLOCK_LENGTH)
		sha256_block(ctx, p);
	memcpy(ctx->buf, p, len);
}

void
SHA256_Final(unsigned char digest[SHA256_DIGEST_LENGTH], SHA256_CTX *ctx)
{
	uint64_t bits = ctx->count * 8;
	size_t used = (size_t)(ctx->count % SHA256_BLOCK_LENGTH);
	int i;

	ctx->buf[used++] = 0x80;
	if (used > SHA256_BLOCK_LENGTH - 8) {
		memset(ctx->buf + used, 0, SHA256_BLOCK_LENGTH - used);
		sha256_block(ctx, ctx->buf);
		used = 0;
	}
	memset(ctx->buf + used, 0, SHA256_BLOCK_LENGTH - 8 - used);
	store_be32(ctx->buf + 56, (uint32_t)(bits >> 32));
	store_be32(ctx->buf + 60, (uint32_t)bits);
	sha256_block(ctx, ctx->buf);
	for (i = 0; i < 8; i++)
		store_be32(digest + 4 * i, ctx->state[i]);
	memset(ctx, 0, sizeof(*ctx));
}

/* SHA-512 */

static const uint64_t K512[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
	0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
	0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
	0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
	0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
	0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
	0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
	0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
	0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
	0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
	0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
	0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
	0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
	0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
};

static inline uint64_t
ror64(uint64_t x, int n)
{
	return (x >> n) | (x << (64 - n));
}

static inline uint64_t
load_be64(const unsigned char *p)
{
	return ((uint64_t)load_be32(p) << 32) | load_be32(p + 4);
}

static inline void
store_be64(unsigned char *p, uint64_t v)
{
	store_be32(p, (uint32_t)(v >> 32));
	store_be32(p + 4, (uint32_t)v);
}

static void
sha512_block(SHA512_CTX *ctx, const unsigned char *block)
{
	uint64_t w[80], a, b, c, d, e, f, g, h, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = load_be64(block + 8 * i);
	for (; i < 80; i++) {
		uint64_t s0 = ror64(w[i - 15], 1) ^ ror64(w[i - 15], 8) ^ (w[i - 15] >> 7);
		uint64_t s1 = ror64(w[i - 2], 19) ^ ror64(w[i - 2], 61) ^ (w[i - 2] >> 6);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
	e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];
	for (i = 0; i < 80; i++) {
		t1 = h + (ror64(e, 14) ^ ror64(e, 18) ^ ror64(e, 41)) +
		    ((e & f) ^ (~e & g)) + K512[i] + w[i];
		t2 = (ror64(a, 28) ^ ror64(a, 34) ^ ror64(a, 39)) +
		    ((a & b) ^ (a & c) ^ (b & c));
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}
	ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
	ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void
SHA512_Init(SHA512_CTX *ctx)
{
	static const uint64_t init[8] = {
		0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL,
		0xa54ff53a5f1d36f1ULL, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
		0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
	};

	memcpy(ctx->state, init, sizeof(init));
	ctx->count = 0;
}

void
SHA512_Update(SHA512_CTX *ctx, const void *data, size_t len)
{
	const unsigned char *p = data;
	size_t used = (size_t)(ctx->count % SHA512_BLOCK_LENGTH);

	ctx->count += len;
	if (used != 0) {
		size_t room = SHA512_BLOCK_LENGTH - used;
		if (len < room) {
			memcpy(ctx->buf + used, p, len);
			return;
		}
		memcpy(ctx->buf + used, p, room);
		sha512_block(ctx, ctx->buf);
		p += room;
		len -= room;
	}
	for (; len >= SHA512_BLOCK_LENGTH; p += SHA512_BLOCK_LENGTH, len -= SHA512_BLOCK_LENGTH)
		sha512_block(ctx, p);
	memcpy(ctx->buf, p, len);
}

void
SHA512_Final(unsigned char digest[SHA512_DIGEST_LENGTH], SHA512_CTX *ctx)
{
	/* Message lengths here never reach 2^61 bytes: the high 64 bits are 0. */
	uint64_t bits = ctx->count * 8;
	size_t used = (size_t)(ctx->count % SHA512_BLOCK_LENGTH);
	int i;

	ctx->buf[used++] = 0x80;
	if (used > SHA512_BLOCK_LENGTH - 16) {
		memset(ctx->buf + used, 0, SHA512_BLOCK_LENGTH - used);
		sha512_block(ctx, ctx->buf);
		used = 0;
	}
	memset(ctx->buf + used, 0, SHA512_BLOCK_LENGTH - 8 - used);
	store_be64(ctx->buf + 120, bits);
	sha512_block(ctx, ctx->buf);
	for (i = 0; i < 8; i++)
		store_be64(digest + 8 * i, ctx->state[i]);
	memset(ctx, 0, sizeof(*ctx));
}
