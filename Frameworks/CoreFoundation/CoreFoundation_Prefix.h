/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*	CoreFoundation_Prefix.h
	Copyright (c) 2005-2014, Apple Inc. All rights reserved.
*/


#define _DARWIN_UNLIMITED_SELECT 1

#include <CoreFoundation/CFBase.h>


#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if DEPLOYMENT_TARGET_IPHONESIMULATOR // work around <rdar://problem/16507706>
#include <pthread/qos.h>
#define qos_class_self() (QOS_CLASS_UTILITY)
#define qos_class_main() (QOS_CLASS_UTILITY)
#define pthread_set_qos_class_self_np(A, B) do {} while (0)
#define pthread_override_qos_class_start_np(A, B, C) (NULL)
#define pthread_override_qos_class_end_np(A) do {} while (0)
#elif (DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED)
#include <pthread/qos.h>
#endif

#define SystemIntegrityCheck(A, B)	do {} while (0)

    
#if INCLUDE_OBJC
#include <objc/objc.h>
#else
typedef signed char	BOOL; 
typedef char * id;
typedef char * Class;
#ifndef YES
#define YES (BOOL)1
#endif
#ifndef NO
#define NO (BOOL)0
#endif
#ifndef nil
#define nil NULL
#endif
#endif

#define CRSetCrashLogMessage(A) do {} while (0)
#define CRSetCrashLogMessage2(A) do {} while (0)

#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED || DEPLOYMENT_TARGET_EMBEDDED_MINI
#import <libkern/OSAtomic.h>
#import <pthread.h>
#endif

/* This macro creates some helper functions which are useful in dealing with libdispatch:
 *  __ PREFIX Queue -- manages and returns a singleton serial queue
 *
 * Use the macro like this:
 *   DISPATCH_HELPER_FUNCTIONS(fh, NSFileHandle)
 */

#define DISPATCH_HELPER_FUNCTIONS(PREFIX, QNAME)			\
static dispatch_queue_t __ ## PREFIX ## Queue(void) {			\
    static volatile dispatch_queue_t __ ## PREFIX ## dq = NULL;		\
    if (!__ ## PREFIX ## dq) {						\
        dispatch_queue_t dq = dispatch_queue_create("com.apple." # QNAME, NULL); \
        void * volatile *loc = (void * volatile *)&__ ## PREFIX ## dq;	\
        if (!OSAtomicCompareAndSwapPtrBarrier(NULL, dq, loc)) {		\
            dispatch_release(dq);					\
        }								\
    }									\
    return __ ## PREFIX ## dq;						\
}									\


#define LIBAUTO_STUB	1

#ifndef LIBAUTO_STUB

#if DEPLOYMENT_TARGET_MACOSX
#include <auto_zone.h>
#endif
#if DEPLOYMENT_TARGET_MACOSX || DEPLOYMENT_TARGET_EMBEDDED
#include <objc/objc-auto.h>
#endif

#endif // LIBAUTO_STUB

#if !defined(CF_PRIVATE)
#define CF_PRIVATE __attribute__((__visibility__("hidden")))
#endif

#ifdef LIBAUTO_STUB

#include <stddef.h>

/* Stubs for functions in libauto. */

enum {OBJC_GENERATIONAL = (1 << 0)};

enum {
    OBJC_RATIO_COLLECTION        = (0 << 0), 
    OBJC_GENERATIONAL_COLLECTION = (1 << 0),
    OBJC_FULL_COLLECTION         = (2 << 0),
    OBJC_EXHAUSTIVE_COLLECTION   = (3 << 0),
    OBJC_COLLECT_IF_NEEDED       = (1 << 3),
    OBJC_WAIT_UNTIL_DONE         = (1 << 4),
};

    
enum {
    AUTO_TYPE_UNKNOWN = -1,
    AUTO_UNSCANNED = (1 << 0),
    AUTO_OBJECT = (1 << 1),
    AUTO_POINTERS_ONLY = (1 << 2),
    AUTO_MEMORY_SCANNED = !AUTO_UNSCANNED,
    AUTO_MEMORY_UNSCANNED = AUTO_UNSCANNED,
    AUTO_MEMORY_ALL_POINTERS = AUTO_POINTERS_ONLY,
    AUTO_MEMORY_ALL_WEAK_POINTERS = (AUTO_UNSCANNED | AUTO_POINTERS_ONLY),
    AUTO_OBJECT_SCANNED = AUTO_OBJECT,
    AUTO_OBJECT_UNSCANNED = AUTO_OBJECT | AUTO_UNSCANNED, 
    AUTO_OBJECT_ALL_POINTERS = AUTO_OBJECT | AUTO_POINTERS_ONLY
};
typedef unsigned long auto_memory_type_t;
typedef struct _auto_zone_t auto_zone_t;
typedef struct auto_weak_callback_block {
    struct auto_weak_callback_block *next;
    void (*callback_function)(void *arg1, void *arg2);
    void *arg1;
    void *arg2;
} auto_weak_callback_block_t;

CF_INLINE void *objc_memmove_collectable(void *a, const void *b, size_t c) { return memmove(a, b, c); }
CF_INLINE void *objc_collectableZone(void) { return 0; }

CF_INLINE void *auto_zone_allocate_object(void *zone, size_t size, auto_memory_type_t type, int rc, int clear) { return 0; }
CF_INLINE const void *auto_zone_base_pointer(void *zone, const void *ptr) { return 0; }
CF_INLINE void auto_zone_set_scan_exactly(void *zone, void *ptr) {}
CF_INLINE void auto_zone_retain(void *zone, void *ptr) {}
CF_INLINE unsigned int auto_zone_release(void *zone, void *ptr) { return 0; }
CF_INLINE unsigned int auto_zone_retain_count(void *zone, const void *ptr) { return 0; }
CF_INLINE void auto_zone_set_unscanned(void *zone, void *ptr) {}
CF_INLINE void auto_zone_set_nofinalize(void *zone, void *ptr) {}
CF_INLINE int auto_zone_is_finalized(void *zone, const void *ptr) { return 0; }
CF_INLINE size_t auto_zone_size(void *zone, const void *ptr) { return 0; }
CF_INLINE void auto_register_weak_reference(void *zone, const void *referent, void **referrer, uintptr_t *counter, void **listHead, void **listElement) {}
CF_INLINE void auto_unregister_weak_reference(void *zone, const void *referent, void **referrer) {}
CF_INLINE int auto_zone_is_valid_pointer(void *zone, const void *ptr) { return 0; }
CF_INLINE BOOL objc_isAuto(id object) { return 0; }
CF_INLINE void* auto_read_weak_reference(void *zone, void **referrer) { void *result = *referrer; return result; }
CF_INLINE void auto_assign_weak_reference(void *zone, const void *value, const void **location, auto_weak_callback_block_t *block) { *location = (void *)value; }
CF_INLINE auto_memory_type_t auto_zone_get_layout_type(void *zone, void *ptr) { return AUTO_UNSCANNED; }
CF_INLINE int auto_zone_set_write_barrier(void *zone, const void *dest, const void *new_value) { return false; }

CF_INLINE void objc_assertRegisteredThreadWithCollector(void) {}
CF_INLINE void objc_registerThreadWithCollector(void) {}

CF_INLINE uintptr_t _object_getExternalHash(id obj) {
    return (uintptr_t)obj;
}
    
// from objc-auto.h

CF_INLINE BOOL objc_atomicCompareAndSwapPtr(id predicate, id replacement, volatile id *objectLocation) 
{ return OSAtomicCompareAndSwapPtr((void *)predicate, (void *)replacement, (void * volatile *)objectLocation); }

CF_INLINE BOOL objc_atomicCompareAndSwapPtrBarrier(id predicate, id replacement, volatile id *objectLocation) 
{ return OSAtomicCompareAndSwapPtrBarrier((void *)predicate, (void *)replacement, (void * volatile *)objectLocation); }

CF_INLINE BOOL objc_atomicCompareAndSwapGlobal(id predicate, id replacement, volatile id *objectLocation) 
{ return OSAtomicCompareAndSwapPtr((void *)predicate, (void *)replacement, (void * volatile *)objectLocation); }

CF_INLINE BOOL objc_atomicCompareAndSwapGlobalBarrier(id predicate, id replacement, volatile id *objectLocation) 
{ return OSAtomicCompareAndSwapPtrBarrier((void *)predicate, (void *)replacement, (void * volatile *)objectLocation); }

CF_INLINE BOOL objc_atomicCompareAndSwapInstanceVariable(id predicate, id replacement, volatile id *objectLocation) 
{ return OSAtomicCompareAndSwapPtr((void *)predicate, (void *)replacement, (void * volatile *)objectLocation); }

CF_INLINE BOOL objc_atomicCompareAndSwapInstanceVariableBarrier(id predicate, id replacement, volatile id *objectLocation) 
{ return OSAtomicCompareAndSwapPtrBarrier((void *)predicate, (void *)replacement, (void * volatile *)objectLocation); }

CF_INLINE id objc_assign_strongCast(id val, id *dest) 
{ return (*dest = val); }

CF_INLINE id objc_assign_global(id val, id *dest) 
{ return (*dest = val); }

CF_INLINE id objc_assign_ivar(id val, id dest, ptrdiff_t offset) 
{ return (*(id*)((char *)dest+offset) = val); }

//CF_INLINE void *objc_memmove_collectable(void *dst, const void *src, size_t size) { return memmove(dst, src, size); }

CF_INLINE id objc_read_weak(id *location) 
{ return *location; }

CF_INLINE id objc_assign_weak(id value, id *location) 
{ return (*location = value); }


CF_INLINE void objc_finalizeOnMainThread(Class cls) { }
CF_INLINE BOOL objc_is_finalized(void *ptr) { return NO; }
CF_INLINE void objc_clear_stack(unsigned long options) { }

CF_INLINE BOOL objc_collectingEnabled(void) { return NO; }
CF_INLINE void objc_start_collector_thread(void) { }

CF_INLINE void objc_collect(unsigned long options) { }
    
#endif

