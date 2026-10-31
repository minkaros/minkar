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
 * os/activity.h: activities group the work done for one request, across
 * threads and processes, so logs can tell them apart. Our own, after the
 * public interface: an activity has a number, and the thread's current
 * activity is the one whose scope it is in.
 */

#ifndef __OS_ACTIVITY_H__
#define __OS_ACTIVITY_H__

#include <os/base.h>
#include <os/object.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <Availability.h>

__BEGIN_DECLS

#if OS_OBJECT_USE_OBJC
OS_OBJECT_DECL(os_activity);
#else
typedef struct os_activity_s *os_activity_t;
#endif

typedef uint64_t os_activity_id_t;

typedef struct os_activity_scope_state_s {
	uint64_t opaque[2];
} *os_activity_scope_state_t;

OS_ENUM(os_activity_flag, uint32_t,
	OS_ACTIVITY_FLAG_DEFAULT = 0,
	OS_ACTIVITY_FLAG_DETACHED = 0x1,
	OS_ACTIVITY_FLAG_IF_NONE_PRESENT = 0x2
);

#if !defined(__DISPATCH_BLOCK_T__) && !defined(OS_BLOCK_T)
typedef void (^os_block_t)(void);
#define OS_BLOCK_T 1
#endif
typedef void (*os_function_t)(void *_Nullable);

extern struct os_activity_s _os_activity_none;
extern struct os_activity_s _os_activity_current;

/* No activity; and the thread's current one, as a parent. */
#define OS_ACTIVITY_NONE \
	((os_activity_t)(void *)&_os_activity_none)
#define OS_ACTIVITY_CURRENT \
	((os_activity_t)(void *)&_os_activity_current)

extern void *__dso_handle;

API_AVAILABLE(macos(10.12))
os_activity_t _os_activity_create(void *dso, const char *description,
    os_activity_t parent, os_activity_flag_t flags);
#define os_activity_create(description, parent, flags) \
	_os_activity_create(&__dso_handle, (description), (parent), (flags))

API_AVAILABLE(macos(10.12))
void os_activity_scope_enter(os_activity_t activity,
    os_activity_scope_state_t state);
API_AVAILABLE(macos(10.12))
void os_activity_scope_leave(os_activity_scope_state_t state);

#define os_activity_scope(activity) \
	struct os_activity_scope_state_s __os_activity_state \
	    __attribute__((cleanup(os_activity_scope_leave))); \
	os_activity_scope_enter((activity), &__os_activity_state)

API_AVAILABLE(macos(10.10))
void _os_activity_initiate(void *dso, const char *description,
    os_activity_flag_t flags, os_block_t activity_block);
#define os_activity_initiate(description, flags, activity_block) \
	_os_activity_initiate(&__dso_handle, (description), (flags), \
	    (activity_block))

API_AVAILABLE(macos(10.10))
void _os_activity_initiate_f(void *dso, const char *description,
    os_activity_flag_t flags, void *context, os_function_t function);
#define os_activity_initiate_f(description, flags, context, function) \
	_os_activity_initiate_f(&__dso_handle, (description), (flags), \
	    (context), (function))

API_AVAILABLE(macos(10.12))
void os_activity_apply(os_activity_t activity, os_block_t block);
API_AVAILABLE(macos(10.12))
void os_activity_apply_f(os_activity_t activity, void *context,
    os_function_t function);

API_AVAILABLE(macos(10.12))
os_activity_id_t os_activity_get_identifier(os_activity_t activity,
    os_activity_id_t *parent_id);
API_AVAILABLE(macos(10.10))
os_activity_id_t os_activity_get_active(os_activity_id_t *entries,
    unsigned int *count);

API_AVAILABLE(macos(10.12))
void _os_activity_label_useraction(void *dso, const char *name);
#define os_activity_label_useraction(name) \
	_os_activity_label_useraction(&__dso_handle, (name))

__END_DECLS

#endif /* __OS_ACTIVITY_H__ */
