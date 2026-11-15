/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CoreMedia's notification center: listeners called, on the poster's
 * thread, with a notification's name, object and payload.
 */

#ifndef __CMNOTIFICATIONCENTER__
#define __CMNOTIFICATIONCENTER__

#include <CoreMedia/CMBase.h>

typedef struct OpaqueCMNotificationCenter *CMNotificationCenterRef;

typedef void (*CMNotificationCallback)(CMNotificationCenterRef notificationCenter, const void *listener,
                                       CFStringRef notificationName, const void *object, CFTypeRef payload);

enum {
    kCMNotificationCenterError_InvalidParameter = -12500,
    kCMNotificationCenterError_ListenerAlreadyRegistered = -12501,
    kCMNotificationCenterError_ListenerNotRegistered = -12502,
};

enum {
    kCMNotificationCenterNoListenerFlags = 0,
};

CM_EXPORT CFTypeID CMNotificationCenterGetTypeID(void);
CM_EXPORT CMNotificationCenterRef CMNotificationCenterGetDefaultLocalCenter(void);
CM_EXPORT OSStatus CMNotificationCenterAddListener(CMNotificationCenterRef notificationCenter, const void *listener,
                                                   CMNotificationCallback callback, CFStringRef notificationName,
                                                   const void *object, UInt32 flags);
CM_EXPORT OSStatus CMNotificationCenterRemoveListener(CMNotificationCenterRef notificationCenter, const void *listener,
                                                      CMNotificationCallback callback, CFStringRef notificationName,
                                                      const void *object);
CM_EXPORT OSStatus CMNotificationCenterBarrier(CMNotificationCenterRef notificationCenter);
CM_EXPORT OSStatus CMNotificationCenterPostNotification(CMNotificationCenterRef notificationCenter,
                                                        CFStringRef notificationName, const void *notifyingObject,
                                                        CFTypeRef payload);

#endif
