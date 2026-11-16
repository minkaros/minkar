/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A context's window surface. A context given a window (the private
 * kCGLCPSurfaceWindowNumber) and a frame (kCGLCPSurfaceFrame: where its
 * view is, and its size) draws into a drawable of that size, or of
 * kCGLCPSurfaceBackingSize while kCGLCESurfaceBackingSize is enabled.
 * The renderer reads each frame back at CGLFlushDrawable (as an offscreen
 * drawable); the frame goes into the next of three IOSurfaces (32-bit
 * BGRA, rows from the top, the frame's size: a backing size is scaled to
 * it), opaque unless kCGLCPSurfaceOpacity is 0, and the presenter is
 * given it to show (AppKit's NSOpenGLContext: the window server composes
 * it under the window). A hidden surface is drawn but not shown. Three
 * surfaces in turn: the window server may still be composing the one
 * before while the next is written.
 */

#include "CGLInternal.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurfaceRef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static GLint param(CGLContextObj ctx, GLint pname, int i)
{
    GLint slot = cgl_param_slot(pname);
    return slot >= 0 ? ctx->params[slot][i] : 0;
}

static GLboolean backing_size_enabled(CGLContextObj ctx)
{
    GLint on = 0;
    /* The lock is recursive: the context is locked already. */
    CGLIsEnabled(ctx, kCGLCESurfaceBackingSize, &on);
    return on != 0;
}

static void drop_surfaces(CGLContextObj ctx)
{
    for (int i = 0; i < 3; i++) {
        if (ctx->surfaces[i] != NULL)
            CFRelease(ctx->surfaces[i]);
        ctx->surfaces[i] = NULL;
    }
    ctx->surfaceNext = 0;
}

static void set_number(CFMutableDictionaryRef d, CFStringRef key, long long v)
{
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongLongType, &v);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

static IOSurfaceRef new_surface(GLsizei width, GLsizei height)
{
    CFMutableDictionaryRef p = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    set_number(p, kIOSurfaceWidth, width);
    set_number(p, kIOSurfaceHeight, height);
    set_number(p, kIOSurfaceBytesPerElement, 4);
    set_number(p, kIOSurfacePixelFormat, 'BGRA');
    IOSurfaceRef s = IOSurfaceCreate(p);
    CFRelease(p);
    return s;
}

/* The drawable as the parameters say: made, resized or taken away. */
void cgl_surface_update(CGLContextObj ctx)
{
    GLsizei width = param(ctx, kCGLCPSurfaceFrame, 2), height = param(ctx, kCGLCPSurfaceFrame, 3);
    GLsizei drawWidth = width, drawHeight = height;
    GLboolean active;

    if (backing_size_enabled(ctx) && param(ctx, kCGLCPSurfaceBackingSize, 0) > 0 &&
        param(ctx, kCGLCPSurfaceBackingSize, 1) > 0) {
        drawWidth = param(ctx, kCGLCPSurfaceBackingSize, 0);
        drawHeight = param(ctx, kCGLCPSurfaceBackingSize, 1);
    }
    /* An offscreen drawable or a pbuffer the app set comes first. */
    active = !ctx->hasOffScreen && ctx->pbuffer == NULL && ctx->driver->setOffScreen != NULL &&
             param(ctx, kCGLCPSurfaceWindowNumber, 0) != 0 && width > 0 && height > 0;
    if (!active) {
        if (ctx->surfaceActive && !ctx->hasOffScreen && ctx->pbuffer == NULL &&
            ctx->driver->clearDrawable != NULL)
            ctx->driver->clearDrawable(ctx->driverContext);
        free(ctx->surfaceStaging);
        ctx->surfaceStaging = NULL;
        drop_surfaces(ctx);
        ctx->surfaceActive = 0;
        ctx->surfaceWidth = ctx->surfaceHeight = 0;
        return;
    }
    if (ctx->surfaceActive && width == ctx->surfaceWidth && height == ctx->surfaceHeight &&
        drawWidth == ctx->surfaceDrawWidth && drawHeight == ctx->surfaceDrawHeight)
        return;
    /* The surfaces follow the frame; the staging buffer the drawable. */
    if (width != ctx->surfaceWidth || height != ctx->surfaceHeight)
        drop_surfaces(ctx);
    void *staging = malloc((size_t)drawWidth * drawHeight * 4);
    if (staging == NULL)
        return;
    if (ctx->driver->setOffScreen(ctx->driverContext, drawWidth, drawHeight, drawWidth * 4, staging) !=
        kCGLNoError) {
        free(staging);
        return;
    }
    free(ctx->surfaceStaging);
    ctx->surfaceStaging = staging;
    ctx->surfaceActive = 1;
    ctx->surfaceWidth = width;
    ctx->surfaceHeight = height;
    ctx->surfaceDrawWidth = drawWidth;
    ctx->surfaceDrawHeight = drawHeight;
}

/* After the renderer read the frame into the staging buffer: into the
   next surface, turned upright (and scaled), then to the presenter. */
CGLError cgl_surface_flush(CGLContextObj ctx)
{
    if (!ctx->surfaceActive || ctx->surfaceStaging == NULL)
        return kCGLNoError;
    GLsizei w = ctx->surfaceWidth, h = ctx->surfaceHeight;
    GLsizei sw = ctx->surfaceDrawWidth, sh = ctx->surfaceDrawHeight;
    IOSurfaceRef s = ctx->surfaces[ctx->surfaceNext];

    if (s == NULL && (s = ctx->surfaces[ctx->surfaceNext] = new_surface(w, h)) == NULL)
        return kCGLBadAlloc;
    uint32_t alpha = param(ctx, kCGLCPSurfaceOpacity, 0) != 0 ? 0xFF000000u : 0;
    IOSurfaceLock(s, 0, NULL);
    uint8_t *base = IOSurfaceGetBaseAddress(s);
    size_t rowBytes = IOSurfaceGetBytesPerRow(s);
    const uint32_t *staging = ctx->surfaceStaging;
    for (GLsizei y = 0; y < h; y++) {
        uint32_t *out = (uint32_t *)(void *)(base + (size_t)y * rowBytes);
        /* The renderer's rows run from the bottom. */
        GLsizei sy = sh - 1 - (GLsizei)((int64_t)y * sh / h);
        const uint32_t *in = staging + (size_t)sy * sw;
        if (sw == w) {
            if (alpha == 0)
                memcpy(out, in, (size_t)w * 4);
            else
                for (GLsizei x = 0; x < w; x++)
                    out[x] = in[x] | alpha;
        } else
            for (GLsizei x = 0; x < w; x++)
                out[x] = in[(int64_t)x * sw / w] | alpha;
    }
    IOSurfaceUnlock(s, 0, NULL);
    ctx->surfaceNext = (ctx->surfaceNext + 1) % 3;
    if (ctx->surfacePresenter != NULL && param(ctx, kCGLCPSurfaceHidden, 0) == 0)
        ctx->surfacePresenter(ctx->surfacePresenterInfo, ctx, s);
    return kCGLNoError;
}

/* After glFlush or glFinish: a single-buffered context shows what it drew
   then (a double-buffered one at CGLFlushDrawable only). */
void cgl_surface_gl_flushed(void)
{
    CGLContextObj ctx = CGLGetCurrentContext();
    GLint doubleBuffer = 0;

    if (ctx == NULL || !ctx->surfaceActive)
        return;
    CGLDescribePixelFormat(ctx->pixelFormat, ctx->virtualScreen, kCGLPFADoubleBuffer, &doubleBuffer);
    if (!doubleBuffer)
        CGLFlushDrawable(ctx);
}

void cgl_surface_free(CGLContextObj ctx)
{
    free(ctx->surfaceStaging);
    ctx->surfaceStaging = NULL;
    drop_surfaces(ctx);
    ctx->surfaceActive = 0;
}

CGL_EXPORT void _CGLSetSurfacePresenter(CGLContextObj ctx, CGLSurfacePresenter presenter, void *info)
{
    if (ctx == NULL)
        return;
    pthread_mutex_lock(&ctx->lock);
    ctx->surfacePresenter = (void (*)(void *, CGLContextObj, void *))presenter;
    ctx->surfacePresenterInfo = info;
    pthread_mutex_unlock(&ctx->lock);
}
