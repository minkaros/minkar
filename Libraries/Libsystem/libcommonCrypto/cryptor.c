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
 * CCCryptor (<CommonCrypto/CommonCryptor.h>): AES-128/192/256 in ECB and
 * CBC (optionally with PKCS#7 padding), CTR (big-endian counter), CFB,
 * CFB8 and OFB; and RC4. DES, 3DES, CAST, RC2, Blowfish and XTS answer
 * kCCUnimplemented.
 *
 * Block modes buffer partial blocks, so CCCryptorUpdate takes any length.
 * Decrypting with padding holds back the last full block until
 * CCCryptorFinal, which strips and checks the padding. The other modes
 * work byte by byte and need no CCCryptorFinal.
 */

#include <CommonCrypto/CommonCryptor.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "aes.h"

#define BLOCK	CC_AES_BLOCK

struct rc4 {
	uint8_t	S[256];
	uint8_t	i, j;
};

struct _CCCryptor {
	uint32_t	magic;
	bool		allocated;	/* by us, not CCCryptorCreateFromData */
	CCOperation	op;
	CCAlgorithm	alg;
	CCMode		mode;
	bool		padding;
	union {
		struct cc_aes_key aes;
		struct rc4	rc4;
	} key;
	uint8_t		iv[BLOCK];	/* chaining value, counter or register */
	uint8_t		iv0[BLOCK];	/* the IV given, for CCCryptorReset */
	uint8_t		ks[BLOCK];	/* key stream (stream modes) */
	uint8_t		buf[BLOCK];	/* partial block (ECB, CBC) */
	size_t		used;		/* bytes in buf, or of ks consumed */
};

#define CRYPTOR_MAGIC	0x43436372U	/* "CCcr" */

static bool
block_mode(const struct _CCCryptor *c)
{
	return c->mode == kCCModeECB || c->mode == kCCModeCBC;
}

static void
rc4_init(struct rc4 *r, const uint8_t *key, size_t keylen)
{
	unsigned i, j = 0;

	for (i = 0; i < 256; i++)
		r->S[i] = (uint8_t)i;
	for (i = 0; i < 256; i++) {
		uint8_t t = r->S[i];
		j = (j + t + key[i % keylen]) & 0xff;
		r->S[i] = r->S[j];
		r->S[j] = t;
	}
	r->i = r->j = 0;
}

static uint8_t
rc4_byte(struct rc4 *r)
{
	uint8_t t;

	r->i++;
	r->j += r->S[r->i];
	t = r->S[r->i];
	r->S[r->i] = r->S[r->j];
	r->S[r->j] = t;
	return r->S[(uint8_t)(r->S[r->i] + r->S[r->j])];
}

static CCCryptorStatus
setup(struct _CCCryptor *c, CCOperation op, CCMode mode, CCAlgorithm alg,
    bool padding, const void *iv, const void *key, size_t keyLength)
{
	if (op != kCCEncrypt && op != kCCDecrypt)
		return kCCParamError;
	if (key == NULL && keyLength > 0)
		return kCCParamError;

	memset(c, 0, sizeof(*c));
	c->magic = CRYPTOR_MAGIC;
	c->op = op;
	c->alg = alg;
	c->mode = mode;

	switch (alg) {
	case kCCAlgorithmAES:
		switch (mode) {
		case kCCModeECB: case kCCModeCBC:
			c->padding = padding;
			break;
		case kCCModeCTR: case kCCModeCFB: case kCCModeCFB8: case kCCModeOFB:
			if (padding)
				return kCCParamError;
			break;
		default:
			return kCCUnimplemented;
		}
		if (_cc_aes_set_key(&c->key.aes, key, keyLength) != 0)
			return kCCKeySizeError;
		break;
	case kCCAlgorithmRC4:
		if (mode != kCCModeRC4)
			return kCCParamError;
		if (keyLength < kCCKeySizeMinRC4 || keyLength > kCCKeySizeMaxRC4)
			return kCCKeySizeError;
		rc4_init(&c->key.rc4, key, keyLength);
		return kCCSuccess;
	case kCCAlgorithmDES: case kCCAlgorithm3DES: case kCCAlgorithmCAST:
	case kCCAlgorithmRC2: case kCCAlgorithmBlowfish:
		return kCCUnimplemented;
	default:
		return kCCParamError;
	}

	if (iv != NULL && mode != kCCModeECB)
		memcpy(c->iv0, iv, BLOCK);
	memcpy(c->iv, c->iv0, BLOCK);
	return kCCSuccess;
}

/* ------------------------------------------------------ creation */

CCCryptorStatus
CCCryptorCreateFromData(CCOperation op, CCAlgorithm alg, CCOptions options,
    const void *key, size_t keyLength, const void *iv, const void *data,
    size_t dataLength, CCCryptorRef *cryptorRef, size_t *dataUsed)
{
	CCMode mode;
	CCCryptorStatus st;

	if (cryptorRef == NULL)
		return kCCParamError;
	if (data == NULL || dataLength < sizeof(struct _CCCryptor)) {
		if (dataUsed != NULL)
			*dataUsed = sizeof(struct _CCCryptor);
		return kCCBufferTooSmall;
	}
	if (alg == kCCAlgorithmRC4)
		mode = kCCModeRC4;
	else
		mode = (options & kCCOptionECBMode) ? kCCModeECB : kCCModeCBC;
	st = setup((struct _CCCryptor *)(uintptr_t)data, op, mode, alg,
	    (options & kCCOptionPKCS7Padding) != 0, iv, key, keyLength);
	if (st != kCCSuccess)
		return st;
	*cryptorRef = (CCCryptorRef)(uintptr_t)data;
	if (dataUsed != NULL)
		*dataUsed = sizeof(struct _CCCryptor);
	return kCCSuccess;
}

static CCCryptorStatus
create_allocated(CCOperation op, CCMode mode, CCAlgorithm alg, bool padding,
    const void *iv, const void *key, size_t keyLength, CCCryptorRef *cryptorRef)
{
	struct _CCCryptor *c;
	CCCryptorStatus st;

	if (cryptorRef == NULL)
		return kCCParamError;
	if ((c = malloc(sizeof(*c))) == NULL)
		return kCCMemoryFailure;
	st = setup(c, op, mode, alg, padding, iv, key, keyLength);
	if (st != kCCSuccess) {
		memset(c, 0, sizeof(*c));
		free(c);
		return st;
	}
	c->allocated = true;
	*cryptorRef = c;
	return kCCSuccess;
}

CCCryptorStatus
CCCryptorCreate(CCOperation op, CCAlgorithm alg, CCOptions options,
    const void *key, size_t keyLength, const void *iv, CCCryptorRef *cryptorRef)
{
	CCMode mode;

	if (alg == kCCAlgorithmRC4)
		mode = kCCModeRC4;
	else
		mode = (options & kCCOptionECBMode) ? kCCModeECB : kCCModeCBC;
	return create_allocated(op, mode, alg, (options & kCCOptionPKCS7Padding) != 0,
	    iv, key, keyLength, cryptorRef);
}

CCCryptorStatus
CCCryptorCreateWithMode(CCOperation op, CCMode mode, CCAlgorithm alg,
    CCPadding padding, const void *iv, const void *key, size_t keyLength,
    const void *tweak, size_t tweakLength, int numRounds, CCModeOptions options,
    CCCryptorRef *cryptorRef)
{
	(void)tweak;
	(void)options;		/* CTR counts big endian, the only choice */
	if (tweakLength != 0)
		return kCCUnimplemented;	/* XTS */
	if (numRounds != 0)
		return kCCParamError;
	if (padding != ccNoPadding && padding != ccPKCS7Padding)
		return kCCParamError;
	return create_allocated(op, mode, alg, padding == ccPKCS7Padding, iv, key,
	    keyLength, cryptorRef);
}

CCCryptorStatus
CCCryptorRelease(CCCryptorRef c)
{
	bool allocated;

	if (c == NULL || c->magic != CRYPTOR_MAGIC)
		return kCCParamError;
	allocated = c->allocated;
	memset(c, 0, sizeof(*c));	/* keys and state */
	if (allocated)
		free(c);
	return kCCSuccess;
}

CCCryptorStatus
CCCryptorReset(CCCryptorRef c, const void *iv)
{
	if (c == NULL || c->magic != CRYPTOR_MAGIC)
		return kCCParamError;
	if (c->mode != kCCModeCBC)
		return kCCUnimplemented;
	if (iv != NULL)
		memcpy(c->iv0, iv, BLOCK);
	else
		memset(c->iv0, 0, BLOCK);
	memcpy(c->iv, c->iv0, BLOCK);
	memset(c->buf, 0, BLOCK);
	c->used = 0;
	return kCCSuccess;
}

/* ------------------------------------------------------ lengths */

/* Whole blocks Update can output for len more bytes. */
static size_t
update_blocks_len(const struct _CCCryptor *c, size_t len)
{
	size_t total = c->used + len;

	if (c->op == kCCDecrypt && c->padding)
		return total == 0 ? 0 : (total - 1) / BLOCK * BLOCK;
	return total / BLOCK * BLOCK;
}

size_t
CCCryptorGetOutputLength(CCCryptorRef c, size_t inputLength, bool final)
{
	size_t total;

	if (c == NULL || c->magic != CRYPTOR_MAGIC)
		return 0;
	if (!block_mode(c))
		return inputLength;
	if (!final)
		return update_blocks_len(c, inputLength);
	total = c->used + inputLength;
	if (c->op == kCCEncrypt && c->padding)
		return (total / BLOCK + 1) * BLOCK;
	return (total + BLOCK - 1) / BLOCK * BLOCK;
}

/* ------------------------------------------------------ processing */

static void
process_block(struct _CCCryptor *c, const uint8_t *in, uint8_t *out)
{
	uint8_t tmp[BLOCK];
	int i;

	if (c->mode == kCCModeECB) {
		if (c->op == kCCEncrypt)
			_cc_aes_encrypt(&c->key.aes, in, out);
		else
			_cc_aes_decrypt(&c->key.aes, in, out);
		return;
	}
	/* CBC */
	if (c->op == kCCEncrypt) {
		for (i = 0; i < BLOCK; i++)
			tmp[i] = in[i] ^ c->iv[i];
		_cc_aes_encrypt(&c->key.aes, tmp, out);
		memcpy(c->iv, out, BLOCK);
	} else {
		memcpy(tmp, in, BLOCK);		/* in may be out */
		_cc_aes_decrypt(&c->key.aes, in, out);
		for (i = 0; i < BLOCK; i++)
			out[i] ^= c->iv[i];
		memcpy(c->iv, tmp, BLOCK);
	}
}

static void
ctr_increment(uint8_t ctr[BLOCK])
{
	int i;

	for (i = BLOCK - 1; i >= 0; i--)
		if (++ctr[i] != 0)
			break;
}

static uint8_t
stream_byte(struct _CCCryptor *c, uint8_t in)
{
	uint8_t out;

	switch (c->mode) {
	case kCCModeRC4:
		return in ^ rc4_byte(&c->key.rc4);
	case kCCModeCFB8:
		_cc_aes_encrypt(&c->key.aes, c->iv, c->ks);
		out = in ^ c->ks[0];
		memmove(c->iv, c->iv + 1, BLOCK - 1);
		c->iv[BLOCK - 1] = (c->op == kCCEncrypt) ? out : in;
		return out;
	default:
		break;
	}
	if (c->used == 0) {
		_cc_aes_encrypt(&c->key.aes, c->iv, c->ks);
		if (c->mode == kCCModeCTR)
			ctr_increment(c->iv);
		else if (c->mode == kCCModeOFB)
			memcpy(c->iv, c->ks, BLOCK);
	}
	out = in ^ c->ks[c->used];
	if (c->mode == kCCModeCFB)
		c->iv[c->used] = (c->op == kCCEncrypt) ? out : in;
	c->used = (c->used + 1) % BLOCK;
	return out;
}

CCCryptorStatus
CCCryptorUpdate(CCCryptorRef c, const void *dataIn, size_t dataInLength,
    void *dataOut, size_t dataOutAvailable, size_t *dataOutMoved)
{
	const uint8_t *in = dataIn;
	uint8_t *out = dataOut;
	size_t need, done = 0, i;

	if (c == NULL || c->magic != CRYPTOR_MAGIC ||
	    (dataIn == NULL && dataInLength > 0))
		return kCCParamError;

	if (!block_mode(c)) {
		if (dataOutAvailable < dataInLength) {
			if (dataOutMoved != NULL)
				*dataOutMoved = dataInLength;
			return kCCBufferTooSmall;
		}
		for (i = 0; i < dataInLength; i++)
			out[i] = stream_byte(c, in[i]);
		if (dataOutMoved != NULL)
			*dataOutMoved = dataInLength;
		return kCCSuccess;
	}

	need = update_blocks_len(c, dataInLength);
	if (dataOutAvailable < need) {
		if (dataOutMoved != NULL)
			*dataOutMoved = need;
		return kCCBufferTooSmall;
	}
	while (done < need) {
		/* Complete the buffered block first, then work from the input. */
		if (c->used > 0 || dataInLength < BLOCK) {
			size_t n = BLOCK - c->used;
			memcpy(c->buf + c->used, in, n);
			in += n;
			dataInLength -= n;
			process_block(c, c->buf, out + done);
			c->used = 0;
		} else {
			process_block(c, in, out + done);
			in += BLOCK;
			dataInLength -= BLOCK;
		}
		done += BLOCK;
	}
	/* What remains (at most a block) waits in the buffer. */
	memcpy(c->buf + c->used, in, dataInLength);
	c->used += dataInLength;
	if (dataOutMoved != NULL)
		*dataOutMoved = done;
	return kCCSuccess;
}

CCCryptorStatus
CCCryptorFinal(CCCryptorRef c, void *dataOut, size_t dataOutAvailable,
    size_t *dataOutMoved)
{
	uint8_t block[BLOCK];
	size_t n = 0, i;
	uint8_t pad;

	if (c == NULL || c->magic != CRYPTOR_MAGIC)
		return kCCParamError;
	if (dataOutMoved != NULL)
		*dataOutMoved = 0;
	if (!block_mode(c))
		return kCCSuccess;

	if (c->op == kCCEncrypt && c->padding) {
		if (dataOutAvailable < BLOCK) {
			if (dataOutMoved != NULL)
				*dataOutMoved = BLOCK;
			return kCCBufferTooSmall;
		}
		pad = (uint8_t)(BLOCK - c->used);
		memset(c->buf + c->used, pad, pad);
		process_block(c, c->buf, dataOut);
		n = BLOCK;
	} else if (c->op == kCCDecrypt && c->padding) {
		if (c->used != BLOCK)
			return kCCAlignmentError;
		process_block(c, c->buf, block);
		pad = block[BLOCK - 1];
		if (pad == 0 || pad > BLOCK)
			return kCCDecodeError;
		for (i = BLOCK - pad; i < BLOCK; i++)
			if (block[i] != pad)
				return kCCDecodeError;
		n = BLOCK - pad;
		if (dataOutAvailable < n) {
			if (dataOutMoved != NULL)
				*dataOutMoved = n;
			return kCCBufferTooSmall;
		}
		memcpy(dataOut, block, n);
		memset(block, 0, sizeof(block));
	} else if (c->used != 0) {
		return kCCAlignmentError;
	}
	c->used = 0;
	if (dataOutMoved != NULL)
		*dataOutMoved = n;
	return kCCSuccess;
}

CCCryptorStatus
CCCrypt(CCOperation op, CCAlgorithm alg, CCOptions options, const void *key,
    size_t keyLength, const void *iv, const void *dataIn, size_t dataInLength,
    void *dataOut, size_t dataOutAvailable, size_t *dataOutMoved)
{
	struct _CCCryptor c;
	CCCryptorRef ref;
	CCCryptorStatus st;
	size_t need, moved = 0, last = 0;

	st = CCCryptorCreateFromData(op, alg, options, key, keyLength, iv, &c,
	    sizeof(c), &ref, NULL);
	if (st != kCCSuccess)
		return st;
	need = CCCryptorGetOutputLength(ref, dataInLength, true);
	if (dataOutAvailable < need && !(op == kCCDecrypt && block_mode(ref))) {
		CCCryptorRelease(ref);
		if (dataOutMoved != NULL)
			*dataOutMoved = need;
		return kCCBufferTooSmall;
	}
	st = CCCryptorUpdate(ref, dataIn, dataInLength, dataOut, dataOutAvailable,
	    &moved);
	if (st == kCCSuccess)
		st = CCCryptorFinal(ref, (uint8_t *)dataOut + moved,
		    dataOutAvailable - moved, &last);
	CCCryptorRelease(ref);
	if (dataOutMoved != NULL)
		*dataOutMoved = moved + last;	/* the size needed, on kCCBufferTooSmall */
	return st;
}
