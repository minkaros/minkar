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
 * AES (FIPS 197) with 128, 192 and 256-bit keys, in ECB and CBC mode, for
 * the corecrypto kext's function table (struct ccmode_ecb and ccmode_cbc
 * in the published <corecrypto/ccmode_impl.h>). The S-box is computed for
 * Minkar from its definition in FIPS 197, section 5.1.1. A plain
 * byte-oriented implementation: the kernel uses it for little.
 */

#include <stddef.h>
#include <stdint.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccn.h>

static const uint8_t sbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16,
};

static const uint8_t inv_sbox[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d,
};

struct aes_key {
	uint32_t rounds;
	uint8_t rk[15][16];	/* the round keys, as bytes */
};

static inline uint8_t
xtime(uint8_t x)
{
	return (uint8_t)((x << 1) ^ ((x & 0x80) ? 0x1b : 0));
}

static uint8_t
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

/* Key lengths are bytes (16, 24, 32) or bits (128, 192, 256). */
static int
expand_key(struct aes_key *k, size_t key_len, const uint8_t *key)
{
	uint8_t w[60][4];
	size_t nk;
	uint8_t rcon = 1;

	if (key_len > 32)
		key_len /= 8;
	if (key_len != 16 && key_len != 24 && key_len != 32)
		return -1;
	nk = key_len / 4;
	k->rounds = (uint32_t)nk + 6;
	for (size_t i = 0; i < nk; i++)
		for (int j = 0; j < 4; j++)
			w[i][j] = key[4 * i + j];
	for (size_t i = nk; i < 4 * (k->rounds + 1); i++) {
		uint8_t t[4] = { w[i - 1][0], w[i - 1][1], w[i - 1][2], w[i - 1][3] };

		if (i % nk == 0) {
			uint8_t first = t[0];
			t[0] = sbox[t[1]] ^ rcon;
			t[1] = sbox[t[2]];
			t[2] = sbox[t[3]];
			t[3] = sbox[first];
			rcon = xtime(rcon);
		} else if (nk > 6 && i % nk == 4) {
			for (int j = 0; j < 4; j++)
				t[j] = sbox[t[j]];
		}
		for (int j = 0; j < 4; j++)
			w[i][j] = w[i - nk][j] ^ t[j];
	}
	for (uint32_t r = 0; r <= k->rounds; r++)
		for (int i = 0; i < 16; i++)
			k->rk[r][i] = w[4 * r + i / 4][i % 4];
	return 0;
}

static void
encrypt_block(const struct aes_key *k, const uint8_t *in, uint8_t *out)
{
	uint8_t s[16], t[16];

	for (int i = 0; i < 16; i++)
		s[i] = in[i] ^ k->rk[0][i];
	for (uint32_t r = 1; r <= k->rounds; r++) {
		/* SubBytes and ShiftRows: byte i is row i % 4, column i / 4. */
		for (int i = 0; i < 16; i++)
			t[i] = sbox[s[(i + 4 * (i % 4)) % 16]];
		if (r < k->rounds) {
			for (int c = 0; c < 4; c++) {
				uint8_t *a = &t[4 * c];
				uint8_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];

				a[0] = xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3;
				a[1] = a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3;
				a[2] = a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3);
				a[3] = (xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
			}
		}
		for (int i = 0; i < 16; i++)
			s[i] = t[i] ^ k->rk[r][i];
	}
	__builtin_memcpy(out, s, 16);
}

static void
decrypt_block(const struct aes_key *k, const uint8_t *in, uint8_t *out)
{
	uint8_t s[16], t[16];

	for (int i = 0; i < 16; i++)
		s[i] = in[i] ^ k->rk[k->rounds][i];
	for (uint32_t r = k->rounds; r >= 1; r--) {
		/* InvShiftRows and InvSubBytes. */
		for (int i = 0; i < 16; i++)
			t[(i + 4 * (i % 4)) % 16] = inv_sbox[s[i]];
		for (int i = 0; i < 16; i++)
			t[i] ^= k->rk[r - 1][i];
		if (r > 1) {
			for (int c = 0; c < 4; c++) {
				uint8_t *a = &t[4 * c];
				uint8_t a0 = a[0], a1 = a[1], a2 = a[2], a3 = a[3];

				a[0] = gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9);
				a[1] = gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13);
				a[2] = gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11);
				a[3] = gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14);
			}
		}
		__builtin_memcpy(s, t, 16);
	}
	__builtin_memcpy(out, s, 16);
}

static int
ecb_init(const struct ccmode_ecb *ecb, ccecb_ctx *ctx, size_t key_len, const void *key)
{
	(void)ecb;
	return expand_key((struct aes_key *)ctx, key_len, key);
}

static int
ecb_encrypt(const ccecb_ctx *ctx, size_t nblocks, const void *in, void *out)
{
	const uint8_t *i = in;
	uint8_t *o = out;

	for (; nblocks > 0; nblocks--, i += 16, o += 16)
		encrypt_block((const struct aes_key *)ctx, i, o);
	return 0;
}

static int
ecb_decrypt(const ccecb_ctx *ctx, size_t nblocks, const void *in, void *out)
{
	const uint8_t *i = in;
	uint8_t *o = out;

	for (; nblocks > 0; nblocks--, i += 16, o += 16)
		decrypt_block((const struct aes_key *)ctx, i, o);
	return 0;
}

static int
cbc_init(const struct ccmode_cbc *cbc, cccbc_ctx *ctx, size_t key_len, const void *key)
{
	(void)cbc;
	return expand_key((struct aes_key *)ctx, key_len, key);
}

/* CBC: the IV is updated to the last ciphertext block, for chaining. */
static int
cbc_encrypt(const cccbc_ctx *ctx, cccbc_iv *iv, size_t nblocks, const void *in, void *out)
{
	uint8_t *v = (uint8_t *)iv;
	const uint8_t *i = in;
	uint8_t *o = out;

	for (; nblocks > 0; nblocks--, i += 16, o += 16) {
		uint8_t x[16];

		for (int j = 0; j < 16; j++)
			x[j] = i[j] ^ v[j];
		encrypt_block((const struct aes_key *)ctx, x, o);
		__builtin_memcpy(v, o, 16);
	}
	return 0;
}

static int
cbc_decrypt(const cccbc_ctx *ctx, cccbc_iv *iv, size_t nblocks, const void *in, void *out)
{
	uint8_t *v = (uint8_t *)iv;
	const uint8_t *i = in;
	uint8_t *o = out;

	for (; nblocks > 0; nblocks--, i += 16, o += 16) {
		uint8_t c[16], x[16];

		__builtin_memcpy(c, i, 16);	/* in and out may be the same */
		decrypt_block((const struct aes_key *)ctx, c, x);
		for (int j = 0; j < 16; j++)
			o[j] = x[j] ^ v[j];
		__builtin_memcpy(v, c, 16);
	}
	return 0;
}

const struct ccmode_ecb pdcaes_ecb_encrypt = {
	.size = ccn_sizeof_size(sizeof(struct aes_key)),
	.block_size = CCAES_BLOCK_SIZE,
	.init = ecb_init,
	.ecb = ecb_encrypt,
};

const struct ccmode_ecb pdcaes_ecb_decrypt = {
	.size = ccn_sizeof_size(sizeof(struct aes_key)),
	.block_size = CCAES_BLOCK_SIZE,
	.init = ecb_init,
	.ecb = ecb_decrypt,
};

const struct ccmode_cbc pdcaes_cbc_encrypt = {
	.size = ccn_sizeof_size(sizeof(struct aes_key)),
	.block_size = CCAES_BLOCK_SIZE,
	.init = cbc_init,
	.cbc = cbc_encrypt,
	.custom = NULL,
};

const struct ccmode_cbc pdcaes_cbc_decrypt = {
	.size = ccn_sizeof_size(sizeof(struct aes_key)),
	.block_size = CCAES_BLOCK_SIZE,
	.init = cbc_init,
	.cbc = cbc_decrypt,
	.custom = NULL,
};
