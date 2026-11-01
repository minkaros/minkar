/*
 * Copyright 2014-2015 iXsystems, Inc.
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

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <xpc/xpc.h>
#include "xpc_internal.h"

/*
 * Minkar: an array keeps its elements in a vector of retained pointers
 * (xo_array.items), xo_size of them. Elements were linked through their
 * own xo_link before, so an object could be in one array only, the count
 * never grew, and a replaced element was freed while others held it.
 */

static bool
xpc_array_reserve(struct xpc_object *xo, size_t count)
{
	struct xpc_object **items;
	size_t capacity;

	if (count <= xo->xo_array.capacity)
		return (true);
	capacity = xo->xo_array.capacity ? xo->xo_array.capacity * 2 : 8;
	while (capacity < count)
		capacity *= 2;
	items = realloc(xo->xo_array.items, capacity * sizeof(*items));
	if (items == NULL)
		return (false);
	xo->xo_array.items = items;
	xo->xo_array.capacity = capacity;
	return (true);
}

static bool
xpc_array_check(xpc_object_t xarray)
{
	return (xarray != NULL &&
	    ((struct xpc_object *)xarray)->xo_xpc_type == _XPC_TYPE_ARRAY);
}

xpc_object_t
xpc_array_create(const xpc_object_t *objects, size_t count)
{
	struct xpc_object *xo;
	size_t i;
	xpc_u val;

	memset(&val, 0, sizeof(val));
	xo = _xpc_prim_create(_XPC_TYPE_ARRAY, val, 0);
	if (xo == NULL)
		return (NULL);
	for (i = 0; i < count; i++)
		xpc_array_append_value(xo, objects[i]);
	return (xo);
}

/* Every element released, the vector freed (xpc_object_destroy). */
__private_extern__ void
_xpc_array_destroy(struct xpc_object *xo)
{
	size_t i;

	for (i = 0; i < xo->xo_size; i++)
		xpc_release(xo->xo_array.items[i]);
	free(xo->xo_array.items);
	xo->xo_array.items = NULL;
	xo->xo_array.capacity = 0;
	xo->xo_size = 0;
}

void
xpc_array_set_value(xpc_object_t xarray, size_t index, xpc_object_t value)
{
	struct xpc_object *xo = xarray, *old;

	if (!xpc_array_check(xarray) || value == NULL)
		return;
	if (index == XPC_ARRAY_APPEND) {
		xpc_array_append_value(xarray, value);
		return;
	}
	if (index >= xo->xo_size)
		return;
	old = xo->xo_array.items[index];
	xo->xo_array.items[index] = xpc_retain(value);
	xpc_release(old);
}

void
xpc_array_append_value(xpc_object_t xarray, xpc_object_t value)
{
	struct xpc_object *xo = xarray;

	if (!xpc_array_check(xarray) || value == NULL)
		return;
	if (!xpc_array_reserve(xo, xo->xo_size + 1))
		return;
	xo->xo_array.items[xo->xo_size++] = xpc_retain(value);
}

xpc_object_t
xpc_array_get_value(xpc_object_t xarray, size_t index)
{
	struct xpc_object *xo = xarray;

	if (!xpc_array_check(xarray) || index >= xo->xo_size)
		return (NULL);
	return (xo->xo_array.items[index]);
}

size_t
xpc_array_get_count(xpc_object_t xarray)
{
	if (!xpc_array_check(xarray))
		return (0);
	return (((struct xpc_object *)xarray)->xo_size);
}

/* The setters take a new object: the array keeps it, the setter lets go. */
static void
xpc_array_set_new(xpc_object_t xarray, size_t index, xpc_object_t value)
{
	if (value == NULL)
		return;
	xpc_array_set_value(xarray, index, value);
	xpc_release(value);
}

void
xpc_array_set_bool(xpc_object_t xarray, size_t index, bool value)
{
	xpc_array_set_value(xarray, index, value ? XPC_BOOL_TRUE : XPC_BOOL_FALSE);
}

void
xpc_array_set_int64(xpc_object_t xarray, size_t index, int64_t value)
{
	xpc_array_set_new(xarray, index, xpc_int64_create(value));
}

void
xpc_array_set_uint64(xpc_object_t xarray, size_t index, uint64_t value)
{
	xpc_array_set_new(xarray, index, xpc_uint64_create(value));
}

void
xpc_array_set_double(xpc_object_t xarray, size_t index, double value)
{
	xpc_array_set_new(xarray, index, xpc_double_create(value));
}

void
xpc_array_set_date(xpc_object_t xarray, size_t index, int64_t value)
{
	xpc_array_set_new(xarray, index, xpc_date_create(value));
}

void
xpc_array_set_data(xpc_object_t xarray, size_t index, const void *data,
    size_t length)
{
	xpc_array_set_new(xarray, index, xpc_data_create(data, length));
}

void
xpc_array_set_string(xpc_object_t xarray, size_t index, const char *string)
{
	xpc_array_set_new(xarray, index, xpc_string_create(string));
}

void
xpc_array_set_uuid(xpc_object_t xarray, size_t index, const uuid_t value)
{
	xpc_array_set_new(xarray, index, xpc_uuid_create(value));
}

void
xpc_array_set_fd(xpc_object_t xarray, size_t index, int value)
{
	xpc_array_set_new(xarray, index, xpc_fd_create(value));
}

void
xpc_array_set_connection(xpc_object_t xarray, size_t index,
    xpc_connection_t value)
{
	xpc_array_set_value(xarray, index, value);
}

/* The getters give zero, NULL or -1 for an index out of range or another type. */
static struct xpc_object *
xpc_array_get_typed(xpc_object_t xarray, size_t index, xpc_type_t type)
{
	xpc_object_t xo = xpc_array_get_value(xarray, index);

	if (xo == NULL || xpc_get_type(xo) != type)
		return (NULL);
	return (xo);
}

bool
xpc_array_get_bool(xpc_object_t xarray, size_t index)
{
	struct xpc_object *xo = xpc_array_get_typed(xarray, index, XPC_TYPE_BOOL);

	return (xo != NULL && xpc_bool_get_value(xo));
}

int64_t
xpc_array_get_int64(xpc_object_t xarray, size_t index)
{
	struct xpc_object *xo = xpc_array_get_typed(xarray, index, XPC_TYPE_INT64);

	/* Signed and unsigned are one type on the wire (xpc_dictionary.c). */
	if (xo == NULL && (xo = xpc_array_get_typed(xarray, index, XPC_TYPE_UINT64)) != NULL)
		return ((int64_t)xpc_uint64_get_value(xo));
	return (xo != NULL ? xpc_int64_get_value(xo) : 0);
}

uint64_t
xpc_array_get_uint64(xpc_object_t xarray, size_t index)
{
	struct xpc_object *xo = xpc_array_get_typed(xarray, index, XPC_TYPE_UINT64);

	/* Signed and unsigned are one type on the wire (xpc_dictionary.c). */
	if (xo == NULL && (xo = xpc_array_get_typed(xarray, index, XPC_TYPE_INT64)) != NULL)
		return ((uint64_t)xpc_int64_get_value(xo));
	return (xo != NULL ? xpc_uint64_get_value(xo) : 0);
}

double
xpc_array_get_double(xpc_object_t xarray, size_t index)
{
	struct xpc_object *xo = xpc_array_get_typed(xarray, index, XPC_TYPE_DOUBLE);

	return (xo != NULL ? xpc_double_get_value(xo) : 0);
}

int64_t
xpc_array_get_date(xpc_object_t xarray, size_t index)
{
	struct xpc_object *xo = xpc_array_get_typed(xarray, index, XPC_TYPE_DATE);

	return (xo != NULL ? xpc_date_get_value(xo) : 0);
}

const void *
xpc_array_get_data(xpc_object_t xarray, size_t index, size_t *length)
{
	struct xpc_object *xo = xpc_array_get_typed(xarray, index, XPC_TYPE_DATA);

	if (length != NULL)
		*length = xo != NULL ? xpc_data_get_length(xo) : 0;
	return (xo != NULL ? xpc_data_get_bytes_ptr(xo) : NULL);
}

const uint8_t *
xpc_array_get_uuid(xpc_object_t xarray, size_t index)
{
	struct xpc_object *xo = xpc_array_get_typed(xarray, index, XPC_TYPE_UUID);

	return (xo != NULL ? xpc_uuid_get_bytes(xo) : NULL);
}

const char *
xpc_array_get_string(xpc_object_t xarray, size_t index)
{
	struct xpc_object *xo = xpc_array_get_typed(xarray, index, XPC_TYPE_STRING);

	return (xo != NULL ? xpc_string_get_string_ptr(xo) : NULL);
}

int
xpc_array_dup_fd(xpc_object_t xarray, size_t index)
{
	struct xpc_object *xo = xpc_array_get_typed(xarray, index, XPC_TYPE_FD);

	return (xo != NULL ? xpc_fd_dup(xo) : -1);
}

xpc_connection_t
xpc_array_get_connection(xpc_object_t xarray, size_t index)
{
	return ((xpc_connection_t)xpc_array_get_typed(xarray, index, XPC_TYPE_CONNECTION));
}

bool
xpc_array_apply(xpc_object_t xarray, xpc_array_applier_t applier)
{
	struct xpc_object *xo = xarray;
	size_t i;

	if (!xpc_array_check(xarray))
		return (true);
	for (i = 0; i < xo->xo_size; i++)
		if (!applier(i, xo->xo_array.items[i]))
			return (false);
	return (true);
}
