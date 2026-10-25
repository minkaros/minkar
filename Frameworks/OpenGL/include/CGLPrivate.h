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

/*
 * Minkar-private CGL additions used by AppKit and QuartzCore to bind a
 * context to a WindowServer surface. Not part of the macOS API.
 */

#ifndef _OPENGL_CGLPRIVATE_H_
#define _OPENGL_CGLPRIVATE_H_

#include <OpenGL/CGLTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Private context parameters (CGLContextParameter values). The range
 * 499-505 is unused by the public API.
 */
enum {
    kCGLCPSurfaceFrame          = 499, /* 4 values: x, y, width, height */
    kCGLCPSurfaceIsChildWindow  = 500, /* 1 value */
    kCGLCPWindowNumber          = 501, /* 1 value */
    kCGLCPOverlayPointer        = 502, /* 2 values: pointer bits */
    kCGLCPSurfaceBackingOrigin  = 503, /* 2 values: x, y */
    kCGLCPSurfaceWindowNumber   = 504, /* 1 value */
    kCGLCPSurfaceHidden         = 505, /* 1 value: 0 or 1 */
};

/* Resize the context's surface; same as setting kCGLCPSurfaceBackingSize. */
CGL_EXPORT CGLError CGLSurfaceResize(CGLContextObj ctx, GLint width, GLint height);

/* Ask the renderer to install its default compositing shaders. */
CGL_EXPORT CGLError CGLUseShaders(CGLContextObj ctx);

#ifdef __cplusplus
}
#endif

#endif /* _OPENGL_CGLPRIVATE_H_ */
