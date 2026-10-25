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
 * Contexts: lifetime, per-thread current context, parameters, enables,
 * drawables, locking and share groups.
 */

#include "CGLInternal.h"
#include "GLDispatch.gen.h"

#include <stdlib.h>
#include <string.h>

/* ---- Parameter table ---------------------------------------------------- */

enum { PARAM_READONLY = 1 };

static const struct cgl_param_info {
    GLint   pname;
    uint8_t count;
    uint8_t flags;
    GLint   initial[CGL_PARAM_MAX_VALUES];
} param_info[] = {
    { kCGLCPSwapRectangle,                      4, 0, { 0 } },
    { kCGLCPSwapInterval,                       1, 0, { 0 } },
    { kCGLCPDispatchTableSize,                  1, PARAM_READONLY, { 0 } },
    { kCGLCPClientStorage,                      1, 0, { 0 } },
    { kCGLCPSurfaceTexture,                     3, 0, { 0 } },
    { kCGLCPSurfaceOrder,                       1, 0, { 1 } },
    { kCGLCPSurfaceOpacity,                     1, 0, { 1 } },
    { kCGLCPSurfaceBackingSize,                 2, 0, { 0 } },
    { kCGLCPSurfaceSurfaceVolatile,             1, 0, { 0 } },
    { kCGLCPReclaimResources,                   1, 0, { 0 } },
    { kCGLCPCurrentRendererID,                  1, PARAM_READONLY, { 0 } },
    { kCGLCPGPUVertexProcessing,                1, PARAM_READONLY, { 0 } },
    { kCGLCPGPUFragmentProcessing,              1, PARAM_READONLY, { 0 } },
    { kCGLCPHasDrawable,                        1, PARAM_READONLY, { 0 } },
    { kCGLCPMPSwapsInFlight,                    1, 0, { 1 } },
    { kCGLCPGPURestartStatus,                   1, PARAM_READONLY, { 0 } },
    { kCGLCPAbortOnGPURestartStatusBlacklisted, 1, 0, { 0 } },
    { kCGLCPSupportGPURestart,                  1, 0, { 0 } },
    { kCGLCPSupportSeparateAddressSpace,        1, 0, { 0 } },
    { kCGLCPContextPriorityRequest,             1, 0,
      { kCGLCPContextPriorityRequestNormal } },

    /* Minkar private (CGLPrivate.h) */
    { kCGLCPSurfaceFrame,                       4, 0, { 0 } },
    { kCGLCPSurfaceIsChildWindow,               1, 0, { 0 } },
    { kCGLCPWindowNumber,                       1, 0, { 0 } },
    { kCGLCPOverlayPointer,                     2, 0, { 0 } },
    { kCGLCPSurfaceBackingOrigin,               2, 0, { 0 } },
    { kCGLCPSurfaceWindowNumber,                1, 0, { 0 } },
    { kCGLCPSurfaceHidden,                      1, 0, { 0 } },
};

#define PARAM_COUNT ((GLint)(sizeof(param_info) / sizeof(param_info[0])))

_Static_assert(sizeof(param_info) / sizeof(param_info[0]) <= CGL_PARAM_SLOTS,
               "CGL_PARAM_SLOTS too small");

static GLint param_slot(GLint pname)
{
    GLint i;

    for (i = 0; i < PARAM_COUNT; i++) {
        if (param_info[i].pname == pname)
            return i;
    }
    return -1;
}

/* ---- Enable table ------------------------------------------------------- */

static const struct {
    CGLContextEnable pname;
    GLboolean        initial;
} enable_info[] = {
    { kCGLCESwapRectangle,           0 },
    { kCGLCESwapLimit,               0 },
    { kCGLCERasterization,           1 },
    { kCGLCEStateValidation,         0 },
    { kCGLCESurfaceBackingSize,      0 },
    { kCGLCEDisplayListOptimization, 0 },
    { kCGLCEMPEngine,                0 },
    { kCGLCECrashOnRemovedFunctions, 0 },
};

#define ENABLE_COUNT ((GLint)(sizeof(enable_info) / sizeof(enable_info[0])))

static GLint enable_slot(CGLContextEnable pname)
{
    GLint i;

    for (i = 0; i < ENABLE_COUNT; i++) {
        if (enable_info[i].pname == pname)
            return i;
    }
    return -1;
}

/* ---- Current context (per thread) --------------------------------------- */

static pthread_key_t  current_key;
static pthread_once_t current_once = PTHREAD_ONCE_INIT;

static void current_key_create(void)
{
    pthread_key_create(&current_key, NULL);
}

static CGLContextObj current_get(void)
{
    pthread_once(&current_once, current_key_create);
    return pthread_getspecific(current_key);
}

static void current_set(CGLContextObj ctx)
{
    pthread_once(&current_once, current_key_create);
    pthread_setspecific(current_key, ctx);
}

CGLContextObj CGLGetCurrentContext(void)
{
    return current_get();
}

CGLGLProc cgl_gl_proc(int index)
{
    CGLContextObj ctx = current_get();

    return ctx != NULL ? ctx->glProcs[index] : NULL;
}

/* Resolve every GL entry point through the driver, once per context. */
static CGLGLProc *resolve_gl_procs(const CGLDriver *driver, void *driverContext)
{
    CGLGLProc *procs = calloc(CGL_GL_COUNT, sizeof(*procs));
    int i;

    if (procs == NULL || driver->getProcAddress == NULL)
        return procs;
    for (i = 0; i < CGL_GL_COUNT; i++) {
        void *p = driver->getProcAddress(driverContext, cgl_gl_names[i]);

        /* Function pointers travel as void * through getProcAddress. */
        memcpy(&procs[i], &p, sizeof(procs[i]));
    }
    return procs;
}

CGLError CGLSetCurrentContext(CGLContextObj ctx)
{
    CGLContextObj prev = current_get();
    CGLError err;

    if (ctx == prev)
        return kCGLNoError;

    /* Unbind the previous driver unless the new context shares it. */
    if (prev != NULL && (ctx == NULL || prev->driver != ctx->driver))
        prev->driver->makeCurrent(NULL);

    if (ctx != NULL) {
        err = ctx->driver->makeCurrent(ctx->driverContext);
        if (err != kCGLNoError) {
            current_set(NULL);
            return err;
        }
    }
    current_set(ctx);
    return kCGLNoError;
}

/* ---- Share groups ------------------------------------------------------- */

static struct _CGLShareGroupObject *share_group_create(void)
{
    struct _CGLShareGroupObject *g = calloc(1, sizeof(*g));

    if (g != NULL)
        atomic_init(&g->refcount, 1);
    return g;
}

static void share_group_release(struct _CGLShareGroupObject *g)
{
    if (g != NULL && atomic_fetch_sub(&g->refcount, 1) == 1)
        free(g);
}

CGLShareGroupObj CGLGetShareGroup(CGLContextObj ctx)
{
    return ctx != NULL ? ctx->shareGroup : NULL;
}

/* ---- Lifetime ----------------------------------------------------------- */

CGLError CGLCreateContext(CGLPixelFormatObj pix, CGLContextObj share,
                          CGLContextObj *ctxp)
{
    pthread_mutexattr_t attr;
    CGLContextObj ctx;
    CGLError err;
    GLint i;

    if (ctxp == NULL)
        return kCGLBadAddress;
    *ctxp = NULL;
    if (pix == NULL || pix->nscreens == 0)
        return kCGLBadPixelFormat;
    if (share != NULL && share->driver != pix->drivers[0])
        return kCGLBadMatch;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
        return kCGLBadAlloc;

    if (share != NULL) {
        ctx->shareGroup = share->shareGroup;
        atomic_fetch_add(&ctx->shareGroup->refcount, 1);
    } else if ((ctx->shareGroup = share_group_create()) == NULL) {
        free(ctx);
        return kCGLBadAlloc;
    }

    ctx->driver = pix->drivers[0];
    ctx->virtualScreen = 0;
    err = ctx->driver->createContext(ctx->driver, &pix->formats[0],
                                     share != NULL ? share->driverContext : NULL,
                                     &ctx->driverContext);
    if (err != kCGLNoError) {
        share_group_release(ctx->shareGroup);
        free(ctx);
        return err;
    }

    ctx->glProcs = resolve_gl_procs(ctx->driver, ctx->driverContext);
    if (ctx->glProcs == NULL) {
        ctx->driver->destroyContext(ctx->driverContext);
        share_group_release(ctx->shareGroup);
        free(ctx);
        return kCGLBadAlloc;
    }

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&ctx->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    for (i = 0; i < PARAM_COUNT; i++)
        memcpy(ctx->params[i], param_info[i].initial, sizeof(ctx->params[i]));
    for (i = 0; i < ENABLE_COUNT; i++) {
        if (enable_info[i].initial)
            ctx->enables |= 1u << i;
    }

    ctx->pixelFormat = CGLRetainPixelFormat(pix);
    atomic_init(&ctx->refcount, 1);
    *ctxp = ctx;
    return kCGLNoError;
}

CGLContextObj CGLRetainContext(CGLContextObj ctx)
{
    if (ctx != NULL)
        atomic_fetch_add(&ctx->refcount, 1);
    return ctx;
}

void CGLReleaseContext(CGLContextObj ctx)
{
    if (ctx == NULL || atomic_fetch_sub(&ctx->refcount, 1) != 1)
        return;

    /* Never leave a dangling current context on this thread. */
    if (current_get() == ctx) {
        ctx->driver->makeCurrent(NULL);
        current_set(NULL);
    }

    ctx->driver->destroyContext(ctx->driverContext);
    free(ctx->glProcs);
    CGLReleasePBuffer(ctx->pbuffer);
    CGLReleasePixelFormat(ctx->pixelFormat);
    share_group_release(ctx->shareGroup);
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
}

CGLError CGLDestroyContext(CGLContextObj ctx)
{
    if (ctx == NULL)
        return kCGLBadContext;
    CGLReleaseContext(ctx);
    return kCGLNoError;
}

GLuint CGLGetContextRetainCount(CGLContextObj ctx)
{
    if (ctx == NULL)
        return 0;
    return atomic_load(&ctx->refcount);
}

CGLPixelFormatObj CGLGetPixelFormat(CGLContextObj ctx)
{
    return ctx != NULL ? ctx->pixelFormat : NULL;
}

void *CGLGetDriverContext(CGLContextObj ctx)
{
    return ctx != NULL ? ctx->driverContext : NULL;
}

CGLError CGLCopyContext(CGLContextObj src, CGLContextObj dst, GLbitfield mask)
{
    if (src == NULL || dst == NULL)
        return kCGLBadContext;
    if (src == dst || src->driver != dst->driver)
        return kCGLBadMatch;
    if (src->driver->copyContext == NULL)
        return kCGLNoError;
    return src->driver->copyContext(src->driverContext, dst->driverContext,
                                    mask);
}

/* ---- Locking ------------------------------------------------------------ */

CGLError CGLLockContext(CGLContextObj ctx)
{
    if (ctx == NULL)
        return kCGLBadContext;
    return pthread_mutex_lock(&ctx->lock) == 0 ? kCGLNoError : kCGLBadState;
}

CGLError CGLUnlockContext(CGLContextObj ctx)
{
    if (ctx == NULL)
        return kCGLBadContext;
    return pthread_mutex_unlock(&ctx->lock) == 0 ? kCGLNoError : kCGLBadState;
}

/* ---- Parameters --------------------------------------------------------- */

static CGLError validate_parameter(GLint pname, const GLint *v)
{
    switch (pname) {
    case kCGLCPSurfaceOpacity:
    case kCGLCPSurfaceHidden:
    case kCGLCPSurfaceIsChildWindow:
        return (v[0] == 0 || v[0] == 1) ? kCGLNoError : kCGLBadValue;
    case kCGLCPSurfaceOrder:
        return (v[0] == 1 || v[0] == -1) ? kCGLNoError : kCGLBadValue;
    case kCGLCPSwapInterval:
    case kCGLCPMPSwapsInFlight:
        return v[0] >= 0 ? kCGLNoError : kCGLBadValue;
    case kCGLCPSurfaceBackingSize:
        return (v[0] >= 0 && v[1] >= 0) ? kCGLNoError : kCGLBadValue;
    case kCGLCPSwapRectangle:
        return (v[2] >= 0 && v[3] >= 0) ? kCGLNoError : kCGLBadValue;
    case kCGLCPContextPriorityRequest:
        return (v[0] >= kCGLCPContextPriorityRequestHigh &&
                v[0] <= kCGLCPContextPriorityRequestLow) ?
               kCGLNoError : kCGLBadValue;
    default:
        return kCGLNoError;
    }
}

static CGLError set_parameter(CGLContextObj ctx, GLint pname, const GLint *params)
{
    GLint slot;
    CGLError err;

    if (ctx == NULL)
        return kCGLBadContext;
    if (params == NULL)
        return kCGLBadAddress;
    slot = param_slot(pname);
    if (slot < 0 || (param_info[slot].flags & PARAM_READONLY))
        return kCGLBadEnumeration;
    if ((err = validate_parameter(pname, params)) != kCGLNoError)
        return err;

    pthread_mutex_lock(&ctx->lock);
    if (ctx->driver->setParameter != NULL)
        err = ctx->driver->setParameter(ctx->driverContext, pname, params,
                                        param_info[slot].count);
    if (err == kCGLNoError)
        memcpy(ctx->params[slot], params,
               sizeof(GLint) * param_info[slot].count);
    pthread_mutex_unlock(&ctx->lock);
    return err;
}

CGLError CGLSetParameter(CGLContextObj ctx, CGLContextParameter pname,
                         const GLint *params)
{
    return set_parameter(ctx, (GLint)pname, params);
}

static GLboolean has_drawable(CGLContextObj ctx)
{
    return ctx->hasOffScreen || ctx->pbuffer != NULL ||
           ctx->params[param_slot(kCGLCPSurfaceWindowNumber)][0] != 0 ||
           ctx->params[param_slot(kCGLCPWindowNumber)][0] != 0;
}

CGLError CGLGetParameter(CGLContextObj ctx, CGLContextParameter pname,
                         GLint *params)
{
    GLint slot;

    if (ctx == NULL)
        return kCGLBadContext;
    if (params == NULL)
        return kCGLBadAddress;
    slot = param_slot((GLint)pname);
    if (slot < 0)
        return kCGLBadEnumeration;

    pthread_mutex_lock(&ctx->lock);
    switch ((GLint)pname) {
    case kCGLCPCurrentRendererID:
        params[0] = ctx->driver->rendererID;
        break;
    case kCGLCPGPUVertexProcessing:
        params[0] = cgl_driver_property(ctx->driver, kCGLRPGPUVertProcCapable, 0);
        break;
    case kCGLCPGPUFragmentProcessing:
        params[0] = cgl_driver_property(ctx->driver, kCGLRPGPUFragProcCapable, 0);
        break;
    case kCGLCPHasDrawable:
        params[0] = has_drawable(ctx) ? 1 : 0;
        break;
    case kCGLCPGPURestartStatus:
        params[0] = kCGLCPGPURestartStatusNone;
        break;
    case kCGLCPDispatchTableSize:
        params[0] = CGL_GL_COUNT;
        break;
    default:
        memcpy(params, ctx->params[slot],
               sizeof(GLint) * param_info[slot].count);
        break;
    }
    pthread_mutex_unlock(&ctx->lock);
    return kCGLNoError;
}

CGLError CGLSurfaceResize(CGLContextObj ctx, GLint width, GLint height)
{
    GLint size[2] = { width, height };

    return set_parameter(ctx, kCGLCPSurfaceBackingSize, size);
}

CGLError CGLUseShaders(CGLContextObj ctx)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (ctx->driver->useShaders == NULL)
        return kCGLNoError;
    return ctx->driver->useShaders(ctx->driverContext);
}

/* ---- Enables ------------------------------------------------------------ */

static CGLError set_enable(CGLContextObj ctx, CGLContextEnable pname,
                           GLboolean on)
{
    CGLError err = kCGLNoError;
    GLint slot;

    if (ctx == NULL)
        return kCGLBadContext;
    if ((slot = enable_slot(pname)) < 0)
        return kCGLBadEnumeration;

    pthread_mutex_lock(&ctx->lock);
    if (ctx->driver->setEnable != NULL)
        err = ctx->driver->setEnable(ctx->driverContext, pname, on);
    if (err == kCGLNoError) {
        if (on)
            ctx->enables |= 1u << slot;
        else
            ctx->enables &= ~(1u << slot);
    }
    pthread_mutex_unlock(&ctx->lock);
    return err;
}

CGLError CGLEnable(CGLContextObj ctx, CGLContextEnable pname)
{
    return set_enable(ctx, pname, 1);
}

CGLError CGLDisable(CGLContextObj ctx, CGLContextEnable pname)
{
    return set_enable(ctx, pname, 0);
}

CGLError CGLIsEnabled(CGLContextObj ctx, CGLContextEnable pname, GLint *enable)
{
    GLint slot;

    if (ctx == NULL)
        return kCGLBadContext;
    if (enable == NULL)
        return kCGLBadAddress;
    if ((slot = enable_slot(pname)) < 0)
        return kCGLBadEnumeration;
    pthread_mutex_lock(&ctx->lock);
    *enable = (ctx->enables >> slot) & 1;
    pthread_mutex_unlock(&ctx->lock);
    return kCGLNoError;
}

/* ---- Virtual screens ---------------------------------------------------- */

CGLError CGLSetVirtualScreen(CGLContextObj ctx, GLint screen)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (screen < 0 || screen >= ctx->pixelFormat->nscreens)
        return kCGLBadValue;
    /*
     * TODO: switching to a screen served by another renderer needs the
     * driver context to be recreated on that renderer.
     */
    if (ctx->pixelFormat->drivers[screen] != ctx->driver)
        return kCGLBadMatch;
    pthread_mutex_lock(&ctx->lock);
    ctx->virtualScreen = screen;
    pthread_mutex_unlock(&ctx->lock);
    return kCGLNoError;
}

CGLError CGLGetVirtualScreen(CGLContextObj ctx, GLint *screen)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (screen == NULL)
        return kCGLBadAddress;
    *screen = ctx->virtualScreen;
    return kCGLNoError;
}

CGLError CGLUpdateContext(CGLContextObj ctx)
{
    CGLError err = kCGLNoError;

    if (ctx == NULL)
        return kCGLBadContext;
    pthread_mutex_lock(&ctx->lock);
    if (ctx->driver->updateContext != NULL)
        err = ctx->driver->updateContext(ctx->driverContext);
    pthread_mutex_unlock(&ctx->lock);
    return err;
}

/* ---- Drawables ---------------------------------------------------------- */

static void detach_pbuffer(CGLContextObj ctx)
{
    CGLReleasePBuffer(ctx->pbuffer);
    ctx->pbuffer = NULL;
    ctx->pbufferFace = 0;
    ctx->pbufferLevel = 0;
}

CGLError CGLSetOffScreen(CGLContextObj ctx, GLsizei width, GLsizei height,
                         GLint rowbytes, void *baseaddr)
{
    GLint bits, bpp;
    CGLError err = kCGLNoError;

    if (ctx == NULL)
        return kCGLBadContext;
    if (baseaddr == NULL)
        return kCGLBadAddress;
    if (width <= 0 || height <= 0)
        return kCGLBadValue;

    bits = ctx->pixelFormat->formats[ctx->virtualScreen].value[kCGLPFAColorSize];
    bpp = bits > 0 ? (bits + 7) / 8 : 1;
    if (rowbytes < width * bpp)
        return kCGLBadValue;

    pthread_mutex_lock(&ctx->lock);
    if (ctx->driver->setOffScreen != NULL)
        err = ctx->driver->setOffScreen(ctx->driverContext, width, height,
                                        rowbytes, baseaddr);
    if (err == kCGLNoError) {
        detach_pbuffer(ctx);
        ctx->hasOffScreen = 1;
        ctx->offWidth = width;
        ctx->offHeight = height;
        ctx->offRowBytes = rowbytes;
        ctx->offBase = baseaddr;
    }
    pthread_mutex_unlock(&ctx->lock);
    return err;
}

CGLError CGLGetOffScreen(CGLContextObj ctx, GLsizei *width, GLsizei *height,
                         GLint *rowbytes, void **baseaddr)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (width == NULL || height == NULL || rowbytes == NULL || baseaddr == NULL)
        return kCGLBadAddress;

    pthread_mutex_lock(&ctx->lock);
    if (ctx->hasOffScreen) {
        *width = ctx->offWidth;
        *height = ctx->offHeight;
        *rowbytes = ctx->offRowBytes;
        *baseaddr = ctx->offBase;
    } else {
        *width = 0;
        *height = 0;
        *rowbytes = 0;
        *baseaddr = NULL;
    }
    pthread_mutex_unlock(&ctx->lock);
    return kCGLNoError;
}

/* Full-screen contexts are deprecated; use a full-screen window instead. */
CGLError CGLSetFullScreen(CGLContextObj ctx)
{
    return ctx == NULL ? kCGLBadContext : kCGLBadFullScreen;
}

CGLError CGLSetFullScreenOnDisplay(CGLContextObj ctx, GLuint display_mask)
{
    (void)display_mask;
    return ctx == NULL ? kCGLBadContext : kCGLBadFullScreen;
}

CGLError CGLSetPBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer, GLenum face,
                       GLint level, GLint screen)
{
    GLsizei w, h;
    GLenum target, format;
    GLint maxLevel;
    CGLError err;

    if (ctx == NULL)
        return kCGLBadContext;
    if (pbuffer == NULL)
        return kCGLBadAddress;
    CGLDescribePBuffer(pbuffer, &w, &h, &target, &format, &maxLevel);
    if (level < 0 || level > maxLevel)
        return kCGLBadValue;
    if (target == CGL_GL_TEXTURE_CUBE_MAP) {
        if (face < CGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
            face > CGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
            return kCGLBadValue;
    } else if (face != 0) {
        return kCGLBadValue;
    }
    if (screen != ctx->virtualScreen &&
        (err = CGLSetVirtualScreen(ctx, screen)) != kCGLNoError)
        return err;

    pthread_mutex_lock(&ctx->lock);
    err = kCGLNoError;
    if (ctx->driver->setPBuffer != NULL)
        err = ctx->driver->setPBuffer(ctx->driverContext, pbuffer, face, level);
    if (err == kCGLNoError) {
        CGLRetainPBuffer(pbuffer);
        detach_pbuffer(ctx);
        ctx->pbuffer = pbuffer;
        ctx->pbufferFace = face;
        ctx->pbufferLevel = level;
        ctx->hasOffScreen = 0;
    }
    pthread_mutex_unlock(&ctx->lock);
    return err;
}

CGLError CGLGetPBuffer(CGLContextObj ctx, CGLPBufferObj *pbuffer, GLenum *face,
                       GLint *level, GLint *screen)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (pbuffer == NULL || face == NULL || level == NULL || screen == NULL)
        return kCGLBadAddress;
    pthread_mutex_lock(&ctx->lock);
    *pbuffer = ctx->pbuffer;
    *face = ctx->pbufferFace;
    *level = ctx->pbufferLevel;
    *screen = ctx->virtualScreen;
    pthread_mutex_unlock(&ctx->lock);
    return kCGLNoError;
}

CGLError CGLTexImagePBuffer(CGLContextObj ctx, CGLPBufferObj pbuffer,
                            GLenum source)
{
    if (ctx == NULL)
        return kCGLBadContext;
    if (pbuffer == NULL)
        return kCGLBadAddress;
    if (source < CGL_GL_FRONT_LEFT || source > CGL_GL_AUX3)
        return kCGLBadValue;
    if (ctx->driver->texImagePBuffer == NULL)
        return kCGLNoError;
    return ctx->driver->texImagePBuffer(ctx->driverContext, pbuffer, source);
}

CGLError CGLClearDrawable(CGLContextObj ctx)
{
    CGLError err = kCGLNoError;

    if (ctx == NULL)
        return kCGLBadContext;
    pthread_mutex_lock(&ctx->lock);
    if (ctx->driver->clearDrawable != NULL)
        err = ctx->driver->clearDrawable(ctx->driverContext);
    if (err == kCGLNoError) {
        detach_pbuffer(ctx);
        ctx->hasOffScreen = 0;
        ctx->params[param_slot(kCGLCPSurfaceWindowNumber)][0] = 0;
        ctx->params[param_slot(kCGLCPWindowNumber)][0] = 0;
    }
    pthread_mutex_unlock(&ctx->lock);
    return err;
}

CGLError CGLFlushDrawable(CGLContextObj ctx)
{
    CGLError err = kCGLNoError;

    if (ctx == NULL)
        return kCGLBadContext;
    pthread_mutex_lock(&ctx->lock);
    if (ctx->driver->flushDrawable != NULL)
        err = ctx->driver->flushDrawable(ctx->driverContext);
    pthread_mutex_unlock(&ctx->lock);
    return err;
}
