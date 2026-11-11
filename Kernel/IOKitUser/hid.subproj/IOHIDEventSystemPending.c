/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The HID event system's calls IOKit's own code makes (IOHIDEvent,
 * IOHIDEventSystemClient, IOHIDServiceClient...), until Minkar's event
 * system comes with WindowServer's input: no events, clients or services
 * yet, so each answers as an empty system would (NULL, 0, false) and the
 * release calls have nothing to free.
 *
 * They used to stay undefined (looked up when called). A process that
 * loads IOKit with every symbol bound at once (dlopen with RTLD_NOW, as
 * Python loads its modules) then failed to load it.
 *
 * The references are opaque pointers here, as in the headers.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <mach/boolean.h>
#include <stdint.h>

typedef const void *HIDRef;

void IOHIDEventAppendEvent(HIDRef event, HIDRef child, uint32_t flags) {
	(void)event; (void)child; (void)flags;
}

Boolean IOHIDEventConformsTo(HIDRef event, uint32_t type) {
	(void)event; (void)type;
	return false;
}

CFStringRef IOHIDEventCopyDescription(HIDRef event) {
	(void)event;
	return NULL;
}

HIDRef IOHIDEventCreateVendorDefinedEvent(CFAllocatorRef allocator, uint64_t timeStamp,
    uint16_t usagePage, uint16_t usage, uint32_t version, const uint8_t *data,
    CFIndex length, uint32_t options) {
	(void)allocator; (void)timeStamp; (void)usagePage; (void)usage;
	(void)version; (void)data; (void)length; (void)options;
	return NULL;
}

CFArrayRef IOHIDEventGetChildren(HIDRef event) {
	(void)event;
	return NULL;
}

CFIndex IOHIDEventGetIntegerValue(HIDRef event, uint32_t field) {
	(void)event; (void)field;
	return 0;
}

void IOHIDEventGetVendorDefinedData(HIDRef event, uint8_t **data, uint32_t *length) {
	(void)event;
	if (data != NULL)
		*data = NULL;
	if (length != NULL)
		*length = 0;
}

CFTypeRef IOHIDEventSystemClientCopyProperty(HIDRef client, CFStringRef key) {
	(void)client; (void)key;
	return NULL;
}

HIDRef IOHIDEventSystemClientCopyServiceForRegistryID(HIDRef client, CFStringRef entryID) {
	(void)client; (void)entryID;
	return NULL;
}

CFArrayRef IOHIDEventSystemClientCopyServices(HIDRef client) {
	(void)client;
	return NULL;
}

HIDRef IOHIDEventSystemClientCreateWithType(CFAllocatorRef allocator, uint32_t type, void *data) {
	(void)allocator; (void)type; (void)data;
	return NULL;
}

Boolean IOHIDEventSystemClientSetProperty(HIDRef client, CFStringRef key, CFTypeRef property) {
	(void)client; (void)key; (void)property;
	return false;
}

CFStringRef IOHIDEventSystemConnectionCopyDescription(HIDRef connection) {
	(void)connection;
	return NULL;
}

void _IOHIDEventSystemConnectionReleasePrivate(HIDRef connection) {
	(void)connection;
}

boolean_t IOHIDServiceClientConformsTo(HIDRef service, uint32_t usagePage, uint32_t usage) {
	(void)service; (void)usagePage; (void)usage;
	return 0;
}

CFStringRef IOHIDServiceClientCopyDescription(HIDRef service) {
	(void)service;
	return NULL;
}

CFTypeRef IOHIDServiceClientCopyProperty(HIDRef service, CFStringRef key) {
	(void)service; (void)key;
	return NULL;
}

void _IOHIDServiceClientReleasePrivate(HIDRef service) {
	(void)service;
}

Boolean IOHIDServiceClientSetProperty(HIDRef service, CFStringRef key, CFTypeRef property) {
	(void)service; (void)key; (void)property;
	return false;
}

CFStringRef IOHIDServiceCopyDescription(HIDRef service) {
	(void)service;
	return NULL;
}

void _IOHIDServiceReleasePrivate(HIDRef service) {
	(void)service;
}

void _IOHIDSessionReleasePrivate(HIDRef session) {
	(void)session;
}
