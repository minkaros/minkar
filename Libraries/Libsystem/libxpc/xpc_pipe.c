/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * XPC pipes: a request and its reply over Mach, to a service found by
 * its name (libinfo asks the directory service this way). A pipe is a
 * dictionary holding the service's send right, so releasing the pipe
 * releases the right. Each routine sends the request with a reply port
 * of its own and waits on it; the reply carries the replier's audit
 * token (xpc_dictionary_get_audit_token).
 *
 * A server receives with xpc_pipe_receive_request and answers with
 * xpc_pipe_send_reply (Minkar's own calls, in xpc/launchd.h).
 */

#include <errno.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "xpc/launchd.h"
#include "xpc_internal.h"

extern kern_return_t bootstrap_look_up2(mach_port_t bp, const name_t service_name, mach_port_t *sp,
    pid_t target_pid, uint64_t flags);
#define BOOTSTRAP_PRIVILEGED_SERVER (1 << 3)

static const char kPortKey[] = "pipe-port";
static _Atomic uint64_t next_id = 1;

xpc_pipe_t
xpc_pipe_create_from_port(mach_port_t port, uint64_t flags)
{
	xpc_object_t pipe;

	(void)flags;
	if (!MACH_PORT_VALID(port))
		return NULL;
	pipe = xpc_dictionary_create(NULL, NULL, 0);
	if (pipe != NULL)
		xpc_dictionary_set_mach_send(pipe, kPortKey, port);
	return pipe;
}

xpc_pipe_t
xpc_pipe_create(const char *name, uint64_t flags)
{
	mach_port_t port = MACH_PORT_NULL;
	kern_return_t kr;
	xpc_pipe_t pipe;

	if (name == NULL)
		return NULL;
	if (flags & XPC_PIPE_FLAG_PRIVILEGED)
		kr = bootstrap_look_up2(bootstrap_port, name, &port, 0, BOOTSTRAP_PRIVILEGED_SERVER);
	else
		kr = bootstrap_look_up(bootstrap_port, name, &port);
	if (kr != KERN_SUCCESS)
		return NULL;
	pipe = xpc_pipe_create_from_port(port, flags);
	/* The dictionary holds its own right. */
	mach_port_deallocate(mach_task_self(), port);
	return pipe;
}

void
xpc_pipe_invalidate(xpc_pipe_t pipe)
{
	if (pipe != NULL)
		xpc_dictionary_set_value(pipe, kPortKey, NULL);
}

/* Waits until a message is queued on the port, without taking it. */
static int
wait_for_message(mach_port_t port)
{
	mach_msg_header_t header;
	kern_return_t kr;

	for (;;) {
		kr = mach_msg(&header, MACH_RCV_MSG | MACH_RCV_LARGE, 0, sizeof(header), port,
		    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
		if (kr == MACH_RCV_TOO_LARGE)
			return 0;
		if (kr == MACH_MSG_SUCCESS) {
			/* An empty message (a no-senders notification): not ours. */
			mach_msg_destroy(&header);
			continue;
		}
		if (kr == MACH_RCV_INTERRUPTED)
			continue;
		return EPIPE;
	}
}

int
xpc_pipe_routine(xpc_pipe_t pipe, xpc_object_t msg, xpc_object_t *reply)
{
	mach_port_t service, reply_port = MACH_PORT_NULL, remote = MACH_PORT_NULL;
	struct xpc_credentials creds;
	xpc_object_t result = NULL;
	uint64_t id, got_id = 0;
	int error = 0;

	if (reply != NULL)
		*reply = NULL;
	if (pipe == NULL || msg == NULL || xpc_get_type(msg) != XPC_TYPE_DICTIONARY)
		return EINVAL;
	service = xpc_dictionary_copy_mach_send(pipe, kPortKey);
	if (!MACH_PORT_VALID(service))
		return EPIPE;
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &reply_port) != KERN_SUCCESS) {
		mach_port_deallocate(mach_task_self(), service);
		return ENOMEM;
	}
	id = atomic_fetch_add(&next_id, 1);
	if (xpc_pipe_send(msg, id, reply_port, service) != 0)
		error = errno == EPIPE ? EPIPE : EIO;
	mach_port_deallocate(mach_task_self(), service);
	while (error == 0) {
		error = wait_for_message(reply_port);
		if (error != 0)
			break;
		if (xpc_pipe_receive(reply_port, &remote, &result, &got_id, &creds) < 0) {
			error = EIO;
			break;
		}
		if (MACH_PORT_VALID(remote))
			mach_port_deallocate(mach_task_self(), remote);
		if (got_id == id)
			break;
		/* An answer to another request: not this one's. */
		xpc_release(result);
		result = NULL;
	}
	mach_port_mod_refs(mach_task_self(), reply_port, MACH_PORT_RIGHT_RECEIVE, -1);
	if (error != 0) {
		if (result != NULL)
			xpc_release(result);
		return error;
	}
	if (reply != NULL)
		*reply = result;
	else
		xpc_release(result);
	return 0;
}

int
xpc_pipe_receive_request(mach_port_t port, xpc_object_t *request, mach_port_t *reply_port, uint64_t *id)
{
	struct xpc_credentials creds;
	int error;

	if (request == NULL || reply_port == NULL || id == NULL)
		return EINVAL;
	*request = NULL;
	*reply_port = MACH_PORT_NULL;
	for (;;) {
		if ((error = wait_for_message(port)) != 0)
			return error;
		if (xpc_pipe_receive(port, reply_port, request, id, &creds) >= 0)
			return 0;
		/* A message that is not a pipe request: dropped, wait again. */
	}
}

int
xpc_pipe_send_reply(xpc_object_t reply, mach_port_t reply_port, uint64_t id)
{
	int error = 0;

	if (reply == NULL || !MACH_PORT_VALID(reply_port))
		return EINVAL;
	if (xpc_pipe_send(reply, id, MACH_PORT_NULL, reply_port) != 0)
		error = errno == EPIPE ? EPIPE : EIO;
	mach_port_deallocate(mach_task_self(), reply_port);
	return error;
}
