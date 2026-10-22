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
 * GL dispatch tests: the exported gl* entry points must forward to the
 * driver of the calling thread's current context, pass arguments through
 * unchanged (including floats and doubles), return the driver's results,
 * and do nothing (returning 0/NULL) without a context or a driver
 * function.
 */

#include <OpenGL/gl.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLDriver.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* Driver-side GL implementations, tagged with the driver context. */
static GLfloat cc[4];
static GLdouble ortho[6];
static int clears, which;

static void d_clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{ cc[0] = r; cc[1] = g; cc[2] = b; cc[3] = a; }
static void d_ortho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{ ortho[0] = l; ortho[1] = r; ortho[2] = b; ortho[3] = t; ortho[4] = n; ortho[5] = f; }
static void d_clear(GLbitfield m) { (void)m; clears++; }
static const GLubyte *d_get_string(GLenum name)
{ return name == GL_VENDOR ? (const GLubyte *)"ravynOS test" : NULL; }
static GLboolean d_is_enabled(GLenum cap) { return cap == GL_BLEND; }

static void *t_proc(void *ctx, const char *name)
{
    which = *(int *)ctx;
    if (strcmp(name, "glClearColor") == 0) return (void *)d_clear_color;
    if (strcmp(name, "glOrtho") == 0) return (void *)d_ortho;
    if (strcmp(name, "glClear") == 0) return (void *)d_clear;
    if (strcmp(name, "glGetString") == 0) return (void *)d_get_string;
    if (strcmp(name, "glIsEnabled") == 0) return (void *)d_is_enabled;
    return NULL;
}
static CGLError t_desc(const CGLDriver *d, CGLRendererProperty p, GLint *v)
{ (void)d; *v = (p == kCGLRPOnline); return kCGLNoError; }
static CGLError t_choose(const CGLDriver *d, const CGLDriverPixelFormatRequest *r,
                         CGLDriverPixelFormat *f) { (void)d; (void)r; (void)f; return kCGLNoError; }
static int ids;
static CGLError t_create(const CGLDriver *d, const CGLDriverPixelFormat *f, void *s, void **c)
{ static int tags[8]; (void)d; (void)f; (void)s; tags[ids] = ids + 1; *c = &tags[ids++]; return kCGLNoError; }
static void t_destroy(void *c) { (void)c; }
static CGLError t_current(void *c) { (void)c; return kCGLNoError; }

static const CGLDriver driver = {
    .abiVersion = CGL_DRIVER_ABI_VERSION, .name = "gl-test", .rendererID = 0x00ABCE00,
    .displayMask = 1, .describeRenderer = t_desc, .choosePixelFormat = t_choose,
    .createContext = t_create, .destroyContext = t_destroy, .makeCurrent = t_current,
    .getProcAddress = t_proc,
};

static void *other_thread(void *arg)
{
    (void)arg;
    /* No context on this thread: nothing is forwarded. */
    CHECK(glGetString(GL_VENDOR) == NULL);
    glClear(GL_COLOR_BUFFER_BIT);
    return NULL;
}

int main(void)
{
    CGLPixelFormatAttribute attrs[] = { kCGLPFADoubleBuffer, 0 };
    CGLPixelFormatObj pf;
    CGLContextObj ctx;
    pthread_t th;
    GLint n, v;

    /* No context at all: calls are harmless no-ops returning 0/NULL. */
    glClearColor(1, 1, 1, 1);
    glFlush();
    CHECK(glGetString(GL_VENDOR) == NULL);
    CHECK(glIsEnabled(GL_BLEND) == GL_FALSE);
    CHECK(glGetError() == GL_NO_ERROR);

    CHECK(CGLRegisterDriver(&driver) == kCGLNoError);
    CHECK(CGLChoosePixelFormat(attrs, &pf, &n) == kCGLNoError && pf != NULL);
    CHECK(CGLCreateContext(pf, NULL, &ctx) == kCGLNoError);
    CHECK(which == 1);                          /* resolved with this driver context */
    CHECK(CGLGetParameter(ctx, kCGLCPDispatchTableSize, &v) == kCGLNoError && v > 800);

    /* Created but not current: still nothing forwarded. */
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK(clears == 0);

    CHECK(CGLSetCurrentContext(ctx) == kCGLNoError);
    glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
    CHECK(cc[0] == 0.25f && cc[1] == 0.5f && cc[2] == 0.75f && cc[3] == 1.0f);
    glOrtho(-1.5, 2.5, -3.5, 4.5, 0.125, 1000.0);
    CHECK(ortho[0] == -1.5 && ortho[1] == 2.5 && ortho[2] == -3.5 &&
          ortho[3] == 4.5 && ortho[4] == 0.125 && ortho[5] == 1000.0);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK(clears == 1);
    CHECK(glGetString(GL_VENDOR) != NULL &&
          strcmp((const char *)glGetString(GL_VENDOR), "ravynOS test") == 0);
    CHECK(glIsEnabled(GL_BLEND) == GL_TRUE && glIsEnabled(GL_DEPTH_TEST) == GL_FALSE);
    glFlush();                                  /* driver lacks it: no-op */
    CHECK(glGetError() == GL_NO_ERROR);

    /* Core-profile era entry points exist too (up to OpenGL 4.1). */
    {
        GLuint vao = 7;
        glGenVertexArrays(1, &vao);             /* not provided: untouched */
        CHECK(vao == 7);
        glProgramUniform1d(1, 2, 3.0);           /* 4.1 */
    }

    /* The current context is per thread. */
    pthread_create(&th, NULL, other_thread, NULL);
    pthread_join(th, NULL);
    CHECK(clears == 1);

    CHECK(CGLSetCurrentContext(NULL) == kCGLNoError);
    glClear(GL_COLOR_BUFFER_BIT);
    CHECK(clears == 1);

    CGLReleaseContext(ctx);
    CGLReleasePixelFormat(pf);
    printf("%d checks, %d failures\n", checks, fails);
    return fails != 0;
}
