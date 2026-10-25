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
 * Null renderer: a complete CGL driver that renders nothing.
 *
 * It exercises the driver interface end to end and lets the CGL and
 * AppKit paths be tested without a rasterizer. Enabled only when the
 * environment variable CGL_ENABLE_NULL_RENDERER=1 is set.
 */

#include "CGLInternal.h"

#include <stdlib.h>

struct null_context {
    GLint dummy;
};

static CGLError null_describe(const CGLDriver *driver, CGLRendererProperty prop,
                              GLint *value)
{
    (void)driver;
    switch (prop) {
    case kCGLRPAccelerated:
    case kCGLRPAcceleratedCompute:
    case kCGLRPCompliant:
    case kCGLRPFullScreen:
    case kCGLRPGPUVertProcCapable:
    case kCGLRPGPUFragProcCapable:
    case kCGLRPRemovable:
    case kCGLRPMaxAuxBuffers:
    case kCGLRPMaxSampleBuffers:
    case kCGLRPMaxSamples:
    case kCGLRPSampleModes:
    case kCGLRPSampleAlpha:
    case kCGLRPVideoMemory:
    case kCGLRPTextureMemory:
    case kCGLRPVideoMemoryMegabytes:
    case kCGLRPTextureMemoryMegabytes:
    case kCGLRPRegistryIDLow:
    case kCGLRPRegistryIDHigh:
    case kCGLRPMajorGLVersion:
        *value = 0;
        return kCGLNoError;
    case kCGLRPOnline:
    case kCGLRPWindow:
    case kCGLRPOffScreen:
    case kCGLRPBackingStore:
    case kCGLRPMPSafe:
    case kCGLRPMultiScreen:
        *value = 1;
        return kCGLNoError;
    case kCGLRPBufferModes:
        *value = kCGLMonoscopicBit | kCGLSingleBufferBit | kCGLDoubleBufferBit;
        return kCGLNoError;
    case kCGLRPColorModes:
        *value = kCGLRGB888Bit | kCGLARGB8888Bit;
        return kCGLNoError;
    case kCGLRPDepthModes:
        *value = kCGL0Bit | kCGL16Bit | kCGL24Bit | kCGL32Bit;
        return kCGLNoError;
    case kCGLRPStencilModes:
        *value = kCGL0Bit | kCGL8Bit;
        return kCGLNoError;
    case kCGLRPAccumModes:
        *value = kCGL0Bit;
        return kCGLNoError;
    default:
        return kCGLBadProperty;
    }
}

static GLint pick_depth(const CGLDriverPixelFormatRequest *req)
{
    GLint want = req->value[kCGLPFADepthSize];

    if (!req->present[kCGLPFADepthSize] || want == 0)
        return req->present[kCGLPFAMaximumPolicy] ? 32 : 0;
    if (req->present[kCGLPFAMaximumPolicy] || want > 24)
        return 32;
    return want > 16 ? 24 : 16;
}

static CGLError null_choose(const CGLDriver *driver,
                            const CGLDriverPixelFormatRequest *req,
                            CGLDriverPixelFormat *fmt)
{
    (void)driver;

    /* Things this renderer cannot provide. */
    if (req->value[kCGLPFAColorSize] > 32 || req->value[kCGLPFAAlphaSize] > 8 ||
        req->value[kCGLPFAStencilSize] > 8 || req->value[kCGLPFAAccumSize] > 0 ||
        req->value[kCGLPFAAuxBuffers] > 0 || req->value[kCGLPFASamples] > 0 ||
        req->value[kCGLPFASampleBuffers] > 0 || req->present[kCGLPFAStereo] ||
        req->present[kCGLPFAColorFloat] || req->present[kCGLPFATripleBuffer] ||
        req->present[kCGLPFAFullScreen])
        return kCGLBadMatch;

    fmt->value[kCGLPFAColorSize] = req->value[kCGLPFAColorSize] > 24 ? 32 : 24;
    fmt->value[kCGLPFAAlphaSize] = req->present[kCGLPFAAlphaSize] &&
                                   req->value[kCGLPFAAlphaSize] > 0 ? 8 : 0;
    fmt->value[kCGLPFADepthSize] = pick_depth(req);
    fmt->value[kCGLPFAStencilSize] = req->value[kCGLPFAStencilSize] > 0 ? 8 : 0;
    fmt->value[kCGLPFADoubleBuffer] = req->present[kCGLPFADoubleBuffer];
    fmt->value[kCGLPFABackingStore] = req->present[kCGLPFABackingStore];
    fmt->value[kCGLPFAWindow] = 1;
    fmt->value[kCGLPFAOffScreen] = 1;
    fmt->value[kCGLPFAPBuffer] = 1;
    fmt->value[kCGLPFAMPSafe] = 1;
    fmt->value[kCGLPFAMultiScreen] = 1;
    fmt->value[kCGLPFAAllowOfflineRenderers] =
        req->present[kCGLPFAAllowOfflineRenderers];
    fmt->value[kCGLPFAOpenGLProfile] = req->present[kCGLPFAOpenGLProfile] ?
        req->value[kCGLPFAOpenGLProfile] : kCGLOGLPVersion_Legacy;
    return kCGLNoError;
}

static CGLError null_create(const CGLDriver *driver,
                            const CGLDriverPixelFormat *format, void *share,
                            void **context)
{
    (void)driver;
    (void)format;
    (void)share;
    *context = calloc(1, sizeof(struct null_context));
    return *context != NULL ? kCGLNoError : kCGLBadAlloc;
}

static void null_destroy(void *context)
{
    free(context);
}

static CGLError null_make_current(void *context)
{
    (void)context;
    return kCGLNoError;
}

const CGLDriver cgl_null_driver = {
    .abiVersion         = CGL_DRIVER_ABI_VERSION,
    .name               = "Minkar Null Renderer",
    .rendererID         = kCGLRendererMinkarNullID,
    .displayMask        = 0x1,
    .describeRenderer   = null_describe,
    .choosePixelFormat  = null_choose,
    .createContext      = null_create,
    .destroyContext     = null_destroy,
    .makeCurrent        = null_make_current,
};
