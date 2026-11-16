/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * distnoted: distributed notifications (Foundation's
 * NSDistributedNotificationCenter). A process checks in with its port and
 * says which notifications it observes (a name and an object, either
 * empty for any); a notification posted goes to every process of the same
 * user observing it (the poster too), or of every user with
 * kDNPostToAllSessions. Delivery never waits on a process: a full queue
 * loses the notification for that process alone. A process's
 * observations go when its port dies. Started on demand by launchd.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "DistributedNotificationTypes.h"
#include "DistributedNotificationsServer.h"	/* MIG, generated */

typedef struct {
	char *name;	/* empty: any */
	char *object;
} Observation;

typedef struct Client {
	struct Client *next;
	pid_t pid;
	uid_t uid;
	mach_port_t port;
	Observation *observations;
	int count, capacity;
} Client;

static Client *clients;
static mach_port_t notifyPort = MACH_PORT_NULL;

static Client *clientFor(audit_token_t token)
{
	pid_t pid = audit_token_to_pid(token);

	for (Client *c = clients; c != NULL; c = c->next)
		if (c->pid == pid)
			return c;
	return NULL;
}

static void freeClient(Client *c)
{
	for (int i = 0; i < c->count; i++) {
		free(c->observations[i].name);
		free(c->observations[i].object);
	}
	free(c->observations);
	free(c);
}

static void watchPort(mach_port_t port)
{
	mach_port_t previous = MACH_PORT_NULL;

	mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_DEAD_NAME, 0, notifyPort,
	    MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
	if (previous != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), previous);
}

static void portDied(mach_port_t port)
{
	for (Client **p = &clients; *p != NULL; ) {
		Client *c = *p;
		if (c->port == port) {
			*p = c->next;
			mach_port_deallocate(mach_task_self(), c->port);
			freeClient(c);
		} else
			p = &c->next;
	}
	mach_port_deallocate(mach_task_self(), port);	/* the notification's reference */
}

kern_return_t
dnd_check_in(mach_port_t server, audit_token_t token, mach_port_t port, int32_t *result)
{
	Client *c = clientFor(token);

	if (c != NULL) {
		/* Checked in again (its port changed): the observations stay. */
		if (c->port != port) {
			mach_port_deallocate(mach_task_self(), c->port);
			c->port = port;
			watchPort(port);
		} else
			mach_port_deallocate(mach_task_self(), port);
		*result = kDNSuccess;
		return KERN_SUCCESS;
	}
	c = calloc(1, sizeof(*c));
	if (c == NULL) {
		mach_port_deallocate(mach_task_self(), port);
		*result = kDNTooMany;
		return KERN_SUCCESS;
	}
	c->pid = audit_token_to_pid(token);
	c->uid = audit_token_to_euid(token);
	c->port = port;
	c->next = clients;
	clients = c;
	watchPort(port);
	*result = kDNSuccess;
	return KERN_SUCCESS;
}

static int sameObservation(const Observation *o, const char *name, const char *object)
{
	return strcmp(o->name, name) == 0 && strcmp(o->object, object) == 0;
}

kern_return_t
dnd_observe(mach_port_t server, audit_token_t token, dn_string_t name, mach_msg_type_number_t nameCnt,
    dn_string_t object, mach_msg_type_number_t objectCnt, int32_t *result)
{
	Client *c = clientFor(token);

	if (c == NULL) {
		*result = kDNNotCheckedIn;
		return KERN_SUCCESS;
	}
	char *n = strndup(name, nameCnt), *o = strndup(object, objectCnt);
	for (int i = 0; i < c->count; i++)
		if (sameObservation(&c->observations[i], n, o)) {
			free(n);
			free(o);
			*result = kDNSuccess;
			return KERN_SUCCESS;
		}
	if (c->count == kDNMaxObservations) {
		free(n);
		free(o);
		*result = kDNTooMany;
		return KERN_SUCCESS;
	}
	if (c->count == c->capacity) {
		int capacity = c->capacity ? c->capacity * 2 : 8;
		Observation *grown = realloc(c->observations, sizeof(Observation) * (size_t)capacity);
		if (grown == NULL) {
			free(n);
			free(o);
			*result = kDNTooMany;
			return KERN_SUCCESS;
		}
		c->observations = grown;
		c->capacity = capacity;
	}
	c->observations[c->count++] = (Observation){ n, o };
	*result = kDNSuccess;
	return KERN_SUCCESS;
}

kern_return_t
dnd_unobserve(mach_port_t server, audit_token_t token, dn_string_t name, mach_msg_type_number_t nameCnt,
    dn_string_t object, mach_msg_type_number_t objectCnt, int32_t *result)
{
	Client *c = clientFor(token);

	*result = kDNSuccess;
	if (c == NULL)
		return KERN_SUCCESS;
	char *n = strndup(name, nameCnt), *o = strndup(object, objectCnt);
	for (int i = 0; i < c->count; i++)
		if (sameObservation(&c->observations[i], n, o)) {
			free(c->observations[i].name);
			free(c->observations[i].object);
			c->observations[i] = c->observations[--c->count];
			break;
		}
	free(n);
	free(o);
	return KERN_SUCCESS;
}

static int observes(const Client *c, const char *name, const char *object)
{
	for (int i = 0; i < c->count; i++) {
		const Observation *o = &c->observations[i];
		if ((o->name[0] == 0 || strcmp(o->name, name) == 0) &&
		    (o->object[0] == 0 || strcmp(o->object, object) == 0))
			return 1;
	}
	return 0;
}

static void deliver(const Client *c, const void *bytes, uint32_t length, int32_t options)
{
	DNMessage msg;

	memset(&msg, 0, sizeof(msg));
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0) | MACH_MSGH_BITS_COMPLEX;
	msg.header.msgh_remote_port = c->port;
	msg.header.msgh_id = kDNMessageNotification;
	msg.header.msgh_size = sizeof(msg);
	msg.body.msgh_descriptor_count = 1;
	msg.notification.address = (void *)(uintptr_t)bytes;
	msg.notification.size = length;
	msg.notification.deallocate = FALSE;
	msg.notification.copy = MACH_MSG_VIRTUAL_COPY;
	msg.notification.type = MACH_MSG_OOL_DESCRIPTOR;
	msg.options = options;
	/* Never waits: a process that does not read loses this one. */
	mach_msg(&msg.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, sizeof(msg), 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
}

/* The name and object of a notification's property list, as C strings. */
static int nameAndObject(CFDictionaryRef d, char *name, size_t nameSize, char *object, size_t objectSize)
{
	CFTypeRef n = CFDictionaryGetValue(d, CFSTR(kDNKeyName));
	CFTypeRef o = CFDictionaryGetValue(d, CFSTR(kDNKeyObject));

	object[0] = 0;
	if (n == NULL || CFGetTypeID(n) != CFStringGetTypeID() ||
	    !CFStringGetCString(n, name, (CFIndex)nameSize, kCFStringEncodingUTF8) || name[0] == 0)
		return 0;
	if (o != NULL && (CFGetTypeID(o) != CFStringGetTypeID() ||
	    !CFStringGetCString(o, object, (CFIndex)objectSize, kCFStringEncodingUTF8)))
		return 0;
	return 1;
}

kern_return_t
dnd_post(mach_port_t server, audit_token_t token, dn_bytes_t notification, mach_msg_type_number_t length,
    int32_t options, int32_t *result)
{
	uid_t uid = audit_token_to_euid(token);
	char name[1024], object[1024];

	*result = kDNBadNotification;
	if (length > 0 && length <= kDNMaxNotification) {
		CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)notification, length, kCFAllocatorNull);
		CFPropertyListRef p = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
		if (p != NULL && CFGetTypeID(p) == CFDictionaryGetTypeID() &&
		    nameAndObject(p, name, sizeof(name), object, sizeof(object))) {
			for (Client *c = clients; c != NULL; c = c->next)
				if ((c->uid == uid || (options & kDNPostToAllSessions) != 0) && observes(c, name, object))
					deliver(c, notification, length, options);
			*result = kDNSuccess;
		}
		if (p != NULL)
			CFRelease(p);
		CFRelease(data);
	}
	vm_deallocate(mach_task_self(), (vm_address_t)notification, length);
	return KERN_SUCCESS;
}

int
main(int argc, char *argv[])
{
	mach_port_t service = MACH_PORT_NULL;
	kern_return_t kr;

	setvbuf(stderr, NULL, _IOLBF, 0);
	kr = bootstrap_check_in(bootstrap_port, kDNServiceName, &service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "distnoted: cannot check in %s: 0x%x\n", kDNServiceName, kr);
		return 1;
	}
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notifyPort) != KERN_SUCCESS) {
		fprintf(stderr, "distnoted: no notification port\n");
		return 1;
	}
	/* One queue: requests and port deaths in order. */
	dispatch_queue_t queue = dispatch_queue_create("org.minkar.distnoted", DISPATCH_QUEUE_SERIAL);
	dispatch_source_t deaths = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, notifyPort, 0, queue);
	dispatch_source_set_event_handler(deaths, ^{
		union {
			mach_dead_name_notification_t dead;
			uint8_t bytes[sizeof(mach_dead_name_notification_t) + MAX_TRAILER_SIZE];
		} msg;

		while (mach_msg(&msg.dead.not_header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), notifyPort, 0,
		    MACH_PORT_NULL) == MACH_MSG_SUCCESS)
			if (msg.dead.not_header.msgh_id == MACH_NOTIFY_DEAD_NAME)
				portDied(msg.dead.not_port);
	});
	dispatch_resume(deaths);
	dispatch_source_t requests = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, service, 0, queue);
	dispatch_source_set_event_handler(requests, ^{
		mach_msg_server_once(DistributedNotifications_server,
		    (mach_msg_size_t)MAX(sizeof(union __RequestUnion__dnd_DistributedNotifications_subsystem),
		    dnd_DistributedNotifications_subsystem.maxsize) + MAX_TRAILER_SIZE, service,
		    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) | MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	});
	dispatch_resume(requests);
	fprintf(stderr, "distnoted: serving %s\n", kDNServiceName);
	dispatch_main();
}
