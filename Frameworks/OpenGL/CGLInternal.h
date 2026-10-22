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

/* Internal declarations shared by the CGL implementation files. */

#ifndef _CGL_INTERNAL_H_
#define _CGL_INTERNAL_H_

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLDriver.h>
#include <OpenGL/CGLPrivate.h>

#include <pthread.h>
#include <stdatomic.h>

#define CGL_HIDDEN __attribute__((visibility("hidden")))

/* GL enums CGL validates; spelled out so CGL does not depend on gl.h. */
#define CGL_GL_TEXTURE_2D                   0x0DE1
#define CGL_GL_TEXTURE_RECTANGLE            0x84F5
#define CGL_GL_TEXTURE_CUBE_MAP             0x8513
#define CGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X  0x8515
#define CGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z  0x851A
#define CGL_GL_DEPTH_COMPONENT              0x1902
#define CGL_GL_RGB                          0x1907
#define CGL_GL_RGBA                         0x1908
#define CGL_GL_FRONT_LEFT                   0x0400
#define CGL_GL_AUX3                         0x040C

/* An OpenGL entry point in a driver, as stored in a context's dispatch table. */
typedef void (*CGLGLProc)(void);

/* Renderer ID of the built-in null renderer (ravynOS private range). */
#define kCGLRendererRavynNullID             0x00FF0100

struct _CGLPixelFormatObject {
    atomic_uint             refcount;
    GLint                   nscreens;   /* virtual screens */
    const CGLDriver       **drivers;    /* one per virtual screen */
    CGLDriverPixelFormat   *formats;    /* one per virtual screen */
};

struct _CGLShareGroupObject {
    atomic_uint             refcount;
};

struct _CGLPBufferObject {
    atomic_uint             refcount;
    GLsizei                 width;
    GLsizei                 height;
    GLenum                  target;
    GLenum                  internalFormat;
    GLint                   maxLevel;
};

/* Number of entries in the context parameter table (CGLContext.c). */
#define CGL_PARAM_SLOTS         32
#define CGL_PARAM_MAX_VALUES    4

struct _CGLContextObject {
    atomic_uint             refcount;
    pthread_mutex_t         lock;       /* recursive */

    CGLPixelFormatObj       pixelFormat;
    struct _CGLShareGroupObject *shareGroup;
    GLint                   virtualScreen;
    const CGLDriver        *driver;
    void                   *driverContext;

    CGLGLProc              *glProcs;    /* indexed by GLDispatch.gen.h */

    GLint                   params[CGL_PARAM_SLOTS][CGL_PARAM_MAX_VALUES];
    uint32_t                enables;    /* bit per CGLContextEnable slot */

    /* Off-screen drawable */
    GLboolean               hasOffScreen;
    GLsizei                 offWidth;
    GLsizei                 offHeight;
    GLint                   offRowBytes;
    void                   *offBase;

    /* Pixel buffer drawable */
    CGLPBufferObj           pbuffer;
    GLenum                  pbufferFace;
    GLint                   pbufferLevel;
};

/* Driver registry (CGLDriverRegistry.c). */
CGL_HIDDEN GLint cgl_driver_count(void);
CGL_HIDDEN const CGLDriver *cgl_driver_at(GLint index);
CGL_HIDDEN GLint cgl_driver_property(const CGLDriver *driver,
                                     CGLRendererProperty prop, GLint fallback);

/* Driver function for GL entry point 'index' in the current context, or NULL. */
CGL_HIDDEN CGLGLProc cgl_gl_proc(int index);

/* Built-in drivers. */
CGL_HIDDEN extern const CGLDriver cgl_null_driver;

#endif /* _CGL_INTERNAL_H_ */
