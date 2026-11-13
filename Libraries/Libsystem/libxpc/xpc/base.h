/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Attribute and availability macros shared by the XPC headers.
 */

#ifndef __XPC_BASE_H__
#define __XPC_BASE_H__

#include <sys/cdefs.h>

#ifndef __has_include
#define __has_include(x) 0
#endif
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#ifndef __has_extension
#define __has_extension(x) 0
#endif

#ifdef __APPLE__
#include <Availability.h>
#endif
#ifndef __OSX_AVAILABLE_STARTING
#define __OSX_AVAILABLE_STARTING(mac, ios)
#endif
#ifndef __OSX_AVAILABLE_BUT_DEPRECATED
#define __OSX_AVAILABLE_BUT_DEPRECATED(macIntro, macDep, iosIntro, iosDep)
#endif
#define __XPC_IOS_SIMULATOR_AVAILABLE_STARTING(version)

#ifndef __XPC_INDIRECT__
#error "Include <xpc/xpc.h>, not <xpc/base.h>."
#endif

/* xpc_service_main(), the entry point before xpc_main(). */
#if XPC_SERVICE_MAIN_IN_LIBXPC
#define XPC_HOSTING_OLD_MAIN 1
#else
#define XPC_HOSTING_OLD_MAIN 0
#endif

#if defined(__GNUC__) || defined(__clang__)
#define XPC_EXPORT		extern __attribute__((__visibility__("default")))
#define XPC_NOEXPORT		__attribute__((__visibility__("hidden")))
#define XPC_WEAKIMPORT		extern __attribute__((__weak_import__))
#define XPC_CONSTRUCTOR		__attribute__((__constructor__))
#define XPC_NORETURN		__attribute__((__noreturn__))
#define XPC_NOTHROW		__attribute__((__nothrow__))
#define XPC_MALLOC		__attribute__((__malloc__))
#define XPC_PURE		__attribute__((__pure__))
#define XPC_USED		__attribute__((__used__))
#define XPC_UNUSED		__attribute__((__unused__))
#define XPC_PACKED		__attribute__((__packed__))
#define XPC_NOINLINE		__attribute__((__noinline__))
#define XPC_INLINE		static __inline__ __attribute__((__always_inline__))
#define XPC_WARN_RESULT		__attribute__((__warn_unused_result__))
#define XPC_SENTINEL		__attribute__((__sentinel__))
#define XPC_PRINTF(fmt, args)	__attribute__((__format__(__printf__, fmt, args)))
#define XPC_TRANSPARENT_UNION	__attribute__((__transparent_union__))
#define XPC_DEPRECATED(msg)	__attribute__((__deprecated__(msg)))
#define XPC_NONNULL(n)		__attribute__((__nonnull__(n)))
#define XPC_NONNULL1		XPC_NONNULL(1)
#define XPC_NONNULL2		XPC_NONNULL(2)
#define XPC_NONNULL3		XPC_NONNULL(3)
#define XPC_NONNULL4		XPC_NONNULL(4)
#define XPC_NONNULL5		XPC_NONNULL(5)
#define XPC_NONNULL6		XPC_NONNULL(6)
#define XPC_NONNULL7		XPC_NONNULL(7)
#define XPC_NONNULL8		XPC_NONNULL(8)
#define XPC_NONNULL9		XPC_NONNULL(9)
#define XPC_NONNULL10		XPC_NONNULL(10)
#define XPC_NONNULL11		XPC_NONNULL(11)
#define XPC_NONNULL_ALL		__attribute__((__nonnull__))
#if __has_extension(attribute_unavailable_with_message)
#define XPC_UNAVAILABLE(msg)	__attribute__((__unavailable__(msg)))
#else
#define XPC_UNAVAILABLE(msg)	__attribute__((__unavailable__))
#endif
#define _xpc_unreachable()	__builtin_unreachable()
#else
#define XPC_EXPORT		extern
#define XPC_NOEXPORT
#define XPC_WEAKIMPORT		extern
#define XPC_CONSTRUCTOR
#define XPC_NORETURN
#define XPC_NOTHROW
#define XPC_MALLOC
#define XPC_PURE
#define XPC_USED
#define XPC_UNUSED
#define XPC_PACKED
#define XPC_NOINLINE
#define XPC_INLINE		static inline
#define XPC_WARN_RESULT
#define XPC_SENTINEL
#define XPC_PRINTF(fmt, args)
#define XPC_TRANSPARENT_UNION
#define XPC_DEPRECATED(msg)
#define XPC_NONNULL(n)
#define XPC_NONNULL1
#define XPC_NONNULL2
#define XPC_NONNULL3
#define XPC_NONNULL4
#define XPC_NONNULL5
#define XPC_NONNULL6
#define XPC_NONNULL7
#define XPC_NONNULL8
#define XPC_NONNULL9
#define XPC_NONNULL10
#define XPC_NONNULL11
#define XPC_NONNULL_ALL
#define XPC_UNAVAILABLE(msg)
#define _xpc_unreachable()	abort()
#endif

/* Only callable from a debugger. */
#define XPC_DEBUGGER_EXCL	XPC_NOEXPORT XPC_USED

/* Not implemented on this platform. */
#define XPC_NOIMPL		XPC_UNAVAILABLE("not implemented")

/* Ownership of what a function returns or takes, for ARC. */
#if __has_feature(objc_arc)
#define XPC_GIVES_REFERENCE	__strong
#define XPC_UNRETAINED		__unsafe_unretained
#define XPC_BRIDGE(xo)		((__bridge void *)(xo))
#define XPC_BRIDGEREF_BEGIN(xo)	((__bridge_retained void *)(xo))
#define XPC_BRIDGEREF_BEGIN_WITH_REF(xo) ((__bridge void *)(xo))
#define XPC_BRIDGEREF_MIDDLE(xo) ((__bridge id)(xo))
#define XPC_BRIDGEREF_END(xo)	((__bridge_transfer id)(xo))
#else
#define XPC_GIVES_REFERENCE
#define XPC_UNRETAINED
#define XPC_BRIDGE(xo)		(xo)
#define XPC_BRIDGEREF_BEGIN(xo)	(xo)
#define XPC_BRIDGEREF_BEGIN_WITH_REF(xo) (xo)
#define XPC_BRIDGEREF_MIDDLE(xo) (xo)
#define XPC_BRIDGEREF_END(xo)	(xo)
#endif

#endif /* __XPC_BASE_H__ */
