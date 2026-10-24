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
 * CommonCrypto's message digests (<CommonCrypto/CommonDigest.h>): MD5
 * (RFC 1321), SHA-1, SHA-224, SHA-256, SHA-384 and SHA-512 (FIPS 180-4).
 *
 * The state lives in the public context structures:
 *   MD5, SHA-1   Nl/Nh: message length in bits; data: the partial block;
 *                num: bytes in it.
 *   SHA-224/256  count: length in bits (low, high word); hash; wbuf: the
 *                partial block (count says how full it is).
 *   SHA-384/512  likewise, with 64-bit words and 128-byte blocks.
 */

#include <CommonCrypto/CommonDigest.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "digest_tables.h"
#include "digest_internal.h"

static inline uint32_t rol32(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }
static inline uint32_t ror32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static inline uint64_t ror64(uint64_t x, int n) { return (x >> n) | (x << (64 - n)); }

static inline uint32_t
le32(const unsigned char *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
	    (uint32_t)p[3] << 24;
}

static inline uint32_t
be32(const unsigned char *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
	    (uint32_t)p[3];
}

static inline uint64_t
be64(const unsigned char *p)
{
	return (uint64_t)be32(p) << 32 | be32(p + 4);
}

static inline void
put_le32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)v;
	p[1] = (unsigned char)(v >> 8);
	p[2] = (unsigned char)(v >> 16);
	p[3] = (unsigned char)(v >> 24);
}

static inline void
put_be32(unsigned char *p, uint32_t v)
{
	p[0] = (unsigned char)(v >> 24);
	p[1] = (unsigned char)(v >> 16);
	p[2] = (unsigned char)(v >> 8);
	p[3] = (unsigned char)v;
}

static inline void
put_be64(unsigned char *p, uint64_t v)
{
	put_be32(p, (uint32_t)(v >> 32));
	put_be32(p + 4, (uint32_t)v);
}

/* ---------------------------------------------------------------- MD5 */

/* Per-round shift amounts (RFC 1321, section 3.4). */
static const unsigned char md5_S[4][4] = {
	{ 7, 12, 17, 22 }, { 5, 9, 14, 20 }, { 4, 11, 16, 23 }, { 6, 10, 15, 21 }
};

static void
md5_block(CC_MD5_CTX *c, const unsigned char *block)
{
	uint32_t X[16], a = c->A, b = c->B, cc = c->C, d = c->D;
	int i;

	for (i = 0; i < 16; i++)
		X[i] = le32(block + 4 * i);
	for (i = 0; i < 64; i++) {
		int round = i / 16;
		uint32_t f;
		int k;

		switch (round) {
		case 0:  f = (b & cc) | (~b & d);  k = i;                break;
		case 1:  f = (b & d) | (cc & ~d); k = (5 * i + 1) % 16; break;
		case 2:  f = b ^ cc ^ d;          k = (3 * i + 5) % 16; break;
		default: f = cc ^ (b | ~d);       k = (7 * i) % 16;     break;
		}
		f = a + f + md5_T[i] + X[k];
		a = d;
		d = cc;
		cc = b;
		b = b + rol32(f, md5_S[round][i % 4]);
	}
	c->A += a;
	c->B += b;
	c->C += cc;
	c->D += d;
}

int
CC_MD5_Init(CC_MD5_CTX *c)
{
	memset(c, 0, sizeof(*c));
	c->A = 0x67452301U;
	c->B = 0xefcdab89U;
	c->C = 0x98badcfeU;
	c->D = 0x10325476U;
	return 1;
}

int
CC_MD5_Update(CC_MD5_CTX *c, const void *data, CC_LONG len)
{
	const unsigned char *p = data;
	unsigned char *buf = (unsigned char *)c->data;
	uint64_t bits = ((uint64_t)c->Nh << 32 | c->Nl) + ((uint64_t)len << 3);

	c->Nl = (CC_LONG)bits;
	c->Nh = (CC_LONG)(bits >> 32);
	while (len > 0) {
		size_t n = CC_MD5_BLOCK_BYTES - (size_t)c->num;

		if (n > len)
			n = len;
		memcpy(buf + c->num, p, n);
		c->num += (int)n;
		p += n;
		len -= (CC_LONG)n;
		if (c->num == CC_MD5_BLOCK_BYTES) {
			md5_block(c, buf);
			c->num = 0;
		}
	}
	return 1;
}

int
CC_MD5_Final(unsigned char *md, CC_MD5_CTX *c)
{
	static const unsigned char pad[CC_MD5_BLOCK_BYTES] = { 0x80 };
	unsigned char length[8];
	size_t padlen;

	put_le32(length, c->Nl);
	put_le32(length + 4, c->Nh);
	padlen = (c->num < 56) ? 56 - (size_t)c->num : 120 - (size_t)c->num;
	CC_MD5_Update(c, pad, (CC_LONG)padlen);
	CC_MD5_Update(c, length, 8);
	put_le32(md, c->A);
	put_le32(md + 4, c->B);
	put_le32(md + 8, c->C);
	put_le32(md + 12, c->D);
	memset(c, 0, sizeof(*c));
	return 1;
}

unsigned char *
CC_MD5(const void *data, CC_LONG len, unsigned char *md)
{
	CC_MD5_CTX c;

	CC_MD5_Init(&c);
	CC_MD5_Update(&c, data, len);
	CC_MD5_Final(md, &c);
	return md;
}

/* The BSD name for CC_MD5_Final (<CommonCrypto/CommonDigest.h>). */
void
MD5Final(unsigned char md[16], CC_MD5_CTX *c)
{
	CC_MD5_Final(md, c);
}

/* -------------------------------------------------------------- SHA-1 */

static void
sha1_block(CC_SHA1_CTX *c, const unsigned char *block)
{
	uint32_t W[80], a = c->h0, b = c->h1, cc = c->h2, d = c->h3, e = c->h4;
	int t;

	for (t = 0; t < 16; t++)
		W[t] = be32(block + 4 * t);
	for (; t < 80; t++)
		W[t] = rol32(W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16], 1);
	for (t = 0; t < 80; t++) {
		uint32_t f, k, tmp;

		if (t < 20) {
			f = (b & cc) | (~b & d);
			k = 0x5a827999U;
		} else if (t < 40) {
			f = b ^ cc ^ d;
			k = 0x6ed9eba1U;
		} else if (t < 60) {
			f = (b & cc) | (b & d) | (cc & d);
			k = 0x8f1bbcdcU;
		} else {
			f = b ^ cc ^ d;
			k = 0xca62c1d6U;
		}
		tmp = rol32(a, 5) + f + e + k + W[t];
		e = d;
		d = cc;
		cc = rol32(b, 30);
		b = a;
		a = tmp;
	}
	c->h0 += a;
	c->h1 += b;
	c->h2 += cc;
	c->h3 += d;
	c->h4 += e;
}

int
CC_SHA1_Init(CC_SHA1_CTX *c)
{
	memset(c, 0, sizeof(*c));
	c->h0 = 0x67452301U;
	c->h1 = 0xefcdab89U;
	c->h2 = 0x98badcfeU;
	c->h3 = 0x10325476U;
	c->h4 = 0xc3d2e1f0U;
	return 1;
}

int
CC_SHA1_Update(CC_SHA1_CTX *c, const void *data, CC_LONG len)
{
	const unsigned char *p = data;
	unsigned char *buf = (unsigned char *)c->data;
	uint64_t bits = ((uint64_t)c->Nh << 32 | c->Nl) + ((uint64_t)len << 3);

	c->Nl = (CC_LONG)bits;
	c->Nh = (CC_LONG)(bits >> 32);
	while (len > 0) {
		size_t n = CC_SHA1_BLOCK_BYTES - (size_t)c->num;

		if (n > len)
			n = len;
		memcpy(buf + c->num, p, n);
		c->num += (int)n;
		p += n;
		len -= (CC_LONG)n;
		if (c->num == CC_SHA1_BLOCK_BYTES) {
			sha1_block(c, buf);
			c->num = 0;
		}
	}
	return 1;
}

int
CC_SHA1_Final(unsigned char *md, CC_SHA1_CTX *c)
{
	static const unsigned char pad[CC_SHA1_BLOCK_BYTES] = { 0x80 };
	unsigned char length[8];
	size_t padlen;

	put_be32(length, c->Nh);
	put_be32(length + 4, c->Nl);
	padlen = (c->num < 56) ? 56 - (size_t)c->num : 120 - (size_t)c->num;
	CC_SHA1_Update(c, pad, (CC_LONG)padlen);
	CC_SHA1_Update(c, length, 8);
	put_be32(md, c->h0);
	put_be32(md + 4, c->h1);
	put_be32(md + 8, c->h2);
	put_be32(md + 12, c->h3);
	put_be32(md + 16, c->h4);
	memset(c, 0, sizeof(*c));
	return 1;
}

unsigned char *
CC_SHA1(const void *data, CC_LONG len, unsigned char *md)
{
	CC_SHA1_CTX c;

	CC_SHA1_Init(&c);
	CC_SHA1_Update(&c, data, len);
	CC_SHA1_Final(md, &c);
	return md;
}

/* ------------------------------------------------------ SHA-224/256 */

static void
sha256_block(CC_SHA256_CTX *c, const unsigned char *block)
{
	uint32_t W[64], s[8];
	int t;

	for (t = 0; t < 16; t++)
		W[t] = be32(block + 4 * t);
	for (; t < 64; t++) {
		uint32_t s0 = ror32(W[t - 15], 7) ^ ror32(W[t - 15], 18) ^ (W[t - 15] >> 3);
		uint32_t s1 = ror32(W[t - 2], 17) ^ ror32(W[t - 2], 19) ^ (W[t - 2] >> 10);
		W[t] = W[t - 16] + s0 + W[t - 7] + s1;
	}
	memcpy(s, c->hash, sizeof(s));
	for (t = 0; t < 64; t++) {
		uint32_t S1 = ror32(s[4], 6) ^ ror32(s[4], 11) ^ ror32(s[4], 25);
		uint32_t ch = (s[4] & s[5]) ^ (~s[4] & s[6]);
		uint32_t t1 = s[7] + S1 + ch + sha256_K[t] + W[t];
		uint32_t S0 = ror32(s[0], 2) ^ ror32(s[0], 13) ^ ror32(s[0], 22);
		uint32_t maj = (s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]);
		uint32_t t2 = S0 + maj;

		memmove(s + 1, s, 7 * sizeof(s[0]));
		s[4] += t1;
		s[0] = t1 + t2;
	}
	for (t = 0; t < 8; t++)
		c->hash[t] += s[t];
}

static void
sha256_init(CC_SHA256_CTX *c, const uint32_t *h0)
{
	memset(c, 0, sizeof(*c));
	memcpy(c->hash, h0, sizeof(c->hash));
}

static void
sha256_update(CC_SHA256_CTX *c, const void *data, size_t len)
{
	const unsigned char *p = data;
	unsigned char *buf = (unsigned char *)c->wbuf;
	uint64_t bits = (uint64_t)c->count[1] << 32 | c->count[0];
	size_t used = (size_t)((bits >> 3) % CC_SHA256_BLOCK_BYTES);

	bits += (uint64_t)len << 3;
	c->count[0] = (CC_LONG)bits;
	c->count[1] = (CC_LONG)(bits >> 32);
	while (len > 0) {
		size_t n = CC_SHA256_BLOCK_BYTES - used;

		if (n > len)
			n = len;
		memcpy(buf + used, p, n);
		used += n;
		p += n;
		len -= n;
		if (used == CC_SHA256_BLOCK_BYTES) {
			sha256_block(c, buf);
			used = 0;
		}
	}
}

static void
sha256_final(unsigned char *md, size_t mdlen, CC_SHA256_CTX *c)
{
	static const unsigned char pad[CC_SHA256_BLOCK_BYTES] = { 0x80 };
	unsigned char length[8];
	size_t used = (size_t)((c->count[0] >> 3) % CC_SHA256_BLOCK_BYTES), i;

	put_be32(length, c->count[1]);
	put_be32(length + 4, c->count[0]);
	sha256_update(c, pad, used < 56 ? 56 - used : 120 - used);
	sha256_update(c, length, 8);
	for (i = 0; i < mdlen / 4; i++)
		put_be32(md + 4 * i, c->hash[i]);
	memset(c, 0, sizeof(*c));
}

int
CC_SHA224_Init(CC_SHA256_CTX *c)
{
	sha256_init(c, sha224_H0);
	return 1;
}

int
CC_SHA224_Update(CC_SHA256_CTX *c, const void *data, CC_LONG len)
{
	sha256_update(c, data, len);
	return 1;
}

int
CC_SHA224_Final(unsigned char *md, CC_SHA256_CTX *c)
{
	sha256_final(md, CC_SHA224_DIGEST_LENGTH, c);
	return 1;
}

unsigned char *
CC_SHA224(const void *data, CC_LONG len, unsigned char *md)
{
	CC_SHA256_CTX c;

	CC_SHA224_Init(&c);
	CC_SHA224_Update(&c, data, len);
	CC_SHA224_Final(md, &c);
	return md;
}

int
CC_SHA256_Init(CC_SHA256_CTX *c)
{
	sha256_init(c, sha256_H0);
	return 1;
}

int
CC_SHA256_Update(CC_SHA256_CTX *c, const void *data, CC_LONG len)
{
	sha256_update(c, data, len);
	return 1;
}

int
CC_SHA256_Final(unsigned char *md, CC_SHA256_CTX *c)
{
	sha256_final(md, CC_SHA256_DIGEST_LENGTH, c);
	return 1;
}

unsigned char *
CC_SHA256(const void *data, CC_LONG len, unsigned char *md)
{
	CC_SHA256_CTX c;

	CC_SHA256_Init(&c);
	CC_SHA256_Update(&c, data, len);
	CC_SHA256_Final(md, &c);
	return md;
}

/* ------------------------------------------------------ SHA-384/512 */

static void
sha512_block(CC_SHA512_CTX *c, const unsigned char *block)
{
	uint64_t W[80], s[8];
	int t;

	for (t = 0; t < 16; t++)
		W[t] = be64(block + 8 * t);
	for (; t < 80; t++) {
		uint64_t s0 = ror64(W[t - 15], 1) ^ ror64(W[t - 15], 8) ^ (W[t - 15] >> 7);
		uint64_t s1 = ror64(W[t - 2], 19) ^ ror64(W[t - 2], 61) ^ (W[t - 2] >> 6);
		W[t] = W[t - 16] + s0 + W[t - 7] + s1;
	}
	memcpy(s, c->hash, sizeof(s));
	for (t = 0; t < 80; t++) {
		uint64_t S1 = ror64(s[4], 14) ^ ror64(s[4], 18) ^ ror64(s[4], 41);
		uint64_t ch = (s[4] & s[5]) ^ (~s[4] & s[6]);
		uint64_t t1 = s[7] + S1 + ch + sha512_K[t] + W[t];
		uint64_t S0 = ror64(s[0], 28) ^ ror64(s[0], 34) ^ ror64(s[0], 39);
		uint64_t maj = (s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]);
		uint64_t t2 = S0 + maj;

		memmove(s + 1, s, 7 * sizeof(s[0]));
		s[4] += t1;
		s[0] = t1 + t2;
	}
	for (t = 0; t < 8; t++)
		c->hash[t] += s[t];
}

static void
sha512_init(CC_SHA512_CTX *c, const uint64_t *h0)
{
	memset(c, 0, sizeof(*c));
	memcpy(c->hash, h0, sizeof(c->hash));
}

static void
sha512_update(CC_SHA512_CTX *c, const void *data, size_t len)
{
	const unsigned char *p = data;
	unsigned char *buf = (unsigned char *)c->wbuf;
	size_t used = (size_t)((c->count[0] >> 3) % CC_SHA512_BLOCK_BYTES);
	uint64_t add = (uint64_t)len << 3;

	c->count[0] += add;
	if (c->count[0] < add)
		c->count[1]++;
	c->count[1] += (uint64_t)len >> 61;
	while (len > 0) {
		size_t n = CC_SHA512_BLOCK_BYTES - used;

		if (n > len)
			n = len;
		memcpy(buf + used, p, n);
		used += n;
		p += n;
		len -= n;
		if (used == CC_SHA512_BLOCK_BYTES) {
			sha512_block(c, buf);
			used = 0;
		}
	}
}

static void
sha512_final(unsigned char *md, size_t mdlen, CC_SHA512_CTX *c)
{
	static const unsigned char pad[CC_SHA512_BLOCK_BYTES] = { 0x80 };
	unsigned char length[16];
	size_t used = (size_t)((c->count[0] >> 3) % CC_SHA512_BLOCK_BYTES), i;

	put_be64(length, c->count[1]);
	put_be64(length + 8, c->count[0]);
	sha512_update(c, pad, used < 112 ? 112 - used : 240 - used);
	sha512_update(c, length, 16);
	for (i = 0; i < mdlen / 8; i++)
		put_be64(md + 8 * i, c->hash[i]);
	memset(c, 0, sizeof(*c));
}

int
CC_SHA384_Init(CC_SHA512_CTX *c)
{
	sha512_init(c, sha384_H0);
	return 1;
}

int
CC_SHA384_Update(CC_SHA512_CTX *c, const void *data, CC_LONG len)
{
	sha512_update(c, data, len);
	return 1;
}

int
CC_SHA384_Final(unsigned char *md, CC_SHA512_CTX *c)
{
	sha512_final(md, CC_SHA384_DIGEST_LENGTH, c);
	return 1;
}

unsigned char *
CC_SHA384(const void *data, CC_LONG len, unsigned char *md)
{
	CC_SHA512_CTX c;

	CC_SHA384_Init(&c);
	CC_SHA384_Update(&c, data, len);
	CC_SHA384_Final(md, &c);
	return md;
}

int
CC_SHA512_Init(CC_SHA512_CTX *c)
{
	sha512_init(c, sha512_H0);
	return 1;
}

int
CC_SHA512_Update(CC_SHA512_CTX *c, const void *data, CC_LONG len)
{
	sha512_update(c, data, len);
	return 1;
}

int
CC_SHA512_Final(unsigned char *md, CC_SHA512_CTX *c)
{
	sha512_final(md, CC_SHA512_DIGEST_LENGTH, c);
	return 1;
}

unsigned char *
CC_SHA512(const void *data, CC_LONG len, unsigned char *md)
{
	CC_SHA512_CTX c;

	CC_SHA512_Init(&c);
	CC_SHA512_Update(&c, data, len);
	CC_SHA512_Final(md, &c);
	return md;
}

/* ------------------------------------------- for HMAC (hmac.c) */

void
_cc_digest_update_sized(const struct cc_digest_info *d, void *ctx,
    const void *data, size_t len)
{
	const unsigned char *p = data;

	/* The public Update functions take a CC_LONG length. */
	while (len > 0) {
		CC_LONG n = len > UINT32_MAX ? UINT32_MAX : (CC_LONG)len;

		d->update(ctx, p, n);
		p += n;
		len -= n;
	}
}

#define DIGEST(name, alg, ctxtype, size, block)				\
static int dig_##name##_init(void *c) { return CC_##alg##_Init(c); }		\
static int dig_##name##_update(void *c, const void *d, CC_LONG n)		\
	{ return CC_##alg##_Update(c, d, n); }				\
static int dig_##name##_final(unsigned char *md, void *c)			\
	{ return CC_##alg##_Final(md, c); }				\
const struct cc_digest_info _cc_digest_##name = {			\
	dig_##name##_init, dig_##name##_update, dig_##name##_final, sizeof(ctxtype), size, block }

DIGEST(md5, MD5, CC_MD5_CTX, CC_MD5_DIGEST_LENGTH, CC_MD5_BLOCK_BYTES);
DIGEST(sha1, SHA1, CC_SHA1_CTX, CC_SHA1_DIGEST_LENGTH, CC_SHA1_BLOCK_BYTES);
DIGEST(sha224, SHA224, CC_SHA256_CTX, CC_SHA224_DIGEST_LENGTH, CC_SHA224_BLOCK_BYTES);
DIGEST(sha256, SHA256, CC_SHA256_CTX, CC_SHA256_DIGEST_LENGTH, CC_SHA256_BLOCK_BYTES);
DIGEST(sha384, SHA384, CC_SHA512_CTX, CC_SHA384_DIGEST_LENGTH, CC_SHA384_BLOCK_BYTES);
DIGEST(sha512, SHA512, CC_SHA512_CTX, CC_SHA512_DIGEST_LENGTH, CC_SHA512_BLOCK_BYTES);
