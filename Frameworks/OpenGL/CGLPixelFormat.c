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

/* Pixel formats: CGLChoosePixelFormat and friends. */

#include "CGLInternal.h"

#include <stdlib.h>
#include <string.h>

enum {
    ATTR_UNKNOWN = 0,
    ATTR_BOOLEAN,   /* no value follows in the attribute list */
    ATTR_VALUED,    /* an integer value follows */
};

static uint8_t attribute_kind(GLint attr)
{
    switch (attr) {
    case kCGLPFAColorSize:
    case kCGLPFAAlphaSize:
    case kCGLPFADepthSize:
    case kCGLPFAStencilSize:
    case kCGLPFAAccumSize:
    case kCGLPFAAuxBuffers:
    case kCGLPFASampleBuffers:
    case kCGLPFASamples:
    case kCGLPFARendererID:
    case kCGLPFADisplayMask:
    case kCGLPFAOpenGLProfile:
        return ATTR_VALUED;

    case kCGLPFAAllRenderers:
    case kCGLPFATripleBuffer:
    case kCGLPFADoubleBuffer:
    case kCGLPFAStereo:
    case kCGLPFAMinimumPolicy:
    case kCGLPFAMaximumPolicy:
    case kCGLPFAClosestPolicy:
    case kCGLPFAColorFloat:
    case kCGLPFAMultisample:
    case kCGLPFASupersample:
    case kCGLPFASampleAlpha:
    case kCGLPFANoRecovery:
    case kCGLPFAAccelerated:
    case kCGLPFABackingStore:
    case kCGLPFABackingVolatile:
    case kCGLPFAAllowOfflineRenderers:
    case kCGLPFAAcceleratedCompute:
    case kCGLPFASupportsAutomaticGraphicsSwitching:
    case kCGLPFAAuxDepthStencil:
    case kCGLPFAOffScreen:
    case kCGLPFAWindow:
    case kCGLPFACompliant:
    case kCGLPFAPBuffer:
    case kCGLPFARemotePBuffer:
    case kCGLPFASingleRenderer:
    case kCGLPFARobust:
    case kCGLPFAMPSafe:
    case kCGLPFAMultiScreen:
    case kCGLPFAFullScreen:
        return ATTR_BOOLEAN;

    default:
        return ATTR_UNKNOWN;
    }
}

static CGLError parse_request(const CGLPixelFormatAttribute *attribs,
                              CGLDriverPixelFormatRequest *req)
{
    const CGLPixelFormatAttribute *p;

    memset(req, 0, sizeof(*req));
    for (p = attribs; *p != 0; p++) {
        GLint attr = (GLint)*p;

        switch (attribute_kind(attr)) {
        case ATTR_BOOLEAN:
            req->value[attr] = 1;
            req->present[attr] = 1;
            break;
        case ATTR_VALUED:
            /* The value may legitimately be 0 (e.g. kCGLPFADepthSize, 0). */
            p++;
            req->value[attr] = (GLint)*p;
            req->present[attr] = 1;
            break;
        default:
            return kCGLBadAttribute;
        }
    }
    return kCGLNoError;
}

static CGLError validate_request(const CGLDriverPixelFormatRequest *req)
{
    static const GLint sizes[] = {
        kCGLPFAColorSize, kCGLPFAAlphaSize, kCGLPFADepthSize,
        kCGLPFAStencilSize, kCGLPFAAccumSize, kCGLPFAAuxBuffers,
        kCGLPFASampleBuffers, kCGLPFASamples,
    };
    size_t i;

    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        if (req->value[sizes[i]] < 0)
            return kCGLBadValue;
    }
    if (req->present[kCGLPFAOpenGLProfile]) {
        switch (req->value[kCGLPFAOpenGLProfile]) {
        case kCGLOGLPVersion_Legacy:
        case kCGLOGLPVersion_3_2_Core:
        case kCGLOGLPVersion_GL4_Core:
            break;
        default:
            return kCGLBadValue;
        }
    }
    return kCGLNoError;
}

/* Renderer-level filters that do not need the driver's format logic. */
static int driver_acceptable(const CGLDriver *d,
                             const CGLDriverPixelFormatRequest *req)
{
    if (req->present[kCGLPFARendererID] &&
        (d->rendererID & kCGLRendererIDMatchingMask) !=
        (req->value[kCGLPFARendererID] & kCGLRendererIDMatchingMask))
        return 0;
    if (req->present[kCGLPFADisplayMask] &&
        (d->displayMask & (GLuint)req->value[kCGLPFADisplayMask]) == 0)
        return 0;
    if (req->present[kCGLPFAAccelerated] &&
        !cgl_driver_property(d, kCGLRPAccelerated, 0))
        return 0;
    if (req->present[kCGLPFAAcceleratedCompute] &&
        !cgl_driver_property(d, kCGLRPAcceleratedCompute, 0))
        return 0;
    if (!req->present[kCGLPFAAllowOfflineRenderers] &&
        !cgl_driver_property(d, kCGLRPOnline, 1))
        return 0;
    return 1;
}

CGLError CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs,
                              CGLPixelFormatObj *pix, GLint *npix)
{
    CGLDriverPixelFormatRequest req;
    CGLPixelFormatObj pf;
    CGLError err;
    GLint i, n;

    if (pix == NULL || npix == NULL)
        return kCGLBadAddress;
    *pix = NULL;
    *npix = 0;
    if (attribs == NULL)
        return kCGLBadAttribute;

    if ((err = parse_request(attribs, &req)) != kCGLNoError)
        return err;
    if ((err = validate_request(&req)) != kCGLNoError)
        return err;

    n = cgl_driver_count();
    if (n == 0)
        return kCGLNoError;     /* no renderer: no pixel format, no error */

    pf = calloc(1, sizeof(*pf));
    if (pf == NULL)
        return kCGLBadAlloc;
    pf->drivers = calloc((size_t)n, sizeof(*pf->drivers));
    pf->formats = calloc((size_t)n, sizeof(*pf->formats));
    if (pf->drivers == NULL || pf->formats == NULL) {
        free(pf->drivers);
        free(pf->formats);
        free(pf);
        return kCGLBadAlloc;
    }

    for (i = 0; i < n; i++) {
        const CGLDriver *d = cgl_driver_at(i);
        CGLDriverPixelFormat *fmt = &pf->formats[pf->nscreens];

        if (d == NULL || !driver_acceptable(d, &req))
            continue;
        memset(fmt, 0, sizeof(*fmt));
        if (d->choosePixelFormat(d, &req, fmt) != kCGLNoError)
            continue;

        fmt->value[kCGLPFARendererID] = d->rendererID;
        fmt->value[kCGLPFADisplayMask] = (GLint)d->displayMask;
        fmt->value[kCGLPFAAccelerated] =
            cgl_driver_property(d, kCGLRPAccelerated, 0) ? 1 : 0;
        if (fmt->value[kCGLPFAOpenGLProfile] == 0)
            fmt->value[kCGLPFAOpenGLProfile] = kCGLOGLPVersion_Legacy;

        pf->drivers[pf->nscreens++] = d;
        if (req.present[kCGLPFASingleRenderer])
            break;
    }

    if (pf->nscreens == 0) {
        free(pf->drivers);
        free(pf->formats);
        free(pf);
        return kCGLNoError;
    }

    atomic_init(&pf->refcount, 1);
    *pix = pf;
    *npix = pf->nscreens;
    return kCGLNoError;
}

CGLError CGLDescribePixelFormat(CGLPixelFormatObj pix, GLint pix_num,
                                CGLPixelFormatAttribute attrib, GLint *value)
{
    GLint attr = (GLint)attrib;

    if (pix == NULL)
        return kCGLBadPixelFormat;
    if (value == NULL)
        return kCGLBadAddress;
    if (attr == kCGLPFAVirtualScreenCount) {
        *value = pix->nscreens;
        return kCGLNoError;
    }
    if (attribute_kind(attr) == ATTR_UNKNOWN)
        return kCGLBadAttribute;
    if (pix_num < 0 || pix_num >= pix->nscreens)
        return kCGLBadValue;

    *value = pix->formats[pix_num].value[attr];
    return kCGLNoError;
}

CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj pix)
{
    if (pix != NULL)
        atomic_fetch_add(&pix->refcount, 1);
    return pix;
}

void CGLReleasePixelFormat(CGLPixelFormatObj pix)
{
    if (pix == NULL)
        return;
    if (atomic_fetch_sub(&pix->refcount, 1) == 1) {
        free(pix->drivers);
        free(pix->formats);
        free(pix);
    }
}

CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pix)
{
    if (pix == NULL)
        return kCGLBadPixelFormat;
    CGLReleasePixelFormat(pix);
    return kCGLNoError;
}

GLuint CGLGetPixelFormatRetainCount(CGLPixelFormatObj pix)
{
    if (pix == NULL)
        return 0;
    return atomic_load(&pix->refcount);
}
