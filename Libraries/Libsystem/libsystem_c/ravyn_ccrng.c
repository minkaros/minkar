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

/* ccrng() mínimo para Minkar: entropía vía getentropy(). */
#include <stddef.h>
int getentropy(void *buffer, size_t size);  /* libsystem_kernel */

struct ccrng_state {
    int (*generate)(struct ccrng_state *rng, size_t outlen, void *out);
};

static int ravyn_ccrng_generate(struct ccrng_state *rng, size_t outlen, void *out)
{
    unsigned char *p = out;
    (void)rng;
    while (outlen > 0) {
        size_t n = outlen > 256 ? 256 : outlen;   /* límite de getentropy */
        if (getentropy(p, n) != 0)
            return -1;
        p += n;
        outlen -= n;
    }
    return 0;
}

static struct ccrng_state ravyn_ccrng_state = { ravyn_ccrng_generate };

struct ccrng_state *ccrng(int *error)
{
    if (error)
        *error = 0;
    return &ravyn_ccrng_state;
}

/*
 * A value in [0, bound), without modulo bias: random 64-bit words, masked
 * to the bit length of bound - 1, are drawn until one is below bound (at
 * worst half are rejected). A bound of 0 is an error; 1 gives 0.
 */
int ccrng_uniform(struct ccrng_state *rng, unsigned long long bound,
    unsigned long long *rand)
{
    unsigned long long mask, r;

    if (rand == NULL || bound == 0)
        return -1;
    if (bound == 1) {
        *rand = 0;
        return 0;
    }
    mask = bound - 1;
    mask |= mask >> 1;
    mask |= mask >> 2;
    mask |= mask >> 4;
    mask |= mask >> 8;
    mask |= mask >> 16;
    mask |= mask >> 32;
    do {
        if (rng->generate(rng, sizeof(r), &r) != 0)
            return -1;
        r &= mask;
    } while (r >= bound);
    *rand = r;
    return 0;
}
