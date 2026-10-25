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
 * CGL API tests: argument validation, pixel formats, contexts, share
 * groups, parameters, enables, per-thread current context, drawables,
 * pixel buffers and global options, using a recording test driver.
 *
 * Must run with no other renderer registered (CGL_ENABLE_NULL_RENDERER
 * unset or not "1").
 */

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLDriver.h>
#include <OpenGL/CGLPrivate.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define OK(e) CHECK((e) == kCGLNoError)

/* A recording test driver, "accelerated", rendererID 0x00ABCD00 */
static int bound, created, destroyed, flushed, params_seen, unbinds;
static CGLError t_desc(const CGLDriver *d, CGLRendererProperty p, GLint *v) {
    (void)d; if (p == kCGLRPAccelerated || p == kCGLRPOnline) { *v = 1; return kCGLNoError; }
    if (p == kCGLRPGPUVertProcCapable) { *v = 1; return kCGLNoError; }
    return kCGLBadProperty; }
static CGLError t_choose(const CGLDriver *d, const CGLDriverPixelFormatRequest *r, CGLDriverPixelFormat *f) {
    (void)d; if (r->value[kCGLPFASamples] > 4) return kCGLBadMatch;
    f->value[kCGLPFAColorSize] = 32; f->value[kCGLPFADepthSize] = r->value[kCGLPFADepthSize];
    f->value[kCGLPFADoubleBuffer] = r->present[kCGLPFADoubleBuffer]; return kCGLNoError; }
static CGLError t_create(const CGLDriver *d, const CGLDriverPixelFormat *f, void *s, void **c) {
    (void)d; (void)f; (void)s; created++; *c = malloc(8); return kCGLNoError; }
static void t_destroy(void *c) { destroyed++; free(c); }
static CGLError t_current(void *c) { if (c) bound++; else unbinds++; return kCGLNoError; }
static CGLError t_flush(void *c) { (void)c; flushed++; return kCGLNoError; }
static CGLError t_param(void *c, GLint p, const GLint *v, GLint n) { (void)c; (void)v;
    params_seen++; if (p == kCGLCPSwapInterval && n != 1) return kCGLBadValue; return kCGLNoError; }
static const CGLDriver test_driver = {
    .abiVersion = CGL_DRIVER_ABI_VERSION, .name = "test", .rendererID = 0x00ABCD00,
    .displayMask = 0x3, .describeRenderer = t_desc, .choosePixelFormat = t_choose,
    .createContext = t_create, .destroyContext = t_destroy, .makeCurrent = t_current,
    .flushDrawable = t_flush, .setParameter = t_param };

static void *thread_fn(void *arg) {
    CGLContextObj c = arg;
    CHECK(CGLGetCurrentContext() == NULL);          /* per-thread */
    OK(CGLSetCurrentContext(c));
    CHECK(CGLGetCurrentContext() == c);
    OK(CGLLockContext(c)); OK(CGLUnlockContext(c));
    OK(CGLSetCurrentContext(NULL));
    return NULL;
}

int main(void) {
    CGLPixelFormatObj pf; CGLContextObj ctx, ctx2; GLint n, v, vals[4];
    CGLRendererInfoObj ri;

    /* 1. No renderers registered: no pixel format, no error (macOS behavior) */
    CGLPixelFormatAttribute basic[] = { kCGLPFADoubleBuffer, kCGLPFADepthSize, 24, 0 };
    OK(CGLChoosePixelFormat(basic, &pf, &n));
    CHECK(pf == NULL && n == 0);
    CHECK(CGLCreateContext(NULL, NULL, &ctx) == kCGLBadPixelFormat && ctx == NULL);
    OK(CGLQueryRendererInfo(0xFFFFFFFF, &ri, &n)); CHECK(n == 0);
    OK(CGLDescribeRenderer(ri, 0, kCGLRPRendererCount, &v)); CHECK(v == 0);
    OK(CGLDestroyRendererInfo(ri));

    /* 2. Argument validation */
    CHECK(CGLChoosePixelFormat(basic, NULL, &n) == kCGLBadAddress);
    CGLPixelFormatAttribute bad[] = { 9999, 0 };
    CHECK(CGLChoosePixelFormat(bad, &pf, &n) == kCGLBadAttribute);
    CGLPixelFormatAttribute badprof[] = { kCGLPFAOpenGLProfile, 0x1234, 0 };
    CHECK(CGLChoosePixelFormat(badprof, &pf, &n) == kCGLBadValue);
    CHECK(CGLSetParameter(NULL, kCGLCPSwapInterval, vals) == kCGLBadContext);

    /* 3. Driver registration */
    CGLDriver broken = test_driver; broken.makeCurrent = NULL;
    CHECK(CGLRegisterDriver(&broken) == kCGLBadCodeModule);
    OK(CGLRegisterDriver(&test_driver));
    CHECK(CGLRegisterDriver(&test_driver) == kCGLBadMatch);

    OK(CGLQueryRendererInfo(0x1, &ri, &n)); CHECK(n == 1);
    OK(CGLDescribeRenderer(ri, 0, kCGLRPRendererID, &v)); CHECK(v == 0x00ABCD00);
    OK(CGLDescribeRenderer(ri, 0, kCGLRPAccelerated, &v)); CHECK(v == 1);
    CHECK(CGLDescribeRenderer(ri, 0, kCGLRPMaxSamples, &v) == kCGLBadProperty);
    CHECK(CGLDescribeRenderer(ri, 5, kCGLRPRendererID, &v) == kCGLBadValue);
    OK(CGLDestroyRendererInfo(ri));
    OK(CGLQueryRendererInfo(0x4, &ri, &n)); CHECK(n == 0); OK(CGLDestroyRendererInfo(ri));

    /* 4. Pixel format with a value of 0 mid-list must not stop parsing */
    CGLPixelFormatAttribute zero_mid[] = { kCGLPFADepthSize, 0, kCGLPFADoubleBuffer, 0 };
    OK(CGLChoosePixelFormat(zero_mid, &pf, &n)); CHECK(pf && n == 1);
    OK(CGLDescribePixelFormat(pf, 0, kCGLPFADoubleBuffer, &v)); CHECK(v == 1);
    CGLReleasePixelFormat(pf);

    OK(CGLChoosePixelFormat(basic, &pf, &n)); CHECK(pf != NULL && n == 1);
    OK(CGLDescribePixelFormat(pf, 0, kCGLPFADepthSize, &v)); CHECK(v == 24);
    OK(CGLDescribePixelFormat(pf, 0, kCGLPFARendererID, &v)); CHECK(v == 0x00ABCD00);
    OK(CGLDescribePixelFormat(pf, 0, kCGLPFAAccelerated, &v)); CHECK(v == 1);
    OK(CGLDescribePixelFormat(pf, 0, kCGLPFAOpenGLProfile, &v)); CHECK(v == kCGLOGLPVersion_Legacy);
    OK(CGLDescribePixelFormat(pf, 0, kCGLPFAVirtualScreenCount, &v)); CHECK(v == 1);
    CHECK(CGLDescribePixelFormat(pf, 3, kCGLPFADepthSize, &v) == kCGLBadValue);
    CGLPixelFormatAttribute ms[] = { kCGLPFASamples, 8, 0 };
    { CGLPixelFormatObj p2; OK(CGLChoosePixelFormat(ms, &p2, &n)); CHECK(p2 == NULL && n == 0); }
    CGLPixelFormatAttribute rid[] = { kCGLPFARendererID, 0x00ABCD07, 0 }; /* masked match */
    { CGLPixelFormatObj p2; OK(CGLChoosePixelFormat(rid, &p2, &n)); CHECK(p2 && n == 1); CGLReleasePixelFormat(p2); }

    /* 5. Contexts, retain counts, share groups */
    OK(CGLCreateContext(pf, NULL, &ctx)); CHECK(created == 1);
    CHECK(CGLGetPixelFormatRetainCount(pf) == 2);
    CHECK(CGLGetPixelFormat(ctx) == pf);
    OK(CGLCreateContext(pf, ctx, &ctx2));
    CHECK(CGLGetShareGroup(ctx) == CGLGetShareGroup(ctx2) && CGLGetShareGroup(ctx) != NULL);
    CHECK(CGLRetainContext(ctx) == ctx && CGLGetContextRetainCount(ctx) == 2);
    CGLReleaseContext(ctx); CHECK(CGLGetContextRetainCount(ctx) == 1);

    /* 6. Parameters and enables */
    OK(CGLGetParameter(ctx, kCGLCPSurfaceOpacity, &v)); CHECK(v == 1);
    vals[0] = 1; OK(CGLSetParameter(ctx, kCGLCPSwapInterval, vals));
    OK(CGLGetParameter(ctx, kCGLCPSwapInterval, &v)); CHECK(v == 1);
    vals[0] = 640; vals[1] = 480; OK(CGLSetParameter(ctx, kCGLCPSurfaceBackingSize, vals));
    OK(CGLSurfaceResize(ctx, 800, 600));
    memset(vals, 0, sizeof vals); OK(CGLGetParameter(ctx, kCGLCPSurfaceBackingSize, vals));
    CHECK(vals[0] == 800 && vals[1] == 600);
    vals[0] = 2; CHECK(CGLSetParameter(ctx, kCGLCPSurfaceOpacity, vals) == kCGLBadValue);
    CHECK(CGLSetParameter(ctx, kCGLCPCurrentRendererID, vals) == kCGLBadEnumeration);
    CHECK(CGLSetParameter(ctx, (CGLContextParameter)12345, vals) == kCGLBadEnumeration);
    OK(CGLGetParameter(ctx, kCGLCPCurrentRendererID, &v)); CHECK(v == 0x00ABCD00);
    OK(CGLGetParameter(ctx, kCGLCPGPUVertexProcessing, &v)); CHECK(v == 1);
    OK(CGLGetParameter(ctx, kCGLCPHasDrawable, &v)); CHECK(v == 0);
    vals[0] = 7; OK(CGLSetParameter(ctx, (CGLContextParameter)kCGLCPSurfaceWindowNumber, vals));
    OK(CGLGetParameter(ctx, kCGLCPHasDrawable, &v)); CHECK(v == 1);
    OK(CGLClearDrawable(ctx));
    OK(CGLGetParameter(ctx, kCGLCPHasDrawable, &v)); CHECK(v == 0);
    CHECK(params_seen >= 4);
    OK(CGLIsEnabled(ctx, kCGLCERasterization, &v)); CHECK(v == 1);
    OK(CGLEnable(ctx, kCGLCESwapLimit)); OK(CGLIsEnabled(ctx, kCGLCESwapLimit, &v)); CHECK(v == 1);
    OK(CGLDisable(ctx, kCGLCESwapLimit)); OK(CGLIsEnabled(ctx, kCGLCESwapLimit, &v)); CHECK(v == 0);
    CHECK(CGLEnable(ctx, (CGLContextEnable)1) == kCGLBadEnumeration);

    /* 7. Current context: binding, per-thread, unbind on release */
    OK(CGLSetCurrentContext(ctx)); CHECK(CGLGetCurrentContext() == ctx && bound == 1);
    OK(CGLSetCurrentContext(ctx));  CHECK(bound == 1);   /* no-op */
    pthread_t th; pthread_create(&th, NULL, thread_fn, ctx2); pthread_join(th, NULL);
    CHECK(CGLGetCurrentContext() == ctx);
    OK(CGLLockContext(ctx)); OK(CGLLockContext(ctx)); /* recursive */
    OK(CGLUnlockContext(ctx)); OK(CGLUnlockContext(ctx));
    OK(CGLFlushDrawable(ctx)); CHECK(flushed == 1);

    /* 8. Offscreen and pbuffers */
    static unsigned char buf[16 * 16 * 4];
    CHECK(CGLSetOffScreen(ctx, 16, 16, 16, buf) == kCGLBadValue);   /* rowbytes too small */
    OK(CGLSetOffScreen(ctx, 16, 16, 64, buf));
    { GLsizei w, h; GLint rb; void *b; OK(CGLGetOffScreen(ctx, &w, &h, &rb, &b));
      CHECK(w == 16 && h == 16 && rb == 64 && b == buf); }
    CGLPBufferObj pb;
    CHECK(CGLCreatePBuffer(64, 32, 0x8513, 0x1908, 0, &pb) == kCGLBadValue);  /* cube not square */
    CHECK(CGLCreatePBuffer(64, 64, 0x0DE1, 0x1908, 9, &pb) == kCGLBadValue);  /* level > log2 */
    OK(CGLCreatePBuffer(64, 64, 0x0DE1, 0x1908, 6, &pb));
    CHECK(CGLSetPBuffer(ctx, pb, 0x8515, 0, 0) == kCGLBadValue);              /* face on 2D */
    OK(CGLSetPBuffer(ctx, pb, 0, 2, 0)); CHECK(CGLGetPBufferRetainCount(pb) == 2);
    { CGLPBufferObj g; GLenum f; GLint l, s; OK(CGLGetPBuffer(ctx, &g, &f, &l, &s));
      CHECK(g == pb && l == 2 && s == 0); }
    OK(CGLTexImagePBuffer(ctx, pb, 0x0405));
    CHECK(CGLTexImagePBuffer(ctx, pb, 0x1234) == kCGLBadValue);
    OK(CGLDestroyPBuffer(pb)); CHECK(CGLGetPBufferRetainCount(pb) == 1);

    /* 9. Virtual screens, update, copy, fullscreen */
    OK(CGLGetVirtualScreen(ctx, &v)); CHECK(v == 0);
    CHECK(CGLSetVirtualScreen(ctx, 1) == kCGLBadValue);
    OK(CGLUpdateContext(ctx));
    OK(CGLCopyContext(ctx, ctx2, 0xFFFFFFFF));
    CHECK(CGLCopyContext(ctx, ctx, 0) == kCGLBadMatch);
    /* Deprecated on purpose: must fail cleanly. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    CHECK(CGLSetFullScreen(ctx) == kCGLBadFullScreen);
#pragma GCC diagnostic pop

    /* 10. Global options, version, strings */
    OK(CGLSetOption(kCGLGOFormatCacheSize, 9)); OK(CGLGetOption(kCGLGOFormatCacheSize, &v)); CHECK(v == 9);
    CHECK(CGLSetOption(kCGLGOFormatCacheSize, 0) == kCGLBadValue);
    CHECK(CGLSetOption((CGLGlobalOption)42, 1) == kCGLBadEnumeration);
    { GLint ma, mi; CGLGetVersion(&ma, &mi); CHECK(ma == 1 && mi == 3); }
    CHECK(strcmp(CGLErrorString(kCGLBadContext), "invalid context") == 0);
    CHECK(strcmp(CGLErrorString((CGLError)1), "unknown error") == 0);

    /* 11. Releasing the current context unbinds it */
    CGLReleaseContext(ctx2);
    int unb = unbinds;
    CGLReleaseContext(ctx);
    CHECK(CGLGetCurrentContext() == NULL && unbinds == unb + 1);
    CHECK(destroyed == 2);
    CHECK(CGLGetPixelFormatRetainCount(pf) == 1);
    OK(CGLDestroyPixelFormat(pf));

    printf("%d checks, %d failures\n", checks, fails);
    return fails != 0;
}
