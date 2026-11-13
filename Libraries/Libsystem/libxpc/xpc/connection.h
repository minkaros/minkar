/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * XPC connections: a channel to a named service (or its listener), whose
 * messages are dictionaries delivered on a dispatch queue.
 */

#ifndef __XPC_CONNECTION_H__
#define __XPC_CONNECTION_H__

#ifndef __XPC_INDIRECT__
#error "Include <xpc/xpc.h>, not <xpc/connection.h>."
#endif
#include <xpc/base.h>

#ifndef __BLOCKS__
#error "XPC connections need blocks (-fblocks)."
#endif

__BEGIN_DECLS

/* Errors a connection's event handler receives, as dictionaries. */
#define XPC_ERROR_CONNECTION_INTERRUPTED \
	XPC_GLOBAL_OBJECT(_xpc_error_connection_interrupted)
#define XPC_ERROR_CONNECTION_INVALID \
	XPC_GLOBAL_OBJECT(_xpc_error_connection_invalid)
#define XPC_ERROR_TERMINATION_IMMINENT \
	XPC_GLOBAL_OBJECT(_xpc_error_termination_imminent)
XPC_EXPORT const struct _xpc_dictionary_s _xpc_error_connection_interrupted;
XPC_EXPORT const struct _xpc_dictionary_s _xpc_error_connection_invalid;
XPC_EXPORT const struct _xpc_dictionary_s _xpc_error_termination_imminent;

/* xpc_connection_create_mach_service() flags. */
#define XPC_CONNECTION_MACH_SERVICE_LISTENER	(1 << 0)
#define XPC_CONNECTION_MACH_SERVICE_PRIVILEGED	(1 << 1)

typedef void (*xpc_finalizer_t)(void *value);

/* Creating. A new connection is suspended until xpc_connection_resume(). */

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_connection_t xpc_connection_create(const char *name,
    dispatch_queue_t targetq);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_connection_t xpc_connection_create_mach_service(const char *name,
    dispatch_queue_t targetq, uint64_t flags);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_connection_t xpc_connection_create_from_endpoint(xpc_endpoint_t endpoint);

/* Configuring. */

XPC_EXPORT XPC_NONNULL1
void xpc_connection_set_target_queue(xpc_connection_t connection,
    dispatch_queue_t targetq);
XPC_EXPORT XPC_NONNULL_ALL
void xpc_connection_set_event_handler(xpc_connection_t connection,
    xpc_handler_t handler);
XPC_EXPORT XPC_NONNULL1
void xpc_connection_set_context(xpc_connection_t connection, void *context);
XPC_EXPORT XPC_NONNULL_ALL XPC_WARN_RESULT
void *xpc_connection_get_context(xpc_connection_t connection);
XPC_EXPORT XPC_NONNULL1
void xpc_connection_set_finalizer_f(xpc_connection_t connection,
    xpc_finalizer_t finalizer);

/* Running. */

XPC_EXPORT XPC_NONNULL_ALL
void xpc_connection_suspend(xpc_connection_t connection);
XPC_EXPORT XPC_NONNULL_ALL
void xpc_connection_resume(xpc_connection_t connection);
XPC_EXPORT XPC_NONNULL_ALL
void xpc_connection_cancel(xpc_connection_t connection);

/* Messages. */

XPC_EXPORT XPC_NONNULL_ALL
void xpc_connection_send_message(xpc_connection_t connection,
    xpc_object_t message);
XPC_EXPORT XPC_NONNULL_ALL
void xpc_connection_send_barrier(xpc_connection_t connection,
    dispatch_block_t barrier);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL4
void xpc_connection_send_message_with_reply(xpc_connection_t connection,
    xpc_object_t message, dispatch_queue_t replyq, xpc_handler_t handler);
XPC_EXPORT XPC_NONNULL_ALL XPC_WARN_RESULT XPC_RETURNS_RETAINED
xpc_object_t xpc_connection_send_message_with_reply_sync(
    xpc_connection_t connection, xpc_object_t message);

/* The peer. */

XPC_EXPORT XPC_NONNULL_ALL XPC_WARN_RESULT
const char *xpc_connection_get_name(xpc_connection_t connection);
XPC_EXPORT XPC_NONNULL_ALL XPC_WARN_RESULT
uid_t xpc_connection_get_euid(xpc_connection_t connection);
XPC_EXPORT XPC_NONNULL_ALL XPC_WARN_RESULT
gid_t xpc_connection_get_egid(xpc_connection_t connection);
XPC_EXPORT XPC_NONNULL_ALL XPC_WARN_RESULT
pid_t xpc_connection_get_pid(xpc_connection_t connection);
XPC_EXPORT XPC_NONNULL_ALL XPC_WARN_RESULT
au_asid_t xpc_connection_get_asid(xpc_connection_t connection);

__END_DECLS

#endif /* __XPC_CONNECTION_H__ */
