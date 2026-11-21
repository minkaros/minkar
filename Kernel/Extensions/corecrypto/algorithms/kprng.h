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
 * The kernel PRNG's state: Minkar's generator is Yarrow
 * (algorithms/yarrow), kept inside the struct cckprng_ctx that the
 * published <corecrypto/cckprng.h> declares for the kernel and that the
 * kext allocates, so the published header is used as it is.
 */

#ifndef _MINKAR_CC_KPRNG_H_
#define _MINKAR_CC_KPRNG_H_

#include <stdint.h>
#include <corecrypto/cckprng.h>

struct PRNG;

struct mcc_kprng {
	struct PRNG *prng;
	uint64_t bytes_since_entropy;
	uint64_t bytes_generated;
};

_Static_assert(sizeof(struct mcc_kprng) <= sizeof(struct cckprng_ctx),
    "the Yarrow state must fit in struct cckprng_ctx");

#define MCC_KPRNG(ctx)	((struct mcc_kprng *)(void *)(ctx))

#endif /* _MINKAR_CC_KPRNG_H_ */
