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
 * Quartz Window Services: window lists.
 *
 * The lists, the descriptions and the images come from the window server
 * (SkyLight), which filters by CGWindowListOption itself.
 */

#include <CoreGraphics/CGWindow.h>
#include <CoreGraphics/CGDirectDisplay.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CoreGraphics/CGBitmapContext.h>
#include <CoreGraphics/CGColorSpace.h>
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGDataProvider.h>
#include <SkyLight/SkyLight.h>


const CFStringRef kCGWindowNumber = CFSTR("kCGWindowNumber");
const CFStringRef kCGWindowStoreType = CFSTR("kCGWindowStoreType");
const CFStringRef kCGWindowLayer = CFSTR("kCGWindowLayer");
const CFStringRef kCGWindowBounds = CFSTR("kCGWindowBounds");
const CFStringRef kCGWindowSharingState = CFSTR("kCGWindowSharingState");
const CFStringRef kCGWindowAlpha = CFSTR("kCGWindowAlpha");
const CFStringRef kCGWindowOwnerPID = CFSTR("kCGWindowOwnerPID");
const CFStringRef kCGWindowMemoryUsage = CFSTR("kCGWindowMemoryUsage");
const CFStringRef kCGWindowOwnerName = CFSTR("kCGWindowOwnerName");
const CFStringRef kCGWindowName = CFSTR("kCGWindowName");
const CFStringRef kCGWindowIsOnscreen = CFSTR("kCGWindowIsOnscreen");
const CFStringRef kCGWindowBackingLocationVideoMemory =
    CFSTR("kCGWindowBackingLocationVideoMemory");

/* The window IDs the option selects, front to back, from the server. */
static uint32_t sls_window_ids(CGWindowListOption option, CGWindowID relativeToWindow,
                               SLSWindowID *ids)
{
    uint32_t n = 0;

    if (SLSWindowListCopy(option, relativeToWindow, kSLSMaxWindowList, ids, &n) !=
        kSLSErrorSuccess)
        return 0;
    return n;
}

static void set_number(CFMutableDictionaryRef d, CFStringRef key, CFNumberType type,
                       const void *value)
{
    CFNumberRef n = CFNumberCreate(NULL, type, value);

    if (n != NULL) {
        CFDictionarySetValue(d, key, n);
        CFRelease(n);
    }
}

/* A window's dictionary, with the reference keys. */
static CFDictionaryRef copy_sls_description(SLSWindowID window)
{
    SLSWindowInfo info;
    CFMutableDictionaryRef d;
    int32_t number, pid, layer, store = kCGBackingStoreBuffered, sharing;
    int64_t memory;
    CFDictionaryRef bounds;

    memset(&info, 0, sizeof(info));
    if (SLSGetWindowInfo(window, &info) != kSLSErrorSuccess)
        return NULL;
    d = CFDictionaryCreateMutable(NULL, 10, &kCFTypeDictionaryKeyCallBacks,
                                  &kCFTypeDictionaryValueCallBacks);
    if (d == NULL)
        return NULL;
    number = (int32_t)info.window;
    pid = info.ownerPID;
    layer = info.level;
    sharing = (int32_t)info.sharingState;
    memory = (int64_t)(info.width * info.height * 4);
    set_number(d, kCGWindowNumber, kCFNumberSInt32Type, &number);
    set_number(d, kCGWindowOwnerPID, kCFNumberSInt32Type, &pid);
    set_number(d, kCGWindowLayer, kCFNumberSInt32Type, &layer);
    set_number(d, kCGWindowStoreType, kCFNumberSInt32Type, &store);
    set_number(d, kCGWindowSharingState, kCFNumberSInt32Type, &sharing);
    set_number(d, kCGWindowMemoryUsage, kCFNumberSInt64Type, &memory);
    set_number(d, kCGWindowAlpha, kCFNumberDoubleType, &info.alpha);
    bounds = CGRectCreateDictionaryRepresentation(CGRectMake(info.x, info.y, info.width,
                                                             info.height));
    if (bounds != NULL) {
        CFDictionarySetValue(d, kCGWindowBounds, bounds);
        CFRelease(bounds);
    }
    if (info.ownerName[0] != '\0') {
        CFStringRef name = CFStringCreateWithCString(NULL, info.ownerName,
                                                     kCFStringEncodingUTF8);
        if (name != NULL) {
            CFDictionarySetValue(d, kCGWindowOwnerName, name);
            CFRelease(name);
        }
    }
    /* As on the reference platform, only present for on-screen windows. */
    if (info.flags & kSLSWindowOnScreen)
        CFDictionarySetValue(d, kCGWindowIsOnscreen, kCFBooleanTrue);
    return d;
}

static void release_pixels(void *info, const void *data, size_t size)
{
    free((void *)data);
}

/*
 * The windows the option selects, drawn back to front into one image of
 * screenBounds (global coordinates, top left origin); with CGRectNull, the
 * smallest rectangle around them. Each window's image comes from the
 * server (SLSCopyWindowImage), so minimized and ordered-out windows can be
 * imaged one at a time (kCGWindowListOptionIncludingWindow), as the
 * Dock's thumbnails are. The ids are freed.
 */
static CGImageRef create_image_of_windows(CGRect screenBounds, SLSWindowID *ids, uint32_t n,
                                          CGWindowImageOption imageOption)
{
    uint32_t i;
    CGRect bounds = screenBounds;
    CGContextRef context;
    CGColorSpaceRef space;
    CGImageRef image = NULL;

    if (n == 0) {
        free(ids);
        return NULL;
    }
    if (CGRectIsNull(bounds) || CGRectIsInfinite(bounds)) {
        bounds = CGRectNull;
        for (i = 0; i < n; i++) {
            SLSWindowInfo info;
            if (SLSGetWindowInfo(ids[i], &info) == kSLSErrorSuccess)
                bounds = CGRectUnion(bounds, CGRectMake(info.x, info.y, info.width,
                                                        info.height));
        }
    }
    bounds = CGRectIntegral(bounds);
    if (CGRectIsNull(bounds) || bounds.size.width < 1 || bounds.size.height < 1) {
        free(ids);
        return NULL;
    }
    size_t width = (size_t)bounds.size.width, height = (size_t)bounds.size.height;
    size_t rowBytes = width * 4;
    uint8_t *buffer = calloc(height, rowBytes);	/* transparent */
    if (buffer == NULL) {
        free(ids);
        return NULL;
    }
    space = CGColorSpaceCreateDeviceRGB();
    context = CGBitmapContextCreate(buffer, width, height, 8, rowBytes, space,
                                    kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    if (context != NULL && (imageOption & kCGWindowImageShouldBeOpaque)) {
        CGContextSetRGBFillColor(context, 1, 1, 1, 1);
        CGContextFillRect(context, CGRectMake(0, 0, bounds.size.width, bounds.size.height));
    }
    /* Back to front: the list is front to back. */
    for (i = n; context != NULL && i-- > 0;) {
        SLSWindowInfo info;
        uint8_t *pixels = NULL;
        uint32_t pw, ph, prb;

        if (SLSGetWindowInfo(ids[i], &info) != kSLSErrorSuccess ||
            SLSCopyWindowImage(ids[i], &pixels, &pw, &ph, &prb) !=
            kSLSErrorSuccess)
            continue;
        CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pixels,
            (size_t)prb * ph, release_pixels);
        CGImageRef part = CGImageCreate(pw, ph, 8, 32, prb, space,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little, provider,
            NULL, false, kCGRenderingIntentDefault);
        if (part != NULL) {
            /* The context's origin is at the bottom left. */
            CGRect r = CGRectMake(info.x - bounds.origin.x,
                                  (bounds.origin.y + bounds.size.height) -
                                  (info.y + info.height), info.width, info.height);
            CGContextDrawImage(context, r, part);
            CGImageRelease(part);
        }
        CGDataProviderRelease(provider);
    }
    if (context != NULL) {
        CGContextRelease(context);
        /* The image takes the buffer the windows were drawn into. */
        CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, buffer,
            height * rowBytes, release_pixels);
        image = CGImageCreate(width, height, 8, 32, rowBytes, space,
            kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little, provider, NULL,
            false, kCGRenderingIntentDefault);
        CGDataProviderRelease(provider);
    } else
        free(buffer);
    CGColorSpaceRelease(space);
    free(ids);
    return image;
}

CGImageRef CGWindowListCreateImage(CGRect screenBounds, CGWindowListOption listOption,
                                   CGWindowID windowID, CGWindowImageOption imageOption)
{
    SLSWindowID *ids = malloc(kSLSMaxWindowList * sizeof(*ids));

    if (ids == NULL)
        return NULL;
    return create_image_of_windows(screenBounds, ids, sls_window_ids(listOption, windowID, ids),
                                   imageOption);
}

/* The windows of an array of window numbers (as CGWindowListCreate gives),
   the first one in front. */
CGImageRef CGWindowListCreateImageFromArray(CGRect screenBounds, CFArrayRef windowArray,
                                            CGWindowImageOption imageOption)
{
    CFIndex count = windowArray != NULL ? CFArrayGetCount(windowArray) : 0;
    SLSWindowID *ids;

    if (count <= 0)
        return NULL;
    ids = malloc((size_t)count * sizeof(*ids));
    if (ids == NULL)
        return NULL;
    for (CFIndex j = 0; j < count; j++)
        ids[j] = (SLSWindowID)(uintptr_t)CFArrayGetValueAtIndex(windowArray, j);
    return create_image_of_windows(screenBounds, ids, (uint32_t)count, imageOption);
}

CFArrayRef CGWindowListCreate(CGWindowListOption option, CGWindowID relativeToWindow)
{
    CFMutableArrayRef array;

    SLSWindowID *ids = malloc(kSLSMaxWindowList * sizeof(*ids));
    uint32_t count = ids ? sls_window_ids(option, relativeToWindow, ids) : 0;

    array = CFArrayCreateMutable(NULL, (CFIndex)count, NULL);
    for (uint32_t j = 0; array != NULL && j < count; j++)
        CFArrayAppendValue(array, (const void *)(uintptr_t)ids[j]);
    free(ids);
    return array;
}

CFArrayRef CGWindowListCopyWindowInfo(CGWindowListOption option,
                                      CGWindowID relativeToWindow)
{
    CFMutableArrayRef array;

    SLSWindowID *ids = malloc(kSLSMaxWindowList * sizeof(*ids));
    uint32_t count = ids ? sls_window_ids(option, relativeToWindow, ids) : 0;

    array = CFArrayCreateMutable(NULL, (CFIndex)count, &kCFTypeArrayCallBacks);
    for (uint32_t j = 0; array != NULL && j < count; j++) {
        CFDictionaryRef d = copy_sls_description(ids[j]);
        if (d != NULL) {
            CFArrayAppendValue(array, d);
            CFRelease(d);
        }
    }
    free(ids);
    return array;
}

CFArrayRef CGWindowListCreateDescriptionFromArray(CFArrayRef windowArray)
{
    CFMutableArrayRef array;
    CFIndex i, n;

    if (windowArray == NULL)
        return NULL;
    /* Windows that no longer exist are skipped. */
    n = CFArrayGetCount(windowArray);
    array = CFArrayCreateMutable(NULL, n, &kCFTypeArrayCallBacks);
    for (i = 0; array != NULL && i < n; i++) {
        CFDictionaryRef d = copy_sls_description(
            (SLSWindowID)(uintptr_t)CFArrayGetValueAtIndex(windowArray, i));
        if (d != NULL) {
            CFArrayAppendValue(array, d);
            CFRelease(d);
        }
    }
    return array;
}

/*
 * The connection functions of the window server's private API, as apps
 * call them (a browser's content processes refuse the connection and
 * close it). A window transform needs the compositor to scale or move a
 * window's image, which it does not do yet: only the identity is taken.
 */
typedef int32_t CGSConnectionID;

extern CGSConnectionID _CGSDefaultConnection(void);
extern CGError CGSSetDenyWindowServerConnections(bool deny);
extern void CGSShutdownServerConnections(void);
extern CGError CGSSetWindowTransform(CGSConnectionID cid, CGWindowID wid, CGAffineTransform transform);

CGSConnectionID _CGSDefaultConnection(void)
{
    return SLSMainConnectionID();
}

CGError CGSSetDenyWindowServerConnections(bool deny)
{
    return SLSSetDenyWindowServerConnections(deny);
}

void CGSShutdownServerConnections(void)
{
    SLSShutdownServerConnections();
}

CGError CGSSetWindowTransform(CGSConnectionID cid, CGWindowID wid, CGAffineTransform transform)
{
    SLSWindowInfo info;

    if (SLSGetWindowInfo(wid, &info) != kSLSErrorSuccess)
        return kCGErrorIllegalArgument;
    return CGAffineTransformIsIdentity(transform) ? kCGErrorSuccess : kCGErrorNotImplemented;
}
