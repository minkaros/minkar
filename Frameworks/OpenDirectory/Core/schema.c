/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The request and result format of OpenDirectory's protocol (odutils.h),
 * Minkar's own: { "function": name, "values": [ ... ] }, a property list,
 * so it travels as binary data. A property list cannot hold NULL, so a
 * NULL value is the marker { "null": true }, given back as NULL.
 */

#include "opendirectory/odutils.h"
#include <dispatch/dispatch.h>

#define kSchemaFunction	CFSTR("function")
#define kSchemaValues	CFSTR("values")
#define kSchemaNull	CFSTR("null")

static CFDictionaryRef
null_marker(void)
{
	static CFDictionaryRef marker;
	static dispatch_once_t once;

	dispatch_once(&once, ^{
		const void *k[] = { kSchemaNull }, *v[] = { kCFBooleanTrue };
		marker = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
	});
	return marker;
}

static int
is_null_marker(CFTypeRef value)
{
	return value == NULL || value == kCFNull ||
	    (CFGetTypeID(value) == CFDictionaryGetTypeID() && CFDictionaryGetCount(value) == 1 &&
	    CFDictionaryGetValue(value, kSchemaNull) == kCFBooleanTrue);
}

CFDictionaryRef
schema_construct_result(CFStringRef funcname, CFIndex count, const CFTypeRef *values)
{
	CFMutableArrayRef array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
	CFDictionaryRef result;

	for (CFIndex i = 0; i < count; i++)
		CFArrayAppendValue(array, values[i] != NULL ? values[i] : null_marker());
	const void *k[] = { kSchemaFunction, kSchemaValues }, *v[] = { funcname, array };
	result = CFDictionaryCreate(NULL, k, v, 2, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFRelease(array);
	return result;
}

CFDictionaryRef
schema_construct_requestv(CFStringRef funcname, CFIndex count, va_list ap)
{
	CFTypeRef values[count > 0 ? count : 1];

	if (funcname == NULL)
		return NULL;
	for (CFIndex i = 0; i < count; i++)
		values[i] = va_arg(ap, CFTypeRef);
	return schema_construct_result(funcname, count, values);
}

CFDictionaryRef
schema_construct_request(CFStringRef funcname, CFIndex count, ...)
{
	CFDictionaryRef request;
	va_list ap;

	va_start(ap, count);
	request = schema_construct_requestv(funcname, count, ap);
	va_end(ap);
	return request;
}

void
schema_deconstruct_result(CFDictionaryRef dict, CFStringRef *funcname, CFArrayRef *values)
{
	CFTypeRef name, array;

	if (funcname != NULL)
		*funcname = NULL;
	if (values != NULL)
		*values = NULL;
	if (dict == NULL || CFGetTypeID(dict) != CFDictionaryGetTypeID())
		return;
	name = CFDictionaryGetValue(dict, kSchemaFunction);
	array = CFDictionaryGetValue(dict, kSchemaValues);
	if (name == NULL || CFGetTypeID(name) != CFStringGetTypeID() ||
	    array == NULL || CFGetTypeID(array) != CFArrayGetTypeID())
		return;
	if (funcname != NULL)
		*funcname = CFRetain(name);
	if (values != NULL)
		*values = CFRetain(array);
}

CFTypeRef
schema_get_value_at_index(CFArrayRef values, CFIndex index)
{
	CFTypeRef value;

	if (values == NULL || index < 0 || index >= CFArrayGetCount(values))
		return NULL;
	value = CFArrayGetValueAtIndex(values, index);
	return is_null_marker(value) ? NULL : value;
}
