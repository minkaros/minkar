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
 * SHA-256 (FIPS 180-4) under CommonCrypto's names, for SystemConfiguration
 * only (hidden): SCNetworkService derives a service's ID from the
 * interface name with it, as on macOS. Minkar builds no CommonCrypto yet.
 */

#include <CommonCrypto/CommonDigest.h>
#include <stdint.h>
#include <string.h>

#define HIDDEN	__attribute__((visibility("hidden")))

static const uint32_t K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define ROR(x, n)	(((x) >> (n)) | ((x) << (32 - (n))))

/*
 * The context: count[0] and [1] the length in bytes (low, high), hash the
 * state, wbuf the partial block (count[0] % 64 bytes of it).
 */
static void
block(CC_SHA256_CTX *c, const uint8_t *p)
{
	uint32_t w[64], a, b, d, e, f, g, h, cc, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 |
		    (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
	for (; i < 64; i++)
		w[i] = (ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10)) + w[i - 7] +
		    (ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 16];
	a = c->hash[0]; b = c->hash[1]; cc = c->hash[2]; d = c->hash[3];
	e = c->hash[4]; f = c->hash[5]; g = c->hash[6]; h = c->hash[7];
	for (i = 0; i < 64; i++) {
		t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
		t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));
		h = g; g = f; f = e; e = d + t1;
		d = cc; cc = b; b = a; a = t1 + t2;
	}
	c->hash[0] += a; c->hash[1] += b; c->hash[2] += cc; c->hash[3] += d;
	c->hash[4] += e; c->hash[5] += f; c->hash[6] += g; c->hash[7] += h;
}

HIDDEN int
CC_SHA256_Init(CC_SHA256_CTX *c)
{
	static const uint32_t iv[8] = {
		0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
		0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
	};

	memset(c, 0, sizeof(*c));
	memcpy(c->hash, iv, sizeof(iv));
	return 1;
}

HIDDEN int
CC_SHA256_Update(CC_SHA256_CTX *c, const void *data, CC_LONG len)
{
	const uint8_t *p = data;
	uint8_t *buf = (uint8_t *)c->wbuf;

	while (len > 0) {
		uint32_t used = c->count[0] % 64;
		uint32_t n = 64 - used < len ? 64 - used : len;

		memcpy(buf + used, p, n);
		if ((c->count[0] += n) < n)
			c->count[1]++;
		p += n;
		len -= n;
		if (used + n == 64)
			block(c, buf);
	}
	return 1;
}

HIDDEN int
CC_SHA256_Final(unsigned char *md, CC_SHA256_CTX *c)
{
	uint64_t bits = ((uint64_t)c->count[1] << 32 | c->count[0]) * 8;
	uint8_t pad[72] = { 0x80 };
	uint32_t used = c->count[0] % 64;
	uint32_t padlen = (used < 56 ? 56 : 120) - used;
	int i;

	for (i = 0; i < 8; i++)
		pad[padlen + i] = (uint8_t)(bits >> (56 - 8 * i));
	CC_SHA256_Update(c, pad, padlen + 8);
	for (i = 0; i < 8; i++) {
		md[4 * i] = (uint8_t)(c->hash[i] >> 24);
		md[4 * i + 1] = (uint8_t)(c->hash[i] >> 16);
		md[4 * i + 2] = (uint8_t)(c->hash[i] >> 8);
		md[4 * i + 3] = (uint8_t)c->hash[i];
	}
	memset(c, 0, sizeof(*c));
	return 1;
}
