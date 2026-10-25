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

/* Renderer driver registry and renderer information queries. */

#include "CGLInternal.h"

#include <stdlib.h>
#include <string.h>

#define CGL_MAX_DRIVERS 16

static pthread_mutex_t   registry_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t    registry_once = PTHREAD_ONCE_INIT;
static const CGLDriver  *registry[CGL_MAX_DRIVERS];
static GLint             registry_count;

static CGLError register_locked(const CGLDriver *driver)
{
    GLint i;

    for (i = 0; i < registry_count; i++) {
        if (registry[i] == driver ||
            registry[i]->rendererID == driver->rendererID)
            return kCGLBadMatch;
    }
    if (registry_count == CGL_MAX_DRIVERS)
        return kCGLBadAlloc;
    registry[registry_count++] = driver;
    return kCGLNoError;
}

static void register_builtin_drivers(void)
{
    /*
     * The null renderer creates contexts that draw nothing. It is only
     * useful for testing the CGL/AppKit paths, so it is opt-in.
     */
    const char *env = getenv("CGL_ENABLE_NULL_RENDERER");

    if (env != NULL && strcmp(env, "1") == 0) {
        pthread_mutex_lock(&registry_lock);
        register_locked(&cgl_null_driver);
        pthread_mutex_unlock(&registry_lock);
    }
}

static void registry_init(void)
{
    pthread_once(&registry_once, register_builtin_drivers);
}

CGLError CGLRegisterDriver(const CGLDriver *driver)
{
    CGLError err;

    if (driver == NULL)
        return kCGLBadAddress;
    if (driver->abiVersion != CGL_DRIVER_ABI_VERSION)
        return kCGLBadCodeModule;
    if (driver->describeRenderer == NULL || driver->choosePixelFormat == NULL ||
        driver->createContext == NULL || driver->destroyContext == NULL ||
        driver->makeCurrent == NULL)
        return kCGLBadCodeModule;

    registry_init();
    pthread_mutex_lock(&registry_lock);
    err = register_locked(driver);
    pthread_mutex_unlock(&registry_lock);
    return err;
}

GLint cgl_driver_count(void)
{
    GLint n;

    registry_init();
    pthread_mutex_lock(&registry_lock);
    n = registry_count;
    pthread_mutex_unlock(&registry_lock);
    return n;
}

const CGLDriver *cgl_driver_at(GLint index)
{
    const CGLDriver *d = NULL;

    registry_init();
    pthread_mutex_lock(&registry_lock);
    if (index >= 0 && index < registry_count)
        d = registry[index];
    pthread_mutex_unlock(&registry_lock);
    return d;
}

GLint cgl_driver_property(const CGLDriver *driver, CGLRendererProperty prop,
                          GLint fallback)
{
    GLint value = fallback;

    if (driver->describeRenderer(driver, prop, &value) != kCGLNoError)
        value = fallback;
    return value;
}

/* ---- Renderer information ---------------------------------------------- */

struct _CGLRendererInfoObject {
    GLint             count;
    const CGLDriver  *drivers[CGL_MAX_DRIVERS];
};

CGLError CGLQueryRendererInfo(GLuint display_mask, CGLRendererInfoObj *rend,
                              GLint *nrend)
{
    CGLRendererInfoObj info;
    GLint i, n;

    if (rend == NULL || nrend == NULL)
        return kCGLBadAddress;
    *rend = NULL;
    *nrend = 0;
    if (display_mask == 0)
        return kCGLBadDisplay;

    info = calloc(1, sizeof(*info));
    if (info == NULL)
        return kCGLBadAlloc;

    n = cgl_driver_count();
    for (i = 0; i < n; i++) {
        const CGLDriver *d = cgl_driver_at(i);

        if (d != NULL && (d->displayMask & display_mask) != 0)
            info->drivers[info->count++] = d;
    }

    *rend = info;
    *nrend = info->count;
    return kCGLNoError;
}

CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend)
{
    if (rend == NULL)
        return kCGLBadRendererInfo;
    free(rend);
    return kCGLNoError;
}

CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, GLint rend_num,
                             CGLRendererProperty prop, GLint *value)
{
    const CGLDriver *d;

    if (rend == NULL)
        return kCGLBadRendererInfo;
    if (value == NULL)
        return kCGLBadAddress;
    if (prop == kCGLRPRendererCount) {
        *value = rend->count;
        return kCGLNoError;
    }
    if (rend_num < 0 || rend_num >= rend->count)
        return kCGLBadValue;

    d = rend->drivers[rend_num];
    switch (prop) {
    case kCGLRPRendererID:
        *value = d->rendererID;
        return kCGLNoError;
    case kCGLRPDisplayMask:
        *value = (GLint)d->displayMask;
        return kCGLNoError;
    default:
        return d->describeRenderer(d, prop, value);
    }
}
