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

/* Pixel buffers (deprecated on macOS in favor of FBOs, still supported). */

#include "CGLInternal.h"

#include <stdlib.h>

static GLint max_mip_level(GLsizei width, GLsizei height)
{
    GLsizei size = width > height ? width : height;
    GLint level = 0;

    while (size > 1) {
        size >>= 1;
        level++;
    }
    return level;
}

CGLError CGLCreatePBuffer(GLsizei width, GLsizei height, GLenum target,
                          GLenum internalFormat, GLint max_level,
                          CGLPBufferObj *pbuffer)
{
    CGLPBufferObj pb;

    if (pbuffer == NULL)
        return kCGLBadAddress;
    *pbuffer = NULL;
    if (width <= 0 || height <= 0 || max_level < 0)
        return kCGLBadValue;

    switch (target) {
    case CGL_GL_TEXTURE_2D:
        break;
    case CGL_GL_TEXTURE_RECTANGLE:
        if (max_level != 0)
            return kCGLBadValue;
        break;
    case CGL_GL_TEXTURE_CUBE_MAP:
        if (width != height)
            return kCGLBadValue;
        break;
    default:
        return kCGLBadValue;
    }
    switch (internalFormat) {
    case CGL_GL_RGB:
    case CGL_GL_RGBA:
    case CGL_GL_DEPTH_COMPONENT:
        break;
    default:
        return kCGLBadValue;
    }
    if (max_level > max_mip_level(width, height))
        return kCGLBadValue;

    pb = calloc(1, sizeof(*pb));
    if (pb == NULL)
        return kCGLBadAlloc;
    atomic_init(&pb->refcount, 1);
    pb->width = width;
    pb->height = height;
    pb->target = target;
    pb->internalFormat = internalFormat;
    pb->maxLevel = max_level;
    *pbuffer = pb;
    return kCGLNoError;
}

CGLError CGLDescribePBuffer(CGLPBufferObj obj, GLsizei *width, GLsizei *height,
                            GLenum *target, GLenum *internalFormat,
                            GLint *mipmap)
{
    if (obj == NULL)
        return kCGLBadAddress;
    if (width != NULL)
        *width = obj->width;
    if (height != NULL)
        *height = obj->height;
    if (target != NULL)
        *target = obj->target;
    if (internalFormat != NULL)
        *internalFormat = obj->internalFormat;
    if (mipmap != NULL)
        *mipmap = obj->maxLevel;
    return kCGLNoError;
}

CGLPBufferObj CGLRetainPBuffer(CGLPBufferObj pbuffer)
{
    if (pbuffer != NULL)
        atomic_fetch_add(&pbuffer->refcount, 1);
    return pbuffer;
}

void CGLReleasePBuffer(CGLPBufferObj pbuffer)
{
    if (pbuffer != NULL && atomic_fetch_sub(&pbuffer->refcount, 1) == 1)
        free(pbuffer);
}

CGLError CGLDestroyPBuffer(CGLPBufferObj pbuffer)
{
    if (pbuffer == NULL)
        return kCGLBadAddress;
    CGLReleasePBuffer(pbuffer);
    return kCGLNoError;
}

GLuint CGLGetPBufferRetainCount(CGLPBufferObj pbuffer)
{
    if (pbuffer == NULL)
        return 0;
    return atomic_load(&pbuffer->refcount);
}
