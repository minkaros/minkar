/*
 * Original code Copyright 2014-2015 iXsystems, Inc.
 * Changes for ravynOS Copyright (C) 2026 Zoe Knox
 * All rights reserved
 * Modifications Copyright (c) 2026 The Minkar contributors
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * Minkar: connections rewritten. Each connection delivers its events
 * (messages, replies, errors) one at a time, on a serial queue of its own
 * that targets the queue it was given and starts suspended, until
 * xpc_connection_resume(). A reply is known by a bit of its message id
 * (a message the peer sends could otherwise share the id of a pending
 * call). The death of the other side (its port becomes a dead name) is an
 * error event: XPC_ERROR_CONNECTION_INTERRUPTED for a client of a named
 * service, XPC_ERROR_CONNECTION_INVALID otherwise; xpc_connection_cancel()
 * ends a connection with XPC_ERROR_CONNECTION_INVALID. A connection made
 * with no name is an anonymous listener, reached through an endpoint.
 */

#include <string.h>
#include <stdlib.h>
#include <sys/bsm/audit.h>
#include <bsm/libbsm.h>
#include <errno.h>
#include <xpc/xpc.h>
#include <stdatomic.h>
#include <Block.h>
#include <servers/bootstrap.h>
#include "xpc_internal.h"

char *strdup(const char *);
int asprintf(char **, const char *, ...);
pid_t getpid(void);

#define XPC_REPLY_BIT	(1ULL << 63)
#define XPC_CONNECTION_NEXT_ID(conn) \
	((atomic_fetch_add(&conn->xc_last_id, 1)) & ~XPC_REPLY_BIT)

static void xpc_connection_deliver_error(struct xpc_connection *conn, xpc_object_t error);

static dispatch_queue_t
xpc_default_target(void)
{
	return (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
}

/* A connection's common parts; its event queue starts suspended. */
static struct xpc_connection *
xpc_connection_alloc(const char *name, dispatch_queue_t targetq)
{
	struct xpc_connection *conn;
	char *qname;

	if ((conn = calloc(1, sizeof(struct xpc_connection))) == NULL) {
		errno = ENOMEM;
		return (NULL);
	}
	conn->xc_xpc_type = _XPC_TYPE_CONNECTION;
	conn->xc_refcnt = 1;
	conn->xc_last_id = 1;
	conn->xc_name = name ? strdup(name) : NULL;
	conn->xc_local_port = (xpc_port_t)MACH_PORT_NULL;
	conn->xc_remote_port = (xpc_port_t)MACH_PORT_NULL;
	pthread_mutex_init(&conn->xc_lock, NULL);
	TAILQ_INIT(&conn->xc_peers);
	TAILQ_INIT(&conn->xc_pending);

	asprintf(&qname, "org.minkar.xpc.send.%p", conn);
	conn->xc_send_queue = dispatch_queue_create(qname, NULL);
	free(qname);
	asprintf(&qname, "org.minkar.xpc.events.%p", conn);
	conn->xc_recv_queue = dispatch_queue_create(qname, NULL);
	free(qname);
	conn->xc_target_queue = targetq;
	dispatch_set_target_queue(conn->xc_recv_queue, targetq ? targetq : xpc_default_target());
	dispatch_suspend(conn->xc_recv_queue);
	return (conn);
}

static mach_port_t
xpc_new_receive_port(void)
{
	mach_port_t port = MACH_PORT_NULL;

	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port) != KERN_SUCCESS)
		return (MACH_PORT_NULL);
	return (port);
}

/* Without a name: an anonymous listener. With one: a client of the service. */
xpc_connection_t
xpc_connection_create(const char *name, dispatch_queue_t targetq)
{
	struct xpc_connection *conn;

	if (name != NULL)
		return (xpc_connection_create_mach_service(name, targetq, 0));
	if ((conn = xpc_connection_alloc(NULL, targetq)) == NULL)
		return (NULL);
	conn->xc_flags = XPC_CONNECTION_MACH_SERVICE_LISTENER;
	conn->xc_anonymous = true;
	conn->xc_local_port = (xpc_port_t)xpc_new_receive_port();
	return ((xpc_connection_t)conn);
}

/*
 * A service that is not there is not an error yet: the connection reports
 * XPC_ERROR_CONNECTION_INVALID when it is resumed or used, as the API has.
 */
xpc_connection_t
xpc_connection_create_mach_service(const char *name, dispatch_queue_t targetq,
    uint64_t flags)
{
	struct xpc_transport *transport = xpc_get_transport();
	struct xpc_connection *conn;

	if ((conn = xpc_connection_alloc(name, targetq)) == NULL)
		return (NULL);
	conn->xc_flags = flags;
	if (flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) {
		if (transport->xt_listen(name, &conn->xc_local_port) != 0)
			debugf("cannot check in %s", name);
		return ((xpc_connection_t)conn);
	}
	if (transport->xt_lookup(name, &conn->xc_local_port, &conn->xc_remote_port) != 0) {
		debugf("cannot look up %s", name);
		conn->xc_remote_port = (xpc_port_t)MACH_PORT_NULL;
	}
	return ((xpc_connection_t)conn);
}

xpc_connection_t
xpc_connection_create_from_endpoint(xpc_endpoint_t endpoint)
{
	struct xpc_object *ep = (struct xpc_object *)endpoint;
	struct xpc_connection *conn;

	if (ep == NULL || ep->xo_xpc_type != _XPC_TYPE_ENDPOINT)
		return (NULL);
	if ((conn = xpc_connection_alloc(NULL, NULL)) == NULL)
		return (NULL);
	conn->xc_local_port = (xpc_port_t)xpc_new_receive_port();
	if (MACH_PORT_VALID(ep->xo_port) &&
	    mach_port_mod_refs(mach_task_self(), ep->xo_port, MACH_PORT_RIGHT_SEND, 1) == KERN_SUCCESS)
		conn->xc_remote_port = (xpc_port_t)ep->xo_port;
	return ((xpc_connection_t)conn);
}

/* An endpoint of a listener: a send right to its port. */
xpc_endpoint_t
xpc_endpoint_create(xpc_connection_t connection)
{
	struct xpc_connection *conn = (struct xpc_connection *)connection;
	mach_port_t port = (mach_port_t)conn->xc_local_port;
	xpc_u val;

	if (!(conn->xc_flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) || !MACH_PORT_VALID(port))
		return (NULL);
	if (mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS)
		return (NULL);
	val.port = port;
	return ((xpc_endpoint_t)_xpc_prim_create(_XPC_TYPE_ENDPOINT, val, 0));
}

void
xpc_connection_set_target_queue(xpc_connection_t xconn, dispatch_queue_t targetq)
{
	struct xpc_connection *conn = (struct xpc_connection *)xconn;

	conn->xc_target_queue = targetq;
	dispatch_set_target_queue(conn->xc_recv_queue, targetq ? targetq : xpc_default_target());
}

void
xpc_connection_set_event_handler(xpc_connection_t xconn, xpc_handler_t handler)
{
	struct xpc_connection *conn = (struct xpc_connection *)xconn;
	xpc_handler_t old;

	pthread_mutex_lock(&conn->xc_lock);
	old = conn->xc_handler;
	conn->xc_handler = handler ? (xpc_handler_t)Block_copy(handler) : NULL;
	pthread_mutex_unlock(&conn->xc_lock);
	if (old != NULL)
		Block_release(old);
}

/* An event for the handler, in order, on the connection's queue. */
static void
xpc_connection_deliver(struct xpc_connection *conn, xpc_object_t object)
{
	xpc_retain(object);
	dispatch_async(conn->xc_recv_queue, ^{
		xpc_handler_t handler;

		pthread_mutex_lock(&conn->xc_lock);
		handler = conn->xc_handler ? Block_copy(conn->xc_handler) : NULL;
		pthread_mutex_unlock(&conn->xc_lock);
		if (handler != NULL) {
			handler(object);
			Block_release(handler);
		}
		xpc_release(object);
	});
}

/* The pending calls end with this error. */
static void
xpc_connection_fail_pending(struct xpc_connection *conn, xpc_object_t error)
{
	struct xpc_pending_call *call, *tmp;
	TAILQ_HEAD(, xpc_pending_call) failed;

	TAILQ_INIT(&failed);
	pthread_mutex_lock(&conn->xc_lock);
	TAILQ_FOREACH_SAFE(call, &conn->xc_pending, xp_link, tmp) {
		TAILQ_REMOVE(&conn->xc_pending, call, xp_link);
		TAILQ_INSERT_TAIL(&failed, call, xp_link);
	}
	pthread_mutex_unlock(&conn->xc_lock);
	TAILQ_FOREACH_SAFE(call, &failed, xp_link, tmp) {
		dispatch_async(call->xp_queue ? call->xp_queue : conn->xc_recv_queue, ^{
			call->xp_handler(error);
			Block_release(call->xp_handler);
			free(call);
		});
	}
}

static void
xpc_connection_deliver_error(struct xpc_connection *conn, xpc_object_t error)
{
	xpc_connection_fail_pending(conn, error);
	if (error == XPC_ERROR_CONNECTION_INVALID) {
		pthread_mutex_lock(&conn->xc_lock);
		bool sent = conn->xc_error_sent;
		conn->xc_error_sent = true;
		pthread_mutex_unlock(&conn->xc_lock);
		if (sent)
			return;
	}
	xpc_connection_deliver(conn, error);
}

/* The remote port died: the peer is gone, or the service ended. */
static void
xpc_connection_watch_remote(struct xpc_connection *conn)
{
	mach_port_t remote = (mach_port_t)conn->xc_remote_port;

	if (!MACH_PORT_VALID(remote) || conn->xc_dead_source != NULL)
		return;
	conn->xc_dead_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_SEND,
	    remote, DISPATCH_MACH_SEND_DEAD, conn->xc_send_queue);
	dispatch_source_set_event_handler(conn->xc_dead_source, ^{
		dispatch_source_cancel(conn->xc_dead_source);
		bool named = conn->xc_parent == NULL && conn->xc_name != NULL;
		if (named) {
			/* The next message looks the service up again. */
			mach_port_deallocate(mach_task_self(), (mach_port_t)conn->xc_remote_port);
			conn->xc_remote_port = (xpc_port_t)MACH_PORT_NULL;
			dispatch_release(conn->xc_dead_source);
			conn->xc_dead_source = NULL;
			xpc_connection_deliver_error(conn, XPC_ERROR_CONNECTION_INTERRUPTED);
		} else {
			xpc_connection_deliver_error(conn, XPC_ERROR_CONNECTION_INVALID);
		}
	});
	dispatch_resume(conn->xc_dead_source);
}

/*
 * A named service's port outlives its process (launchd keeps it, to start
 * the service again): the client watches the process that answers it, and
 * its exit is XPC_ERROR_CONNECTION_INTERRUPTED.
 */
static void
xpc_connection_watch_process(struct xpc_connection *conn, pid_t pid)
{
	if (pid <= 0 || pid == getpid() || pid == conn->xc_proc_pid || conn->xc_parent != NULL ||
	    conn->xc_name == NULL)
		return;
	if (conn->xc_proc_source != NULL) {
		dispatch_source_cancel(conn->xc_proc_source);
		dispatch_release(conn->xc_proc_source);
	}
	conn->xc_proc_pid = pid;
	conn->xc_proc_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)pid,
	    DISPATCH_PROC_EXIT, conn->xc_send_queue);
	dispatch_source_set_event_handler(conn->xc_proc_source, ^{
		dispatch_source_cancel(conn->xc_proc_source);
		if (conn->xc_proc_pid != pid || conn->xc_cancelled)
			return;
		conn->xc_proc_pid = 0;
		xpc_connection_deliver_error(conn, XPC_ERROR_CONNECTION_INTERRUPTED);
	});
	dispatch_resume(conn->xc_proc_source);
}

void
xpc_connection_suspend(xpc_connection_t xconn)
{
	struct xpc_connection *conn = (struct xpc_connection *)xconn;

	dispatch_suspend(conn->xc_recv_queue);
}

void
xpc_connection_resume(xpc_connection_t xconn)
{
	struct xpc_transport *transport = xpc_get_transport();
	struct xpc_connection *conn = (struct xpc_connection *)xconn;
	bool first;

	pthread_mutex_lock(&conn->xc_lock);
	first = !conn->xc_resumed;
	conn->xc_resumed = true;
	pthread_mutex_unlock(&conn->xc_lock);

	if (first && !conn->xc_cancelled) {
		if (conn->xc_flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) {
			if (MACH_PORT_VALID((mach_port_t)conn->xc_local_port)) {
				conn->xc_recv_source = transport->xt_create_server_source(
				    conn->xc_local_port, conn, conn->xc_send_queue);
				dispatch_resume(conn->xc_recv_source);
			} else {
				xpc_connection_deliver_error(conn, XPC_ERROR_CONNECTION_INVALID);
			}
		} else if (conn->xc_parent == NULL) {
			if (MACH_PORT_VALID((mach_port_t)conn->xc_local_port)) {
				conn->xc_recv_source = transport->xt_create_client_source(
				    conn->xc_local_port, conn, conn->xc_send_queue);
				dispatch_resume(conn->xc_recv_source);
			}
			if (MACH_PORT_VALID((mach_port_t)conn->xc_remote_port))
				xpc_connection_watch_remote(conn);
			else if (conn->xc_name == NULL)
				xpc_connection_deliver_error(conn, XPC_ERROR_CONNECTION_INVALID);
		} else {
			xpc_connection_watch_remote(conn);
		}
	}
	dispatch_resume(conn->xc_recv_queue);
}

/* The remote port, looked up again for a named service whose last one died. */
static mach_port_t
xpc_connection_remote(struct xpc_connection *conn)
{
	struct xpc_transport *transport = xpc_get_transport();
	xpc_port_t local, remote;

	if (MACH_PORT_VALID((mach_port_t)conn->xc_remote_port) || conn->xc_cancelled)
		return ((mach_port_t)conn->xc_remote_port);
	if (conn->xc_name != NULL && conn->xc_parent == NULL &&
	    !(conn->xc_flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) &&
	    transport->xt_lookup(conn->xc_name, &local, &remote) == 0) {
		mach_port_mod_refs(mach_task_self(), (mach_port_t)local, MACH_PORT_RIGHT_RECEIVE, -1);
		conn->xc_remote_port = remote;
		if (conn->xc_resumed)
			xpc_connection_watch_remote(conn);
	}
	return ((mach_port_t)conn->xc_remote_port);
}

/* On the send queue: NO when the message could not leave. */
static bool
xpc_send(struct xpc_connection *conn, xpc_object_t message, uint64_t id)
{
	mach_port_t remote = xpc_connection_remote(conn);

	if (!MACH_PORT_VALID(remote))
		return (false);
	return (xpc_pipe_send(message, id, conn->xc_local_port, (xpc_port_t)remote) == 0);
}

void
xpc_connection_send_message(xpc_connection_t xconn, xpc_object_t message)
{
	struct xpc_connection *conn = (struct xpc_connection *)xconn;
	struct xpc_object *xo = message;
	uint64_t id;

	/* A reply carries its request's id. */
	if ((xo->xo_flags & _XPC_REPLY) && xo->xo_seqid != 0)
		id = xo->xo_seqid | XPC_REPLY_BIT;
	else
		id = XPC_CONNECTION_NEXT_ID(conn);

	xpc_retain(message);
	dispatch_async(conn->xc_send_queue, ^{
		if (!conn->xc_cancelled)
			xpc_send(conn, message, id);
		xpc_release(message);
	});
}

void
xpc_connection_send_message_with_reply(xpc_connection_t xconn,
    xpc_object_t message, dispatch_queue_t targetq, xpc_handler_t handler)
{
	struct xpc_connection *conn = (struct xpc_connection *)xconn;
	struct xpc_pending_call *call;

	if ((call = calloc(1, sizeof(*call))) == NULL)
		return;
	call->xp_id = XPC_CONNECTION_NEXT_ID(conn);
	call->xp_handler = (xpc_handler_t)Block_copy(handler);
	call->xp_queue = targetq;
	pthread_mutex_lock(&conn->xc_lock);
	TAILQ_INSERT_TAIL(&conn->xc_pending, call, xp_link);
	pthread_mutex_unlock(&conn->xc_lock);

	xpc_retain(message);
	dispatch_async(conn->xc_send_queue, ^{
		bool sent = !conn->xc_cancelled && xpc_send(conn, message, call->xp_id);

		xpc_release(message);
		if (!sent) {
			bool found = false;
			struct xpc_pending_call *c;

			pthread_mutex_lock(&conn->xc_lock);
			TAILQ_FOREACH(c, &conn->xc_pending, xp_link)
				if (c == call) {
					TAILQ_REMOVE(&conn->xc_pending, call, xp_link);
					found = true;
					break;
				}
			pthread_mutex_unlock(&conn->xc_lock);
			if (found)
				dispatch_async(call->xp_queue ? call->xp_queue : conn->xc_recv_queue, ^{
					call->xp_handler(conn->xc_name && !conn->xc_cancelled ?
					    XPC_ERROR_CONNECTION_INTERRUPTED : XPC_ERROR_CONNECTION_INVALID);
					Block_release(call->xp_handler);
					free(call);
				});
		}
	});
}

xpc_object_t
xpc_connection_send_message_with_reply_sync(xpc_connection_t conn, xpc_object_t message)
{
	__block xpc_object_t result = NULL;
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);

	/* Not the connection's queue: the caller may be blocking it. */
	xpc_connection_send_message_with_reply(conn, message,
	    dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
	    ^(xpc_object_t o) {
		result = xpc_retain(o);
		dispatch_semaphore_signal(sem);
	});
	dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
	dispatch_release(sem);
	return (result);
}

/* After every message sent so far, on the connection's queue. */
void
xpc_connection_send_barrier(xpc_connection_t xconn, dispatch_block_t barrier)
{
	struct xpc_connection *conn = (struct xpc_connection *)xconn;
	dispatch_block_t copy = Block_copy(barrier);

	dispatch_async(conn->xc_send_queue, ^{
		dispatch_async(conn->xc_recv_queue, ^{
			copy();
			Block_release(copy);
		});
	});
}

void
xpc_connection_cancel(xpc_connection_t xconn)
{
	struct xpc_connection *conn = (struct xpc_connection *)xconn;

	pthread_mutex_lock(&conn->xc_lock);
	if (conn->xc_cancelled) {
		pthread_mutex_unlock(&conn->xc_lock);
		return;
	}
	conn->xc_cancelled = true;
	pthread_mutex_unlock(&conn->xc_lock);

	dispatch_async(conn->xc_send_queue, ^{
		struct xpc_connection *parent = conn->xc_parent;

		if (conn->xc_recv_source != NULL && parent == NULL)
			dispatch_source_cancel(conn->xc_recv_source);
		if (conn->xc_dead_source != NULL)
			dispatch_source_cancel(conn->xc_dead_source);
		if (conn->xc_proc_source != NULL)
			dispatch_source_cancel(conn->xc_proc_source);
		if (parent != NULL) {
			pthread_mutex_lock(&parent->xc_lock);
			TAILQ_REMOVE(&parent->xc_peers, conn, xc_link);
			pthread_mutex_unlock(&parent->xc_lock);
		}
		xpc_connection_deliver_error(conn, XPC_ERROR_CONNECTION_INVALID);
	});
	/* A connection never resumed still says it is invalid. */
	if (!conn->xc_resumed)
		dispatch_async(conn->xc_send_queue, ^{ dispatch_resume(conn->xc_recv_queue); });
}

const char *
xpc_connection_get_name(xpc_connection_t connection)
{
	return (((struct xpc_connection *)connection)->xc_name);
}

uid_t
xpc_connection_get_euid(xpc_connection_t xconn)
{
	return (((struct xpc_connection *)xconn)->xc_creds.xc_remote_euid);
}

gid_t
xpc_connection_get_egid(xpc_connection_t xconn)
{
	return (((struct xpc_connection *)xconn)->xc_creds.xc_remote_guid);
}

pid_t
xpc_connection_get_pid(xpc_connection_t xconn)
{
	return (((struct xpc_connection *)xconn)->xc_creds.xc_remote_pid);
}

au_asid_t
xpc_connection_get_asid(xpc_connection_t xconn)
{
	return (((struct xpc_connection *)xconn)->xc_creds.xc_remote_asid);
}

void
xpc_connection_get_audit_token(xpc_connection_t xconn, audit_token_t *token)
{
	if (token != NULL)
		*token = ((struct xpc_connection *)xconn)->xc_creds.xc_audit_token;
}

void
xpc_connection_set_context(xpc_connection_t xconn, void *ctx)
{
	((struct xpc_connection *)xconn)->xc_context = ctx;
}

void *
xpc_connection_get_context(xpc_connection_t xconn)
{
	return (((struct xpc_connection *)xconn)->xc_context);
}

/* Connections are never freed yet: the finalizer is kept, not called. */
void
xpc_connection_set_finalizer_f(xpc_connection_t connection __unused,
    xpc_finalizer_t finalizer __unused)
{
}

/* An XPC service's main: a listener on the service's name. */
void
xpc_main(xpc_connection_handler_t handler)
{
	const char *name = getenv("XPC_SERVICE_NAME");
	xpc_connection_t listener;

	if (name == NULL || handler == NULL) {
		fprintf(stderr, "xpc_main: not started as an XPC service (no XPC_SERVICE_NAME)\n");
		exit(EXIT_FAILURE);
	}
	/* handler is a function, not a block. */
	listener = xpc_connection_create_mach_service(name, NULL,
	    XPC_CONNECTION_MACH_SERVICE_LISTENER);
	xpc_connection_set_event_handler(listener, ^(xpc_object_t peer) {
		if (xpc_get_type(peer) == XPC_TYPE_CONNECTION)
			handler((xpc_connection_t)peer);
	});
	xpc_connection_resume(listener);
	dispatch_main();
}

void
xpc_transaction_begin(void)
{
}

void
xpc_transaction_end(void)
{
}

#pragma mark Receiving

/* A message or a reply for conn. */
static void
xpc_connection_dispatch_callback(struct xpc_connection *conn, xpc_object_t result,
    uint64_t id, struct xpc_credentials *creds)
{
	struct xpc_pending_call *call, *found = NULL;

	if (creds != NULL)
		conn->xc_creds = *creds;
	/* A reply to this message goes back on this connection, with this id. */
	if (xpc_get_type(result) == XPC_TYPE_DICTIONARY) {
		((struct xpc_object *)result)->xo_connection = conn;
		((struct xpc_object *)result)->xo_seqid = id & ~XPC_REPLY_BIT;
	}

	if (id & XPC_REPLY_BIT) {
		pthread_mutex_lock(&conn->xc_lock);
		TAILQ_FOREACH(call, &conn->xc_pending, xp_link)
			if (call->xp_id == (id & ~XPC_REPLY_BIT)) {
				TAILQ_REMOVE(&conn->xc_pending, call, xp_link);
				found = call;
				break;
			}
		pthread_mutex_unlock(&conn->xc_lock);
		if (found != NULL) {
			dispatch_async(found->xp_queue ? found->xp_queue : conn->xc_recv_queue, ^{
				found->xp_handler(result);
				Block_release(found->xp_handler);
				free(found);
				xpc_release(result);
			});
			return;
		}
	}
	xpc_connection_deliver(conn, result);
	xpc_release(result);
}

/* A client: messages and replies on its own port. */
void
xpc_connection_recv_message(void *context)
{
	struct xpc_connection *conn = context;
	struct xpc_credentials creds;
	xpc_object_t result;
	xpc_port_t remote;
	uint64_t id;

	memset(&creds, 0, sizeof(creds));
	if (xpc_pipe_receive(conn->xc_local_port, &remote, &result, &id, &creds) < 0)
		return;
	/* The sender's reply right: the connection keeps its own. */
	if (MACH_PORT_VALID((mach_port_t)remote))
		mach_port_deallocate(mach_task_self(), (mach_port_t)remote);
	if (conn->xc_cancelled) {
		xpc_release(result);
		return;
	}
	xpc_connection_watch_process(conn, creds.xc_remote_pid);
	xpc_connection_dispatch_callback(conn, result, id, &creds);
}

static struct xpc_connection *
xpc_connection_find_peer(struct xpc_connection *conn, mach_port_t remote)
{
	struct xpc_connection *peer;

	TAILQ_FOREACH(peer, &conn->xc_peers, xc_link)
		if ((mach_port_t)peer->xc_remote_port == remote)
			return (peer);
	return (NULL);
}

/* A listener: each sender (its reply port) is a peer connection. */
void
xpc_connection_recv_mach_message(void *context)
{
	struct xpc_connection *conn = context, *peer;
	struct xpc_credentials creds;
	xpc_object_t result;
	xpc_port_t remote;
	uint64_t id;
	bool fresh = false;

	memset(&creds, 0, sizeof(creds));
	if (xpc_pipe_receive(conn->xc_local_port, &remote, &result, &id, &creds) < 0)
		return;
	if (!MACH_PORT_VALID((mach_port_t)remote) || conn->xc_cancelled) {
		if (MACH_PORT_VALID((mach_port_t)remote))
			mach_port_deallocate(mach_task_self(), (mach_port_t)remote);
		xpc_release(result);
		return;
	}

	pthread_mutex_lock(&conn->xc_lock);
	peer = xpc_connection_find_peer(conn, (mach_port_t)remote);
	if (peer == NULL) {
		peer = xpc_connection_alloc(NULL, conn->xc_target_queue);
		if (peer != NULL) {
			peer->xc_parent = conn;
			peer->xc_local_port = conn->xc_local_port;
			peer->xc_remote_port = remote;	/* the peer keeps this right */
			peer->xc_creds = creds;
			TAILQ_INSERT_TAIL(&conn->xc_peers, peer, xc_link);
			fresh = true;
		}
	} else {
		mach_port_deallocate(mach_task_self(), (mach_port_t)remote);
	}
	pthread_mutex_unlock(&conn->xc_lock);
	if (peer == NULL) {
		xpc_release(result);
		return;
	}
	/* The listener hears of the peer first; the peer's messages wait for
	 * it to be resumed. */
	if (fresh)
		xpc_connection_deliver(conn, (xpc_object_t)peer);
	xpc_connection_dispatch_callback(peer, result, id, &creds);
}

/* Kept for the transport's sources (a peer is cancelled, not destroyed). */
void *
xpc_connection_new_peer(void *context __unused, xpc_port_t local __unused,
    xpc_port_t remote __unused, dispatch_source_t src __unused)
{
	return (NULL);
}

void
xpc_connection_destroy_peer(void *context __unused)
{
}

/* No entitlements on Minkar (no code signing yet): every peer has none. */
xpc_object_t
xpc_connection_copy_entitlement_value(xpc_connection_t connection __unused,
    const char *entitlement __unused)
{
	return (NULL);
}
