/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * notificationd, the notification center's session service (Atlas.md,
 * 5.6, N0; NotificationCenterTypes.h). Apps check in with a port and post
 * the notifications they deliver; the presenter, SystemUIServer, checks in
 * with its own and is given each one to show, with the app's process. When
 * the user clicks one, the presenter says so and the app is told, on the
 * port it checked in with.
 *
 * One session, the console's: the daemon runs as root, as launchservicesd
 * does, until sessions per user come with the login window. Notifications
 * posted before the presenter checks in wait for it (the last few).
 */

#include <CoreFoundation/CoreFoundation.h>
#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <libproc.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>

#include "NotificationCenterTypes.h"
#include "NotificationCenterServer.h"	/* MIG, generated */

#define kMaxApps	64
#define kMaxWaiting	8
#define kPresenterPath	"/System/Library/CoreServices/SystemUIServer.app/Contents/MacOS/SystemUIServer"

static struct {
	pid_t pid;
	mach_port_t port;
} apps[kMaxApps];

static mach_port_t presenter = MACH_PORT_NULL;
static pid_t presenterPID;
static mach_port_t notifyPort = MACH_PORT_NULL;

/* Posted while there was no presenter: given to it when it checks in. */
static CFDataRef waiting[kMaxWaiting];
static int waitingCount;

/* A message to an app or the presenter; never waits long for it. */
static kern_return_t
sendMessage(mach_port_t port, mach_msg_id_t id, int32_t arg, const void *payload, uint32_t length)
{
	NCMessage msg;

	if (port == MACH_PORT_NULL || length > kNCMaxPayload)
		return KERN_INVALID_ARGUMENT;
	memset(&msg.header, 0, sizeof(msg.header));
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
	msg.header.msgh_remote_port = port;
	msg.header.msgh_id = id;
	msg.header.msgh_size = (mach_msg_size_t)((offsetof(NCMessage, payload) + length + 3) & ~3u);
	msg.arg = arg;
	msg.length = length;
	memcpy(msg.payload, payload, length);
	return mach_msg(&msg.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT, msg.header.msgh_size, 0,
	    MACH_PORT_NULL, 200, MACH_PORT_NULL);
}

static void
watchPort(mach_port_t port)
{
	mach_port_t previous = MACH_PORT_NULL;

	mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_DEAD_NAME, 0, notifyPort,
	    MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
	if (previous != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), previous);
}

static void
portDied(mach_port_t port)
{
	int ours = 0;

	for (int i = 0; i < kMaxApps; i++)
		if (apps[i].port == port) {
			apps[i].port = MACH_PORT_NULL;
			apps[i].pid = 0;
			ours = 1;
		}
	if (port == presenter) {
		fprintf(stderr, "notificationd: the presenter (%d) went away\n", presenterPID);
		presenter = MACH_PORT_NULL;
		presenterPID = 0;
		ours = 1;
	}
	/* The notification's reference, and ours if it was in use. */
	mach_port_deallocate(mach_task_self(), port);
	if (ours)
		mach_port_deallocate(mach_task_self(), port);
}

static void
showWaiting(void)
{
	for (int i = 0; i < waitingCount; i++) {
		sendMessage(presenter, kNCMessageShow, 0, CFDataGetBytePtr(waiting[i]),
		    (uint32_t)CFDataGetLength(waiting[i]));
		CFRelease(waiting[i]);
	}
	waitingCount = 0;
}

kern_return_t
ncd_check_in(mach_port_t server, audit_token_t token, mach_port_t port, int32_t *result)
{
	pid_t pid = audit_token_to_pid(token);
	int free = -1;

	for (int i = 0; i < kMaxApps; i++) {
		if (apps[i].pid == pid && apps[i].port != MACH_PORT_NULL) {
			/* Again (a new port): the old one goes. */
			mach_port_deallocate(mach_task_self(), apps[i].port);
			apps[i].port = port;
			watchPort(port);
			*result = kNCSuccess;
			return KERN_SUCCESS;
		}
		if (free < 0 && apps[i].port == MACH_PORT_NULL)
			free = i;
	}
	if (free < 0) {
		mach_port_deallocate(mach_task_self(), port);
		*result = kNCNotAllowed;
		return KERN_SUCCESS;
	}
	apps[free].pid = pid;
	apps[free].port = port;
	watchPort(port);
	*result = kNCSuccess;
	return KERN_SUCCESS;
}

kern_return_t
ncd_post(mach_port_t server, audit_token_t token, nc_bytes_t notification,
    mach_msg_type_number_t length, int32_t *result)
{
	pid_t pid = audit_token_to_pid(token);
	CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)notification, length,
	    kCFAllocatorNull);
	CFPropertyListRef plist = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable,
	    NULL, NULL);
	CFMutableDictionaryRef dict;
	CFNumberRef number;
	CFDataRef out;

	CFRelease(data);
	if (plist == NULL || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
		if (plist != NULL)
			CFRelease(plist);
		*result = kNCIllegalArgument;
		return KERN_SUCCESS;
	}
	dict = CFDictionaryCreateMutableCopy(NULL, 0, plist);
	CFRelease(plist);
	number = CFNumberCreate(NULL, kCFNumberIntType, &pid);
	CFDictionarySetValue(dict, CFSTR(kNCKeyPID), number);
	CFRelease(number);
	out = CFPropertyListCreateData(NULL, dict, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	CFRelease(dict);
	if (out == NULL || CFDataGetLength(out) > kNCMaxPayload) {
		if (out != NULL)
			CFRelease(out);
		*result = kNCIllegalArgument;
		return KERN_SUCCESS;
	}
	if (presenter != MACH_PORT_NULL &&
	    sendMessage(presenter, kNCMessageShow, 0, CFDataGetBytePtr(out),
	    (uint32_t)CFDataGetLength(out)) == KERN_SUCCESS) {
		CFRelease(out);
		*result = kNCSuccess;
		return KERN_SUCCESS;
	}
	if (waitingCount == kMaxWaiting) {
		CFRelease(waiting[0]);
		memmove(waiting, waiting + 1, (kMaxWaiting - 1) * sizeof(waiting[0]));
		waitingCount--;
	}
	waiting[waitingCount++] = out;
	*result = kNCNoPresenter;
	return KERN_SUCCESS;
}

kern_return_t
ncd_remove(mach_port_t server, audit_token_t token, nc_bytes_t key,
    mach_msg_type_number_t length, int32_t *result)
{
	*result = kNCSuccess;
	if (presenter != MACH_PORT_NULL)
		sendMessage(presenter, kNCMessageRemove, audit_token_to_pid(token), key, length);
	return KERN_SUCCESS;
}

kern_return_t
ncd_presenter_check_in(mach_port_t server, audit_token_t token, mach_port_t port, int32_t *result)
{
	pid_t pid = audit_token_to_pid(token);
	char path[PROC_PIDPATHINFO_MAXSIZE];

	/* Only the system's SystemUIServer shows the notifications. */
	if (proc_pidpath(pid, path, sizeof(path)) <= 0 || strcmp(path, kPresenterPath) != 0) {
		mach_port_deallocate(mach_task_self(), port);
		*result = kNCNotAllowed;
		return KERN_SUCCESS;
	}
	if (presenter != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), presenter);
	presenter = port;
	presenterPID = pid;
	watchPort(port);
	fprintf(stderr, "notificationd: presenter %d\n", pid);
	showWaiting();
	*result = kNCSuccess;
	return KERN_SUCCESS;
}

kern_return_t
ncd_activated(mach_port_t server, audit_token_t token, int32_t pid, nc_bytes_t key,
    mach_msg_type_number_t length, int32_t activation, int32_t *result)
{
	if (presenter == MACH_PORT_NULL || audit_token_to_pid(token) != presenterPID) {
		*result = kNCNotAllowed;
		return KERN_SUCCESS;
	}
	*result = kNCIllegalArgument;
	for (int i = 0; i < kMaxApps; i++)
		if (apps[i].pid == pid && apps[i].port != MACH_PORT_NULL) {
			*result = sendMessage(apps[i].port, kNCMessageActivated, activation, key, length) ==
			    KERN_SUCCESS ? kNCSuccess : kNCNoService;
			break;
		}
	return KERN_SUCCESS;
}

int
main(int argc, char *argv[])
{
	mach_port_t service = MACH_PORT_NULL;
	kern_return_t kr;

	setvbuf(stderr, NULL, _IOLBF, 0);
	kr = bootstrap_check_in(bootstrap_port, kNCServiceName, &service);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "notificationd: cannot check in %s: 0x%x\n", kNCServiceName, kr);
		return 1;
	}
	if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &notifyPort) != KERN_SUCCESS) {
		fprintf(stderr, "notificationd: no notification port\n");
		return 1;
	}

	/* One queue: requests and port deaths are served in order. */
	dispatch_queue_t queue = dispatch_queue_create("org.minkar.notificationd", DISPATCH_QUEUE_SERIAL);
	dispatch_source_t notifySource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
	    notifyPort, 0, queue);
	dispatch_source_set_event_handler(notifySource, ^{
		union {
			mach_dead_name_notification_t dead;
			uint8_t bytes[sizeof(mach_dead_name_notification_t) + MAX_TRAILER_SIZE];
		} msg;

		while (mach_msg(&msg.dead.not_header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg),
		    notifyPort, 0, MACH_PORT_NULL) == MACH_MSG_SUCCESS)
			if (msg.dead.not_header.msgh_id == MACH_NOTIFY_DEAD_NAME)
				portDied(msg.dead.not_port);
	});
	dispatch_resume(notifySource);

	dispatch_source_t source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, service, 0,
	    queue);
	dispatch_source_set_event_handler(source, ^{
		/* Room for the largest request (the subsystem's maxsize is its
		   largest reply, as pboard found) and for the audit trailer. */
		mach_msg_server_once(NotificationCenter_server,
		    (mach_msg_size_t)MAX(sizeof(union __RequestUnion__ncd_NotificationCenter_subsystem),
		    ncd_NotificationCenter_subsystem.maxsize) + MAX_TRAILER_SIZE, service,
		    MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
		    MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	});
	dispatch_resume(source);
	fprintf(stderr, "notificationd: serving %s\n", kNCServiceName);
	dispatch_main();
}
