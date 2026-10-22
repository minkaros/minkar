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
 * Legacy (fixed-function) OpenGL 1.x declarations used by QuartzCore.
 * Written from the public Khronos OpenGL 1.x/3.0 specifications.
 *
 * OpenGL.framework currently ships only GLES2 headers, which lack the
 * fixed-function API. Declare what we use here so calls get correct
 * prototypes; symbols are resolved at runtime (-undefined dynamic_lookup).
 */
#import <OpenGL/OpenGL.h>

#ifndef GL_MODELVIEW
#define GL_ALPHA_TEST           0x0BC0
#define GL_MODELVIEW            0x1700
#define GL_PROJECTION           0x1701
#define GL_SMOOTH               0x1D01
#define GL_TEXTURE_ENV_MODE     0x2200
#define GL_TEXTURE_ENV          0x2300
#define GL_VERTEX_ARRAY         0x8074
#define GL_TEXTURE_COORD_ARRAY  0x8078
#endif

#ifdef __cplusplus
extern "C" {
#endif

void glAlphaFunc(GLenum func, GLfloat ref);
void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glEnableClientState(GLenum array);
void glLoadIdentity(void);
void glMatrixMode(GLenum mode);
void glOrtho(double left, double right, double bottom, double top, double zNear, double zFar);
void glPopMatrix(void);
void glPushMatrix(void);
void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glScalef(GLfloat x, GLfloat y, GLfloat z);
void glShadeModel(GLenum mode);
void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void *pointer);
void glTexEnvf(GLenum target, GLenum pname, GLfloat param);
void glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void *pointer);
void glGenVertexArrays(GLsizei n, GLuint *arrays);
void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);

void CGLUseShaders(CGLContextObj context);

#ifdef __cplusplus
}
#endif
