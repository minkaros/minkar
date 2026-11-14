/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An IOSurface as a texture's image.
 */

#ifndef _OPENGL_CGLIOSURFACE_H_
#define _OPENGL_CGLIOSURFACE_H_

#include <OpenGL/CGLTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __IOSurface *IOSurfaceRef;

/*
 * The texture bound to 'target' (GL_TEXTURE_RECTANGLE or GL_TEXTURE_2D) of
 * the context takes 'ioSurface' as its image: 'width' by 'height', its
 * pixels in 'format' and 'type' (GL_BGRA and GL_UNSIGNED_INT_8_8_8_8_REV
 * for a 'BGRA' surface). What the context draws into the texture is in the
 * surface when the context is flushed (glFlush, glFinish,
 * CGLFlushDrawable).
 */
CGL_EXPORT CGLError CGLTexImageIOSurface2D(CGLContextObj ctx, GLenum target,
    GLenum internal_format, GLsizei width, GLsizei height, GLenum format,
    GLenum type, IOSurfaceRef ioSurface, GLuint plane);

#ifdef __cplusplus
}
#endif

#endif /* _OPENGL_CGLIOSURFACE_H_ */
