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
 * Basic OpenGL scalar types, sized as the Khronos OpenGL specification
 * requires. Shared by the CGL headers and the GL API headers.
 */

#ifndef _OPENGL_GLTYPES_H_
#define _OPENGL_GLTYPES_H_

#include <stdint.h>
#include <stddef.h>

/*
 * <OpenGL/gl.h> (Mesa) and <OpenGL/gl3.h> (Khronos glcorearb.h) define
 * these types themselves. Skip ours when one of them came first; if this
 * header comes first, their identical redefinitions are valid C11.
 */
#if !defined(__gl_h_) && !defined(__gl_glcorearb_h_)
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
typedef double          GLdouble;
typedef double          GLclampd;
typedef uint16_t        GLhalf;
#endif

#endif /* _OPENGL_GLTYPES_H_ */
