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

/* The AES block cipher (FIPS 197), for cryptor.c. */

#ifndef _CC_AES_H_
#define _CC_AES_H_

#include <stdint.h>
#include <stddef.h>

#define CC_AES_BLOCK	16

struct cc_aes_key {
	uint32_t	rounds;			/* 10, 12 or 14 */
	uint8_t		rk[15 * CC_AES_BLOCK];	/* round keys */
};

/* keylen: 16, 24 or 32 bytes. Returns 0, or -1 for another length. */
int	_cc_aes_set_key(struct cc_aes_key *k, const void *key, size_t keylen);
void	_cc_aes_encrypt(const struct cc_aes_key *k, const uint8_t in[16],
	    uint8_t out[16]);
void	_cc_aes_decrypt(const struct cc_aes_key *k, const uint8_t in[16],
	    uint8_t out[16]);

#endif /* !_CC_AES_H_ */
