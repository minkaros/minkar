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
 * The parts of the XPC API libxpc lacked: dictionary accessors (file
 * descriptors, dates, doubles, connections), shared memory (a Mach memory
 * entry, carried as a port right), and background activities: a simple
 * scheduler on dispatch timers that honours the criteria's delay, interval
 * and repetition (power, idle and battery criteria are not looked at).
 */

#include <sys/types.h>
#include <mach/mach.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Block.h>
#include "xpc/xpc.h"
#include "xpc/activity.h"
#include "xpc_internal.h"

char *strdup(const char *);

#pragma mark Dictionaries

void
xpc_dictionary_set_fd(xpc_object_t xdict, const char *key, int fd)
{
	xpc_object_t value = xpc_fd_create(fd);

	if (value != NULL) {
		xpc_dictionary_set_value(xdict, key, value);
		xpc_release(value);
	}
}

void
xpc_dictionary_set_date(xpc_object_t xdict, const char *key, int64_t value)
{
	xpc_object_t date = xpc_date_create(value);

	xpc_dictionary_set_value(xdict, key, date);
	xpc_release(date);
}

int64_t
xpc_dictionary_get_date(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);

	return (value != NULL && xpc_get_type(value) == XPC_TYPE_DATE ? xpc_date_get_value(value) : 0);
}

void
xpc_dictionary_set_double(xpc_object_t xdict, const char *key, double value)
{
	xpc_object_t number = xpc_double_create(value);

	xpc_dictionary_set_value(xdict, key, number);
	xpc_release(number);
}

double
xpc_dictionary_get_double(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);

	return (value != NULL && xpc_get_type(value) == XPC_TYPE_DOUBLE ? xpc_double_get_value(value) : 0);
}

/* A listener's endpoint goes in the message; the receiver connects to it. */
void
xpc_dictionary_set_connection(xpc_object_t xdict, const char *key, xpc_connection_t connection)
{
	xpc_endpoint_t endpoint = xpc_endpoint_create(connection);

	if (endpoint != NULL) {
		xpc_dictionary_set_value(xdict, key, endpoint);
		xpc_release(endpoint);
	}
}

xpc_connection_t
xpc_dictionary_create_connection(xpc_object_t xdict, const char *key)
{
	xpc_object_t value = xpc_dictionary_get_value(xdict, key);

	if (value == NULL || xpc_get_type(value) != XPC_TYPE_ENDPOINT)
		return (NULL);
	return (xpc_connection_create_from_endpoint((xpc_endpoint_t)value));
}

xpc_connection_t
xpc_array_create_connection(xpc_object_t xarray, size_t index)
{
	xpc_object_t value = xpc_array_get_value(xarray, index);

	if (value == NULL || xpc_get_type(value) != XPC_TYPE_ENDPOINT)
		return (NULL);
	return (xpc_connection_create_from_endpoint((xpc_endpoint_t)value));
}

/* launchd's event streams are not there yet. */
void
xpc_set_event_stream_handler(const char *stream __unused, dispatch_queue_t targetq __unused,
    xpc_handler_t handler __unused)
{
	MINKAR_STUB();
}

#pragma mark Shared memory

/*
 * A region shared by a memory entry: the object holds a send right to it
 * (an endpoint-like port) and the size; xpc_shmem_map() maps it.
 */
xpc_object_t
xpc_shmem_create(void *region, size_t length)
{
	memory_object_size_t size = length;
	mach_port_t entry = MACH_PORT_NULL;
	xpc_u val;
	struct xpc_object *xo;

	if (mach_make_memory_entry_64(mach_task_self(), &size, (memory_object_offset_t)(uintptr_t)region,
	    VM_PROT_READ | VM_PROT_WRITE, &entry, MACH_PORT_NULL) != KERN_SUCCESS)
		return (NULL);
	val.port = entry;
	xo = _xpc_prim_create(_XPC_TYPE_SHMEM, val, (size_t)size);
	return (xo);
}

size_t
xpc_shmem_map(xpc_object_t xshmem, void **region)
{
	struct xpc_object *xo = xshmem;
	vm_address_t address = 0;

	if (xo == NULL || xo->xo_xpc_type != _XPC_TYPE_SHMEM || region == NULL)
		return (0);
	if (vm_map(mach_task_self(), &address, xo->xo_size, 0, VM_FLAGS_ANYWHERE, xo->xo_port, 0, FALSE,
	    VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE) != KERN_SUCCESS)
		return (0);
	*region = (void *)address;
	return (xo->xo_size);
}

#pragma mark Activities

const char *XPC_ACTIVITY_INTERVAL = "Interval";
const char *XPC_ACTIVITY_REPEATING = "Repeating";
const char *XPC_ACTIVITY_DELAY = "Delay";
const char *XPC_ACTIVITY_GRACE_PERIOD = "GracePeriod";
const int64_t XPC_ACTIVITY_INTERVAL_1_MIN = 60;
const int64_t XPC_ACTIVITY_INTERVAL_5_MIN = 300;
const int64_t XPC_ACTIVITY_INTERVAL_15_MIN = 900;
const int64_t XPC_ACTIVITY_INTERVAL_30_MIN = 1800;
const int64_t XPC_ACTIVITY_INTERVAL_1_HOUR = 3600;
const int64_t XPC_ACTIVITY_INTERVAL_4_HOURS = 14400;
const int64_t XPC_ACTIVITY_INTERVAL_8_HOURS = 28800;
const int64_t XPC_ACTIVITY_INTERVAL_1_DAY = 86400;
const int64_t XPC_ACTIVITY_INTERVAL_7_DAYS = 604800;
const char *XPC_ACTIVITY_PRIORITY = "Priority";
const char *XPC_ACTIVITY_PRIORITY_MAINTENANCE = "Maintenance";
const char *XPC_ACTIVITY_PRIORITY_UTILITY = "Utility";
const char *XPC_ACTIVITY_ALLOW_BATTERY = "AllowBattery";
const char *XPC_ACTIVITY_REQUIRE_SCREEN_SLEEP = "RequireScreenSleep";
const char *XPC_ACTIVITY_REQUIRE_BATTERY_LEVEL = "RequireBatteryLevel";
const char *XPC_ACTIVITY_REQUIRE_HDD_SPINNING = "RequireHDDSpinning";

static struct xpc_object check_in = { .xo_xpc_type = _XPC_TYPE_DICTIONARY, .xo_flags = _XPC_GLOBAL,
    .xo_refcnt = 1, .xo_u = { .dict = { NULL, &check_in.xo_u.dict.tqh_first } } };
const xpc_object_t XPC_ACTIVITY_CHECK_IN = &check_in;

struct xpc_activity {
	char *			identifier;
	xpc_object_t		criteria;
	xpc_activity_handler_t	handler;
	dispatch_source_t	timer;
	xpc_activity_state_t	state;
	struct xpc_activity *	next;
};

static struct xpc_activity *activities;
static pthread_mutex_t activities_lock = PTHREAD_MUTEX_INITIALIZER;

static struct xpc_activity *
activity_find(const char *identifier)
{
	struct xpc_activity *a;

	for (a = activities; a != NULL; a = a->next)
		if (strcmp(a->identifier, identifier) == 0)
			return (a);
	return (NULL);
}

static void
activity_schedule(struct xpc_activity *a)
{
	int64_t delay = xpc_dictionary_get_int64(a->criteria, XPC_ACTIVITY_DELAY);
	int64_t interval = xpc_dictionary_get_int64(a->criteria, XPC_ACTIVITY_INTERVAL);
	bool repeating = xpc_dictionary_get_bool(a->criteria, XPC_ACTIVITY_REPEATING);

	if (a->timer != NULL)
		dispatch_source_cancel(a->timer);
	a->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
	    dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
	dispatch_source_set_timer(a->timer, dispatch_time(DISPATCH_TIME_NOW, delay * NSEC_PER_SEC),
	    repeating && interval > 0 ? (uint64_t)interval * NSEC_PER_SEC : DISPATCH_TIME_FOREVER,
	    NSEC_PER_SEC);
	dispatch_source_set_event_handler(a->timer, ^{
		a->state = XPC_ACTIVITY_STATE_RUN;
		a->handler((xpc_activity_t)a);
		if (a->state == XPC_ACTIVITY_STATE_RUN || a->state == XPC_ACTIVITY_STATE_DONE)
			a->state = XPC_ACTIVITY_STATE_WAIT;
		if (!repeating)
			dispatch_source_cancel(a->timer);
	});
	dispatch_resume(a->timer);
}

void
xpc_activity_register(const char *identifier, xpc_object_t criteria, xpc_activity_handler_t handler)
{
	struct xpc_activity *a;

	if (identifier == NULL || handler == NULL)
		return;
	pthread_mutex_lock(&activities_lock);
	a = activity_find(identifier);
	if (a == NULL) {
		a = calloc(1, sizeof(*a));
		a->identifier = strdup(identifier);
		a->next = activities;
		activities = a;
	}
	if (a->handler != NULL)
		Block_release(a->handler);
	a->handler = Block_copy(handler);
	if (criteria != XPC_ACTIVITY_CHECK_IN && criteria != NULL) {
		if (a->criteria != NULL)
			xpc_release(a->criteria);
		a->criteria = xpc_copy(criteria);
	}
	pthread_mutex_unlock(&activities_lock);

	/* A check-in runs the handler with the state CHECK_IN first. */
	a->state = XPC_ACTIVITY_STATE_CHECK_IN;
	handler((xpc_activity_t)a);
	a->state = XPC_ACTIVITY_STATE_WAIT;
	if (a->criteria != NULL)
		activity_schedule(a);
}

xpc_object_t
xpc_activity_copy_criteria(xpc_activity_t activity)
{
	struct xpc_activity *a = (struct xpc_activity *)activity;

	return (a->criteria ? xpc_copy(a->criteria) : NULL);
}

void
xpc_activity_set_criteria(xpc_activity_t activity, xpc_object_t criteria)
{
	struct xpc_activity *a = (struct xpc_activity *)activity;

	if (a->criteria != NULL)
		xpc_release(a->criteria);
	a->criteria = criteria ? xpc_copy(criteria) : NULL;
	if (a->criteria != NULL && a->state != XPC_ACTIVITY_STATE_CHECK_IN)
		activity_schedule(a);
}

xpc_activity_state_t
xpc_activity_get_state(xpc_activity_t activity)
{
	return (((struct xpc_activity *)activity)->state);
}

bool
xpc_activity_set_state(xpc_activity_t activity, xpc_activity_state_t state)
{
	((struct xpc_activity *)activity)->state = state;
	return (true);
}

bool
xpc_activity_should_defer(xpc_activity_t activity __unused)
{
	return (false);
}

void
xpc_activity_unregister(const char *identifier)
{
	struct xpc_activity **p, *a;

	pthread_mutex_lock(&activities_lock);
	for (p = &activities; (a = *p) != NULL; p = &a->next)
		if (strcmp(a->identifier, identifier) == 0) {
			*p = a->next;
			if (a->timer != NULL)
				dispatch_source_cancel(a->timer);
			break;
		}
	pthread_mutex_unlock(&activities_lock);
}
