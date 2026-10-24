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
 * AES (FIPS 197), byte oriented: SubBytes through the S-box, ShiftRows,
 * MixColumns with xtime, and the key expansion of section 5.2. The tables
 * (aes_tables.h) are computed by gen_tables.py from their definitions.
 *
 * Table lookups depend on secret data, so this is not constant-time; a
 * hardware (AES-NI) or bitsliced version can replace it behind the same
 * three functions.
 */

#include <string.h>

#include "aes.h"
#include "aes_tables.h"

static inline uint8_t
xtime(uint8_t x)
{
	return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0));
}

static inline uint8_t
gmul(uint8_t a, uint8_t b)
{
	uint8_t r = 0;

	while (b) {
		if (b & 1)
			r ^= a;
		a = xtime(a);
		b >>= 1;
	}
	return r;
}

int
_cc_aes_set_key(struct cc_aes_key *k, const void *key, size_t keylen)
{
	size_t nk = keylen / 4, i, total;
	uint8_t *w = k->rk, t[4];

	if (keylen != 16 && keylen != 24 && keylen != 32)
		return -1;
	k->rounds = (uint32_t)(nk + 6);
	total = 4 * (k->rounds + 1);		/* words */
	memcpy(w, key, keylen);
	for (i = nk; i < total; i++) {
		memcpy(t, w + 4 * (i - 1), 4);
		if (i % nk == 0) {
			uint8_t first = t[0];
			t[0] = aes_sbox[t[1]] ^ aes_rcon[i / nk - 1];
			t[1] = aes_sbox[t[2]];
			t[2] = aes_sbox[t[3]];
			t[3] = aes_sbox[first];
		} else if (nk > 6 && i % nk == 4) {
			t[0] = aes_sbox[t[0]];
			t[1] = aes_sbox[t[1]];
			t[2] = aes_sbox[t[2]];
			t[3] = aes_sbox[t[3]];
		}
		w[4 * i + 0] = w[4 * (i - nk) + 0] ^ t[0];
		w[4 * i + 1] = w[4 * (i - nk) + 1] ^ t[1];
		w[4 * i + 2] = w[4 * (i - nk) + 2] ^ t[2];
		w[4 * i + 3] = w[4 * (i - nk) + 3] ^ t[3];
	}
	return 0;
}

static void
add_round_key(uint8_t s[16], const uint8_t *rk)
{
	int i;

	for (i = 0; i < 16; i++)
		s[i] ^= rk[i];
}

/* The state is column major: s[4 * c + r]. */
static void
shift_rows(uint8_t s[16], int inverse)
{
	uint8_t t[16];
	int r, c;

	for (c = 0; c < 4; c++)
		for (r = 0; r < 4; r++) {
			int from = inverse ? (c - r + 4) % 4 : (c + r) % 4;
			t[4 * c + r] = s[4 * from + r];
		}
	memcpy(s, t, 16);
}

static void
mix_columns(uint8_t s[16])
{
	int c;

	for (c = 0; c < 4; c++) {
		uint8_t *col = s + 4 * c;
		uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];

		col[0] = xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3;
		col[1] = a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3;
		col[2] = a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3);
		col[3] = (xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
	}
}

static void
inv_mix_columns(uint8_t s[16])
{
	int c;

	for (c = 0; c < 4; c++) {
		uint8_t *col = s + 4 * c;
		uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];

		col[0] = gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9);
		col[1] = gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13);
		col[2] = gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11);
		col[3] = gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14);
	}
}

void
_cc_aes_encrypt(const struct cc_aes_key *k, const uint8_t in[16], uint8_t out[16])
{
	uint8_t s[16];
	uint32_t round;
	int i;

	memcpy(s, in, 16);
	add_round_key(s, k->rk);
	for (round = 1; round <= k->rounds; round++) {
		for (i = 0; i < 16; i++)
			s[i] = aes_sbox[s[i]];
		shift_rows(s, 0);
		if (round != k->rounds)
			mix_columns(s);
		add_round_key(s, k->rk + 16 * round);
	}
	memcpy(out, s, 16);
}

void
_cc_aes_decrypt(const struct cc_aes_key *k, const uint8_t in[16], uint8_t out[16])
{
	uint8_t s[16];
	uint32_t round;
	int i;

	memcpy(s, in, 16);
	add_round_key(s, k->rk + 16 * k->rounds);
	for (round = k->rounds; round >= 1; round--) {
		shift_rows(s, 1);
		for (i = 0; i < 16; i++)
			s[i] = aes_inv_sbox[s[i]];
		add_round_key(s, k->rk + 16 * (round - 1));
		if (round != 1)
			inv_mix_columns(s);
	}
	memcpy(out, s, 16);
}
