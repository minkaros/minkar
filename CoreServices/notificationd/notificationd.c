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
 *
 * N2: the apps' badges are kept here too, for the Dock, and go with the
 * app. So are their scheduled notifications (on disk, kSchedulesPath):
 * at each date, one whose app is no longer running is shown from here;
 * a running app's own timer shows it.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <libproc.h>
#include <mach/mach.h>
#include <notify.h>
#include <servers/bootstrap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
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

#define kSchedulesPath	"/private/var/db/notificationd/schedules.plist"

static mach_port_t presenter = MACH_PORT_NULL;
static pid_t presenterPID;
static mach_port_t notifyPort = MACH_PORT_NULL;

/* The apps' badges (N2): process identifier (a CFString) to label. */
static CFMutableDictionaryRef badges;

static void setBadge(pid_t pid, CFStringRef label)
{
	CFStringRef key = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), pid);
	CFStringRef old = CFDictionaryGetValue(badges, key);

	if (label != NULL && CFStringGetLength(label) > 0) {
		if (old == NULL || !CFEqual(old, label)) {
			CFDictionarySetValue(badges, key, label);
			notify_post(kNCBadgesChanged);
		}
	} else if (old != NULL) {
		CFDictionaryRemoveValue(badges, key);
		notify_post(kNCBadgesChanged);
	}
	CFRelease(key);
}

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
			setBadge(apps[i].pid, NULL);
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

/* A notification (a binary property list) to the presenter, or kept for
   it when there is none yet. */
static int present(CFDataRef out)
{
	if (presenter != MACH_PORT_NULL &&
	    sendMessage(presenter, kNCMessageShow, 0, CFDataGetBytePtr(out),
	    (uint32_t)CFDataGetLength(out)) == KERN_SUCCESS)
		return 1;
	if (waitingCount == kMaxWaiting) {
		CFRelease(waiting[0]);
		memmove(waiting, waiting + 1, (kMaxWaiting - 1) * sizeof(waiting[0]));
		waitingCount--;
	}
	waiting[waitingCount++] = CFRetain(out);
	return 0;
}

/* ---- Scheduled notifications (N2) ---------------------------------------
 * Each: the app's pid and bundle (its identity), its identifier, its dates
 * still to come and the notification. */
static CFMutableArrayRef schedules;
static dispatch_source_t scheduleTimer;

static int appRunning(pid_t pid)
{
	for (int i = 0; i < kMaxApps; i++)
		if (apps[i].pid == pid && apps[i].port != MACH_PORT_NULL)
			return 1;
	return 0;
}

static CFStringRef bundleOf(CFDictionaryRef notification)
{
	CFStringRef b = CFDictionaryGetValue(notification, CFSTR(kNCKeyBundleIdentifier));

	if (b == NULL)
		b = CFDictionaryGetValue(notification, CFSTR(kNCKeyAppName));
	return b != NULL && CFGetTypeID(b) == CFStringGetTypeID() ? b : CFSTR("");
}

static void saveSchedules(void)
{
	CFDataRef data = CFPropertyListCreateData(NULL, schedules, kCFPropertyListBinaryFormat_v1_0, 0, NULL);

	if (data == NULL)
		return;
	mkdir("/private/var/db/notificationd", 0700);
	FILE *f = fopen(kSchedulesPath ".new", "w");
	if (f != NULL) {
		fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data), f);
		if (fclose(f) == 0)
			rename(kSchedulesPath ".new", kSchedulesPath);
	}
	CFRelease(data);
}

static void loadSchedules(void)
{
	FILE *f = fopen(kSchedulesPath, "r");
	CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
	UInt8 buf[4096];
	size_t n;

	schedules = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (f == NULL) {
		CFRelease(data);
		return;
	}
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		CFDataAppendBytes(data, buf, (CFIndex)n);
	fclose(f);
	CFPropertyListRef plist = CFPropertyListCreateWithData(NULL, data, kCFPropertyListMutableContainersAndLeaves,
	    NULL, NULL);
	CFRelease(data);
	if (plist != NULL && CFGetTypeID(plist) == CFArrayGetTypeID())
		CFArrayAppendArray(schedules, plist, CFRangeMake(0, CFArrayGetCount(plist)));
	if (plist != NULL)
		CFRelease(plist);
}

static void armScheduleTimer(void);

/* What is due: shown when its app is gone; its date dropped either way. */
static void fireSchedules(void)
{
	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
	int changed = 0;

	for (CFIndex i = CFArrayGetCount(schedules) - 1; i >= 0; i--) {
		CFMutableDictionaryRef e = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(schedules, i);
		CFMutableArrayRef dates = (CFMutableArrayRef)CFDictionaryGetValue(e, CFSTR(kNCScheduleDates));
		CFDictionaryRef n = CFDictionaryGetValue(e, CFSTR(kNCScheduleNotification));
		CFNumberRef pidNumber = CFDictionaryGetValue(e, CFSTR(kNCKeyPID));
		int pid = 0, due = 0;

		if (pidNumber != NULL)
			CFNumberGetValue(pidNumber, kCFNumberIntType, &pid);
		while (dates != NULL && CFArrayGetCount(dates) > 0 &&
		    CFDateGetAbsoluteTime(CFArrayGetValueAtIndex(dates, 0)) <= now + 0.5) {
			CFArrayRemoveValueAtIndex(dates, 0);
			due = 1;
			changed = 1;
		}
		if (due && n != NULL && !appRunning(pid)) {
			CFMutableDictionaryRef shown = CFDictionaryCreateMutableCopy(NULL, 0, n);
			int none = 0;
			CFNumberRef zero = CFNumberCreate(NULL, kCFNumberIntType, &none);
			CFDictionarySetValue(shown, CFSTR(kNCKeyPID), zero);	/* no process to tell */
			CFRelease(zero);
			CFDataRef out = CFPropertyListCreateData(NULL, shown, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
			CFRelease(shown);
			if (out != NULL) {
				present(out);
				CFRelease(out);
			}
			char bundle[256] = "";
			CFStringGetCString(bundleOf(n), bundle, sizeof(bundle), kCFStringEncodingUTF8);
			fprintf(stderr, "notificationd: scheduled notification of %s shown (its app is not running)\n", bundle);
		}
		if (dates == NULL || CFArrayGetCount(dates) == 0)
			CFArrayRemoveValueAtIndex(schedules, i);
	}
	if (changed)
		saveSchedules();
	armScheduleTimer();
}

static void armScheduleTimer(void)
{
	CFAbsoluteTime next = 0;

	for (CFIndex i = 0; i < CFArrayGetCount(schedules); i++) {
		CFArrayRef dates = CFDictionaryGetValue(CFArrayGetValueAtIndex(schedules, i), CFSTR(kNCScheduleDates));
		if (dates != NULL && CFArrayGetCount(dates) > 0) {
			CFAbsoluteTime t = CFDateGetAbsoluteTime(CFArrayGetValueAtIndex(dates, 0));
			if (next == 0 || t < next)
				next = t;
		}
	}
	if (next == 0) {
		dispatch_source_set_timer(scheduleTimer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 0);
		return;
	}
	double wait = next - CFAbsoluteTimeGetCurrent();
	if (wait < 0)
		wait = 0;
	dispatch_source_set_timer(scheduleTimer, dispatch_walltime(NULL, (int64_t)(wait * NSEC_PER_SEC)),
	    DISPATCH_TIME_FOREVER, NSEC_PER_SEC / 10);
}

/* The app's entry of that identifier taken away (NULL: all the app's). */
static void dropSchedules(CFStringRef bundle, CFStringRef identifier)
{
	for (CFIndex i = CFArrayGetCount(schedules) - 1; i >= 0; i--) {
		CFDictionaryRef e = CFArrayGetValueAtIndex(schedules, i);
		CFStringRef b = CFDictionaryGetValue(e, CFSTR(kNCKeyBundleIdentifier));
		CFStringRef id = CFDictionaryGetValue(e, CFSTR(kNCScheduleIdentifier));
		if (b != NULL && CFEqual(b, bundle) && (identifier == NULL || (id != NULL && CFEqual(id, identifier))))
			CFArrayRemoveValueAtIndex(schedules, i);
	}
}

/* An app's bundle, from its last scheduled notification (unschedule
   carries none). */
static CFStringRef bundleOfPID(pid_t pid)
{
	for (CFIndex i = 0; i < CFArrayGetCount(schedules); i++) {
		CFDictionaryRef e = CFArrayGetValueAtIndex(schedules, i);
		CFNumberRef p = CFDictionaryGetValue(e, CFSTR(kNCKeyPID));
		int v = 0;
		if (p != NULL && CFNumberGetValue(p, kCFNumberIntType, &v) && v == pid)
			return CFDictionaryGetValue(e, CFSTR(kNCKeyBundleIdentifier));
	}
	return NULL;
}

kern_return_t
ncd_schedule(mach_port_t server, audit_token_t token, nc_bytes_t request, mach_msg_type_number_t length,
    int32_t *result)
{
	pid_t pid = audit_token_to_pid(token);
	CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)request, length, kCFAllocatorNull);
	CFPropertyListRef plist = CFPropertyListCreateWithData(NULL, data, kCFPropertyListMutableContainersAndLeaves,
	    NULL, NULL);

	CFRelease(data);
	*result = kNCIllegalArgument;
	if (plist == NULL || CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
		if (plist != NULL)
			CFRelease(plist);
		return KERN_SUCCESS;
	}
	CFMutableDictionaryRef e = (CFMutableDictionaryRef)plist;
	CFDictionaryRef n = CFDictionaryGetValue(e, CFSTR(kNCScheduleNotification));
	CFStringRef id = CFDictionaryGetValue(e, CFSTR(kNCScheduleIdentifier));
	CFArrayRef dates = CFDictionaryGetValue(e, CFSTR(kNCScheduleDates));
	if (n == NULL || CFGetTypeID(n) != CFDictionaryGetTypeID() || id == NULL ||
	    CFGetTypeID(id) != CFStringGetTypeID() || dates == NULL || CFGetTypeID(dates) != CFArrayGetTypeID() ||
	    CFArrayGetCount(dates) > kNCMaxScheduleDates) {
		CFRelease(plist);
		return KERN_SUCCESS;
	}
	CFStringRef bundle = bundleOf(n);
	CFNumberRef pidNumber = CFNumberCreate(NULL, kCFNumberIntType, &pid);
	CFDictionarySetValue(e, CFSTR(kNCKeyPID), pidNumber);
	CFDictionarySetValue(e, CFSTR(kNCKeyBundleIdentifier), bundle);
	CFRelease(pidNumber);
	dropSchedules(bundle, id);
	CFArrayAppendValue(schedules, e);
	CFRelease(plist);
	saveSchedules();
	armScheduleTimer();
	*result = kNCSuccess;
	return KERN_SUCCESS;
}

kern_return_t
ncd_unschedule(mach_port_t server, audit_token_t token, nc_bytes_t identifier, mach_msg_type_number_t length,
    int32_t *result)
{
	CFStringRef bundle = bundleOfPID(audit_token_to_pid(token));
	CFStringRef id = length > 0 ? CFStringCreateWithBytes(NULL, (const UInt8 *)identifier, length,
	    kCFStringEncodingUTF8, false) : NULL;

	if (bundle != NULL) {
		CFRetain(bundle);
		dropSchedules(bundle, id);
		CFRelease(bundle);
		saveSchedules();
		armScheduleTimer();
	}
	if (id != NULL)
		CFRelease(id);
	*result = kNCSuccess;
	return KERN_SUCCESS;
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
	*result = present(out) ? kNCSuccess : kNCNoPresenter;
	CFRelease(out);
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
ncd_set_badge(mach_port_t server, audit_token_t token, nc_bytes_t label,
    mach_msg_type_number_t length, int32_t *result)
{
	pid_t pid = audit_token_to_pid(token);
	CFStringRef text = CFStringCreateWithBytes(NULL, (const UInt8 *)label, length, kCFStringEncodingUTF8, false);
	int known = 0;

	/* Only an app that checked in: its badge goes when it does. */
	for (int i = 0; i < kMaxApps; i++)
		if (apps[i].pid == pid && apps[i].port != MACH_PORT_NULL)
			known = 1;
	if (!known || text == NULL) {
		if (text != NULL)
			CFRelease(text);
		*result = known ? kNCIllegalArgument : kNCNotAllowed;
		return KERN_SUCCESS;
	}
	setBadge(pid, text);
	CFRelease(text);
	*result = kNCSuccess;
	return KERN_SUCCESS;
}

kern_return_t
ncd_copy_badges(mach_port_t server, nc_bytes_t out, mach_msg_type_number_t *length, int32_t *result)
{
	CFDataRef data = CFPropertyListCreateData(NULL, badges, kCFPropertyListBinaryFormat_v1_0, 0, NULL);

	*length = 0;
	*result = kNCIllegalArgument;
	if (data != NULL && CFDataGetLength(data) <= kNCMaxPayload) {
		memcpy(out, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
		*length = (mach_msg_type_number_t)CFDataGetLength(data);
		*result = kNCSuccess;
	}
	if (data != NULL)
		CFRelease(data);
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
ncd_activated(mach_port_t server, audit_token_t token, int32_t pid, nc_bytes_t activation,
    mach_msg_type_number_t length, int32_t activationType, int32_t *result)
{
	if (presenter == MACH_PORT_NULL || audit_token_to_pid(token) != presenterPID) {
		*result = kNCNotAllowed;
		return KERN_SUCCESS;
	}
	*result = kNCIllegalArgument;
	for (int i = 0; i < kMaxApps; i++)
		if (apps[i].pid == pid && apps[i].port != MACH_PORT_NULL) {
			*result = sendMessage(apps[i].port, kNCMessageActivated, activationType, activation, length) ==
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
	badges = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
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

	/* The scheduled notifications kept from before. */
	loadSchedules();
	scheduleTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
	dispatch_source_set_event_handler(scheduleTimer, ^{ fireSchedules(); });
	dispatch_resume(scheduleTimer);
	dispatch_async(queue, ^{ fireSchedules(); });
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
