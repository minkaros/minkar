/*
 * Copyright 2014-2015 iXsystems, Inc.
 * Copyright 2026 Zoe Knox
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

#include <sys/types.h>
#include "xpc/xpc.h"
#include "xpc_internal.h"
#include "mpack.h"

/*
 * MessagePack extension types for the XPC types it has no format for.
 * (Integers: MessagePack sends a non-negative int64 as unsigned; the
 * getters accept either.)
 */
#define	XPC_MPACK_EXT_UUID	1	/* 16 bytes */
#define	XPC_MPACK_EXT_DATE	2	/* int64 nanoseconds, big-endian */

static struct xpc_object *
mpack2xpc_extension(int8_t type, const char *data, size_t length)
{
	switch (type) {
	case XPC_MPACK_EXT_UUID:
		if (length != sizeof(uuid_t))
			return (NULL);
		return (xpc_uuid_create((const unsigned char *)data));

	case XPC_MPACK_EXT_DATE: {
		uint64_t v = 0;

		if (length != sizeof(v))
			return (NULL);
		for (size_t i = 0; i < sizeof(v); i++)
			v = v << 8 | (uint8_t)data[i];
		return (xpc_date_create((int64_t)v));
	}

	default:
		return (NULL);
	}
}

struct xpc_object *
mpack2xpc(const mpack_node_t node)
{
	xpc_object_t xotmp;
	size_t i;
	xpc_u val;

	switch (mpack_node_type(node)) {
	case mpack_type_nil:
		xotmp = xpc_null_create();
		break;

	case mpack_type_int:
		val.i = mpack_node_i64(node);
		xotmp = xpc_int64_create(val.i);
		break;

	case mpack_type_uint:
		val.ui = mpack_node_u64(node);
		xotmp = xpc_uint64_create(val.ui);
		break;

	case mpack_type_bool:
		val.b = mpack_node_bool(node);
		xotmp = xpc_bool_create(val.b);
		break;

	case mpack_type_double:
		val.d = mpack_node_double(node);
		xotmp = xpc_double_create(val.d);
		break;

	case mpack_type_str:
		val.str = mpack_node_cstr_alloc(node, 65536);
		xotmp = xpc_string_create(val.str);
		break;

	case mpack_type_bin:
		xotmp = xpc_data_create(mpack_node_data(node),
		    mpack_node_data_len(node));
		break;

	case mpack_type_array:
		xotmp = xpc_array_create(NULL, 0);
		for (i = 0; i < mpack_node_array_length(node); i++) {
			xpc_object_t item = mpack2xpc(
			    mpack_node_array_at(node, i));

			if (item == NULL)
				continue;
			xpc_array_append_value(xotmp, item);
			xpc_release(item);	/* the array keeps it */
		}
		break;

	case mpack_type_map:
		xotmp = xpc_dictionary_create(NULL, NULL, 0);
		for (i = 0; i < mpack_node_map_count(node); i++) {
			char *key = mpack_node_cstr_alloc(
			    mpack_node_map_key_at(node, i), 1024);
			xpc_object_t value = mpack2xpc(
			    mpack_node_map_value_at(node, i));

			if (value == NULL)
				continue;
			xpc_dictionary_set_value(xotmp, key, value);
			xpc_release(value);	/* the dictionary keeps it */
			free(key);		/* and copies the key */
		}
		break;

	case mpack_type_ext:
		xotmp = mpack2xpc_extension(mpack_node_exttype(node),
		    mpack_node_data(node), mpack_node_data_len(node));
		break;

	default:
		xotmp = NULL;
		break;
	}

	return (xotmp);
}

void
xpc2mpack(mpack_writer_t *writer, xpc_object_t obj)
{
	struct xpc_object *xotmp = obj;

	switch (xotmp->xo_xpc_type) {
	case _XPC_TYPE_DICTIONARY:
		mpack_start_map(writer, xpc_dictionary_get_count(obj));
		xpc_dictionary_apply(obj, ^(const char *k, xpc_object_t v) {
		    mpack_write_cstr(writer, k);
		    xpc2mpack(writer, v);
		    return ((bool)true);
		});
		mpack_finish_map(writer);
		break;

	case _XPC_TYPE_ARRAY:
		mpack_start_array(writer, xpc_array_get_count(obj));
		xpc_array_apply(obj, ^(size_t index __unused, xpc_object_t v) {
		    xpc2mpack(writer, v);
		    return ((bool)true);
		});
		mpack_finish_array(writer);
		break;

	case _XPC_TYPE_NULL:
		mpack_write_nil(writer);
		break;

	case _XPC_TYPE_BOOL:
		mpack_write_bool(writer, xpc_bool_get_value(obj));
		break;

	case _XPC_TYPE_INT64:
		mpack_write_i64(writer, xpc_int64_get_value(obj));
		break;

	case _XPC_TYPE_DOUBLE:
		mpack_write_double(writer, xpc_double_get_value(obj));
		break;

	case _XPC_TYPE_UINT64:
		mpack_write_u64(writer, xpc_uint64_get_value(obj));
		break;

	case _XPC_TYPE_STRING:
		mpack_write_cstr(writer, xpc_string_get_string_ptr(obj));
		break;

	case _XPC_TYPE_DATA:
		mpack_write_bin(writer, xpc_data_get_bytes_ptr(obj),
		    (uint32_t)xpc_data_get_length(obj));
		break;

	case _XPC_TYPE_UUID:
		mpack_write_ext(writer, XPC_MPACK_EXT_UUID,
		    (const char *)xpc_uuid_get_bytes(obj), sizeof(uuid_t));
		break;

	case _XPC_TYPE_DATE: {
		uint64_t v = (uint64_t)xpc_date_get_value(obj);
		char be[sizeof(v)];

		for (size_t i = 0; i < sizeof(v); i++)
			be[i] = (char)(v >> (8 * (sizeof(v) - 1 - i)));
		mpack_write_ext(writer, XPC_MPACK_EXT_DATE, be, sizeof(be));
		break;
	}

	default:
		/*
		 * No wire format (file descriptors, endpoints, ...): nil,
		 * so that the container's count still holds.
		 */
		mpack_write_nil(writer);
		break;
	}
}

xpc_object_t
xpc_dictionary_create(const char * const *keys, const xpc_object_t *values,
    size_t count)
{
	struct xpc_object *xo;
	size_t i;
	xpc_u val;

	xo = _xpc_prim_create(_XPC_TYPE_DICTIONARY, val, count);
	
	for (i = 0; i < count; i++)
		xpc_dictionary_set_value(xo, keys[i], values[i]);
	
	return (xo);
}

xpc_object_t
xpc_dictionary_create_reply(xpc_object_t original)
{
	struct xpc_object *xo_orig;

	struct xpc_object *reply;

	xo_orig = original;
	if ((xo_orig->xo_flags & _XPC_FROM_WIRE) == 0)
		return (NULL);

	reply = xpc_dictionary_create(NULL, NULL, 0);
	if (reply != NULL) {
		reply->xo_connection = xo_orig->xo_connection;
		reply->xo_seqid = xo_orig->xo_seqid;
	}
	return (reply);
}

xpc_connection_t
xpc_dictionary_get_remote_connection(xpc_object_t xdict)
{
	struct xpc_object *xo;

	xo = xdict;
	return ((xpc_connection_t)xo->xo_connection);
}

#ifdef MACH
void
xpc_dictionary_get_audit_token(xpc_object_t xdict, audit_token_t *token)
{
	struct xpc_object *xo;

	xo = xdict;
	if (xo->xo_audit_token != NULL)
		memcpy(token, xo->xo_audit_token, sizeof(*token));
}

void
xpc_dictionary_set_mach_recv(xpc_object_t xdict, const char *key,
    mach_port_t port)
{
	struct xpc_object *xo = xdict;
	struct xpc_object *xotmp;
	xpc_u val;

	val.port = port;
	xotmp = _xpc_prim_create(_XPC_TYPE_ENDPOINT, val, 0);

	xpc_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_dictionary_set_mach_send(xpc_object_t xdict, const char *key,
    mach_port_t port)
{
	struct xpc_object *xotmp;
	xpc_u val;

	val.port = port;
	xotmp = _xpc_prim_create(_XPC_TYPE_ENDPOINT, val, 0);

	xpc_dictionary_set_value(xdict, key, xotmp);
}

mach_port_t
xpc_dictionary_copy_mach_send(xpc_object_t xdict, const char *key)
{
	struct xpc_object *xo;
	const struct xpc_object *xotmp;

}
#endif

void
xpc_dictionary_set_value(xpc_object_t xdict, const char *key,
        xpc_object_t value)
{
	struct xpc_object *xo;
	struct xpc_dict_head *head;
	struct xpc_dict_pair *pair;

	xo = xdict;
	head = &xo->xo_dict;

	TAILQ_FOREACH(pair, head, xo_link) {
		if (!strcmp(pair->key, key)) {
			if (value == NULL) {
				/* A NULL value removes the key, as on macOS. */
				TAILQ_REMOVE(head, pair, xo_link);
				xo->xo_size--;
				xpc_release(pair->value);
				free(pair->key);
				free(pair);
				return;
			}
			xpc_retain(value);
			xpc_release(pair->value);
			pair->value = value;
			return;
		}
	}
	if (value == NULL)
		return;

	/* The dictionary keeps its own copy of the key. */
	/* (No strdup: <string.h> here is the kernel's, via usr/local/include/kernel.) */
	size_t keylen = strlen(key) + 1;
	pair = malloc(sizeof(struct xpc_dict_pair));
	if (pair == NULL || (pair->key = malloc(keylen)) == NULL) {
		free(pair);
		return;
	}
	memcpy(pair->key, key, keylen);
	xo->xo_size++;
	pair->value = value;
	TAILQ_INSERT_TAIL(&xo->xo_dict, pair, xo_link);
	xpc_retain(value);
}

xpc_object_t
xpc_dictionary_get_value(xpc_object_t xdict, const char *key)
{
	struct xpc_object *xo;
	struct xpc_dict_head *head;
	struct xpc_dict_pair *pair;

	xo = xdict;
	head = &xo->xo_dict;

	TAILQ_FOREACH(pair, head, xo_link) {
		if (!strcmp(pair->key, key))
			return (pair->value);
	}

	return (NULL);
}

size_t
xpc_dictionary_get_count(xpc_object_t xdict)
{
	struct xpc_object *xo;

	xo = xdict;
	return (xo->xo_size);
}

void
xpc_dictionary_set_bool(xpc_object_t xdict, const char *key, bool value)
{;
	struct xpc_object *xo, *xotmp;

	xo = xdict;
	xotmp = xpc_bool_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_dictionary_set_int64(xpc_object_t xdict, const char *key, int64_t value)
{
	struct xpc_object *xo, *xotmp;

	xo = xdict;
	xotmp = xpc_int64_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_dictionary_set_uint64(xpc_object_t xdict, const char *key, uint64_t value)
{
	struct xpc_object *xo, *xotmp;

	xo = xdict;
	xotmp = xpc_uint64_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp); 
}

void
xpc_dictionary_set_string(xpc_object_t xdict, const char *key,
    const char *value)
{
	struct xpc_object *xo, *xotmp;

	xo = xdict;
	xotmp = xpc_string_create(value);
	xpc_dictionary_set_value(xdict, key, xotmp);
}

bool
xpc_dictionary_get_bool(xpc_object_t xdict, const char *key)
{
	xpc_object_t xo;

	xo = xpc_dictionary_get_value(xdict, key);
	return (xpc_bool_get_value(xo));
}

int64_t
xpc_dictionary_get_int64(xpc_object_t xdict, const char *key)
{
	xpc_object_t xo;

	xo = xpc_dictionary_get_value(xdict, key);
	return (xpc_int64_get_value(xo));
}

uint64_t
xpc_dictionary_get_uint64(xpc_object_t xdict, const char *key)
{
	xpc_object_t xo;

	xo = xpc_dictionary_get_value(xdict, key);
	return (xpc_uint64_get_value(xo));
}

const char *
xpc_dictionary_get_string(xpc_object_t xdict, const char *key)
{
	xpc_object_t xo;

	xo = xpc_dictionary_get_value(xdict, key);
	return (xpc_string_get_string_ptr(xo));
}

xpc_object_t
xpc_dictionary_get_dictionary(xpc_object_t xdict, const char *key)
{
    xpc_object_t xo;
    xo = xpc_dictionary_get_value(xdict, key); // FIXME: does this need to be unpacked? (see `mpack`)
    if(xo) {
        if(xpc_get_type(xo) == XPC_TYPE_DICTIONARY)
            xpc_retain(xo);
        else
            xo = NULL;
    }
    return xo;
}

xpc_object_t
xpc_dictionary_get_array(xpc_object_t xdict, const char *key)
{
    xpc_object_t xo;
    xo = xpc_dictionary_get_value(xdict, key); // FIXME: does this need to be unpacked? (see `mpack`)
    if(xo) {
        if(xpc_get_type(xo) == XPC_TYPE_ARRAY)
            xpc_retain(xo);
        else
            xo = NULL;
    }
    return xo;
}

void
xpc_dictionary_set_data(xpc_object_t xdict, const char *key, const void *bytes,
    size_t length)
{
	xpc_object_t xotmp;

	xotmp = xpc_data_create(bytes, length);
	xpc_dictionary_set_value(xdict, key, xotmp);
	xpc_release(xotmp);
}

const void *
xpc_dictionary_get_data(xpc_object_t xdict, const char *key, size_t *length)
{
	xpc_object_t xo;

	xo = xpc_dictionary_get_value(xdict, key);
	if (xo == NULL || xpc_get_type(xo) != XPC_TYPE_DATA) {
		if (length != NULL)
			*length = 0;
		return (NULL);
	}
	if (length != NULL)
		*length = xpc_data_get_length(xo);
	return (xpc_data_get_bytes_ptr(xo));
}

void
xpc_dictionary_set_uuid(xpc_object_t xdict, const char *key, const uuid_t uuid)
{
	xpc_object_t xotmp;

	xotmp = xpc_uuid_create(uuid);
	xpc_dictionary_set_value(xdict, key, xotmp);
	xpc_release(xotmp);
}

const uint8_t *
xpc_dictionary_get_uuid(xpc_object_t xdict, const char *key)
{
	xpc_object_t xo;

	xo = xpc_dictionary_get_value(xdict, key);
	if (xo == NULL || xpc_get_type(xo) != XPC_TYPE_UUID)
		return (NULL);
	return (xpc_uuid_get_bytes(xo));
}

bool
xpc_dictionary_apply(xpc_object_t xdict, xpc_dictionary_applier_t applier)
{
	struct xpc_object *xo, *xotmp;
	struct xpc_dict_head *head;
	struct xpc_dict_pair *pair;

	xo = xdict;
	head = &xo->xo_dict;

	TAILQ_FOREACH(pair, head, xo_link) {
		if (!applier(pair->key, pair->value))
			return (false);
	}

	return (true);
}

xpc_object_t
xpc_create_from_plist(uint8_t *buf, size_t length)
{
    return NULL; /* FIXME: stub */
}

int
xpc_dictionary_dup_fd(xpc_object_t dictionary, const char *key)
{
    xpc_object_t obj = xpc_dictionary_get_value(dictionary, key);
    if (obj == NULL) // not found or empty value
        return -1;
    return xpc_fd_dup(obj);
}

