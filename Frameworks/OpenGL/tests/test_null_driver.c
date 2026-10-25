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
 * Null renderer tests. Run with CGL_ENABLE_NULL_RENDERER=1 to check the
 * built-in null driver, and without it to check that no renderer is
 * available by default.
 */

#include <OpenGL/OpenGL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void)
{
    const char *env = getenv("CGL_ENABLE_NULL_RENDERER");
    int enabled = env != NULL && strcmp(env, "1") == 0;
    CGLPixelFormatAttribute a[] = { kCGLPFADoubleBuffer, kCGLPFAColorSize, 24,
                                    kCGLPFADepthSize, 20, 0 };
    CGLPixelFormatAttribute acc[] = { kCGLPFAAccelerated, 0 };
    CGLPixelFormatObj pf, p2;
    CGLContextObj c;
    GLint n, v;

    CHECK(CGLChoosePixelFormat(a, &pf, &n) == kCGLNoError);
    if (!enabled) {
        CHECK(pf == NULL && n == 0);
        printf("null renderer disabled: %d failures\n", fails);
        return fails != 0;
    }

    CHECK(pf != NULL && n == 1);
    if (pf == NULL)
        return 1;
    CHECK(CGLDescribePixelFormat(pf, 0, kCGLPFADepthSize, &v) == kCGLNoError && v == 24);
    CHECK(CGLDescribePixelFormat(pf, 0, kCGLPFARendererID, &v) == kCGLNoError && v == 0x00FF0100);
    CHECK(CGLDescribePixelFormat(pf, 0, kCGLPFAAccelerated, &v) == kCGLNoError && v == 0);
    CHECK(CGLCreateContext(pf, NULL, &c) == kCGLNoError);
    CHECK(CGLSetCurrentContext(c) == kCGLNoError);
    CHECK(CGLFlushDrawable(c) == kCGLNoError);
    /* The null renderer is not accelerated. */
    CHECK(CGLChoosePixelFormat(acc, &p2, &n) == kCGLNoError && p2 == NULL);
    CGLReleaseContext(c);
    CGLReleasePixelFormat(pf);
    CHECK(CGLGetCurrentContext() == NULL);

    printf("null renderer enabled: %d failures\n", fails);
    return fails != 0;
}
