/*
 * Original code Copyright 2014-2015 iXsystems, Inc.
 * Port to ravynOS Copyright 2026 Zoe Knox
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

#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sbuf.h>
#include <stdatomic.h>
#include <assert.h>
#include <syslog.h>
#include <pthread.h>
#include "xpc/xpc.h"
#include "xpc_internal.h"
#include "xpc/launchd.h"

#define RECV_BUFFER_SIZE	65536

static void xpc_copy_description_level(xpc_object_t obj, struct sbuf *sbuf,
    int level);

extern struct xpc_transport unix_transport __attribute__((weak));
extern struct xpc_transport mach_transport __attribute__((weak));
static struct xpc_transport *selected_transport = NULL;

char *strerror(int);
char *strdup(const char *);

struct xpc_transport *
xpc_get_transport()
{
	return &mach_transport;
}

static void
xpc_dictionary_destroy(struct xpc_object *dict)
{
	struct xpc_dict_head *head;
	struct xpc_dict_pair *p, *ptmp;

	head = &dict->xo_dict;

	TAILQ_FOREACH_SAFE(p, head, xo_link, ptmp) {
		TAILQ_REMOVE(head, p, xo_link);
		xpc_release(p->value);	/* set_value retained it */
		free(p->key);
		free(p);
	}
}


__thread struct xpc_resource_list *_xpc_resources;

size_t
_xpc_resource_add(struct xpc_resource_list *list, mach_port_t port,
    mach_msg_type_name_t disposition)
{
	if (list->count == list->capacity) {
		size_t capacity = list->capacity ? list->capacity * 2 : 4;
		struct xpc_resource *items = realloc(list->items,
		    capacity * sizeof(struct xpc_resource));

		if (items == NULL)
			return ((size_t)-1);
		list->items = items;
		list->capacity = capacity;
	}
	list->items[list->count].xr_type = XPC_RESOURCE_PORT;
	list->items[list->count].xr_port = port;
	list->items[list->count].xr_disposition = disposition;
	return (list->count++);
}

/* The rights a packed message made for itself (fileports): its own. */
static void
xpc_resources_release(struct xpc_resource_list *list, bool sent)
{
	for (size_t i = 0; i < list->count; i++)
		if (!sent && list->items[i].xr_disposition == MACH_MSG_TYPE_MOVE_SEND)
			mach_port_deallocate(mach_task_self(), list->items[i].xr_port);
	free(list->items);
	list->items = NULL;
	list->count = list->capacity = 0;
}

static int
xpc_pack(struct xpc_object *xo, void **buf, uint64_t id, size_t *size)
{
	struct xpc_frame_header *header;
	mpack_writer_t writer;
	char *packed, *ret;
	size_t packed_size;

	mpack_writer_init_growable(&writer, &packed, &packed_size);
	xpc2mpack(&writer, xo);

	if (mpack_writer_destroy(&writer) != mpack_ok)
		return (-1);

	ret = malloc(packed_size + sizeof(*header));
	memset(ret, 0, packed_size + sizeof(*header));

	header = (struct xpc_frame_header *)ret;
	header->length = packed_size;
	header->id = id;
	header->version = XPC_PROTOCOL_VERSION;

	memcpy(ret + sizeof(*header), packed, packed_size);
	*buf = ret;
	*size = packed_size + sizeof(*header);

	free(packed);
	return (0);
}

static struct xpc_object *
xpc_unpack(void *buf, size_t size)
{
	mpack_tree_t tree;
	struct xpc_object *xo;

	mpack_tree_init(&tree, (const char *)buf, size);
	if (mpack_tree_error(&tree) != mpack_ok) {
		debugf("unpack failed: %d", mpack_tree_error(&tree))
		return (NULL);
	}

	xo = mpack2xpc(mpack_tree_root(&tree));
	return (xo);
}

void
xpc_object_destroy(struct xpc_object *xo)
{
	/*
	 * A connection stays: there is no xpc_connection_cancel() yet to
	 * stop its sources, and its peers point to it.
	 */
	if (xo->xo_xpc_type == _XPC_TYPE_CONNECTION)
		return;

	if (xo->xo_xpc_type == _XPC_TYPE_DICTIONARY)
		xpc_dictionary_destroy(xo);

	if (xo->xo_xpc_type == _XPC_TYPE_ARRAY)
		_xpc_array_destroy(xo);

	/* Minkar: an object owns its descriptor and its port right. */
	if (xo->xo_xpc_type == _XPC_TYPE_FD && xo->xo_fd >= 0)
		close(xo->xo_fd);
	if (xo->xo_xpc_type == _XPC_TYPE_ENDPOINT && MACH_PORT_VALID(xo->xo_port))
		mach_port_mod_refs(mach_task_self(), xo->xo_port,
		    (xo->xo_flags & _XPC_PORT_RECEIVE) ? MACH_PORT_RIGHT_RECEIVE :
		    MACH_PORT_RIGHT_SEND, -1);
	if (xo->xo_xpc_type == _XPC_TYPE_SHMEM && MACH_PORT_VALID(xo->xo_port))
		mach_port_deallocate(mach_task_self(), xo->xo_port);
	if (xo->xo_xpc_type == _XPC_TYPE_STRING)
		free(xo->xo_str);
	if (xo->xo_xpc_type == _XPC_TYPE_DATA)
		free((void *)xo->xo_ptr);
	free(xo->xo_audit_token);
	free(xo);
}

xpc_object_t
xpc_retain(xpc_object_t obj)
{
	struct xpc_object *xo;

	xo = obj;
	if (xo == NULL || (xo->xo_flags & _XPC_GLOBAL))
		return (obj);
	atomic_fetch_add(&xo->xo_refcnt, 1);
	return (obj);
}

void
xpc_release(xpc_object_t obj)
{
	struct xpc_object *xo;

	xo = obj;
	if (xo == NULL || (xo->xo_flags & _XPC_GLOBAL))
		return;
	if (atomic_fetch_add(&xo->xo_refcnt, -1) > 1)
		return;

	xpc_object_destroy(xo);
}

static const char *xpc_errors[] = {
	"No Error Found",
	"No Memory",
	"Invalid Argument",
	"No Such Process"
};

const char *
xpc_strerror(int error)
{

	if (error > EXMAX || error < 0)
		return "BAD ERROR";
	return (xpc_errors[error]);
}

char *
xpc_copy_description(xpc_object_t obj)
{
	char *result;
	struct sbuf *sbuf;

	sbuf = sbuf_new_auto();
	xpc_copy_description_level(obj, sbuf, 0);
	sbuf_finish(sbuf);
	result = strdup(sbuf_data(sbuf));
	sbuf_delete(sbuf);

	return (result);
}

static void
xpc_copy_description_level(xpc_object_t obj, struct sbuf *sbuf, int level)
{
	struct xpc_object *xo = obj;
	struct uuid *id;
	char *uuid_str;
	uint32_t uuid_status;

	if (obj == NULL) {
		sbuf_printf(sbuf, "<null value>\n");
		return;
	}

	sbuf_printf(sbuf, "(%s) ", _xpc_get_type_name(obj));

	switch (xo->xo_xpc_type) {
	case _XPC_TYPE_DICTIONARY:
		sbuf_printf(sbuf, "\n");
		xpc_dictionary_apply(xo, ^(const char *k, xpc_object_t v) {
			sbuf_printf(sbuf, "%*s\"%s\": ", level * 4, " ", k);
			xpc_copy_description_level(v, sbuf, level + 1);
			return ((bool)true);
		});
		break;

	case _XPC_TYPE_ARRAY:
		sbuf_printf(sbuf, "\n");
		xpc_array_apply(xo, ^(size_t idx, xpc_object_t v) {
			sbuf_printf(sbuf, "%*s%ld: ", level * 4, " ", idx);
			xpc_copy_description_level(v, sbuf, level + 1);
			return ((bool)true);
		});
		break;

	case _XPC_TYPE_BOOL:
		sbuf_printf(sbuf, "%s\n",
		    xpc_bool_get_value(obj) ? "true" : "false");
		break;

	case _XPC_TYPE_STRING:
		sbuf_printf(sbuf, "\"%s\"\n",
		    xpc_string_get_string_ptr(obj));
		break;

	case _XPC_TYPE_INT64:
		sbuf_printf(sbuf, "%ld\n",
		    xpc_int64_get_value(obj));
		break;

	case _XPC_TYPE_UINT64:
		sbuf_printf(sbuf, "%lx\n",
		    xpc_uint64_get_value(obj));
		break;

	case _XPC_TYPE_DATE:
		sbuf_printf(sbuf, "%lu\n",
		    xpc_date_get_value(obj));
		break;	

	case _XPC_TYPE_UUID:
		id = (struct uuid *)xpc_uuid_get_bytes(obj);
		uuid_unparse(id, &uuid_str);
		sbuf_printf(sbuf, "%s\n", uuid_str);
		free(uuid_str);
		break;

	case _XPC_TYPE_ENDPOINT:
		sbuf_printf(sbuf, "<%ld>\n", xo->xo_int);
		break;

	case _XPC_TYPE_NULL:
		sbuf_printf(sbuf, "<null>\n");
		break;
	}
}

struct _launch_data {
	uint64_t type;
	union {
		struct {
			union {
				launch_data_t *_array;
				char *string;
				void *opaque;
				int64_t __junk;
			};
			union {
				uint64_t _array_cnt;
				uint64_t string_len;
				uint64_t opaque_size;
			};
		};
		int64_t fd;
		uint64_t  mp;
		uint64_t err;
		int64_t number;
		uint64_t boolean; /* We'd use 'bool' but this struct needs to be used under Rosetta, and sizeof(bool) is different between PowerPC and Intel */
		double float_num;
	};
};

static uint8_t ld_to_xpc_type[] = {
	_XPC_TYPE_INVALID,
	_XPC_TYPE_DICTIONARY,
	_XPC_TYPE_ARRAY,
	_XPC_TYPE_FD,
	_XPC_TYPE_UINT64,
	_XPC_TYPE_DOUBLE,
	_XPC_TYPE_BOOL,
	_XPC_TYPE_STRING,
	_XPC_TYPE_DATA,
	_XPC_TYPE_ERROR,
	_XPC_TYPE_ENDPOINT
};
	
xpc_object_t
ld2xpc(launch_data_t ld)
{
	struct xpc_object *xo;
	xpc_u val;


	if (ld->type > LAUNCH_DATA_MACHPORT)
		return (NULL);
	if (ld->type == LAUNCH_DATA_STRING || ld->type == LAUNCH_DATA_OPAQUE) {
		val.str = malloc(ld->string_len);
		memcpy(__DECONST(void *, val.str), ld->string, ld->string_len);
		xo = _xpc_prim_create(ld_to_xpc_type[ld->type], val, ld->string_len);
	} else if (ld->type == LAUNCH_DATA_BOOL) {
		val.b = (bool)ld->boolean;
		xo = _xpc_prim_create(ld_to_xpc_type[ld->type], val, 0);
	} else if (ld->type == LAUNCH_DATA_ARRAY) {
		xo = xpc_array_create(NULL, 0);
		for (uint64_t i = 0; i < ld->_array_cnt; i++)
			xpc_array_append_value(xo, ld2xpc(ld->_array[i]));
	} else {
		val.ui = ld->mp;
		xo = _xpc_prim_create(ld_to_xpc_type[ld->type], val, ld->string_len);	
	}
	return (xo);
}

xpc_object_t
xpc_copy_entitlement_for_token(const char *key __unused, audit_token_t *token __unused)
{
	xpc_u val;

	val.b = true;
	return (_xpc_prim_create(_XPC_TYPE_BOOL, val,0));
}

int
xpc_pipe_send(xpc_object_t xobj, uint64_t id, xpc_port_t local, xpc_port_t remote)
{
	struct xpc_transport *transport = xpc_get_transport();
	struct xpc_resource_list resources = { NULL, 0, 0 };
	void *buf;
	size_t size;
	int err;

	assert(xpc_get_type(xobj) == &_xpc_type_dictionary);

	/* The rights in the message are gathered while it is packed. */
	_xpc_resources = &resources;
	err = xpc_pack(xobj, &buf, id, &size);
	_xpc_resources = NULL;
	if (err != 0) {
		debugf("pack failed");
		xpc_resources_release(&resources, false);
		return (-1);
	}

	err = transport->xt_send(local, remote, buf, size, resources.items,
	    resources.count);
	xpc_resources_release(&resources, err == 0);
	free(buf);
	if (err != 0) {
		errno = err;
		debugf("transport send function failed: %s", strerror(errno));
		return (-1);
	}

	return (0);
}

int
xpc_pipe_receive(xpc_port_t local, xpc_port_t *remote, xpc_object_t *result,
    uint64_t *id, struct xpc_credentials *creds)
{
	struct xpc_transport *transport = xpc_get_transport();
	struct xpc_resource_list resources = { NULL, 0, 0 };
	struct xpc_resource *items;
	struct xpc_frame_header *header;
	void *buffer;
	size_t length, nresources;
	int ret;

	ret = transport->xt_recv(local, remote, &buffer, &length, &items,
	    &nresources, creds);
	if (ret < 0) {
		debugf("transport receive function failed");
		return (-1);
	}

	header = (struct xpc_frame_header *)buffer;
	if (length < sizeof(*header) || header->length > length - sizeof(*header) ||
	    header->version != XPC_PROTOCOL_VERSION) {
		debugf("invalid message");
		for (size_t i = 0; i < nresources; i++)
			mach_port_deallocate(mach_task_self(), items[i].xr_port);
		free(items);
		free(buffer);
		return (-1);
	}

	*id = header->id;
	resources.items = items;
	resources.count = resources.capacity = nresources;
	_xpc_resources = &resources;
	*result = xpc_unpack((char *)buffer + sizeof(*header), header->length);
	_xpc_resources = NULL;
	free(items);
	free(buffer);

	if (*result == NULL)
		return (-1);

	/* A message from the wire: xpc_dictionary_create_reply() may answer it. */
	((struct xpc_object *)*result)->xo_flags |= _XPC_FROM_WIRE;
	if (creds != NULL) {
		((struct xpc_object *)*result)->xo_audit_token =
		    malloc(sizeof(audit_token_t));
		if (((struct xpc_object *)*result)->xo_audit_token != NULL)
			*((struct xpc_object *)*result)->xo_audit_token =
			    creds->xc_audit_token;
	}
	return ((int)length);
}
