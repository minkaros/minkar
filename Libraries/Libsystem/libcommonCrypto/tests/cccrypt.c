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

/*
 * Host test driver for cryptor.c and keyderivation.c:
 *   cccrypt enc|dec MODE KEYHEX IVHEX PAD < in > out
 *     MODE: ecb cbc ctr cfb cfb8 ofb rc4; PAD: 0 or 1. Input goes to
 *     CCCryptorUpdate in chunks of varying size; the result is checked
 *     against one-shot CCCrypt where that API applies (ecb, cbc, rc4).
 *   cccrypt pbkdf2 PRF ROUNDS SALTHEX LEN < password
 */

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char in[1 << 20], out[(1 << 20) + 64], ref[(1 << 20) + 64];

static size_t
unhex(const char *s, unsigned char *b, size_t max)
{
	size_t i;

	for (i = 0; s[2 * i] && i < max; i++)
		sscanf(s + 2 * i, "%2hhx", &b[i]);
	return i;
}

int
main(int argc, char **argv)
{
	unsigned char key[600], iv[16];
	size_t keylen, ivlen, len, off = 0, step = 1, moved, total = 0;
	CCCryptorRef c;
	CCCryptorStatus st;
	CCMode mode;
	CCOperation op;
	int pad;

	len = fread(in, 1, sizeof(in), stdin);
	if (argc == 6 && !strcmp(argv[1], "pbkdf2")) {
		unsigned char salt[256], dk[512];
		size_t saltlen = unhex(argv[4], salt, sizeof(salt));
		size_t dklen = (size_t)atoi(argv[5]);
		if (CCKeyDerivationPBKDF(kCCPBKDF2, (const char *)in, len, salt, saltlen,
		    (CCPseudoRandomAlgorithm)atoi(argv[2]), (unsigned)atoi(argv[3]),
		    dk, dklen) != kCCSuccess)
			return 1;
		fwrite(dk, 1, dklen, stdout);
		return 0;
	}
	if (argc != 6)
		return 2;
	op = !strcmp(argv[1], "enc") ? kCCEncrypt : kCCDecrypt;
	keylen = unhex(argv[3], key, sizeof(key));
	ivlen = unhex(argv[4], iv, sizeof(iv));
	pad = atoi(argv[5]);
	if (!strcmp(argv[2], "ecb")) mode = kCCModeECB;
	else if (!strcmp(argv[2], "cbc")) mode = kCCModeCBC;
	else if (!strcmp(argv[2], "ctr")) mode = kCCModeCTR;
	else if (!strcmp(argv[2], "cfb")) mode = kCCModeCFB;
	else if (!strcmp(argv[2], "cfb8")) mode = kCCModeCFB8;
	else if (!strcmp(argv[2], "ofb")) mode = kCCModeOFB;
	else if (!strcmp(argv[2], "rc4")) mode = kCCModeRC4;
	else return 2;

	st = CCCryptorCreateWithMode(op, mode,
	    mode == kCCModeRC4 ? kCCAlgorithmRC4 : kCCAlgorithmAES,
	    pad ? ccPKCS7Padding : ccNoPadding, ivlen ? iv : NULL, key, keylen,
	    NULL, 0, 0, kCCModeOptionCTR_BE, &c);
	if (st != kCCSuccess) {
		fprintf(stderr, "create: %d\n", st);
		return 1;
	}
	while (off < len) {
		size_t n = step < len - off ? step : len - off;
		size_t want = CCCryptorGetOutputLength(c, n, false);
		/* A too-small buffer must fail without losing state. */
		if (want > 0 && CCCryptorUpdate(c, in + off, n, out + total, want - 1,
		    &moved) != kCCBufferTooSmall) {
			fprintf(stderr, "short buffer accepted\n");
			return 1;
		}
		st = CCCryptorUpdate(c, in + off, n, out + total, want, &moved);
		if (st != kCCSuccess) {
			fprintf(stderr, "update: %d\n", st);
			return 1;
		}
		total += moved;
		off += n;
		step = step * 5 % 97 + 1;
	}
	st = CCCryptorFinal(c, out + total, sizeof(out) - total, &moved);
	if (st != kCCSuccess) {
		fprintf(stderr, "final: %d\n", st);
		return 1;
	}
	total += moved;
	CCCryptorRelease(c);

	if (mode == kCCModeECB || mode == kCCModeCBC || mode == kCCModeRC4) {
		CCOptions opt = (pad ? kCCOptionPKCS7Padding : 0) |
		    (mode == kCCModeECB ? kCCOptionECBMode : 0);
		size_t refmoved = 0;
		st = CCCrypt(op, mode == kCCModeRC4 ? kCCAlgorithmRC4 : kCCAlgorithmAES,
		    opt, key, keylen, ivlen ? iv : NULL, in, len, ref, sizeof(ref),
		    &refmoved);
		if (st != kCCSuccess || refmoved != total || memcmp(ref, out, total)) {
			fprintf(stderr, "CCCrypt differs (%d)\n", st);
			return 1;
		}
	}
	fwrite(out, 1, total, stdout);
	return 0;
}
