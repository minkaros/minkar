/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
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
 * The Mesa renderer: OpenGL drawn by Mesa (/usr/lib/libEGL.1.dylib), on
 * EGL's surfaceless platform. A context is an EGL context of the profile
 * the pixel format asks for (legacy 2.1, or a 3.2 or 4.1 core profile);
 * with no drawable it draws into the framebuffer objects the app makes,
 * and an offscreen drawable (CGLSetOffScreen) is a pbuffer read back into
 * the app's memory when it is flushed. Mesa is loaded when CGL first
 * looks for renderers: a system without it keeps the other renderers.
 * Mesa draws in software here (softpipe), so the renderer says it is not
 * accelerated, with the generic software renderer's ID.
 */

#include "CGLInternal.h"

#include <dlfcn.h>
#include <pthread.h>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOSurface/IOSurfaceRef.h>
#define CGL_MESA_IOSURFACE 1
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MESA_EGL_LIBRARY "/usr/lib/libEGL.1.dylib"

/* EGL 1.5's types and values (Khronos's registry): Mesa is built after
   the frameworks, so its headers are not there yet. */
typedef void *EGLDisplay, *EGLConfig, *EGLContext, *EGLSurface;
typedef unsigned int EGLBoolean, EGLenum;
typedef int32_t EGLint;
typedef void (*EGLProc)(void);

#define EGL_TRUE                                    1
#define EGL_NONE                                    0x3038
#define EGL_DEFAULT_DISPLAY                         ((void *)0)
#define EGL_NO_DISPLAY                              ((EGLDisplay)0)
#define EGL_NO_CONTEXT                              ((EGLContext)0)
#define EGL_NO_SURFACE                              ((EGLSurface)0)
#define EGL_ALPHA_SIZE                              0x3021
#define EGL_BLUE_SIZE                               0x3022
#define EGL_GREEN_SIZE                              0x3023
#define EGL_RED_SIZE                                0x3024
#define EGL_DEPTH_SIZE                              0x3025
#define EGL_STENCIL_SIZE                            0x3026
#define EGL_SURFACE_TYPE                            0x3033
#define EGL_RENDERABLE_TYPE                         0x3040
#define EGL_HEIGHT                                  0x3056
#define EGL_WIDTH                                   0x3057
#define EGL_PBUFFER_BIT                             0x0001
#define EGL_OPENGL_BIT                              0x0008
#define EGL_OPENGL_API                              0x30A2
#define EGL_CONTEXT_MAJOR_VERSION                   0x3098
#define EGL_CONTEXT_MINOR_VERSION                   0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK             0x30FD
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE       0x31B1
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT         0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT 0x00000002
#define EGL_PLATFORM_SURFACELESS_MESA               0x31DD

typedef EGLProc    (*PFNEGLGETPROCADDRESSPROC)(const char *);
typedef EGLDisplay (*PFNEGLGETPLATFORMDISPLAYEXTPROC)(EGLenum, void *, const EGLint *);
typedef EGLBoolean (*PFNEGLINITIALIZEPROC)(EGLDisplay, EGLint *, EGLint *);
typedef EGLBoolean (*PFNEGLBINDAPIPROC)(EGLenum);
typedef EGLBoolean (*PFNEGLCHOOSECONFIGPROC)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
typedef EGLContext (*PFNEGLCREATECONTEXTPROC)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
typedef EGLBoolean (*PFNEGLDESTROYCONTEXTPROC)(EGLDisplay, EGLContext);
typedef EGLSurface (*PFNEGLCREATEPBUFFERSURFACEPROC)(EGLDisplay, EGLConfig, const EGLint *);
typedef EGLBoolean (*PFNEGLDESTROYSURFACEPROC)(EGLDisplay, EGLSurface);
typedef EGLBoolean (*PFNEGLMAKECURRENTPROC)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
typedef EGLContext (*PFNEGLGETCURRENTCONTEXTPROC)(void);
typedef EGLint     (*PFNEGLGETERRORPROC)(void);

/* GL's own values, so this file needs no GL header of its own. */
#define MESA_GL_BGRA                        0x80E1
#define MESA_GL_UNSIGNED_INT_8_8_8_8_REV    0x8367
#define MESA_GL_PACK_ROW_LENGTH             0x0D02
#define MESA_GL_PACK_ALIGNMENT              0x0D05
#define MESA_GL_READ_FRAMEBUFFER_BINDING    0x8CAA
#define MESA_GL_READ_FRAMEBUFFER            0x8CA8
#define MESA_GL_PIXEL_PACK_BUFFER_BINDING   0x88ED
#define MESA_GL_PIXEL_PACK_BUFFER           0x88EB

static struct {
    void *library;
    EGLDisplay display;
    PFNEGLGETPROCADDRESSPROC getProcAddress;
    PFNEGLINITIALIZEPROC initialize;
    PFNEGLBINDAPIPROC bindAPI;
    PFNEGLCHOOSECONFIGPROC chooseConfig;
    PFNEGLCREATECONTEXTPROC createContext;
    PFNEGLDESTROYCONTEXTPROC destroyContext;
    PFNEGLCREATEPBUFFERSURFACEPROC createPbufferSurface;
    PFNEGLDESTROYSURFACEPROC destroySurface;
    PFNEGLMAKECURRENTPROC makeCurrent;
    PFNEGLGETCURRENTCONTEXTPROC getCurrentContext;
    PFNEGLGETERRORPROC getError;
    /* GL calls the driver itself makes. */
    void (*glFinish)(void);
    void (*glReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
    void (*glGetIntegerv)(GLenum, GLint *);
    void (*glPixelStorei)(GLenum, GLint);
    void (*glBindFramebuffer)(GLenum, GLuint);
    void (*glBindBuffer)(GLenum, GLuint);
    void (*glFlush)(void);
    void (*glBindTexture)(GLenum, GLuint);
    void (*glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
    void (*glGetTexImage)(GLenum, GLint, GLenum, GLenum, void *);
    void (*glGetTexLevelParameteriv)(GLenum, GLint, GLenum, GLint *);
    GLboolean (*glIsTexture)(GLuint);
} mesa;

/* A texture whose image is an IOSurface (CGLTexImageIOSurface2D). */
struct mesa_iosurface_binding {
    GLuint  texture;
    GLenum  target, format, type;
    GLsizei width, height;
    void   *surface;            /* IOSurfaceRef, retained */
};

struct mesa_context {
    EGLContext context;
    EGLConfig  config;          /* for the pbuffer of an offscreen drawable */
    EGLSurface surface;         /* that pbuffer, or EGL_NO_SURFACE */
    GLsizei    width, height;
    GLint      rowBytes;
    void      *pixels;          /* where the drawable is read back to */
    GLint      profile;         /* kCGLOGLPVersion_* */
    struct mesa_iosurface_binding *bindings;
    int        bindingCount, bindingCapacity;
};

/* The context current on this thread (its flushes write IOSurfaces back). */
static __thread struct mesa_context *mesa_current;

static pthread_once_t mesa_once = PTHREAD_ONCE_INIT;
static int mesa_ready;

static void *egl_symbol(const char *name)
{
    return dlsym(mesa.library, name);
}

static void mesa_load(void)
{
    PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay;
    EGLint major, minor;

    mesa.library = dlopen(MESA_EGL_LIBRARY, RTLD_NOW | RTLD_LOCAL);
    if (mesa.library == NULL)
        return;
    mesa.getProcAddress = (PFNEGLGETPROCADDRESSPROC)egl_symbol("eglGetProcAddress");
    mesa.initialize = (PFNEGLINITIALIZEPROC)egl_symbol("eglInitialize");
    mesa.bindAPI = (PFNEGLBINDAPIPROC)egl_symbol("eglBindAPI");
    mesa.chooseConfig = (PFNEGLCHOOSECONFIGPROC)egl_symbol("eglChooseConfig");
    mesa.createContext = (PFNEGLCREATECONTEXTPROC)egl_symbol("eglCreateContext");
    mesa.destroyContext = (PFNEGLDESTROYCONTEXTPROC)egl_symbol("eglDestroyContext");
    mesa.createPbufferSurface = (PFNEGLCREATEPBUFFERSURFACEPROC)egl_symbol("eglCreatePbufferSurface");
    mesa.destroySurface = (PFNEGLDESTROYSURFACEPROC)egl_symbol("eglDestroySurface");
    mesa.makeCurrent = (PFNEGLMAKECURRENTPROC)egl_symbol("eglMakeCurrent");
    mesa.getCurrentContext = (PFNEGLGETCURRENTCONTEXTPROC)egl_symbol("eglGetCurrentContext");
    mesa.getError = (PFNEGLGETERRORPROC)egl_symbol("eglGetError");
    if (mesa.getProcAddress == NULL || mesa.initialize == NULL || mesa.bindAPI == NULL ||
        mesa.chooseConfig == NULL || mesa.createContext == NULL || mesa.destroyContext == NULL ||
        mesa.createPbufferSurface == NULL || mesa.destroySurface == NULL ||
        mesa.makeCurrent == NULL || mesa.getCurrentContext == NULL || mesa.getError == NULL)
        return;

    getPlatformDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
        mesa.getProcAddress("eglGetPlatformDisplayEXT");
    if (getPlatformDisplay == NULL)
        return;
    mesa.display = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    if (mesa.display == EGL_NO_DISPLAY || !mesa.initialize(mesa.display, &major, &minor))
        return;
    if (!mesa.bindAPI(EGL_OPENGL_API))
        return;

    mesa.glFinish = (void (*)(void))mesa.getProcAddress("glFinish");
    mesa.glReadPixels = (void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *))
        mesa.getProcAddress("glReadPixels");
    mesa.glGetIntegerv = (void (*)(GLenum, GLint *))mesa.getProcAddress("glGetIntegerv");
    mesa.glPixelStorei = (void (*)(GLenum, GLint))mesa.getProcAddress("glPixelStorei");
    mesa.glBindFramebuffer = (void (*)(GLenum, GLuint))mesa.getProcAddress("glBindFramebuffer");
    mesa.glBindBuffer = (void (*)(GLenum, GLuint))mesa.getProcAddress("glBindBuffer");
    mesa.glFlush = (void (*)(void))mesa.getProcAddress("glFlush");
    mesa.glBindTexture = (void (*)(GLenum, GLuint))mesa.getProcAddress("glBindTexture");
    mesa.glTexImage2D = (void (*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *))
        mesa.getProcAddress("glTexImage2D");
    mesa.glGetTexImage = (void (*)(GLenum, GLint, GLenum, GLenum, void *))mesa.getProcAddress("glGetTexImage");
    mesa.glGetTexLevelParameteriv = (void (*)(GLenum, GLint, GLenum, GLint *))
        mesa.getProcAddress("glGetTexLevelParameteriv");
    mesa.glIsTexture = (GLboolean (*)(GLuint))mesa.getProcAddress("glIsTexture");
    if (mesa.glFinish == NULL || mesa.glReadPixels == NULL || mesa.glGetIntegerv == NULL ||
        mesa.glPixelStorei == NULL || mesa.glFlush == NULL || mesa.glBindTexture == NULL ||
        mesa.glTexImage2D == NULL || mesa.glGetTexImage == NULL ||
        mesa.glGetTexLevelParameteriv == NULL || mesa.glIsTexture == NULL)
        return;
    mesa_ready = 1;
}

/* Whether Mesa is there to draw with; loads it the first time. */
CGL_HIDDEN int cgl_mesa_available(void)
{
    pthread_once(&mesa_once, mesa_load);
    return mesa_ready;
}

/* ---- IOSurfaces as textures ------------------------------------------- */

#define MESA_GL_TEXTURE_2D                  0x0DE1
#define MESA_GL_TEXTURE_RECTANGLE           0x84F5
#define MESA_GL_TEXTURE_BINDING_2D          0x8069
#define MESA_GL_TEXTURE_BINDING_RECTANGLE   0x84F6
#define MESA_GL_TEXTURE_WIDTH               0x1000
#define MESA_GL_TEXTURE_HEIGHT              0x1001
#define MESA_GL_UNPACK_ROW_LENGTH           0x0CF2
#define MESA_GL_UNPACK_ALIGNMENT            0x0CF5
#define MESA_GL_PIXEL_UNPACK_BUFFER         0x88EC
#define MESA_GL_PIXEL_UNPACK_BUFFER_BINDING 0x88EF

static void drop_bindings(struct mesa_context *self)
{
#ifdef CGL_MESA_IOSURFACE
    for (int i = 0; i < self->bindingCount; i++)
        CFRelease(self->bindings[i].surface);
#endif
    free(self->bindings);
    self->bindings = NULL;
    self->bindingCount = self->bindingCapacity = 0;
}

#ifdef CGL_MESA_IOSURFACE
static GLenum binding_query(GLenum target)
{
    return target == MESA_GL_TEXTURE_RECTANGLE ? MESA_GL_TEXTURE_BINDING_RECTANGLE
                                               : MESA_GL_TEXTURE_BINDING_2D;
}

/* 4 bytes a pixel for the formats an IOSurface holds here ('BGRA'). */
static GLint pixels_per_row(IOSurfaceRef surface)
{
    size_t bytesPerElement = IOSurfaceGetBytesPerElement(surface);

    return (GLint)(IOSurfaceGetBytesPerRow(surface) / (bytesPerElement > 0 ? bytesPerElement : 4));
}

/* What the context drew into its IOSurface textures, into the surfaces.
   A binding whose texture went away or took another image is dropped. */
static void write_back_iosurfaces(struct mesa_context *self)
{
    GLint previous[2] = { 0, 0 }, packBuffer = 0;
    int haveTarget[2] = { 0, 0 };

    if (self->bindingCount == 0 || mesa_current != self)
        return;
    mesa.glGetIntegerv(MESA_GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
    if (packBuffer != 0)
        mesa.glBindBuffer(MESA_GL_PIXEL_PACK_BUFFER, 0);
    mesa.glPixelStorei(MESA_GL_PACK_ALIGNMENT, 4);
    for (int i = 0; i < self->bindingCount; ) {
        struct mesa_iosurface_binding *b = &self->bindings[i];
        int t = b->target == MESA_GL_TEXTURE_RECTANGLE;
        GLint width = 0, height = 0;

        if (!haveTarget[t]) {
            mesa.glGetIntegerv(binding_query(b->target), &previous[t]);
            haveTarget[t] = 1;
        }
        if (!mesa.glIsTexture(b->texture)) {
            CFRelease(b->surface);
            self->bindings[i] = self->bindings[--self->bindingCount];
            continue;
        }
        mesa.glBindTexture(b->target, b->texture);
        mesa.glGetTexLevelParameteriv(b->target, 0, MESA_GL_TEXTURE_WIDTH, &width);
        mesa.glGetTexLevelParameteriv(b->target, 0, MESA_GL_TEXTURE_HEIGHT, &height);
        if (width != b->width || height != b->height) {
            CFRelease(b->surface);
            self->bindings[i] = self->bindings[--self->bindingCount];
            continue;
        }
        if (IOSurfaceLock((IOSurfaceRef)b->surface, 0, NULL) == 0) {
            mesa.glPixelStorei(MESA_GL_PACK_ROW_LENGTH, pixels_per_row((IOSurfaceRef)b->surface));
            mesa.glGetTexImage(b->target, 0, b->format, b->type,
                               IOSurfaceGetBaseAddress((IOSurfaceRef)b->surface));
            IOSurfaceUnlock((IOSurfaceRef)b->surface, 0, NULL);
        }
        i++;
    }
    mesa.glPixelStorei(MESA_GL_PACK_ROW_LENGTH, 0);
    for (int t = 0; t < 2; t++)
        if (haveTarget[t])
            mesa.glBindTexture(t ? MESA_GL_TEXTURE_RECTANGLE : MESA_GL_TEXTURE_2D, (GLuint)previous[t]);
    if (packBuffer != 0)
        mesa.glBindBuffer(MESA_GL_PIXEL_PACK_BUFFER, (GLuint)packBuffer);
}

static CGLError mesa_tex_image_iosurface(void *context, GLenum target, GLenum internalFormat,
                                         GLsizei width, GLsizei height, GLenum format,
                                         GLenum type, void *ioSurface, GLuint plane)
{
    struct mesa_context *self = context;
    IOSurfaceRef surface = ioSurface;
    GLint texture = 0, unpackBuffer = 0;
    struct mesa_iosurface_binding *b = NULL;

    if (self == NULL || mesa_current != self)
        return kCGLBadContext;
    if ((target != MESA_GL_TEXTURE_RECTANGLE && target != MESA_GL_TEXTURE_2D) || plane != 0 ||
        (size_t)width > IOSurfaceGetWidth(surface) || (size_t)height > IOSurfaceGetHeight(surface))
        return kCGLBadValue;
    mesa.glGetIntegerv(binding_query(target), &texture);
    if (texture == 0)
        return kCGLBadValue;

    /* The surface's pixels are the texture's image to start with. */
    mesa.glGetIntegerv(MESA_GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
    if (unpackBuffer != 0)
        mesa.glBindBuffer(MESA_GL_PIXEL_UNPACK_BUFFER, 0);
    IOSurfaceLock(surface, kIOSurfaceLockReadOnly, NULL);
    mesa.glPixelStorei(MESA_GL_UNPACK_ALIGNMENT, 4);
    mesa.glPixelStorei(MESA_GL_UNPACK_ROW_LENGTH, pixels_per_row(surface));
    mesa.glTexImage2D(target, 0, (GLint)internalFormat, width, height, 0, format, type,
                      IOSurfaceGetBaseAddress(surface));
    mesa.glPixelStorei(MESA_GL_UNPACK_ROW_LENGTH, 0);
    IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, NULL);
    if (unpackBuffer != 0)
        mesa.glBindBuffer(MESA_GL_PIXEL_UNPACK_BUFFER, (GLuint)unpackBuffer);

    /* What it draws there goes back to the surface on each flush. */
    for (int i = 0; i < self->bindingCount; i++)
        if (self->bindings[i].texture == (GLuint)texture) {
            b = &self->bindings[i];
            CFRelease(b->surface);
            break;
        }
    if (b == NULL) {
        if (self->bindingCount == self->bindingCapacity) {
            int capacity = self->bindingCapacity ? self->bindingCapacity * 2 : 8;
            void *grown = realloc(self->bindings, capacity * sizeof(*self->bindings));

            if (grown == NULL)
                return kCGLBadAlloc;
            self->bindings = grown;
            self->bindingCapacity = capacity;
        }
        b = &self->bindings[self->bindingCount++];
    }
    b->texture = (GLuint)texture;
    b->target = target;
    b->format = format;
    b->type = type;
    b->width = width;
    b->height = height;
    CFRetain(surface);
    b->surface = surface;
    return kCGLNoError;
}
#else
static void write_back_iosurfaces(struct mesa_context *self)
{
    (void)self;
}
#endif

/* glFlush and glFinish write the context's IOSurface textures back. */
static void mesa_gl_flush(void)
{
    mesa.glFlush();
    if (mesa_current != NULL)
        write_back_iosurfaces(mesa_current);
}

static void mesa_gl_finish(void)
{
    mesa.glFinish();
    if (mesa_current != NULL)
        write_back_iosurfaces(mesa_current);
}

static CGLError mesa_describe(const CGLDriver *driver, CGLRendererProperty prop,
                              GLint *value)
{
    (void)driver;
    switch (prop) {
    case kCGLRPAccelerated:
    case kCGLRPAcceleratedCompute:
    case kCGLRPFullScreen:
    case kCGLRPRemovable:
    case kCGLRPMaxAuxBuffers:
    case kCGLRPMaxSampleBuffers:
    case kCGLRPMaxSamples:
    case kCGLRPSampleModes:
    case kCGLRPSampleAlpha:
    case kCGLRPVideoMemory:
    case kCGLRPTextureMemory:
    case kCGLRPVideoMemoryMegabytes:
    case kCGLRPTextureMemoryMegabytes:
    case kCGLRPRegistryIDLow:
    case kCGLRPRegistryIDHigh:
        *value = 0;
        return kCGLNoError;
    case kCGLRPCompliant:
    case kCGLRPOnline:
    case kCGLRPWindow:
    case kCGLRPOffScreen:
    case kCGLRPBackingStore:
    case kCGLRPMPSafe:
    case kCGLRPMultiScreen:
    case kCGLRPGPUVertProcCapable:
    case kCGLRPGPUFragProcCapable:
        *value = 1;
        return kCGLNoError;
    case kCGLRPMajorGLVersion:
        *value = 4;
        return kCGLNoError;
    case kCGLRPBufferModes:
        *value = kCGLMonoscopicBit | kCGLSingleBufferBit | kCGLDoubleBufferBit;
        return kCGLNoError;
    case kCGLRPColorModes:
        *value = kCGLRGB888Bit | kCGLARGB8888Bit;
        return kCGLNoError;
    case kCGLRPDepthModes:
        *value = kCGL0Bit | kCGL16Bit | kCGL24Bit | kCGL32Bit;
        return kCGLNoError;
    case kCGLRPStencilModes:
        *value = kCGL0Bit | kCGL8Bit;
        return kCGLNoError;
    case kCGLRPAccumModes:
        *value = kCGL0Bit;
        return kCGLNoError;
    default:
        return kCGLBadProperty;
    }
}

static GLint pick_depth(const CGLDriverPixelFormatRequest *req)
{
    GLint want = req->value[kCGLPFADepthSize];

    if (!req->present[kCGLPFADepthSize] || want == 0)
        return req->present[kCGLPFAMaximumPolicy] ? 24 : 0;
    return want > 16 ? 24 : 16;
}

static CGLError mesa_choose(const CGLDriver *driver,
                            const CGLDriverPixelFormatRequest *req,
                            CGLDriverPixelFormat *fmt)
{
    GLint profile;

    (void)driver;
    if (!cgl_mesa_available())
        return kCGLBadMatch;
    /* Things this renderer cannot provide. */
    if (req->value[kCGLPFAColorSize] > 32 || req->value[kCGLPFAAlphaSize] > 8 ||
        req->value[kCGLPFAStencilSize] > 8 || req->value[kCGLPFAAccumSize] > 0 ||
        req->value[kCGLPFAAuxBuffers] > 0 || req->value[kCGLPFASamples] > 0 ||
        req->value[kCGLPFASampleBuffers] > 0 || req->present[kCGLPFAStereo] ||
        req->present[kCGLPFAColorFloat] || req->present[kCGLPFATripleBuffer] ||
        req->present[kCGLPFAFullScreen] || req->present[kCGLPFAAccelerated] ||
        req->present[kCGLPFANoRecovery])
        return kCGLBadMatch;
    profile = req->present[kCGLPFAOpenGLProfile] ?
        req->value[kCGLPFAOpenGLProfile] : kCGLOGLPVersion_Legacy;
    if (profile != kCGLOGLPVersion_Legacy && profile != kCGLOGLPVersion_3_2_Core &&
        profile != kCGLOGLPVersion_GL3_Core && profile != kCGLOGLPVersion_GL4_Core)
        return kCGLBadMatch;

    fmt->value[kCGLPFAColorSize] = req->value[kCGLPFAColorSize] > 24 ? 32 : 24;
    fmt->value[kCGLPFAAlphaSize] = req->present[kCGLPFAAlphaSize] &&
                                   req->value[kCGLPFAAlphaSize] > 0 ? 8 : 0;
    fmt->value[kCGLPFADepthSize] = pick_depth(req);
    fmt->value[kCGLPFAStencilSize] = req->value[kCGLPFAStencilSize] > 0 ? 8 : 0;
    fmt->value[kCGLPFADoubleBuffer] = req->present[kCGLPFADoubleBuffer];
    fmt->value[kCGLPFABackingStore] = req->present[kCGLPFABackingStore];
    fmt->value[kCGLPFAWindow] = 1;
    fmt->value[kCGLPFAOffScreen] = 1;
    fmt->value[kCGLPFAPBuffer] = 1;
    fmt->value[kCGLPFAMPSafe] = 1;
    fmt->value[kCGLPFAMultiScreen] = 1;
    fmt->value[kCGLPFAAllowOfflineRenderers] =
        req->present[kCGLPFAAllowOfflineRenderers];
    fmt->value[kCGLPFAOpenGLProfile] = profile;
    return kCGLNoError;
}

/* The EGL config for the format's color, depth and stencil. */
static EGLConfig choose_config(const CGLDriverPixelFormat *format)
{
    EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, format->value[kCGLPFAAlphaSize],
        EGL_DEPTH_SIZE, format->value[kCGLPFADepthSize],
        EGL_STENCIL_SIZE, format->value[kCGLPFAStencilSize],
        EGL_NONE
    };
    EGLConfig config = NULL;
    EGLint count = 0;

    if (!mesa.chooseConfig(mesa.display, attributes, &config, 1, &count) || count < 1)
        return NULL;
    return config;
}

static CGLError mesa_create(const CGLDriver *driver,
                            const CGLDriverPixelFormat *format, void *share,
                            void **context)
{
    struct mesa_context *self;
    struct mesa_context *shared = share;
    EGLint attributes[16];
    int n = 0;
    GLint profile = format->value[kCGLPFAOpenGLProfile];

    (void)driver;
    if (!cgl_mesa_available())
        return kCGLBadMatch;
    self = calloc(1, sizeof(*self));
    if (self == NULL)
        return kCGLBadAlloc;
    self->profile = profile;
    self->surface = EGL_NO_SURFACE;
    self->config = choose_config(format);
    if (self->config == NULL) {
        free(self);
        return kCGLBadMatch;
    }

    /* The legacy profile is a compatibility context, as the reference
       platform's is 2.1 with every old call; the core profiles are
       forward compatible, as there. */
    if (profile == kCGLOGLPVersion_Legacy) {
        attributes[n++] = EGL_CONTEXT_MAJOR_VERSION;
        attributes[n++] = 2;
        attributes[n++] = EGL_CONTEXT_MINOR_VERSION;
        attributes[n++] = 1;
        attributes[n++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
        attributes[n++] = EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT;
    } else {
        attributes[n++] = EGL_CONTEXT_MAJOR_VERSION;
        attributes[n++] = profile == kCGLOGLPVersion_GL4_Core ? 4 : 3;
        attributes[n++] = EGL_CONTEXT_MINOR_VERSION;
        attributes[n++] = profile == kCGLOGLPVersion_GL4_Core ? 1 : 2;
        attributes[n++] = EGL_CONTEXT_OPENGL_PROFILE_MASK;
        attributes[n++] = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
        attributes[n++] = EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE;
        attributes[n++] = EGL_TRUE;
    }
    attributes[n++] = EGL_NONE;

    /* The API is the calling thread's: an app makes contexts on any. */
    if (!mesa.bindAPI(EGL_OPENGL_API)) {
        free(self);
        return kCGLBadMatch;
    }
    self->context = mesa.createContext(mesa.display, self->config,
        shared != NULL ? shared->context : EGL_NO_CONTEXT, attributes);
    if (self->context == EGL_NO_CONTEXT) {
        free(self);
        return kCGLBadMatch;
    }
    *context = self;
    return kCGLNoError;
}

static void mesa_destroy(void *context)
{
    struct mesa_context *self = context;

    if (self == NULL)
        return;
    if (self->surface != EGL_NO_SURFACE)
        mesa.destroySurface(mesa.display, self->surface);
    if (mesa_current == self)
        mesa_current = NULL;
    mesa.destroyContext(mesa.display, self->context);
    drop_bindings(self);
    free(self);
}

static CGLError mesa_make_current(void *context)
{
    struct mesa_context *self = context;
    EGLBoolean ok;

    if (self == NULL)
        ok = mesa.makeCurrent(mesa.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    else
        ok = mesa.makeCurrent(mesa.display, self->surface, self->surface, self->context);
    if (!ok)
        return kCGLBadContext;
    mesa_current = self;
    return kCGLNoError;
}

/* An offscreen drawable: a pbuffer of its size, read back into the app's
   memory (rows from the bottom, as GL reads them; 32 bits a pixel, ARGB
   in the host's order) when it is flushed. A zero size takes it away. */
static CGLError mesa_set_offscreen(void *context, GLsizei width, GLsizei height,
                                   GLint rowbytes, void *baseaddr)
{
    struct mesa_context *self = context;
    EGLSurface surface = EGL_NO_SURFACE;

    if (width > 0 && height > 0 && baseaddr != NULL) {
        EGLint attributes[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE };

        if (rowbytes < width * 4)
            return kCGLBadValue;
        surface = mesa.createPbufferSurface(mesa.display, self->config, attributes);
        if (surface == EGL_NO_SURFACE)
            return kCGLBadAlloc;
    }
    if (self->surface != EGL_NO_SURFACE)
        mesa.destroySurface(mesa.display, self->surface);
    self->surface = surface;
    self->width = width;
    self->height = height;
    self->rowBytes = rowbytes;
    self->pixels = surface != EGL_NO_SURFACE ? baseaddr : NULL;
    /* A context current on this thread draws there from now on. */
    if (mesa.getCurrentContext() == self->context &&
        mesa.makeCurrent(mesa.display, self->surface, self->surface, self->context) != EGL_TRUE)
        return kCGLBadDrawable;
    return kCGLNoError;
}

static CGLError mesa_clear_drawable(void *context)
{
    return mesa_set_offscreen(context, 0, 0, 0, NULL);
}

static CGLError mesa_flush(void *context)
{
    struct mesa_context *self = context;
    GLint readFramebuffer = 0, packBuffer = 0;

    if (self == NULL)
        return kCGLBadContext;
    write_back_iosurfaces(self);
    if (self->pixels == NULL) {
        mesa.glFinish();
        return kCGLNoError;
    }
    /* The drawable's own framebuffer, whatever the app has bound, into the
       app's memory as it lays it out. */
    if (mesa.glBindFramebuffer != NULL) {
        mesa.glGetIntegerv(MESA_GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        mesa.glBindFramebuffer(MESA_GL_READ_FRAMEBUFFER, 0);
    }
    if (mesa.glBindBuffer != NULL) {
        mesa.glGetIntegerv(MESA_GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
        mesa.glBindBuffer(MESA_GL_PIXEL_PACK_BUFFER, 0);
    }
    mesa.glPixelStorei(MESA_GL_PACK_ALIGNMENT, 4);
    mesa.glPixelStorei(MESA_GL_PACK_ROW_LENGTH, self->rowBytes / 4);
    mesa.glReadPixels(0, 0, self->width, self->height, MESA_GL_BGRA,
                      MESA_GL_UNSIGNED_INT_8_8_8_8_REV, self->pixels);
    mesa.glPixelStorei(MESA_GL_PACK_ROW_LENGTH, 0);
    if (mesa.glBindBuffer != NULL)
        mesa.glBindBuffer(MESA_GL_PIXEL_PACK_BUFFER, (GLuint)packBuffer);
    if (mesa.glBindFramebuffer != NULL)
        mesa.glBindFramebuffer(MESA_GL_READ_FRAMEBUFFER, (GLuint)readFramebuffer);
    return kCGLNoError;
}

static void *mesa_get_proc_address(void *context, const char *name)
{
    (void)context;
    if (strcmp(name, "glFlush") == 0)
        return (void *)mesa_gl_flush;
    if (strcmp(name, "glFinish") == 0)
        return (void *)mesa_gl_finish;
    return (void *)mesa.getProcAddress(name);
}

CGL_HIDDEN const CGLDriver cgl_mesa_driver = {
    .abiVersion         = CGL_DRIVER_ABI_VERSION,
    .name               = "Mesa",
    .rendererID         = kCGLRendererGenericFloatID,
    .displayMask        = 0x1,
    .describeRenderer   = mesa_describe,
    .choosePixelFormat  = mesa_choose,
    .createContext      = mesa_create,
    .destroyContext     = mesa_destroy,
    .makeCurrent        = mesa_make_current,
    .flushDrawable      = mesa_flush,
    .clearDrawable      = mesa_clear_drawable,
    .setOffScreen       = mesa_set_offscreen,
    .getProcAddress     = mesa_get_proc_address,
#ifdef CGL_MESA_IOSURFACE
    .texImageIOSurface  = mesa_tex_image_iosurface,
#endif
};
