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
 * ccdigest_init, ccdigest_update and the final steps every digest shares.
 * A context holds, after the state, the number of bits already
 * compressed, a block of pending bytes and how many of them there are
 * (the ccdigest_nbits, ccdigest_data and ccdigest_num macros).
 */

#include <stddef.h>
#include <stdint.h>
#include <corecrypto/ccdigest.h>
#include "digest.h"

void
ccdigest_init(const struct ccdigest_info *di, ccdigest_ctx_t ctx)
{
	__builtin_memcpy(ccdigest_state_u8(di, ctx), di->initial_state, di->state_size);
	ccdigest_nbits(di, ctx) = 0;
	ccdigest_num(di, ctx) = 0;
}

void
ccdigest_update(const struct ccdigest_info *di, ccdigest_ctx_t ctx,
    size_t len, const void *data)
{
	const uint8_t *p = data;
	uint8_t *buf = ccdigest_data(di, ctx);
	size_t bs = di->block_size;
	size_t n = ccdigest_num(di, ctx);

	if (n > 0) {
		size_t take = bs - n < len ? bs - n : len;
		__builtin_memcpy(buf + n, p, take);
		n += take;
		p += take;
		len -= take;
		if (n < bs) {
			ccdigest_num(di, ctx) = (unsigned int)n;
			return;
		}
		di->compress(ccdigest_state(di, ctx), 1, buf);
		ccdigest_nbits(di, ctx) += bs * 8;
		n = 0;
	}
	if (len >= bs) {
		size_t blocks = len / bs;
		di->compress(ccdigest_state(di, ctx), blocks, p);
		ccdigest_nbits(di, ctx) += (uint64_t)blocks * bs * 8;
		p += blocks * bs;
		len -= blocks * bs;
	}
	if (len > 0)
		__builtin_memcpy(buf, p, len);
	ccdigest_num(di, ctx) = (unsigned int)len;
}

/*
 * Pads the pending bytes (a 1 bit, zeros, the message's length in bits at
 * the block's end: a 64-bit field, or a 128-bit one for 128-byte blocks,
 * whose high half is always zero here) and compresses what is left.
 */
static void
pad(const struct ccdigest_info *di, ccdigest_ctx_t ctx, int big_endian)
{
	uint8_t *buf = ccdigest_data(di, ctx);
	size_t bs = di->block_size;
	size_t n = ccdigest_num(di, ctx);
	uint64_t bits = ccdigest_nbits(di, ctx) + (uint64_t)n * 8;

	size_t field = bs == 128 ? 16 : 8;

	buf[n++] = 0x80;
	if (n > bs - field) {
		__builtin_memset(buf + n, 0, bs - n);
		di->compress(ccdigest_state(di, ctx), 1, buf);
		n = 0;
	}
	__builtin_memset(buf + n, 0, bs - 8 - n);
	for (int i = 0; i < 8; i++)
		buf[big_endian ? bs - 1 - i : bs - 8 + i] = (uint8_t)(bits >> (8 * i));
	di->compress(ccdigest_state(di, ctx), 1, buf);
	ccdigest_num(di, ctx) = 0;
}

void
mcc_digest_final_le(const struct ccdigest_info *di, ccdigest_ctx_t ctx, unsigned char *digest)
{
	const uint32_t *h = ccdigest_state_u32(di, ctx);

	pad(di, ctx, 0);
	for (size_t i = 0; i < di->output_size; i++)
		digest[i] = (uint8_t)(h[i / 4] >> (8 * (i % 4)));
}

void
mcc_digest_final_be(const struct ccdigest_info *di, ccdigest_ctx_t ctx, unsigned char *digest)
{
	pad(di, ctx, 1);
	if (di->block_size == 128) {
		const uint64_t *h = ccdigest_state_u64(di, ctx);
		for (size_t i = 0; i < di->output_size; i++)
			digest[i] = (uint8_t)(h[i / 8] >> (8 * (7 - i % 8)));
	} else {
		const uint32_t *h = ccdigest_state_u32(di, ctx);
		for (size_t i = 0; i < di->output_size; i++)
			digest[i] = (uint8_t)(h[i / 4] >> (8 * (3 - i % 4)));
	}
}

/* The table's ccdigest_final_fn: whatever final step the digest has. */
void
pdcdigest_final_fn(const struct ccdigest_info *di, ccdigest_ctx_t ctx, void *digest)
{
	di->final(di, ctx, digest);
}
