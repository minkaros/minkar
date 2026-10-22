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

/* Tests for the CGWindowListOption filtering (CGWindowListFilter.c). */

#include "../CGWindowListFilter.h"

#include <CoreGraphics/CGWindowLevel.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;

#define ON  kWSWindowListOnScreen
#define OFF 0

/* Front-most first, as WindowServer reports it. */
static const struct wsWindowListEntry W[] = {
    { 10, 100, kCGPopUpMenuWindowLevelKey,   ON,  0, 0, 1, 1 },  /* 0 */
    { 20, 100, kCGNormalWindowLevelKey,      ON,  0, 0, 1, 1 },  /* 1 */
    { 30, 200, kCGNormalWindowLevelKey,      OFF, 0, 0, 1, 1 },  /* 2 hidden */
    { 40, 200, kCGNormalWindowLevelKey,      ON,  0, 0, 1, 1 },  /* 3 */
    { 50, 300, kCGDesktopIconWindowLevelKey, ON,  0, 0, 1, 1 },  /* 4 desktop */
    { 60, 300, kCGDesktopWindowLevelKey,     ON,  0, 0, 1, 1 },  /* 5 desktop */
};
#define N (sizeof(W) / sizeof(W[0]))

static void expect(const char *name, uint32_t option, uint32_t rel,
                   const char *want)
{
    size_t idx[N], n, i;
    char got[64] = "";

    n = _CGWindowListFilter(W, N, option, rel, idx);
    for (i = 0; i < n; i++) {
        char d[4];
        snprintf(d, sizeof(d), "%zu", idx[i]);
        strcat(got, d);
    }
    checks++;
    if (strcmp(got, want) != 0) {
        fails++;
        printf("FAIL %s: got \"%s\", want \"%s\"\n", name, got, want);
    }
}

int main(void)
{
    const uint32_t A = _CGWLOptionOnScreenAboveWindow;
    const uint32_t B = _CGWLOptionOnScreenBelowWindow;
    const uint32_t I = _CGWLOptionIncludingWindow;
    const uint32_t D = _CGWLExcludeDesktopElements;
    const uint32_t S = _CGWLOptionOnScreenOnly;

    expect("all", 0, 0, "012345");
    expect("all ignores relative", 0, 40, "012345");
    expect("on screen", S, 0, "01345");
    expect("on screen, no desktop", S | D, 0, "013");
    expect("all, no desktop", D, 0, "0123");

    expect("above 40", A, 40, "01");
    expect("above 40 incl", A | I, 40, "013");
    expect("below 20", B, 20, "345");          /* 30 is hidden */
    expect("below 20 incl", B | I, 20, "1345");
    expect("below 20 no desktop", B | D, 20, "3");
    expect("above+below 40", A | B, 40, "0145");
    expect("above hidden 30 incl", A | I, 30, "012"); /* itself even if hidden */
    expect("above front-most", A, 10, "");
    expect("below back-most", B, 60, "");

    expect("above null window", A, 0, "01345");
    expect("below null window", B | D, 0, "013");
    expect("unknown window", A, 99, "");
    expect("including only", I, 30, "2");
    expect("including null", I, 0, "");

    {   /* empty list */
        size_t idx[1];
        checks++;
        if (_CGWindowListFilter(W, 0, S, 0, idx) != 0) {
            fails++;
            puts("FAIL empty list");
        }
    }

    printf("%d checks, %d failures\n", checks, fails);
    return fails != 0;
}
