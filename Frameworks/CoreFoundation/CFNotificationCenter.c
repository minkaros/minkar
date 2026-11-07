/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CFNotificationCenter. Each center keeps its observers (observer, callback,
 * name or any, object or any) under a lock and calls the matching ones on
 * the posting thread, outside the lock, so a callback may add or remove
 * observers or post again.
 *
 * The local center is this process's. The Darwin notify center goes
 * through notifyd (notify_post, notify_register_dispatch): names only,
 * no object or user info, callbacks on a private serial queue, one
 * registration per observer. The
 * distributed center delivers in this process only until Minkar has a
 * distributed notification server; its objects are strings.
 */

#include <CoreFoundation/CFNotificationCenter.h>
#include <CoreFoundation/CFString.h>
#include "CFRuntime.h"
#include "CFInternal.h"
#include <dispatch/dispatch.h>
#include <notify.h>
#include <os/lock.h>
#include <stdlib.h>
#include <string.h>

enum { kLocalCenter, kDistributedCenter, kDarwinCenter };

typedef struct {
    const void *observer;
    CFNotificationCallback callBack;
    CFStringRef name;           /* NULL: every name */
    const void *object;         /* NULL: any object */
    int token;                  /* Darwin center: notify_register_dispatch's */
} __CFObservation;

struct __CFNotificationCenter {
    CFRuntimeBase _base;
    int _kind;
    os_unfair_lock _lock;
    __CFObservation *_observations;
    CFIndex _count, _capacity;
    dispatch_queue_t _queue;    /* Darwin center: where callbacks run */
};

static CFTypeID __kCFNotificationCenterTypeID = _kCFRuntimeNotATypeID;

static CFStringRef __CFNotificationCenterCopyDescription(CFTypeRef cf) {
    static const char *const kinds[] = { "local", "distributed", "Darwin notify" };
    CFNotificationCenterRef center = (CFNotificationCenterRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorSystemDefault, NULL,
        CFSTR("<CFNotificationCenter %p [%s]>"), cf, kinds[center->_kind]);
}

static const CFRuntimeClass __CFNotificationCenterClass = {
    0,
    "CFNotificationCenter",
    NULL,      // init
    NULL,      // copy
    NULL,      // finalize: the centers live as long as the process
    NULL,      // equal
    NULL,      // hash
    NULL,      //
    __CFNotificationCenterCopyDescription
};

CFTypeID CFNotificationCenterGetTypeID(void) {
    static dispatch_once_t initOnce;
    dispatch_once(&initOnce, ^{
        __kCFNotificationCenterTypeID = _CFRuntimeRegisterClass(&__CFNotificationCenterClass);
    });
    return __kCFNotificationCenterTypeID;
}

static CFNotificationCenterRef __CFNotificationCenterCreate(int kind) {
    CFNotificationCenterRef center = (CFNotificationCenterRef)_CFRuntimeCreateInstance(
        kCFAllocatorSystemDefault, CFNotificationCenterGetTypeID(),
        sizeof(struct __CFNotificationCenter) - sizeof(CFRuntimeBase), NULL);
    if (center == NULL)
        return NULL;
    center->_kind = kind;
    center->_lock = OS_UNFAIR_LOCK_INIT;
    center->_observations = NULL;
    center->_count = center->_capacity = 0;
    center->_queue = kind == kDarwinCenter
        ? dispatch_queue_create("org.minkar.CFNotificationCenter.darwin", DISPATCH_QUEUE_SERIAL) : NULL;
    return center;
}

CFNotificationCenterRef CFNotificationCenterGetLocalCenter(void) {
    static CFNotificationCenterRef center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ center = __CFNotificationCenterCreate(kLocalCenter); });
    return center;
}

CFNotificationCenterRef CFNotificationCenterGetDistributedCenter(void) {
    static CFNotificationCenterRef center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ center = __CFNotificationCenterCreate(kDistributedCenter); });
    return center;
}

CFNotificationCenterRef CFNotificationCenterGetDarwinNotifyCenter(void) {
    static CFNotificationCenterRef center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ center = __CFNotificationCenterCreate(kDarwinCenter); });
    return center;
}

/* A copy of a CFString name as a C string, for notifyd (NULL if none). */
static char *__CFNotificationCopyCName(CFStringRef name) {
    if (name == NULL)
        return NULL;
    CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(name), kCFStringEncodingUTF8) + 1;
    char *buffer = malloc(size);
    if (buffer != NULL && !CFStringGetCString(name, buffer, size, kCFStringEncodingUTF8)) {
        free(buffer);
        buffer = NULL;
    }
    return buffer;
}

static void __CFNotificationCenterDeliver(CFNotificationCenterRef center, CFStringRef name,
    const void *object, CFDictionaryRef userInfo);

void CFNotificationCenterAddObserver(CFNotificationCenterRef center, const void *observer,
    CFNotificationCallback callBack, CFStringRef name, const void *object,
    CFNotificationSuspensionBehavior suspensionBehavior) {
    (void)suspensionBehavior; /* nothing here is ever suspended */
    if (center == NULL || callBack == NULL)
        return;
    if (center->_kind == kDarwinCenter && name == NULL)
        return; /* the Darwin center has no "every name" */

    __CFObservation entry = { observer, callBack, name ? CFStringCreateCopy(kCFAllocatorSystemDefault, name) : NULL,
        center->_kind == kDarwinCenter ? NULL : object, NOTIFY_TOKEN_INVALID };
    if (center->_kind == kDistributedCenter && object != NULL)
        entry.object = CFStringCreateCopy(kCFAllocatorSystemDefault, (CFStringRef)object);

    if (center->_kind == kDarwinCenter) {
        /* One registration each: a post calls this observer once (its
           block stops when the registration is cancelled). */
        char *cname = __CFNotificationCopyCName(name);
        CFStringRef blockName = entry.name;
        if (cname == NULL || notify_register_dispatch(cname, &entry.token, center->_queue, ^(int token) {
                (void)token;
                callBack(center, (void *)observer, blockName, NULL, NULL);
            }) != NOTIFY_STATUS_OK) {
            free(cname);
            if (entry.name) CFRelease(entry.name);
            return;
        }
        free(cname);
        CFRetain(blockName); /* the block's, for as long as it may run */
    }

    os_unfair_lock_lock(&center->_lock);
    if (center->_count == center->_capacity) {
        CFIndex capacity = center->_capacity ? 2 * center->_capacity : 8;
        __CFObservation *grown = realloc(center->_observations, capacity * sizeof(__CFObservation));
        if (grown == NULL) {
            os_unfair_lock_unlock(&center->_lock);
            if (entry.token != NOTIFY_TOKEN_INVALID) notify_cancel(entry.token);
            if (entry.name) CFRelease(entry.name);
            return;
        }
        center->_observations = grown;
        center->_capacity = capacity;
    }
    center->_observations[center->_count++] = entry;
    os_unfair_lock_unlock(&center->_lock);
}

/* Removes the observer's entries that match name and object (NULL: any). */
static void __CFNotificationCenterRemove(CFNotificationCenterRef center, const void *observer,
    CFStringRef name, const void *object, Boolean every) {
    if (center == NULL)
        return;
    os_unfair_lock_lock(&center->_lock);
    CFIndex kept = 0;
    for (CFIndex i = 0; i < center->_count; i++) {
        __CFObservation *entry = &center->_observations[i];
        Boolean match = entry->observer == observer &&
            (every || name == NULL || (entry->name != NULL && CFEqual(entry->name, name))) &&
            (every || object == NULL || entry->object == object ||
             (center->_kind == kDistributedCenter && entry->object != NULL && CFEqual(entry->object, object)));
        if (match) {
            if (entry->token != NOTIFY_TOKEN_INVALID)
                notify_cancel(entry->token);
            if (entry->name) CFRelease(entry->name);
            if (center->_kind == kDistributedCenter && entry->object) CFRelease(entry->object);
        } else
            center->_observations[kept++] = *entry;
    }
    center->_count = kept;
    os_unfair_lock_unlock(&center->_lock);
}

void CFNotificationCenterRemoveObserver(CFNotificationCenterRef center, const void *observer,
    CFNotificationName name, const void *object) {
    __CFNotificationCenterRemove(center, observer, name, object, false);
}

void CFNotificationCenterRemoveEveryObserver(CFNotificationCenterRef center, const void *observer) {
    __CFNotificationCenterRemove(center, observer, NULL, NULL, true);
}

/* Calls the observers matching name and object, outside the lock. */
static void __CFNotificationCenterDeliver(CFNotificationCenterRef center, CFStringRef name,
    const void *object, CFDictionaryRef userInfo) {
    os_unfair_lock_lock(&center->_lock);
    CFIndex count = 0;
    __CFObservation *matches = center->_count ? malloc(center->_count * sizeof(__CFObservation)) : NULL;
    for (CFIndex i = 0; matches != NULL && i < center->_count; i++) {
        __CFObservation *entry = &center->_observations[i];
        if (entry->name != NULL && !CFEqual(entry->name, name))
            continue;
        if (entry->object != NULL && entry->object != object &&
            !(center->_kind == kDistributedCenter && object != NULL && CFEqual(entry->object, object)))
            continue;
        matches[count++] = *entry;
    }
    os_unfair_lock_unlock(&center->_lock);

    for (CFIndex i = 0; i < count; i++)
        matches[i].callBack(center, (void *)matches[i].observer, name, object, userInfo);
    free(matches);
}

void CFNotificationCenterPostNotificationWithOptions(CFNotificationCenterRef center,
    CFNotificationName name, const void *object, CFDictionaryRef userInfo, CFOptionFlags options) {
    (void)options;
    if (center == NULL || name == NULL)
        return;
    if (center->_kind == kDarwinCenter) {
        /* Every process's observers hear it through notifyd, this one's too. */
        char *cname = __CFNotificationCopyCName(name);
        if (cname != NULL)
            notify_post(cname);
        free(cname);
        return;
    }
    __CFNotificationCenterDeliver(center, name, object, userInfo);
}

void CFNotificationCenterPostNotification(CFNotificationCenterRef center, CFNotificationName name,
    const void *object, CFDictionaryRef userInfo, Boolean deliverImmediately) {
    CFNotificationCenterPostNotificationWithOptions(center, name, object, userInfo,
        deliverImmediately ? kCFNotificationDeliverImmediately : 0);
}
