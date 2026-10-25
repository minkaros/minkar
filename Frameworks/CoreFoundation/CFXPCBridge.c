/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Converts property-list objects between Core Foundation and XPC, for
 * clients that send CF data over XPC connections (IOKit's power management,
 * for example).
 *
 *   CFString      <-> string          CFBoolean     <-> bool
 *   CFNumber      <-> int64/double    CFData        <-> data
 *   CFDate        <-> date            CFUUID        <-> uuid
 *   CFArray       <-> array           CFDictionary  <-> dictionary
 *   CFNull        <-> null
 *
 * Dictionary keys must be strings. XPC uint64 values become SInt64
 * CFNumbers (bit for bit). Anything else cannot be converted and makes the
 * whole conversion return NULL.
 */

#include <CoreFoundation/CFXPCBridge.h>
#include <CoreFoundation/CoreFoundation.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <xpc/xpc.h>

/* The key under which a "message" carries its object. */
#define CFXPC_MESSAGE_KEY "_CFXPCMessageObject"

/* CFDate counts seconds from 2001; xpc dates nanoseconds from 1970. */
#define NSEC_PER_SEC_D 1000000000.0

static xpc_object_t createXPCFromCF(CFTypeRef cf);

static bool
appendArrayValue(CFTypeRef value, xpc_object_t array)
{
	xpc_object_t xo = createXPCFromCF(value);

	if (xo == NULL)
		return false;
	xpc_array_append_value(array, xo);
	xpc_release(xo);
	return true;
}

struct dictContext {
	xpc_object_t dict;
	bool ok;
};

static void
setDictionaryValue(const void *key, const void *value, void *ctx)
{
	struct dictContext *c = ctx;
	xpc_object_t xo;
	char stackKey[256], *k = stackKey;
	CFIndex len;

	if (!c->ok)
		return;
	if (CFGetTypeID(key) != CFStringGetTypeID()) {
		c->ok = false;
		return;
	}
	len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(key),
	    kCFStringEncodingUTF8) + 1;
	if (len > (CFIndex)sizeof(stackKey) && (k = malloc(len)) == NULL) {
		c->ok = false;
		return;
	}
	if (!CFStringGetCString(key, k, len, kCFStringEncodingUTF8) ||
	    (xo = createXPCFromCF(value)) == NULL) {
		c->ok = false;
	} else {
		xpc_dictionary_set_value(c->dict, k, xo);
		xpc_release(xo);
	}
	if (k != stackKey)
		free(k);
}

static xpc_object_t
createXPCString(CFStringRef s)
{
	const char *p = CFStringGetCStringPtr(s, kCFStringEncodingUTF8);
	CFIndex len;
	char *buf;
	xpc_object_t xo;

	if (p != NULL)
		return xpc_string_create(p);
	len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
	    kCFStringEncodingUTF8) + 1;
	if ((buf = malloc(len)) == NULL)
		return NULL;
	xo = CFStringGetCString(s, buf, len, kCFStringEncodingUTF8) ?
	    xpc_string_create(buf) : NULL;
	free(buf);
	return xo;
}

static xpc_object_t
createXPCFromCF(CFTypeRef cf)
{
	CFTypeID type;

	if (cf == NULL)
		return NULL;
	type = CFGetTypeID(cf);

	if (type == CFStringGetTypeID())
		return createXPCString(cf);
	if (type == CFBooleanGetTypeID())
		return xpc_bool_create(CFBooleanGetValue(cf));
	if (type == CFNumberGetTypeID()) {
		if (CFNumberIsFloatType(cf)) {
			double d;
			CFNumberGetValue(cf, kCFNumberDoubleType, &d);
			return xpc_double_create(d);
		}
		int64_t i;
		CFNumberGetValue(cf, kCFNumberSInt64Type, &i);
		return xpc_int64_create(i);
	}
	if (type == CFDataGetTypeID())
		return xpc_data_create(CFDataGetBytePtr(cf), CFDataGetLength(cf));
	if (type == CFDateGetTypeID()) {
		double secs = CFDateGetAbsoluteTime(cf) + kCFAbsoluteTimeIntervalSince1970;
		return xpc_date_create((int64_t)(secs * NSEC_PER_SEC_D));
	}
	if (type == CFUUIDGetTypeID()) {
		CFUUIDBytes b = CFUUIDGetUUIDBytes(cf);
		return xpc_uuid_create((const unsigned char *)&b);
	}
	if (type == CFNullGetTypeID())
		return xpc_null_create();
	if (type == CFArrayGetTypeID()) {
		xpc_object_t array = xpc_array_create(NULL, 0);
		CFIndex i, n = CFArrayGetCount(cf);

		for (i = 0; i < n; i++) {
			if (!appendArrayValue(CFArrayGetValueAtIndex(cf, i), array)) {
				xpc_release(array);
				return NULL;
			}
		}
		return array;
	}
	if (type == CFDictionaryGetTypeID()) {
		struct dictContext c = { xpc_dictionary_create(NULL, NULL, 0), true };

		CFDictionaryApplyFunction(cf, setDictionaryValue, &c);
		if (!c.ok) {
			xpc_release(c.dict);
			return NULL;
		}
		return c.dict;
	}
	return NULL;
}

static CFTypeRef
createCFFromXPC(xpc_object_t xo)
{
	xpc_type_t type;

	if (xo == NULL)
		return NULL;
	type = xpc_get_type(xo);

	if (type == XPC_TYPE_STRING)
		return CFStringCreateWithCString(kCFAllocatorDefault,
		    xpc_string_get_string_ptr(xo), kCFStringEncodingUTF8);
	if (type == XPC_TYPE_BOOL)
		return CFRetain(xpc_bool_get_value(xo) ? kCFBooleanTrue : kCFBooleanFalse);
	if (type == XPC_TYPE_INT64) {
		int64_t i = xpc_int64_get_value(xo);
		return CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &i);
	}
	if (type == XPC_TYPE_UINT64) {
		uint64_t u = xpc_uint64_get_value(xo);
		int64_t i;
		memcpy(&i, &u, sizeof(i));
		return CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &i);
	}
	if (type == XPC_TYPE_DOUBLE) {
		double d = xpc_double_get_value(xo);
		return CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &d);
	}
	if (type == XPC_TYPE_DATA)
		return CFDataCreate(kCFAllocatorDefault, xpc_data_get_bytes_ptr(xo),
		    (CFIndex)xpc_data_get_length(xo));
	if (type == XPC_TYPE_DATE) {
		double secs = (double)xpc_date_get_value(xo) / NSEC_PER_SEC_D;
		return CFDateCreate(kCFAllocatorDefault,
		    secs - kCFAbsoluteTimeIntervalSince1970);
	}
	if (type == XPC_TYPE_UUID) {
		CFUUIDBytes b;
		memcpy(&b, xpc_uuid_get_bytes(xo), sizeof(b));
		return CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, b);
	}
	if (type == XPC_TYPE_NULL)
		return CFRetain(kCFNull);
	if (type == XPC_TYPE_ARRAY) {
		size_t i, n = xpc_array_get_count(xo);
		CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorDefault,
		    (CFIndex)n, &kCFTypeArrayCallBacks);

		for (i = 0; i < n; i++) {
			CFTypeRef v = createCFFromXPC(xpc_array_get_value(xo, i));
			if (v == NULL) {
				CFRelease(array);
				return NULL;
			}
			CFArrayAppendValue(array, v);
			CFRelease(v);
		}
		return array;
	}
	if (type == XPC_TYPE_DICTIONARY) {
		CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
		    kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		__block bool ok = true;

		xpc_dictionary_apply(xo, ^bool(const char *key, xpc_object_t value) {
			CFStringRef k = CFStringCreateWithCString(kCFAllocatorDefault,
			    key, kCFStringEncodingUTF8);
			CFTypeRef v = createCFFromXPC(value);

			if (k != NULL && v != NULL)
				CFDictionarySetValue(dict, k, v);
			else
				ok = false;
			if (k != NULL)
				CFRelease(k);
			if (v != NULL)
				CFRelease(v);
			return ok;
		});
		if (!ok) {
			CFRelease(dict);
			return NULL;
		}
		return dict;
	}
	return NULL;
}

xpc_object_t
_CFXPCCreateXPCObjectFromCFObject(CFTypeRef cf)
{
	return createXPCFromCF(cf);
}

CFTypeRef
_CFXPCCreateCFObjectFromXPCObject(xpc_object_t xo)
{
	return createCFFromXPC(xo);
}

xpc_object_t
_CFXPCCreateXPCMessageWithCFObject(CFTypeRef cf)
{
	xpc_object_t xo = createXPCFromCF(cf);
	xpc_object_t msg;

	if (xo == NULL)
		return NULL;
	msg = xpc_dictionary_create(NULL, NULL, 0);
	xpc_dictionary_set_value(msg, CFXPC_MESSAGE_KEY, xo);
	xpc_release(xo);
	return msg;
}

CFTypeRef
_CFXPCCreateCFObjectFromXPCMessage(xpc_object_t msg)
{
	if (msg == NULL || xpc_get_type(msg) != XPC_TYPE_DICTIONARY)
		return NULL;
	return createCFFromXPC(xpc_dictionary_get_value(msg, CFXPC_MESSAGE_KEY));
}

/*
 * Power management tells the run loop why the process holds or dropped a
 * power assertion. On macOS the reason only feeds diagnostics; Minkar
 * keeps no run loop assertion state, so there is nothing to record.
 */
void
__CFRunLoopSetOptionsReason(CFOptionFlags opts, CFStringRef reason)
{
	(void)opts;
	(void)reason;
}
