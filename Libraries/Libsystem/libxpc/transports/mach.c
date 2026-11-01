/*
 * Copyright 2014-2015 iXsystems, Inc.
 * All rights reserved
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <mach/mach.h>
#include <bsm/libbsm.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <xpc/xpc.h>
#include <servers/bootstrap.h>

#include "../xpc_internal.h"


static int
mach_lookup(const char *name, xpc_port_t *local, xpc_port_t *remote)
{
	mach_port_t mp, rmp;
	kern_return_t kr;

	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
	    &mp);
	if (kr != KERN_SUCCESS) {
		errno = EPERM;
		return (-1);
	}

	kr = mach_port_insert_right(mach_task_self(), mp, mp,
	    MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
		errno = EPERM;
		return (-1);
	}

	kr = bootstrap_look_up(bootstrap_port, name, &rmp);
	if (kr != KERN_SUCCESS) {
		debugf("bootstrap_look_up failed kr=%d", kr);
		errno = EBUSY;
		return (-1);
	}

	*local = (xpc_port_t)mp;
	*remote = (xpc_port_t)rmp;
	return (0);
}

static int
mach_listen(const char *name, xpc_port_t *port)
{
	mach_port_t mp;
	kern_return_t kr;

	kr = bootstrap_check_in(bootstrap_port, name, &mp);
	if (kr != KERN_SUCCESS) {
		debugf("bootstrap_check_in failed kr=%d", kr);
		errno = EBUSY;
		return (-1);
	}

	*(mach_port_t *)port = mp;
	return (0);
}

static char *
mach_port_to_string(xpc_port_t port)
{
	mach_port_t p = (mach_port_t)port;
	char *ret;

	asprintf(&ret, "<%d>", p);
	return (ret);
}

static int
mach_port_compare(xpc_port_t p1, xpc_port_t p2)
{
	return (mach_port_t)p1 == (mach_port_t)p2;
}

static dispatch_source_t
mach_create_client_source(xpc_port_t port, void *context, dispatch_queue_t tq)
{
	mach_port_t mp = (mach_port_t)port;
	dispatch_source_t ret;

	ret = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
	    (uintptr_t)mp, 0, tq);

	dispatch_set_context(ret, context);
	dispatch_source_set_event_handler_f(ret, xpc_connection_recv_message);
	dispatch_source_set_cancel_handler(ret, ^{
	    xpc_connection_destroy_peer(dispatch_get_context(ret));
	});

	return (ret);
}

static dispatch_source_t
mach_create_server_source(xpc_port_t port, void *context, dispatch_queue_t tq)
{
	mach_port_t mp = (mach_port_t)port;
	void *client_ctx;
	dispatch_source_t ret;

	ret = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
	    (uintptr_t)mp, 0, tq);

	dispatch_set_context(ret, context);
	dispatch_source_set_event_handler_f(ret,
	    xpc_connection_recv_mach_message);
	
	return (ret);
}

#ifdef __x86_64__
#define _ALIGN(x) (((uint64_t)x + 63) & ~63) /* align to register size */
#else
#error Unsupported platform!
#endif

/*
 * Minkar: a message is a complex Mach message: its packed bytes as
 * out-of-line memory (any size; it was inline, 8 KB at most), then one
 * port descriptor per right it carries (endpoints, file descriptors as
 * fileports). The receiver gets the credentials of the sender from the
 * audit trailer.
 */
struct xpc_mach_message {
	mach_msg_header_t		header;
	mach_msg_body_t			body;
	mach_msg_ool_descriptor_t	data;
	mach_msg_port_descriptor_t	ports[];
};

static int
mach_send(xpc_port_t local, xpc_port_t remote, void *buf, size_t len,
    struct xpc_resource *res, size_t nres)
{
	struct xpc_mach_message *message;
	size_t size = sizeof(*message) + nres * sizeof(mach_msg_port_descriptor_t);
	kern_return_t kr;

	if ((message = calloc(1, size)) == NULL)
		return (ENOMEM);
	message->header.msgh_bits = MACH_MSGH_BITS_COMPLEX |
	    MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,
	    (mach_port_t)local != MACH_PORT_NULL ? MACH_MSG_TYPE_MAKE_SEND : 0);
	message->header.msgh_size = (mach_msg_size_t)size;
	message->header.msgh_remote_port = (mach_port_t)remote;
	message->header.msgh_local_port = (mach_port_t)local;
	message->header.msgh_id = XPC_PROTOCOL_VERSION;
	message->body.msgh_descriptor_count = (mach_msg_size_t)(1 + nres);
	message->data.address = buf;
	message->data.size = (mach_msg_size_t)len;
	message->data.deallocate = FALSE;
	message->data.copy = MACH_MSG_VIRTUAL_COPY;
	message->data.type = MACH_MSG_OOL_DESCRIPTOR;
	for (size_t i = 0; i < nres; i++) {
		message->ports[i].name = res[i].xr_port;
		message->ports[i].disposition = res[i].xr_disposition;
		message->ports[i].type = MACH_MSG_PORT_DESCRIPTOR;
	}

	kr = mach_msg(&message->header, MACH_SEND_MSG, (mach_msg_size_t)size, 0,
	    MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	free(message);
	if (kr != KERN_SUCCESS) {
		debugf("failed, kr=%d", kr);
		return (kr == MACH_SEND_INVALID_DEST ? EPIPE : EINVAL);
	}
	return (0);
}

static int
mach_recv(xpc_port_t local, xpc_port_t *remote, void **buf, size_t *len,
    struct xpc_resource **res, size_t *nres, struct xpc_credentials *creds)
{
	mach_msg_size_t size = 4096;
	mach_msg_header_t *header = NULL;
	mach_msg_audit_trailer_t *trailer;
	kern_return_t kr;

	*buf = NULL;
	*len = 0;
	*res = NULL;
	*nres = 0;
	for (;;) {
		mach_msg_size_t room = size + MAX_TRAILER_SIZE;

		free(header);
		if ((header = malloc(room)) == NULL)
			return (-1);
		kr = mach_msg(header, MACH_RCV_MSG | MACH_RCV_LARGE | MACH_RCV_TIMEOUT |
		    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
		    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT), 0,
		    room, (mach_port_t)local, 0, MACH_PORT_NULL);
		if (kr == MACH_RCV_TOO_LARGE) {
			size = header->msgh_size;
			continue;
		}
		break;
	}
	if (kr != KERN_SUCCESS) {
		free(header);
		return (-1);
	}

	*remote = (xpc_port_t)header->msgh_remote_port;
	trailer = (mach_msg_audit_trailer_t *)((char *)header + round_msg(header->msgh_size));
	if (creds != NULL) {
		/* The token's fields: auid, euid, egid, ruid, rgid, pid, asid. */
		creds->xc_remote_euid = (uid_t)trailer->msgh_audit.val[1];
		creds->xc_remote_guid = (gid_t)trailer->msgh_audit.val[2];
		creds->xc_remote_pid = (pid_t)trailer->msgh_audit.val[5];
		creds->xc_remote_asid = (au_asid_t)trailer->msgh_audit.val[6];
		creds->xc_audit_token = trailer->msgh_audit;
	}

	if (!(header->msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
		mach_msg_destroy(header);
		free(header);
		return (-1);
	}

	struct xpc_mach_message *message = (struct xpc_mach_message *)header;
	mach_msg_size_t count = message->body.msgh_descriptor_count;
	if (count < 1 || message->data.type != MACH_MSG_OOL_DESCRIPTOR) {
		mach_msg_destroy(header);
		free(header);
		return (-1);
	}
	*len = message->data.size;
	*buf = malloc(*len ? *len : 1);
	if (*buf != NULL && *len > 0)
		memcpy(*buf, message->data.address, *len);
	vm_deallocate(mach_task_self(), (vm_address_t)message->data.address, message->data.size);

	if (count > 1) {
		*res = calloc(count - 1, sizeof(struct xpc_resource));
		for (mach_msg_size_t i = 0; i < count - 1; i++) {
			(*res)[i].xr_type = XPC_RESOURCE_PORT;
			(*res)[i].xr_port = message->ports[i].name;
			(*res)[i].xr_disposition = message->ports[i].disposition;
		}
		*nres = count - 1;
	}
	free(header);
	return (*buf != NULL ? (int)*len : -1);
}

struct xpc_transport mach_transport = {
    	.xt_name = "mach",
	.xt_listen = mach_listen,
	.xt_lookup = mach_lookup,
    	.xt_port_to_string = mach_port_to_string,
    	.xt_port_compare = mach_port_compare,
    	.xt_create_client_source = mach_create_client_source,
    	.xt_create_server_source = mach_create_server_source,
	.xt_send = mach_send,
	.xt_recv = mach_recv
};
