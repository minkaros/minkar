/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CFNotificationCenter: notifications posted by name to the observers that
 * asked for them, in this process (the local center), between processes
 * (the Darwin notify center, by name only) or, for now, in this process too
 * (the distributed center).
 */

#if !defined(__COREFOUNDATION_CFNOTIFICATIONCENTER__)
#define __COREFOUNDATION_CFNOTIFICATIONCENTER__ 1

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFDictionary.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_EXTERN_C_BEGIN

typedef CFStringRef CFNotificationName;

typedef struct __CFNotificationCenter *CFNotificationCenterRef;

typedef void (*CFNotificationCallback)(CFNotificationCenterRef center, void *observer,
    CFNotificationName name, const void *object, CFDictionaryRef userInfo);

typedef CF_ENUM(CFIndex, CFNotificationSuspensionBehavior) {
    CFNotificationSuspensionBehaviorDrop = 1,
    CFNotificationSuspensionBehaviorCoalesce = 2,
    CFNotificationSuspensionBehaviorHold = 3,
    CFNotificationSuspensionBehaviorDeliverImmediately = 4
};

enum {
    kCFNotificationDeliverImmediately = (1UL << 0),
    kCFNotificationPostToAllSessions = (1UL << 1)
};

CF_EXPORT CFTypeID CFNotificationCenterGetTypeID(void);

CF_EXPORT CFNotificationCenterRef CFNotificationCenterGetLocalCenter(void);
CF_EXPORT CFNotificationCenterRef CFNotificationCenterGetDistributedCenter(void);
CF_EXPORT CFNotificationCenterRef CFNotificationCenterGetDarwinNotifyCenter(void);

CF_EXPORT void CFNotificationCenterAddObserver(CFNotificationCenterRef center, const void *observer,
    CFNotificationCallback callBack, CFStringRef name, const void *object,
    CFNotificationSuspensionBehavior suspensionBehavior);
CF_EXPORT void CFNotificationCenterRemoveObserver(CFNotificationCenterRef center, const void *observer,
    CFNotificationName name, const void *object);
CF_EXPORT void CFNotificationCenterRemoveEveryObserver(CFNotificationCenterRef center, const void *observer);

CF_EXPORT void CFNotificationCenterPostNotification(CFNotificationCenterRef center, CFNotificationName name,
    const void *object, CFDictionaryRef userInfo, Boolean deliverImmediately);
CF_EXPORT void CFNotificationCenterPostNotificationWithOptions(CFNotificationCenterRef center,
    CFNotificationName name, const void *object, CFDictionaryRef userInfo, CFOptionFlags options);

CF_EXTERN_C_END
CF_IMPLICIT_BRIDGING_DISABLED

#endif /* ! __COREFOUNDATION_CFNOTIFICATIONCENTER__ */
