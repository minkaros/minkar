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
 * Core OpenGL (CGL): pixel formats, renderers, contexts, drawables and
 * pixel buffers. Source compatible with the macOS CGL API.
 */

#ifndef _OPENGL_H_
#define _OPENGL_H_

/* Like macOS, the GL API itself comes from <OpenGL/gl.h> or <OpenGL/gl3.h>. */
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLDevice.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/CGLTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CGL_VERSION_1_0  1
#define CGL_VERSION_1_1  1
#define CGL_VERSION_1_2  1
#define CGL_VERSION_1_3  1

/* Pixel formats */
CGL_EXPORT CGLError CGLChoosePixelFormat(const CGLPixelFormatAttribute *attribs,
    CGLPixelFormatObj *pix, GLint *npix);
CGL_EXPORT CGLError CGLDestroyPixelFormat(CGLPixelFormatObj pix);
CGL_EXPORT CGLError CGLDescribePixelFormat(CGLPixelFormatObj pix, GLint pix_num,
    CGLPixelFormatAttribute attrib, GLint *value);
CGL_EXPORT void CGLReleasePixelFormat(CGLPixelFormatObj pix);
CGL_EXPORT CGLPixelFormatObj CGLRetainPixelFormat(CGLPixelFormatObj pix);
CGL_EXPORT GLuint CGLGetPixelFormatRetainCount(CGLPixelFormatObj pix);

/* Renderer information */
CGL_EXPORT CGLError CGLQueryRendererInfo(GLuint display_mask,
    CGLRendererInfoObj *rend, GLint *nrend);
CGL_EXPORT CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend);
CGL_EXPORT CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, GLint rend_num,
    CGLRendererProperty prop, GLint *value);

/* Contexts */
CGL_EXPORT CGLError CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share,
    CGLContextObj *ctx);
CGL_EXPORT CGLError CGLDestroyContext(CGLContextObj ctx);
CGL_EXPORT CGLError CGLCopyContext(CGLContextObj src, CGLContextObj dst,
    GLbitfield mask);
CGL_EXPORT CGLContextObj CGLRetainContext(CGLContextObj ctx);
CGL_EXPORT void CGLReleaseContext(CGLContextObj ctx);
CGL_EXPORT GLuint CGLGetContextRetainCount(CGLContextObj ctx);
CGL_EXPORT CGLPixelFormatObj CGLGetPixelFormat(CGLContextObj ctx);

/* Pixel buffers */
CGL_EXPORT CGLError CGLCreatePBuffer(GLsizei width, GLsizei height,
    GLenum target, GLenum internalFormat, GLint max_level, CGLPBufferObj *pbuffer);
CGL_EXPORT CGLError CGLDestroyPBuffer(CGLPBufferObj pbuffer);
CGL_EXPORT CGLError CGLDescribePBuffer(CGLPBufferObj obj, GLsizei *width,
    GLsizei *height, GLenum *target, GLenum *internalFormat, GLint *mipmap);
CGL_EXPORT CGLError CGLTexImagePBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer,
    GLenum source);
CGL_EXPORT CGLPBufferObj CGLRetainPBuffer(CGLPBufferObj pbuffer);
CGL_EXPORT void CGLReleasePBuffer(CGLPBufferObj pbuffer);
CGL_EXPORT GLuint CGLGetPBufferRetainCount(CGLPBufferObj pbuffer);

/* Drawables */
CGL_EXPORT CGLError CGLSetOffScreen(CGLContextObj ctx, GLsizei width,
    GLsizei height, GLint rowbytes, void *baseaddr);
CGL_EXPORT CGLError CGLGetOffScreen(CGLContextObj ctx, GLsizei *width,
    GLsizei *height, GLint *rowbytes, void **baseaddr);
CGL_EXPORT CGLError CGLSetFullScreen(CGLContextObj ctx) CGL_DEPRECATED_ATTRIBUTE;
CGL_EXPORT CGLError CGLSetFullScreenOnDisplay(CGLContextObj ctx,
    GLuint display_mask) CGL_DEPRECATED_ATTRIBUTE;
CGL_EXPORT CGLError CGLSetPBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer,
    GLenum face, GLint level, GLint screen);
CGL_EXPORT CGLError CGLGetPBuffer(CGLContextObj ctx, CGLPBufferObj *pbuffer,
    GLenum *face, GLint *level, GLint *screen);
CGL_EXPORT CGLError CGLClearDrawable(CGLContextObj ctx);
CGL_EXPORT CGLError CGLFlushDrawable(CGLContextObj ctx);

/* Context options and parameters */
CGL_EXPORT CGLError CGLEnable(CGLContextObj ctx, CGLContextEnable pname);
CGL_EXPORT CGLError CGLDisable(CGLContextObj ctx, CGLContextEnable pname);
CGL_EXPORT CGLError CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname,
    GLint *enable);
CGL_EXPORT CGLError CGLSetParameter(CGLContextObj ctx, CGLContextParameter pname,
    const GLint *params);
CGL_EXPORT CGLError CGLGetParameter(CGLContextObj ctx, CGLContextParameter pname,
    GLint *params);

/* Virtual screens */
CGL_EXPORT CGLError CGLSetVirtualScreen(CGLContextObj ctx, GLint screen);
CGL_EXPORT CGLError CGLGetVirtualScreen(CGLContextObj ctx, GLint *screen);
CGL_EXPORT CGLError CGLUpdateContext(CGLContextObj ctx);

/* Global library options */
CGL_EXPORT CGLError CGLSetGlobalOption(CGLGlobalOption pname, const GLint *params);
CGL_EXPORT CGLError CGLGetGlobalOption(CGLGlobalOption pname, GLint *params);
CGL_EXPORT CGLError CGLSetOption(CGLGlobalOption pname, GLint param);
CGL_EXPORT CGLError CGLGetOption(CGLGlobalOption pname, GLint *param);

/* Locking */
CGL_EXPORT CGLError CGLLockContext(CGLContextObj ctx);
CGL_EXPORT CGLError CGLUnlockContext(CGLContextObj ctx);

/* Version and errors */
CGL_EXPORT void CGLGetVersion(GLint *majorvers, GLint *minorvers);
CGL_EXPORT const char *CGLErrorString(CGLError error);

#ifdef __cplusplus
}
#endif

#endif /* _OPENGL_H_ */
