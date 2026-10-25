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

/* Library-wide options, version and error strings. */

#include "CGLInternal.h"

static pthread_mutex_t options_lock = PTHREAD_MUTEX_INITIALIZER;

static GLint option_format_cache_size = 5;
static GLint option_retain_renderers;
static GLint option_use_build_cache;
static GLint option_use_error_handler;

static GLint *option_storage(CGLGlobalOption pname)
{
    switch (pname) {
    case kCGLGOFormatCacheSize:  return &option_format_cache_size;
    case kCGLGORetainRenderers:  return &option_retain_renderers;
    case kCGLGOUseBuildCache:    return &option_use_build_cache;
    case kCGLGOUseErrorHandler:  return &option_use_error_handler;
    default:                     return NULL;
    }
}

CGLError CGLSetGlobalOption(CGLGlobalOption pname, const GLint *params)
{
    GLint *slot;

    if (params == NULL)
        return kCGLBadAddress;

    switch (pname) {
    case kCGLGOClearFormatCache:
    case kCGLGOResetLibrary:
        /* Nothing is cached yet; accepted as no-ops. */
        return kCGLNoError;
    case kCGLGOFormatCacheSize:
        if (params[0] < 1)
            return kCGLBadValue;
        break;
    default:
        break;
    }

    if ((slot = option_storage(pname)) == NULL)
        return kCGLBadEnumeration;
    pthread_mutex_lock(&options_lock);
    *slot = params[0];
    pthread_mutex_unlock(&options_lock);
    return kCGLNoError;
}

CGLError CGLGetGlobalOption(CGLGlobalOption pname, GLint *params)
{
    GLint *slot;

    if (params == NULL)
        return kCGLBadAddress;
    if (pname == kCGLGOClearFormatCache || pname == kCGLGOResetLibrary) {
        params[0] = 0;
        return kCGLNoError;
    }
    if ((slot = option_storage(pname)) == NULL)
        return kCGLBadEnumeration;
    pthread_mutex_lock(&options_lock);
    params[0] = *slot;
    pthread_mutex_unlock(&options_lock);
    return kCGLNoError;
}

CGLError CGLSetOption(CGLGlobalOption pname, GLint param)
{
    return CGLSetGlobalOption(pname, &param);
}

CGLError CGLGetOption(CGLGlobalOption pname, GLint *param)
{
    return CGLGetGlobalOption(pname, param);
}

void CGLGetVersion(GLint *majorvers, GLint *minorvers)
{
    if (majorvers != NULL)
        *majorvers = 1;
    if (minorvers != NULL)
        *minorvers = 3;
}

const char *CGLErrorString(CGLError error)
{
    switch (error) {
    case kCGLNoError:         return "no error";
    case kCGLBadAttribute:    return "invalid pixel format attribute";
    case kCGLBadProperty:     return "invalid renderer property";
    case kCGLBadPixelFormat:  return "invalid pixel format";
    case kCGLBadRendererInfo: return "invalid renderer info";
    case kCGLBadContext:      return "invalid context";
    case kCGLBadDrawable:     return "invalid drawable";
    case kCGLBadDisplay:      return "invalid display";
    case kCGLBadState:        return "invalid context state";
    case kCGLBadValue:        return "invalid numerical value";
    case kCGLBadMatch:        return "invalid share context";
    case kCGLBadEnumeration:  return "invalid enumerant";
    case kCGLBadOffScreen:    return "invalid offscreen drawable";
    case kCGLBadFullScreen:   return "invalid fullscreen drawable";
    case kCGLBadWindow:       return "invalid window";
    case kCGLBadAddress:      return "invalid pointer";
    case kCGLBadCodeModule:   return "invalid code module";
    case kCGLBadAlloc:        return "invalid memory allocation";
    case kCGLBadConnection:   return "invalid connection to the window server";
    default:                  return "unknown error";
    }
}
