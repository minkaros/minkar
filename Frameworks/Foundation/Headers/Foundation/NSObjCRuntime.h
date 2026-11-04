/*
 * Foundation.framework Objective-C SPI
 * Copyright (C) 2026 Zoe Knox <zoe@pixin.net>
 * Modifications Copyright (c) 2026 The Minkar contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __clang__
#error Unsupported compiler!
#endif

#ifndef _FOUNDATION_OBJCRUNTIME_H
#define _FOUNDATION_OBJCRUNTIME_H

#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <objc/NSObjCRuntime.h>
#include <objc/runtime.h>
#include <CoreFoundation/CFBase.h>
#include <objc/objc.h>
#include "objc_size_alignment.h"

#ifdef __cplusplus
#define FOUNDATION_EXPORT extern "C"
#else
#define FOUNDATION_EXPORT extern
#endif /* __cplusplus */

#define NS_INLINE static inline
#define NS_ROOT_CLASS __attribute__((objc_root_class))
#ifndef NS_ASSUME_NONNULL_BEGIN
#define NS_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#define NS_ASSUME_NONNULL_END _Pragma("clang assume_nonnull end")
#endif
/* Minkar: as CF's, so that Swift sees enums as enums and options as
 * OptionSets; and the annotations that name the API for Swift. */
#ifndef NS_ENUM
#define NS_ENUM(_type, _name) CF_ENUM(_type, _name)
#endif
#ifndef NS_OPTIONS
#define NS_OPTIONS(_type, _name) CF_OPTIONS(_type, _name)
#endif
#ifndef NS_CLOSED_ENUM
#define NS_CLOSED_ENUM(_type, _name) CF_CLOSED_ENUM(_type, _name)
#endif
#ifndef NS_ERROR_ENUM
#define NS_ERROR_ENUM(_type, _name, _domain) \
    enum __attribute__((ns_error_domain(_domain))) _name : _type _name; \
    enum _name : _type
#endif
#ifndef NS_UNAVAILABLE
#define NS_UNAVAILABLE __attribute__((unavailable))
#endif
#ifndef NS_SWIFT_NAME
#define NS_SWIFT_NAME(_name) CF_SWIFT_NAME(_name)
#endif
#ifndef NS_SWIFT_UNAVAILABLE
#define NS_SWIFT_UNAVAILABLE(_msg) CF_SWIFT_UNAVAILABLE(_msg)
#endif
#ifndef NS_REFINED_FOR_SWIFT
#define NS_REFINED_FOR_SWIFT CF_REFINED_FOR_SWIFT
#endif
#ifndef NS_SWIFT_NOTHROW
#define NS_SWIFT_NOTHROW __attribute__((swift_error(none)))
#endif
#ifndef NS_TYPED_ENUM
#define NS_TYPED_ENUM _CF_TYPED_ENUM
#endif
#ifndef NS_TYPED_EXTENSIBLE_ENUM
#define NS_TYPED_EXTENSIBLE_ENUM _CF_TYPED_EXTENSIBLE_ENUM
#endif
#ifndef NS_STRING_ENUM
#define NS_STRING_ENUM _CF_TYPED_ENUM
#endif
#ifndef NS_EXTENSIBLE_STRING_ENUM
#define NS_EXTENSIBLE_STRING_ENUM _CF_TYPED_EXTENSIBLE_ENUM
#endif
#ifndef NS_SWIFT_BRIDGED_TYPEDEF
#define NS_SWIFT_BRIDGED_TYPEDEF CF_SWIFT_BRIDGED_TYPEDEF
#endif
#ifndef NS_RELEASES_ARGUMENT
#define NS_RELEASES_ARGUMENT __attribute__((ns_consumed))
#endif
/* A type Swift should not bridge (it stays the Objective-C class). */
#ifndef NS_NON_BRIDGED
#define NS_NON_BRIDGED(type) type
#endif
#ifndef NS_NOESCAPE
#define NS_NOESCAPE __attribute__((noescape))
#endif
#ifndef NS_DESIGNATED_INITIALIZER
#define NS_DESIGNATED_INITIALIZER __attribute__((objc_designated_initializer))
#endif
#define NS_RETURNS_RETAINED __attribute__((ns_returns_retained))
#define NS_RETURNS_NOT_RETAINED __attribute__((ns_returns_not_retained))

#ifndef CF_RETURNS_RETAINED
#define CF_RETURNS_RETAINED __attribute__((cf_returns_retained))
#endif

/* MIN, MAX and ABS, evaluating each argument once. */
#if !defined(MIN)
#define MIN(A, B) ({ __typeof__(A) __min_a = (A); __typeof__(B) __min_b = (B); __min_a < __min_b ? __min_a : __min_b; })
#endif
#if !defined(MAX)
#define MAX(A, B) ({ __typeof__(A) __max_a = (A); __typeof__(B) __max_b = (B); __max_a < __max_b ? __max_b : __max_a; })
#endif
#if !defined(ABS)
#define ABS(A) ({ __typeof__(A) __abs_a = (A); __abs_a < 0 ? -__abs_a : __abs_a; })
#endif

/* The quality of service of work (NSOperation, NSOperationQueue, NSThread):
 * the same values as dispatch's QOS_CLASS_* classes. */
typedef NS_ENUM(NSInteger, NSQualityOfService) {
    NSQualityOfServiceUserInteractive = 0x21,
    NSQualityOfServiceUserInitiated = 0x19,
    NSQualityOfServiceUtility = 0x11,
    NSQualityOfServiceBackground = 0x09,
    NSQualityOfServiceDefault = -1
};

#if __OBJC__
@class NSString;
const char *NSGetSizeAndAlignment(const char *type, NSUInteger *size, NSUInteger *alignment);
SEL NSSelectorFromString(NSString *selectorName);
Class NSClassFromString(NSString *className);
NSString *NSStringFromSelector(SEL selector);
NSString *NSStringFromClass(Class _class);
#endif /* __OBJC__ */
#endif /* _FOUNDATION_OBJCRUNTIME_H */
