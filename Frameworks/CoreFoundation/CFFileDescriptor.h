/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CFFileDescriptor: a file descriptor watched by a run loop. Its callback
 * runs on the run loop's thread when the descriptor is readable or
 * writable, once for each time those callbacks are enabled.
 */

#if !defined(__COREFOUNDATION_CFFILEDESCRIPTOR__)
#define __COREFOUNDATION_CFFILEDESCRIPTOR__ 1

#include <CoreFoundation/CFRunLoop.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_EXTERN_C_BEGIN

typedef int CFFileDescriptorNativeDescriptor;

typedef struct __CFFileDescriptor *CFFileDescriptorRef;

enum {
    kCFFileDescriptorReadCallBack = 1UL << 0,
    kCFFileDescriptorWriteCallBack = 1UL << 1
};

typedef void (*CFFileDescriptorCallBack)(CFFileDescriptorRef f, CFOptionFlags callBackTypes, void *info);

typedef struct {
    CFIndex version;
    void *info;
    void *(*retain)(void *info);
    void (*release)(void *info);
    CFStringRef (*copyDescription)(void *info);
} CFFileDescriptorContext;

CF_EXPORT CFTypeID CFFileDescriptorGetTypeID(void);

CF_EXPORT CFFileDescriptorRef CFFileDescriptorCreate(CFAllocatorRef allocator, CFFileDescriptorNativeDescriptor fd,
                                                     Boolean closeOnInvalidate, CFFileDescriptorCallBack callout,
                                                     const CFFileDescriptorContext *context);

CF_EXPORT CFFileDescriptorNativeDescriptor CFFileDescriptorGetNativeDescriptor(CFFileDescriptorRef f);
CF_EXPORT void CFFileDescriptorGetContext(CFFileDescriptorRef f, CFFileDescriptorContext *context);

/* The callbacks wanted next; each runs once, then is disabled again. */
CF_EXPORT void CFFileDescriptorEnableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes);
CF_EXPORT void CFFileDescriptorDisableCallBacks(CFFileDescriptorRef f, CFOptionFlags callBackTypes);

CF_EXPORT void CFFileDescriptorInvalidate(CFFileDescriptorRef f);
CF_EXPORT Boolean CFFileDescriptorIsValid(CFFileDescriptorRef f);

CF_EXPORT CFRunLoopSourceRef CFFileDescriptorCreateRunLoopSource(CFAllocatorRef allocator, CFFileDescriptorRef f,
                                                                CFIndex order);

CF_EXTERN_C_END
CF_IMPLICIT_BRIDGING_DISABLED

#endif /* ! __COREFOUNDATION_CFFILEDESCRIPTOR__ */
