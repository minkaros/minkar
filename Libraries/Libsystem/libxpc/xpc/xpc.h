/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * XPC: typed objects (dictionaries, arrays, strings, numbers, file
 * descriptors, ...) and the messages and connections that carry them
 * between processes. Declarations of what libxpc provides.
 */

#ifndef __XPC_H__
#define __XPC_H__

#include <os/object.h>
#include <dispatch/dispatch.h>
#include <sys/mman.h>
#include <uuid/uuid.h>
#include <bsm/audit.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef __XPC_INDIRECT__
#define __XPC_INDIRECT__
#endif
#include <xpc/base.h>

__BEGIN_DECLS

#define XPC_API_VERSION 20121012

/* Types. */

typedef const struct _xpc_type_s *xpc_type_t;
#ifndef __XPC_BUILDING_XPC__
#define XPC_TYPE(type) const struct _xpc_type_s type
#endif

#if OS_OBJECT_USE_OBJC
OS_OBJECT_DECL(xpc_object);
#ifndef __XPC_PROJECT_BUILD__
#define XPC_DECL(name) typedef xpc_object_t name##_t
#endif
#define XPC_GLOBAL_OBJECT(object) ((OS_OBJECT_BRIDGE xpc_object_t)&(object))
#define XPC_RETURNS_RETAINED OS_OBJECT_RETURNS_RETAINED
XPC_INLINE XPC_NONNULL_ALL void
_xpc_object_validate(xpc_object_t object)
{
	/* Reads the object's first word, so a bad pointer faults here. */
	void *first = *(void * volatile *)(OS_OBJECT_BRIDGE void *)object;
	(void)first;
}
#else
typedef void *xpc_object_t;
#define XPC_DECL(name) typedef struct _##name##_s *name##_t
#define XPC_GLOBAL_OBJECT(object) ((xpc_object_t)(uintptr_t)&(object))
#define XPC_RETURNS_RETAINED
#endif

XPC_DECL(xpc_connection);
XPC_DECL(xpc_endpoint);

#ifdef __BLOCKS__
typedef void (^xpc_handler_t)(xpc_object_t object);
typedef bool (^xpc_array_applier_t)(size_t index, xpc_object_t value);
typedef bool (^xpc_dictionary_applier_t)(const char *key, xpc_object_t value);
#endif
typedef void (*xpc_connection_handler_t)(xpc_connection_t connection);

/* Each type, for comparison with xpc_get_type(). */

#define XPC_TYPE_CONNECTION	(&_xpc_type_connection)
#define XPC_TYPE_ENDPOINT	(&_xpc_type_endpoint)
#define XPC_TYPE_NULL		(&_xpc_type_null)
#define XPC_TYPE_BOOL		(&_xpc_type_bool)
#define XPC_TYPE_INT64		(&_xpc_type_int64)
#define XPC_TYPE_UINT64		(&_xpc_type_uint64)
#define XPC_TYPE_DOUBLE		(&_xpc_type_double)
#define XPC_TYPE_DATE		(&_xpc_type_date)
#define XPC_TYPE_DATA		(&_xpc_type_data)
#define XPC_TYPE_STRING		(&_xpc_type_string)
#define XPC_TYPE_UUID		(&_xpc_type_uuid)
#define XPC_TYPE_FD		(&_xpc_type_fd)
#define XPC_TYPE_SHMEM		(&_xpc_type_shmem)
#define XPC_TYPE_ARRAY		(&_xpc_type_array)
#define XPC_TYPE_DICTIONARY	(&_xpc_type_dictionary)
#define XPC_TYPE_ERROR		(&_xpc_type_error)

XPC_EXPORT XPC_TYPE(_xpc_type_connection);
XPC_EXPORT XPC_TYPE(_xpc_type_endpoint);
XPC_EXPORT XPC_TYPE(_xpc_type_null);
XPC_EXPORT XPC_TYPE(_xpc_type_bool);
XPC_EXPORT XPC_TYPE(_xpc_type_int64);
XPC_EXPORT XPC_TYPE(_xpc_type_uint64);
XPC_EXPORT XPC_TYPE(_xpc_type_double);
XPC_EXPORT XPC_TYPE(_xpc_type_date);
XPC_EXPORT XPC_TYPE(_xpc_type_data);
XPC_EXPORT XPC_TYPE(_xpc_type_string);
XPC_EXPORT XPC_TYPE(_xpc_type_uuid);
XPC_EXPORT XPC_TYPE(_xpc_type_fd);
XPC_EXPORT XPC_TYPE(_xpc_type_shmem);
XPC_EXPORT XPC_TYPE(_xpc_type_array);
XPC_EXPORT XPC_TYPE(_xpc_type_dictionary);
XPC_EXPORT XPC_TYPE(_xpc_type_error);

/* The two booleans, shared by every process. */
#define XPC_BOOL_TRUE	XPC_GLOBAL_OBJECT(_xpc_bool_true)
#define XPC_BOOL_FALSE	XPC_GLOBAL_OBJECT(_xpc_bool_false)
XPC_EXPORT const struct _xpc_bool_s _xpc_bool_true;
XPC_EXPORT const struct _xpc_bool_s _xpc_bool_false;

/* The key of an error dictionary's description string. */
#define XPC_ERROR_KEY_DESCRIPTION _xpc_error_key_description
XPC_EXPORT const char *const _xpc_error_key_description;

/* The key of an event's name in an event stream's message. */
#define XPC_EVENT_KEY_NAME _xpc_event_key_name
XPC_EXPORT const char *const _xpc_event_key_name;

#ifndef __XPC_BUILDING_XPC__
#include <xpc/endpoint.h>
#include <xpc/debug.h>
#ifdef __BLOCKS__
#include <xpc/connection.h>
#include <xpc/activity.h>
#endif
#undef __XPC_INDIRECT__
#ifdef __APPLE__
#include <launch.h>
#endif
#endif

/* Any object: reference counting, copies, comparison. */

XPC_EXPORT XPC_NONNULL1
xpc_object_t xpc_retain(xpc_object_t object);
XPC_EXPORT XPC_NONNULL1
void xpc_release(xpc_object_t object);
#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
#undef xpc_retain
#define xpc_retain(object) ({ xpc_object_t _xo = (object); \
	_xpc_object_validate(_xo); [_xo retain]; })
#undef xpc_release
#define xpc_release(object) ({ xpc_object_t _xo = (object); \
	_xpc_object_validate(_xo); [_xo release]; })
#endif

XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_type_t xpc_get_type(xpc_object_t object);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_object_t xpc_copy(xpc_object_t object);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
bool xpc_equal(xpc_object_t object1, xpc_object_t object2);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
size_t xpc_hash(xpc_object_t object);
/* A description for people to read; the caller frees it. */
XPC_EXPORT XPC_MALLOC XPC_WARN_RESULT XPC_NONNULL1
char *xpc_copy_description(xpc_object_t object);

/* Null, booleans and numbers. */

XPC_EXPORT XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_null_create(void);
XPC_EXPORT XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_bool_create(bool value);
XPC_EXPORT
bool xpc_bool_get_value(xpc_object_t xbool);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_int64_create(int64_t value);
XPC_EXPORT
int64_t xpc_int64_get_value(xpc_object_t xint);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_uint64_create(uint64_t value);
XPC_EXPORT
uint64_t xpc_uint64_get_value(xpc_object_t xuint);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_double_create(double value);
XPC_EXPORT
double xpc_double_get_value(xpc_object_t xdouble);

/* Dates: nanoseconds since 1 January 1970. */

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_date_create(int64_t interval);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_date_create_from_current(void);
XPC_EXPORT
int64_t xpc_date_get_value(xpc_object_t xdate);

/* Data. */

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_data_create(const void *bytes, size_t length);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_object_t xpc_data_create_with_dispatch_data(dispatch_data_t ddata);
XPC_EXPORT
size_t xpc_data_get_length(xpc_object_t xdata);
XPC_EXPORT
const void *xpc_data_get_bytes_ptr(xpc_object_t xdata);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1 XPC_NONNULL2
size_t xpc_data_get_bytes(xpc_object_t xdata, void *buffer, size_t offset,
    size_t length);

/* Strings (UTF-8). */

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_object_t xpc_string_create(const char *string);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
XPC_PRINTF(1, 2)
xpc_object_t xpc_string_create_with_format(const char *format, ...);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
XPC_PRINTF(1, 0)
xpc_object_t xpc_string_create_with_format_and_arguments(const char *format,
    va_list args);
XPC_EXPORT
size_t xpc_string_get_length(xpc_object_t xstring);
XPC_EXPORT
const char *xpc_string_get_string_ptr(xpc_object_t xstring);

/* UUIDs. */

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_object_t xpc_uuid_create(const uuid_t uuid);
XPC_EXPORT XPC_NONNULL1
const uint8_t *xpc_uuid_get_bytes(xpc_object_t xuuid);

/* File descriptors, sent as a duplicate. */

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_fd_create(int fd);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
int xpc_fd_dup(xpc_object_t xfd);

/* Shared memory: a region mapped again in the receiver. */

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_object_t xpc_shmem_create(void *region, size_t length);
XPC_EXPORT XPC_NONNULL_ALL
size_t xpc_shmem_map(xpc_object_t xshmem, void **region);

/* Arrays. XPC_ARRAY_APPEND as an index adds at the end. */

#define XPC_ARRAY_APPEND ((size_t)(-1))

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_array_create(const xpc_object_t *objects, size_t count);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_array_set_value(xpc_object_t xarray, size_t index, xpc_object_t value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_array_append_value(xpc_object_t xarray, xpc_object_t value);
XPC_EXPORT XPC_NONNULL1
size_t xpc_array_get_count(xpc_object_t xarray);
XPC_EXPORT XPC_NONNULL_ALL
xpc_object_t xpc_array_get_value(xpc_object_t xarray, size_t index);
#ifdef __BLOCKS__
XPC_EXPORT XPC_NONNULL_ALL
bool xpc_array_apply(xpc_object_t xarray, xpc_array_applier_t applier);
#endif

XPC_EXPORT XPC_NONNULL1
void xpc_array_set_bool(xpc_object_t xarray, size_t index, bool value);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_int64(xpc_object_t xarray, size_t index, int64_t value);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_uint64(xpc_object_t xarray, size_t index, uint64_t value);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_double(xpc_object_t xarray, size_t index, double value);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_date(xpc_object_t xarray, size_t index, int64_t value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_array_set_data(xpc_object_t xarray, size_t index, const void *bytes,
    size_t length);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_array_set_string(xpc_object_t xarray, size_t index,
    const char *string);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_array_set_uuid(xpc_object_t xarray, size_t index, const uuid_t uuid);
XPC_EXPORT XPC_NONNULL1
void xpc_array_set_fd(xpc_object_t xarray, size_t index, int fd);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_array_set_connection(xpc_object_t xarray, size_t index,
    xpc_connection_t connection);

XPC_EXPORT XPC_NONNULL1
bool xpc_array_get_bool(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_NONNULL1
int64_t xpc_array_get_int64(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_NONNULL1
uint64_t xpc_array_get_uint64(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_NONNULL1
double xpc_array_get_double(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_NONNULL1
int64_t xpc_array_get_date(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_NONNULL1
const void *xpc_array_get_data(xpc_object_t xarray, size_t index,
    size_t *length);
XPC_EXPORT XPC_NONNULL1
const char *xpc_array_get_string(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_NONNULL1
const uint8_t *xpc_array_get_uuid(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL1
int xpc_array_dup_fd(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL1
xpc_connection_t xpc_array_create_connection(xpc_object_t xarray, size_t index);
XPC_EXPORT XPC_NONNULL1
xpc_connection_t xpc_array_get_connection(xpc_object_t xarray, size_t index);

/* Dictionaries, keyed by C strings. */

XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT
xpc_object_t xpc_dictionary_create(const char *const *keys,
    const xpc_object_t *values, size_t count);
/* A reply to a message received on a connection, or NULL if it wants none. */
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_object_t xpc_dictionary_create_reply(xpc_object_t original);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_value(xpc_object_t xdict, const char *key,
    xpc_object_t value);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_object_t xpc_dictionary_get_value(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_NONNULL1
size_t xpc_dictionary_get_count(xpc_object_t xdict);
#ifdef __BLOCKS__
XPC_EXPORT XPC_NONNULL_ALL
bool xpc_dictionary_apply(xpc_object_t xdict, xpc_dictionary_applier_t applier);
#endif
/* The connection a received message came from. */
XPC_EXPORT XPC_NONNULL_ALL
xpc_connection_t xpc_dictionary_get_remote_connection(xpc_object_t xdict);

XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_bool(xpc_object_t xdict, const char *key, bool value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_int64(xpc_object_t xdict, const char *key,
    int64_t value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_uint64(xpc_object_t xdict, const char *key,
    uint64_t value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_double(xpc_object_t xdict, const char *key,
    double value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_date(xpc_object_t xdict, const char *key,
    int64_t value);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL3
void xpc_dictionary_set_data(xpc_object_t xdict, const char *key,
    const void *bytes, size_t length);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL3
void xpc_dictionary_set_string(xpc_object_t xdict, const char *key,
    const char *string);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL3
void xpc_dictionary_set_uuid(xpc_object_t xdict, const char *key,
    const uuid_t uuid);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
void xpc_dictionary_set_fd(xpc_object_t xdict, const char *key, int fd);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2 XPC_NONNULL3
void xpc_dictionary_set_connection(xpc_object_t xdict, const char *key,
    xpc_connection_t connection);

XPC_EXPORT XPC_NONNULL_ALL
bool xpc_dictionary_get_bool(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_NONNULL_ALL
int64_t xpc_dictionary_get_int64(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_NONNULL_ALL
uint64_t xpc_dictionary_get_uint64(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_NONNULL_ALL
double xpc_dictionary_get_double(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_NONNULL_ALL
int64_t xpc_dictionary_get_date(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL2
const void *xpc_dictionary_get_data(xpc_object_t xdict, const char *key,
    size_t *length);
XPC_EXPORT XPC_NONNULL_ALL
const char *xpc_dictionary_get_string(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_NONNULL_ALL
const uint8_t *xpc_dictionary_get_uuid(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_WARN_RESULT XPC_NONNULL_ALL
int xpc_dictionary_dup_fd(xpc_object_t xdict, const char *key);
XPC_EXPORT XPC_MALLOC XPC_RETURNS_RETAINED XPC_WARN_RESULT XPC_NONNULL_ALL
xpc_connection_t xpc_dictionary_create_connection(xpc_object_t xdict,
    const char *key);
XPC_EXPORT XPC_NONNULL_ALL
xpc_object_t xpc_dictionary_get_dictionary(xpc_object_t xdict,
    const char *key);
XPC_EXPORT XPC_NONNULL_ALL
xpc_object_t xpc_dictionary_get_array(xpc_object_t xdict, const char *key);

/* Services. */

/* Runs an XPC service: handler gets each new connection. Never returns. */
XPC_EXPORT XPC_NORETURN XPC_NONNULL1
void xpc_main(xpc_connection_handler_t handler);

#if XPC_HOSTING_OLD_MAIN
typedef void (*xpc_service_event_handler_t)(xpc_connection_t, xpc_object_t);
XPC_EXPORT XPC_NORETURN XPC_NONNULL3
void xpc_service_main(int argc, const char *argv[],
    xpc_service_event_handler_t handler);
#endif

/* A transaction keeps the service from being stopped while it is open. */
XPC_EXPORT
void xpc_transaction_begin(void);
XPC_EXPORT
void xpc_transaction_end(void);

#ifdef __BLOCKS__
/* Events that launchd delivers for the named stream. */
XPC_EXPORT XPC_NONNULL1 XPC_NONNULL3
void xpc_set_event_stream_handler(const char *stream,
    dispatch_queue_t targetq, xpc_handler_t handler);
#endif

__END_DECLS

#endif /* __XPC_H__ */
