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

/* ccrng() mínimo para ravynOS: entropía vía getentropy(). */
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
