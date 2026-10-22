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
 * Quartz Window Services: window lists.
 *
 * WindowServer reports every window front-most first (kWSWindowList);
 * the CGWindowListOption filtering happens here (CGWindowListFilter.c).
 */

#include <CoreGraphics/CGWindow.h>
#include <CoreGraphics/CGDirectDisplay.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <string.h>

#include "CGWindowListFilter.h"

kern_return_t _windowServerRPC(void *data, size_t len, void *replyBuf, int *replyLen);

_Static_assert(kCGWindowListOptionOnScreenOnly == _CGWLOptionOnScreenOnly &&
               kCGWindowListOptionOnScreenAboveWindow == _CGWLOptionOnScreenAboveWindow &&
               kCGWindowListOptionOnScreenBelowWindow == _CGWLOptionOnScreenBelowWindow &&
               kCGWindowListOptionIncludingWindow == _CGWLOptionIncludingWindow &&
               kCGWindowListExcludeDesktopElements == _CGWLExcludeDesktopElements,
               "CGWindowListOption values out of sync");

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

/* Largest reply WindowServer can send inline. */
#define WINDOW_LIST_REPLY_MAX   (128 * 1024)

/*
 * Fetch the full window list into a malloc'ed array (*entries may be
 * NULL when there are no windows). Returns false if WindowServer could
 * not be asked.
 */
static bool fetch_window_list(struct wsWindowListEntry **entries, size_t *count)
{
    struct wsRPCBase request = { kWSWindowList, 0 };
    struct wsRPCBase *reply;
    int replyLen = WINDOW_LIST_REPLY_MAX;
    void *buf;      /* malloc'ed, so suitably aligned for the reply */

    *entries = NULL;
    *count = 0;
    buf = malloc(WINDOW_LIST_REPLY_MAX);
    if (buf == NULL)
        return false;

    if (_windowServerRPC(&request, sizeof(request), buf, &replyLen) != KERN_SUCCESS ||
        replyLen < (int)sizeof(struct wsRPCBase)) {
        free(buf);
        return false;
    }

    reply = buf;
    if (reply->code != kWSWindowList ||
        reply->len > (uint32_t)replyLen - sizeof(struct wsRPCBase)) {
        free(buf);
        return false;
    }

    *count = reply->len / sizeof(struct wsWindowListEntry);
    if (*count > 0) {
        *entries = malloc(*count * sizeof(**entries));
        if (*entries == NULL) {
            free(buf);
            *count = 0;
            return false;
        }
        memcpy(*entries, (uint8_t *)buf + sizeof(struct wsRPCBase),
               *count * sizeof(**entries));
    }
    free(buf);
    return true;
}

static CFDictionaryRef copy_description(const struct wsWindowListEntry *e)
{
    const void *keys[8], *values[8];
    int32_t number = (int32_t)e->windowID;
    int32_t pid = (int32_t)e->ownerPID;
    int32_t layer = CGWindowLevelForKey((CGWindowLevelKey)e->levelKey);
    int32_t store = kCGBackingStoreBuffered;
    int32_t sharing = kCGWindowSharingNone;
    int64_t memory = (int64_t)(e->w * e->h * 4);
    double alpha = 1.0;
    CGRect bounds = CGRectMake(e->x, e->y, e->w, e->h);
    CFMutableDictionaryRef dict;
    int i, n = 0;

    keys[n] = kCGWindowNumber;
    values[n++] = CFNumberCreate(NULL, kCFNumberSInt32Type, &number);
    keys[n] = kCGWindowOwnerPID;
    values[n++] = CFNumberCreate(NULL, kCFNumberSInt32Type, &pid);
    keys[n] = kCGWindowLayer;
    values[n++] = CFNumberCreate(NULL, kCFNumberSInt32Type, &layer);
    keys[n] = kCGWindowStoreType;
    values[n++] = CFNumberCreate(NULL, kCFNumberSInt32Type, &store);
    keys[n] = kCGWindowSharingState;
    values[n++] = CFNumberCreate(NULL, kCFNumberSInt32Type, &sharing);
    keys[n] = kCGWindowMemoryUsage;
    values[n++] = CFNumberCreate(NULL, kCFNumberSInt64Type, &memory);
    keys[n] = kCGWindowAlpha;
    values[n++] = CFNumberCreate(NULL, kCFNumberDoubleType, &alpha);
    keys[n] = kCGWindowBounds;
    values[n++] = CGRectCreateDictionaryRepresentation(bounds);

    dict = CFDictionaryCreateMutable(NULL, n + 1, &kCFTypeDictionaryKeyCallBacks,
                                     &kCFTypeDictionaryValueCallBacks);
    if (dict != NULL) {
        for (i = 0; i < n; i++) {
            if (values[i] != NULL)
                CFDictionarySetValue(dict, keys[i], values[i]);
        }
        /* Like macOS, only present for on-screen windows. */
        if (e->flags & kWSWindowListOnScreen)
            CFDictionarySetValue(dict, kCGWindowIsOnscreen, kCFBooleanTrue);
    }
    for (i = 0; i < n; i++) {
        if (values[i] != NULL)
            CFRelease(values[i]);
    }
    return dict;
}

/*
 * Fetch and filter. Returns false if WindowServer could not be asked;
 * otherwise *n windows were selected (entries/indexes are malloc'ed).
 */
static bool select_windows(CGWindowListOption option, CGWindowID relativeToWindow,
                           struct wsWindowListEntry **entries, size_t **indexes,
                           size_t *n)
{
    size_t count;

    *indexes = NULL;
    *n = 0;
    if (!fetch_window_list(entries, &count))
        return false;
    if (count == 0)
        return true;
    *indexes = malloc(count * sizeof(**indexes));
    if (*indexes == NULL) {
        free(*entries);
        *entries = NULL;
        return false;
    }
    *n = _CGWindowListFilter(*entries, count, option, relativeToWindow, *indexes);
    return true;
}

CFArrayRef CGWindowListCreate(CGWindowListOption option, CGWindowID relativeToWindow)
{
    struct wsWindowListEntry *entries;
    size_t *indexes, i, n;
    CFMutableArrayRef array;

    if (!select_windows(option, relativeToWindow, &entries, &indexes, &n))
        return NULL;            /* WindowServer unreachable */

    /* Values are the window IDs themselves, as on macOS. */
    array = CFArrayCreateMutable(NULL, (CFIndex)n, NULL);
    for (i = 0; array != NULL && i < n; i++)
        CFArrayAppendValue(array,
                           (const void *)(uintptr_t)entries[indexes[i]].windowID);
    free(indexes);
    free(entries);
    return array;
}

CFArrayRef CGWindowListCopyWindowInfo(CGWindowListOption option,
                                      CGWindowID relativeToWindow)
{
    struct wsWindowListEntry *entries;
    size_t *indexes, i, n;
    CFMutableArrayRef array;

    if (!select_windows(option, relativeToWindow, &entries, &indexes, &n))
        return NULL;

    array = CFArrayCreateMutable(NULL, (CFIndex)n, &kCFTypeArrayCallBacks);
    for (i = 0; array != NULL && i < n; i++) {
        CFDictionaryRef d = copy_description(&entries[indexes[i]]);

        if (d != NULL) {
            CFArrayAppendValue(array, d);
            CFRelease(d);
        }
    }
    free(indexes);
    free(entries);
    return array;
}

CFArrayRef CGWindowListCreateDescriptionFromArray(CFArrayRef windowArray)
{
    struct wsWindowListEntry *entries;
    CFMutableArrayRef array;
    CFIndex i, n;
    size_t j, count;

    if (windowArray == NULL)
        return NULL;
    if (!fetch_window_list(&entries, &count))
        return NULL;

    n = CFArrayGetCount(windowArray);
    array = CFArrayCreateMutable(NULL, n, &kCFTypeArrayCallBacks);
    for (i = 0; array != NULL && i < n; i++) {
        CGWindowID wid = (CGWindowID)(uintptr_t)CFArrayGetValueAtIndex(windowArray, i);

        /* Windows that no longer exist are skipped. */
        for (j = 0; j < count; j++) {
            if (entries[j].windowID == wid) {
                CFDictionaryRef d = copy_description(&entries[j]);

                if (d != NULL) {
                    CFArrayAppendValue(array, d);
                    CFRelease(d);
                }
                break;
            }
        }
    }
    free(entries);
    return array;
}
