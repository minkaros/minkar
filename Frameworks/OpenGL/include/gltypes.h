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
 * Basic OpenGL scalar types, sized as the Khronos OpenGL specification
 * requires. Shared by the CGL headers and the GL API headers.
 */

#ifndef _OPENGL_GLTYPES_H_
#define _OPENGL_GLTYPES_H_

#include <stdint.h>
#include <stddef.h>

/*
 * The GLES2 gl.h that OpenGL.framework still ships (until the desktop GL
 * headers land) defines most of these itself; identical redefinitions are
 * valid C11 but we avoid them for older language modes.
 */
#ifndef __gles2_gl2_h_
typedef uint32_t        GLbitfield;
typedef uint8_t         GLboolean;
typedef int8_t          GLbyte;
typedef float           GLclampf;
typedef uint32_t        GLenum;
typedef float           GLfloat;
typedef int32_t         GLint;
typedef int16_t         GLshort;
typedef int32_t         GLsizei;
typedef uint8_t         GLubyte;
typedef uint32_t        GLuint;
typedef uint16_t        GLushort;
typedef void            GLvoid;
typedef char            GLchar;
typedef int32_t         GLfixed;
typedef intptr_t        GLintptr;
typedef intptr_t        GLsizeiptr;
typedef int64_t         GLint64;
typedef uint64_t        GLuint64;
typedef struct __GLsync *GLsync;
#endif

typedef double          GLdouble;
typedef double          GLclampd;
typedef uint16_t        GLhalf;
typedef char            GLcharARB;
typedef intptr_t        GLintptrARB;
typedef intptr_t        GLsizeiptrARB;
typedef int64_t         GLint64EXT;
typedef uint64_t        GLuint64EXT;

#endif /* _OPENGL_GLTYPES_H_ */
