/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * XPC activities: work the system runs later, once or at an interval,
 * under the criteria given in a dictionary.
 */

#ifndef __XPC_ACTIVITY_H__
#define __XPC_ACTIVITY_H__

#ifndef __XPC_INDIRECT__
#error "Include <xpc/xpc.h>, not <xpc/activity.h>."
#endif
#include <xpc/base.h>

#ifdef __BLOCKS__

__BEGIN_DECLS

/* Criteria keys and values. */
XPC_EXPORT const char *XPC_ACTIVITY_INTERVAL;		/* int64, seconds */
XPC_EXPORT const char *XPC_ACTIVITY_REPEATING;		/* bool */
XPC_EXPORT const char *XPC_ACTIVITY_DELAY;		/* int64, seconds */
XPC_EXPORT const char *XPC_ACTIVITY_GRACE_PERIOD;	/* int64, seconds */
XPC_EXPORT const int64_t XPC_ACTIVITY_INTERVAL_1_MIN;
XPC_EXPORT const int64_t XPC_ACTIVITY_INTERVAL_5_MIN;
XPC_EXPORT const int64_t XPC_ACTIVITY_INTERVAL_15_MIN;
XPC_EXPORT const int64_t XPC_ACTIVITY_INTERVAL_30_MIN;
XPC_EXPORT const int64_t XPC_ACTIVITY_INTERVAL_1_HOUR;
XPC_EXPORT const int64_t XPC_ACTIVITY_INTERVAL_4_HOURS;
XPC_EXPORT const int64_t XPC_ACTIVITY_INTERVAL_8_HOURS;
XPC_EXPORT const int64_t XPC_ACTIVITY_INTERVAL_1_DAY;
XPC_EXPORT const int64_t XPC_ACTIVITY_INTERVAL_7_DAYS;
XPC_EXPORT const char *XPC_ACTIVITY_PRIORITY;		/* string, below */
XPC_EXPORT const char *XPC_ACTIVITY_PRIORITY_MAINTENANCE;
XPC_EXPORT const char *XPC_ACTIVITY_PRIORITY_UTILITY;
XPC_EXPORT const char *XPC_ACTIVITY_ALLOW_BATTERY;	/* bool */
XPC_EXPORT const char *XPC_ACTIVITY_REQUIRE_SCREEN_SLEEP; /* bool */
XPC_EXPORT const char *XPC_ACTIVITY_REQUIRE_BATTERY_LEVEL; /* int64 */
XPC_EXPORT const char *XPC_ACTIVITY_REQUIRE_HDD_SPINNING; /* bool */

/* As the criteria: the activity keeps those it was registered with. */
XPC_EXPORT const xpc_object_t XPC_ACTIVITY_CHECK_IN;

#define XPC_TYPE_ACTIVITY (&_xpc_type_activity)
XPC_EXPORT XPC_TYPE(_xpc_type_activity);

XPC_DECL(xpc_activity);

typedef void (^xpc_activity_handler_t)(xpc_activity_t activity);

typedef long xpc_activity_state_t;
enum {
	XPC_ACTIVITY_STATE_CHECK_IN,
	XPC_ACTIVITY_STATE_WAIT,
	XPC_ACTIVITY_STATE_RUN,
	XPC_ACTIVITY_STATE_DEFER,
	XPC_ACTIVITY_STATE_CONTINUE,
	XPC_ACTIVITY_STATE_DONE,
};

XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL3
void xpc_activity_register(const char *identifier, xpc_object_t criteria,
    xpc_activity_handler_t handler);
XPC_EXPORT XPC_NONNULL1
void xpc_activity_unregister(const char *identifier);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_object_t xpc_activity_copy_criteria(xpc_activity_t activity);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_activity_set_criteria(xpc_activity_t activity, xpc_object_t criteria);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
xpc_activity_state_t xpc_activity_get_state(xpc_activity_t activity);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
bool xpc_activity_set_state(xpc_activity_t activity,
    xpc_activity_state_t state);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
bool xpc_activity_should_defer(xpc_activity_t activity);

__END_DECLS

#endif /* __BLOCKS__ */

#endif /* __XPC_ACTIVITY_H__ */
