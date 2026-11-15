/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CoreMedia's notification center. A listener is registered for a name
 * (NULL: every name) and an object (NULL: every object); posting calls the
 * matching ones on the poster's thread, outside the center's lock.
 */

#include "CMNotificationCenter.h"
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    const void *listener;
    CMNotificationCallback callback;
    CFStringRef name;
    const void *object;
} Listener;

struct OpaqueCMNotificationCenter {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    Listener *listeners;
    size_t count, capacity;
};

static CFTypeID typeID;
static const CFRuntimeClass centerClass = { 0, "CMNotificationCenter", NULL, NULL, NULL, NULL, NULL, NULL, NULL };

CFTypeID CMNotificationCenterGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ typeID = _CFRuntimeRegisterClass(&centerClass); });
    return typeID;
}

CMNotificationCenterRef CMNotificationCenterGetDefaultLocalCenter(void) {
    static CMNotificationCenterRef center;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        size_t extra = sizeof(struct OpaqueCMNotificationCenter) - sizeof(CFRuntimeBase);
        center = (CMNotificationCenterRef)_CFRuntimeCreateInstance(NULL, CMNotificationCenterGetTypeID(), extra, NULL);
        pthread_mutex_init(&center->lock, NULL);
    });
    return center;
}

static Boolean sameName(CFStringRef a, CFStringRef b) {
    return a == b || (a != NULL && b != NULL && CFEqual(a, b));
}

OSStatus CMNotificationCenterAddListener(CMNotificationCenterRef center, const void *listener,
                                         CMNotificationCallback callback, CFStringRef name, const void *object,
                                         UInt32 flags) {
    if (center == NULL || callback == NULL)
        return kCMNotificationCenterError_InvalidParameter;
    pthread_mutex_lock(&center->lock);
    for (size_t i = 0; i < center->count; i++) {
        Listener *l = &center->listeners[i];
        if (l->listener == listener && l->callback == callback && sameName(l->name, name) && l->object == object) {
            pthread_mutex_unlock(&center->lock);
            return kCMNotificationCenterError_ListenerAlreadyRegistered;
        }
    }
    if (center->count == center->capacity) {
        size_t capacity = center->capacity > 0 ? center->capacity * 2 : 8;
        Listener *grown = realloc(center->listeners, capacity * sizeof(Listener));
        if (grown == NULL) {
            pthread_mutex_unlock(&center->lock);
            return kCMNotificationCenterError_InvalidParameter;
        }
        center->listeners = grown;
        center->capacity = capacity;
    }
    center->listeners[center->count++] = (Listener){ listener, callback, name != NULL ? CFStringCreateCopy(NULL, name) : NULL,
                                                     object };
    pthread_mutex_unlock(&center->lock);
    return noErr;
}

OSStatus CMNotificationCenterRemoveListener(CMNotificationCenterRef center, const void *listener,
                                            CMNotificationCallback callback, CFStringRef name, const void *object) {
    if (center == NULL || callback == NULL)
        return kCMNotificationCenterError_InvalidParameter;
    pthread_mutex_lock(&center->lock);
    for (size_t i = 0; i < center->count; i++) {
        Listener *l = &center->listeners[i];
        if (l->listener == listener && l->callback == callback && sameName(l->name, name) && l->object == object) {
            if (l->name != NULL)
                CFRelease(l->name);
            center->listeners[i] = center->listeners[--center->count];
            pthread_mutex_unlock(&center->lock);
            return noErr;
        }
    }
    pthread_mutex_unlock(&center->lock);
    return kCMNotificationCenterError_ListenerNotRegistered;
}

/* Posting is synchronous: nothing is pending. */
OSStatus CMNotificationCenterBarrier(CMNotificationCenterRef center) {
    return center != NULL ? noErr : kCMNotificationCenterError_InvalidParameter;
}

OSStatus CMNotificationCenterPostNotification(CMNotificationCenterRef center, CFStringRef name, const void *object,
                                              CFTypeRef payload) {
    if (center == NULL || name == NULL)
        return kCMNotificationCenterError_InvalidParameter;
    pthread_mutex_lock(&center->lock);
    size_t n = 0;
    Listener *matching = malloc((center->count > 0 ? center->count : 1) * sizeof(Listener));
    for (size_t i = 0; i < center->count; i++) {
        Listener *l = &center->listeners[i];
        if ((l->name == NULL || CFEqual(l->name, name)) && (l->object == NULL || l->object == object))
            matching[n++] = *l;
    }
    pthread_mutex_unlock(&center->lock);
    for (size_t i = 0; i < n; i++)
        matching[i].callback(center, matching[i].listener, name, object, payload);
    free(matching);
    return noErr;
}
