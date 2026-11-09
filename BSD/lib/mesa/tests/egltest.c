/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * minkar-egltest: OpenGL ES through EGL's surfaceless platform, as WebKit
 * uses it: a display, a context without a surface, a triangle drawn with
 * shaders into a texture, and its pixels read back. With MESA_LOADER_
 * DRIVER_OVERRIDE=zink it draws on the Vulkan driver instead.
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>

static GLuint shader(GLenum type, const char *source) {
    GLuint s = glCreateShader(type);
    GLint ok = 0;
    glShaderSource(s, 1, &source, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    return ok ? s : 0;
}

int main(void) {
    PFNEGLGETPLATFORMDISPLAYEXTPROC getDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    EGLDisplay display = getDisplay ? getDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL) : EGL_NO_DISPLAY;
    EGLint major = 0, minor = 0;
    if (!eglInitialize(display, &major, &minor)) {
        printf("egl init failed 0x%x\n", eglGetError());
        return 1;
    }
    printf("egl %d.%d vendor=%s\n", major, minor, eglQueryString(display, EGL_VENDOR));
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLint attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    if (context == EGL_NO_CONTEXT || !eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context)) {
        printf("context failed 0x%x\n", eglGetError());
        return 1;
    }
    printf("renderer=%s version=%s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

    GLuint texture, framebuffer;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    GLenum complete = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    GLuint program = glCreateProgram();
    GLuint vs = shader(GL_VERTEX_SHADER, "attribute vec2 p; void main() { gl_Position = vec4(p, 0.0, 1.0); }");
    GLuint fs = shader(GL_FRAGMENT_SHADER, "precision mediump float; void main() { gl_FragColor = vec4(0.0, 0.5, 1.0, 1.0); }");
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "p");
    glLinkProgram(program);
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    static const GLfloat triangle[] = { -1, -1, 1, -1, -1, 1 };   /* the lower left half */
    glViewport(0, 0, 64, 64);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, triangle);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    unsigned char inside[4] = { 0 }, outside[4] = { 0 };
    glReadPixels(8, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, inside);
    glReadPixels(56, 56, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, outside);
    printf("framebuffer=%s shaders=%d,%d linked=%d\n", complete == GL_FRAMEBUFFER_COMPLETE ? "complete" : "incomplete",
           vs != 0, fs != 0, linked);
    printf("inside=%d,%d,%d,%d outside=%d,%d,%d,%d error=0x%x\n", inside[0], inside[1], inside[2], inside[3],
           outside[0], outside[1], outside[2], outside[3], glGetError());
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context);
    eglTerminate(display);
    return 0;
}
