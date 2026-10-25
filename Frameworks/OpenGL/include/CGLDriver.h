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
 * CGL renderer driver interface (Minkar private).
 *
 * CGL itself is hardware independent: it validates arguments and keeps
 * track of pixel formats, contexts, share groups, parameters and the
 * per-thread current context. Everything that touches pixels is
 * delegated to a renderer driver (software rasterizer, GPU driver, ...)
 * registered with CGLRegisterDriver(). Each driver is identified by a
 * renderer ID, as on macOS.
 *
 * Optional callbacks may be NULL; CGL then treats the operation as a
 * successful no-op unless documented otherwise.
 */

#ifndef _OPENGL_CGLDRIVER_H_
#define _OPENGL_CGLDRIVER_H_

#include <OpenGL/CGLTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CGL_DRIVER_ABI_VERSION      1

/* Pixel format attribute tables are indexed by CGLPixelFormatAttribute. */
#define CGL_PFA_TABLE_SIZE          128

/* What the application asked for in CGLChoosePixelFormat. */
typedef struct CGLDriverPixelFormatRequest {
    GLint   value[CGL_PFA_TABLE_SIZE];   /* attribute value (1 for booleans) */
    uint8_t present[CGL_PFA_TABLE_SIZE]; /* attribute was given */
} CGLDriverPixelFormatRequest;

/*
 * A resolved pixel format for one virtual screen, as reported by
 * CGLDescribePixelFormat. Booleans are 0 or 1.
 */
typedef struct CGLDriverPixelFormat {
    GLint value[CGL_PFA_TABLE_SIZE];
} CGLDriverPixelFormat;

typedef struct CGLDriver {
    uint32_t    abiVersion;     /* CGL_DRIVER_ABI_VERSION */
    const char *name;           /* for diagnostics */
    GLint       rendererID;     /* kCGLRPRendererID */
    GLuint      displayMask;    /* displays this renderer can drive */

    /* Renderer capabilities for CGLDescribeRenderer (required). */
    CGLError (*describeRenderer)(const struct CGLDriver *driver,
                                 CGLRendererProperty prop, GLint *value);

    /*
     * Resolve a request into a concrete format (required). Return
     * kCGLBadMatch when the renderer cannot satisfy it.
     */
    CGLError (*choosePixelFormat)(const struct CGLDriver *driver,
                                  const CGLDriverPixelFormatRequest *request,
                                  CGLDriverPixelFormat *format);

    /* Context lifetime (required). 'share' is the share context's driver
     * context from the same driver, or NULL. */
    CGLError (*createContext)(const struct CGLDriver *driver,
                              const CGLDriverPixelFormat *format,
                              void *share, void **context);
    void     (*destroyContext)(void *context);

    /* Bind 'context' to the calling thread; NULL unbinds (required). */
    CGLError (*makeCurrent)(void *context);

    /* Drawables (optional). */
    CGLError (*flushDrawable)(void *context);
    CGLError (*clearDrawable)(void *context);
    CGLError (*updateContext)(void *context);
    CGLError (*setOffScreen)(void *context, GLsizei width, GLsizei height,
                             GLint rowbytes, void *baseaddr);
    CGLError (*setPBuffer)(void *context, CGLPBufferObj pbuffer,
                           GLenum face, GLint level);
    CGLError (*texImagePBuffer)(void *context, CGLPBufferObj pbuffer,
                                GLenum source);

    /* State (optional). Called after CGL has validated and stored values. */
    CGLError (*setParameter)(void *context, GLint pname,
                             const GLint *values, GLint count);
    CGLError (*setEnable)(void *context, CGLContextEnable pname,
                          GLboolean enabled);
    CGLError (*copyContext)(void *source, void *destination, GLbitfield mask);

    /* Minkar: default compositing shaders (optional). */
    CGLError (*useShaders)(void *context);

    /* GL entry point lookup for API dispatch (optional, used later). */
    void    *(*getProcAddress)(void *context, const char *name);
} CGLDriver;

/*
 * Register a renderer driver. Drivers are tried in registration order,
 * so register accelerated renderers first. The driver structure must
 * stay valid for the life of the process.
 */
CGL_EXPORT CGLError CGLRegisterDriver(const CGLDriver *driver);

/* Return the driver context behind a CGL context (for driver code). */
CGL_EXPORT void *CGLGetDriverContext(CGLContextObj ctx);

/* Pixel buffer accessors for drivers. */
CGL_EXPORT CGLError CGLDescribePBuffer(CGLPBufferObj obj, GLsizei *width,
    GLsizei *height, GLenum *target, GLenum *internalFormat, GLint *mipmap);

#ifdef __cplusplus
}
#endif

#endif /* _OPENGL_CGLDRIVER_H_ */
