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
 * Activities (os/activity.h): each one has a number, unique in the
 * process, and its parent's; a thread is in the scope of one at a time.
 * os_activity_initiate runs a block or function in a new activity. Our
 * own; nothing reads the numbers back yet but os_activity_get_active.
 */

#include <limits.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <objc/NSObject.h>

#include "activity.h"

@interface OS_os_activity : NSObject
{
@public
	os_activity_id_t _identifier;
	os_activity_id_t _parent;
}
@end

struct os_activity_s {
	Class isa;
	os_activity_id_t identifier;
	os_activity_id_t parent;
};

extern const char OS_os_activity_class[] __asm__("_OBJC_CLASS_$_OS_os_activity");
struct os_activity_s _os_activity_none = { (Class)OS_os_activity_class, 0, 0 };
struct os_activity_s _os_activity_current = { (Class)OS_os_activity_class, 0, 0 };

static _Atomic(os_activity_id_t) next_identifier = 1;
static __thread os_activity_id_t thread_activity;
static __thread os_activity_id_t thread_parent;

static int
is_static(os_activity_t a)
{
	return ((void *)a == &_os_activity_none ||
	    (void *)a == &_os_activity_current);
}

static os_activity_id_t
activity_id(os_activity_t a)
{
	if ((void *)a == &_os_activity_current)
		return (thread_activity);
	if ((void *)a == &_os_activity_none || a == NULL)
		return (0);
	return (((OS_os_activity *)a)->_identifier);
}

os_activity_t
_os_activity_create(void *dso, const char *description, os_activity_t parent,
    os_activity_flag_t flags)
{
	OS_os_activity *a;

	(void)dso;
	(void)description;
	a = [OS_os_activity new];
	if (a == nil)
		return (OS_ACTIVITY_NONE);
	a->_identifier = atomic_fetch_add(&next_identifier, 1);
	if ((flags & OS_ACTIVITY_FLAG_DETACHED) == 0)
		a->_parent = activity_id(parent);
	return ((os_activity_t)a);
}

void
os_activity_scope_enter(os_activity_t activity, os_activity_scope_state_t state)
{
	state->opaque[0] = thread_activity;
	state->opaque[1] = thread_parent;
	if ((void *)activity == &_os_activity_current)
		return;
	if ((void *)activity == &_os_activity_none || activity == NULL) {
		thread_activity = 0;
		thread_parent = 0;
		return;
	}
	thread_activity = ((OS_os_activity *)activity)->_identifier;
	thread_parent = ((OS_os_activity *)activity)->_parent;
}

void
os_activity_scope_leave(os_activity_scope_state_t state)
{
	thread_activity = state->opaque[0];
	thread_parent = state->opaque[1];
}

void
_os_activity_initiate_f(void *dso, const char *description,
    os_activity_flag_t flags, void *context, os_function_t function)
{
	struct os_activity_scope_state_s state;
	os_activity_t a;

	if ((flags & OS_ACTIVITY_FLAG_IF_NONE_PRESENT) && thread_activity != 0) {
		function(context);
		return;
	}
	a = _os_activity_create(dso, description, OS_ACTIVITY_CURRENT, flags);
	os_activity_scope_enter(a, &state);
	function(context);
	os_activity_scope_leave(&state);
	if (!is_static(a))
		[(OS_os_activity *)a release];
}

static void
call_block(void *context)
{
	((os_block_t)context)();
}

void
_os_activity_initiate(void *dso, const char *description,
    os_activity_flag_t flags, os_block_t activity_block)
{
	_os_activity_initiate_f(dso, description, flags, (void *)activity_block,
	    call_block);
}

void
os_activity_apply_f(os_activity_t activity, void *context,
    os_function_t function)
{
	struct os_activity_scope_state_s state;

	os_activity_scope_enter(activity, &state);
	function(context);
	os_activity_scope_leave(&state);
}

void
os_activity_apply(os_activity_t activity, os_block_t block)
{
	os_activity_apply_f(activity, (void *)block, call_block);
}

os_activity_id_t
os_activity_get_identifier(os_activity_t activity, os_activity_id_t *parent_id)
{
	if (parent_id != NULL) {
		if ((void *)activity == &_os_activity_current)
			*parent_id = thread_parent;
		else if (is_static(activity) || activity == NULL)
			*parent_id = 0;
		else
			*parent_id = ((OS_os_activity *)activity)->_parent;
	}
	return (activity_id(activity));
}

/* The thread's activity, then its parent, as far as they go. */
os_activity_id_t
os_activity_get_active(os_activity_id_t *entries, unsigned int *count)
{
	unsigned int n = 0;

	if (entries != NULL && count != NULL && *count > 0) {
		if (thread_activity != 0)
			entries[n++] = thread_activity;
		if (thread_parent != 0 && n < *count)
			entries[n++] = thread_parent;
		*count = n;
	} else if (count != NULL)
		*count = thread_activity != 0;
	return (thread_activity);
}

void
_os_activity_label_useraction(void *dso, const char *name)
{
	(void)dso;
	(void)name;
}

@implementation OS_os_activity
/* The two static activities live for the process. */
- (instancetype)retain
{
	if (is_static((os_activity_t)self))
		return (self);
	return ([super retain]);
}

- (oneway void)release
{
	if (!is_static((os_activity_t)self))
		[super release];
}

- (NSUInteger)retainCount
{
	if (is_static((os_activity_t)self))
		return (NSUIntegerMax);
	return ([super retainCount]);
}
@end
