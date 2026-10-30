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
 * powerd: the power management daemon, the server IOKit's power functions
 * talk to (the com.apple.PowerManagement.control Mach service, the
 * powermanagement MIG subsystem).
 *
 * Assertions: processes create, change, retain and release them; they are
 * listed by process, by type and as a status; a process's go when it
 * exits, and an assertion with a timeout when it expires. Their combined
 * levels go to the kernel's root domain (which keeps the display awake,
 * the system from idle sleep), and user activity tickles it.
 *
 * Settings: the active power settings (System Preferences' and pmset's)
 * go to the kernel, as idle timers (display, disk, system) and the root
 * domain's own settings, when powerd starts and whenever they change.
 * Scheduled power events and power sources other processes publish are
 * kept and listed. Whether the machine can sleep is the platform's to
 * say (IOSleepSupported); powerd never forces it.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/powermanagement_mig.h>

#include <sys/types.h>
#include <sys/time.h>

#include <bsm/libbsm.h>
#include <dispatch/dispatch.h>
#include <dispatch/private.h>
#include <err.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <notify.h>
#include <servers/bootstrap.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "pmServer.h"

#define SERVICE		"com.apple.PowerManagement.control"
#define MAX_MSG		(16 * 1024)
#define kPMSetUserAssertionLevelsSel	9
#define kPMActivityTickleSel		10

CFDictionaryRef IOPMCopyActivePMPreferences(void);

/* ---- Assertions ------------------------------------------------------------ */

struct assertion {
	int			 id;
	pid_t			 pid;
	int			 retain;
	CFMutableDictionaryRef	 props;
	time_t			 created;
	time_t			 expires;	/* 0: never */
	struct assertion	*next;
};

static struct assertion *assertions;
static int next_id = 1;
static dispatch_queue_t queue;
static uint32_t applied_levels = UINT32_MAX;

/* The kernel's levels for each type that has one. */
static const struct {
	CFStringRef	type;
	uint32_t	bit;
} type_bits[] = {
	{ CFSTR("PreventUserIdleDisplaySleep"),
	    kIOPMDriverAssertionPreventDisplaySleepBit },
	{ CFSTR("NoDisplaySleepAssertion"),
	    kIOPMDriverAssertionPreventDisplaySleepBit },
	{ CFSTR("PreventUserIdleSystemSleep"), kIOPMDriverAssertionCPUBit },
	{ CFSTR("NoIdleSleepAssertion"), kIOPMDriverAssertionCPUBit },
	{ CFSTR("PreventSystemSleep"), kIOPMDriverAssertionCPUBit },
	{ CFSTR("DenySystemSleep"), kIOPMDriverAssertionCPUBit },
	{ CFSTR("UserIsActive"), kIOPMDriverAssertionPreventDisplaySleepBit },
};

static long
dict_long(CFDictionaryRef d, CFStringRef key, long dflt)
{
	CFTypeRef v = d ? CFDictionaryGetValue(d, key) : NULL;
	long n = dflt;

	if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
		CFNumberGetValue(v, kCFNumberLongType, &n);
	return (n);
}

static void
dict_set_long(CFMutableDictionaryRef d, CFStringRef key, long v)
{
	CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongType, &v);

	CFDictionarySetValue(d, key, n);
	CFRelease(n);
}

static CFPropertyListRef
unpack(vm_offset_t data, mach_msg_type_number_t len)
{
	CFDataRef d;
	CFPropertyListRef pl;

	if (data == 0 || len == 0)
		return (NULL);
	d = CFDataCreate(NULL, (const UInt8 *)data, (CFIndex)len);
	pl = CFPropertyListCreateWithData(NULL, d,
	    kCFPropertyListMutableContainersAndLeaves, NULL, NULL);
	CFRelease(d);
	return (pl);
}

/* A property list into a buffer MIG sends and frees. */
static int
pack(CFPropertyListRef pl, vm_offset_t *out, mach_msg_type_number_t *len)
{
	CFDataRef d = CFPropertyListCreateData(NULL, pl,
	    kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	vm_address_t buf = 0;

	*out = 0;
	*len = 0;
	if (d == NULL)
		return (kIOReturnError);
	if (vm_allocate(mach_task_self(), &buf, (vm_size_t)CFDataGetLength(d),
	    VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
		CFRelease(d);
		return (kIOReturnNoMemory);
	}
	memcpy((void *)buf, CFDataGetBytePtr(d), (size_t)CFDataGetLength(d));
	*out = buf;
	*len = (mach_msg_type_number_t)CFDataGetLength(d);
	CFRelease(d);
	return (kIOReturnSuccess);
}

static bool
active(const struct assertion *a)
{
	return (dict_long(a->props, kIOPMAssertionLevelKey,
	    kIOPMAssertionLevelOn) != kIOPMAssertionLevelOff);
}

static struct assertion *
find(int id)
{
	struct assertion *a;

	for (a = assertions; a != NULL; a = a->next)
		if (a->id == id)
			return (a);
	return (NULL);
}

/* The levels to the kernel, when they change. */
static void
apply_levels(void)
{
	io_service_t root = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceNameMatching("IOPMrootDomain"));
	io_connect_t conn;
	struct assertion *a;
	uint32_t bits = 0;
	uint64_t in;
	size_t i;

	for (a = assertions; a != NULL; a = a->next) {
		CFTypeRef type;

		if (!active(a))
			continue;
		type = CFDictionaryGetValue(a->props, kIOPMAssertionTypeKey);
		for (i = 0; type != NULL && i < sizeof(type_bits) /
		    sizeof(type_bits[0]); i++)
			if (CFEqual(type, type_bits[i].type))
				bits |= type_bits[i].bit;
	}
	if (bits == applied_levels || root == MACH_PORT_NULL) {
		if (root != MACH_PORT_NULL)
			IOObjectRelease(root);
		return;
	}
	if (IOServiceOpen(root, mach_task_self(), 0, &conn) == KERN_SUCCESS) {
		in = bits;
		IOConnectCallScalarMethod(conn, kPMSetUserAssertionLevelsSel, &in,
		    1, NULL, NULL);
		IOServiceClose(conn);
		applied_levels = bits;
	}
	IOObjectRelease(root);
}

static void
user_active(void)
{
	io_service_t root = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceNameMatching("IOPMrootDomain"));
	io_connect_t conn;

	if (root == MACH_PORT_NULL)
		return;
	if (IOServiceOpen(root, mach_task_self(), 0, &conn) == KERN_SUCCESS) {
		IOConnectCallScalarMethod(conn, kPMActivityTickleSel, NULL, 0,
		    NULL, NULL);
		IOServiceClose(conn);
	}
	IOObjectRelease(root);
}

static void
remove_assertion(struct assertion *gone)
{
	struct assertion **pp;

	for (pp = &assertions; *pp != NULL; pp = &(*pp)->next)
		if (*pp == gone) {
			*pp = gone->next;
			break;
		}
	CFRelease(gone->props);
	free(gone);
}

/* A process's assertions go with it. */
static void
watch_process(pid_t pid)
{
	struct assertion *a;
	dispatch_source_t src;

	for (a = assertions; a != NULL; a = a->next)
		if (a->pid == pid)
			return;		/* already watched */
	src = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)pid,
	    DISPATCH_PROC_EXIT, queue);
	if (src == NULL)
		return;
	dispatch_source_set_event_handler(src, ^{
		struct assertion *x, *next;

		for (x = assertions; x != NULL; x = next) {
			next = x->next;
			if (x->pid == pid)
				remove_assertion(x);
		}
		apply_levels();
		dispatch_source_cancel(src);
	});
	dispatch_resume(src);
}

static void
expire_check(void)
{
	struct assertion *a, *next;
	time_t now = time(NULL);
	bool changed = false;

	for (a = assertions; a != NULL; a = next) {
		next = a->next;
		if (a->expires != 0 && now >= a->expires) {
			syslog(LOG_INFO, "assertion %d timed out", a->id);
			remove_assertion(a);
			changed = true;
		}
	}
	if (changed)
		apply_levels();
}

/* An assertion as listed: its properties and what powerd knows of it. */
static CFDictionaryRef
describe(const struct assertion *a)
{
	CFMutableDictionaryRef d = CFDictionaryCreateMutableCopy(NULL, 0,
	    a->props);
	CFDateRef created = CFDateCreate(NULL, (CFAbsoluteTime)a->created -
	    kCFAbsoluteTimeIntervalSince1970);

	dict_set_long(d, kIOPMAssertionPIDKey, a->pid);
	dict_set_long(d, kIOPMAssertionRetainCountKey, a->retain);
	dict_set_long(d, CFSTR("AssertionId"), a->id);
	CFDictionarySetValue(d, CFSTR("AssertStartWhen"), created);
	CFRelease(created);
	if (!CFDictionaryContainsKey(d, kIOPMAssertionLevelKey))
		dict_set_long(d, kIOPMAssertionLevelKey, kIOPMAssertionLevelOn);
	return (d);
}

/* {AssertPID, PerTaskAssertions} for each process, as IOKit rebuilds it. */
static CFArrayRef
by_process(bool inactive)
{
	CFMutableArrayRef out = CFArrayCreateMutable(NULL, 0,
	    &kCFTypeArrayCallBacks);
	struct assertion *a, *b;

	for (a = assertions; a != NULL; a = a->next) {
		CFMutableDictionaryRef entry;
		CFMutableArrayRef list;
		bool first = true;

		if (active(a) == inactive)
			continue;
		for (b = assertions; b != a; b = b->next)
			if (b->pid == a->pid && active(b) != inactive)
				first = false;
		if (!first)
			continue;
		list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		for (b = a; b != NULL; b = b->next)
			if (b->pid == a->pid && active(b) != inactive) {
				CFDictionaryRef d = describe(b);

				CFArrayAppendValue(list, d);
				CFRelease(d);
			}
		entry = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		dict_set_long(entry, kIOPMAssertionPIDKey, a->pid);
		CFDictionarySetValue(entry, CFSTR("PerTaskAssertions"), list);
		CFRelease(list);
		CFArrayAppendValue(out, entry);
		CFRelease(entry);
	}
	return (out);
}

/* Each known type, 1 when an active assertion of it is held. */
static CFDictionaryRef
status(void)
{
	static CFStringRef const types[] = {
		CFSTR("PreventUserIdleSystemSleep"),
		CFSTR("PreventUserIdleDisplaySleep"),
		CFSTR("PreventSystemSleep"),
		CFSTR("NoIdleSleepAssertion"),
		CFSTR("NoDisplaySleepAssertion"),
		CFSTR("UserIsActive"),
		CFSTR("ExternalMedia"),
		CFSTR("PreventDiskIdle"),
		CFSTR("BackgroundTask"),
		CFSTR("ApplePushServiceTask"),
		CFSTR("InteractivePushServiceTask"),
		CFSTR("NetworkClientActive"),
		CFSTR("DeclaredUserActivity"),
		CFSTR("DeclaredSystemActivity"),
	};
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	struct assertion *a;
	size_t i;

	for (i = 0; i < sizeof(types) / sizeof(types[0]); i++)
		dict_set_long(d, types[i], 0);
	for (a = assertions; a != NULL; a = a->next) {
		CFTypeRef type = CFDictionaryGetValue(a->props,
		    kIOPMAssertionTypeKey);

		if (type != NULL && active(a))
			dict_set_long(d, type, 1);
	}
	return (d);
}

/* ---- Settings ---------------------------------------------------------------- */

/* The active settings, for the current power source (AC on a desktop). */
static void
apply_settings(void)
{
	CFDictionaryRef all = IOPMCopyActivePMPreferences(), ac;
	io_service_t root;
	io_connect_t pm;

	if (all == NULL)
		return;
	ac = CFDictionaryGetValue(all, CFSTR(kIOPMACPowerKey));
	if (ac == NULL || CFGetTypeID(ac) != CFDictionaryGetTypeID()) {
		CFRelease(all);
		return;
	}
	pm = IOPMFindPowerManagement(MACH_PORT_NULL);
	if (pm != MACH_PORT_NULL) {
		IOPMSetAggressiveness(pm, kPMMinutesToDim, (unsigned long)dict_long(
		    ac, CFSTR(kIOPMDisplaySleepKey), 0));
		IOPMSetAggressiveness(pm, kPMMinutesToSpinDown,
		    (unsigned long)dict_long(ac, CFSTR(kIOPMDiskSleepKey), 0));
		IOPMSetAggressiveness(pm, kPMMinutesToSleep, (unsigned long)
		    dict_long(ac, CFSTR(kIOPMSystemSleepKey), 0));
		IOServiceClose(pm);
	}
	/* The root domain takes the settings it knows (Wake On LAN...). */
	root = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceNameMatching("IOPMrootDomain"));
	if (root != MACH_PORT_NULL) {
		IORegistryEntrySetCFProperties(root, ac);
		IOObjectRelease(root);
	}
	syslog(LOG_INFO, "settings applied: display sleep %ld, disk sleep %ld, "
	    "system sleep %ld", dict_long(ac, CFSTR(kIOPMDisplaySleepKey), 0),
	    dict_long(ac, CFSTR(kIOPMDiskSleepKey), 0),
	    dict_long(ac, CFSTR(kIOPMSystemSleepKey), 0));
	CFRelease(all);
}

/* ---- Scheduled events and power sources -------------------------------------- */

static CFMutableArrayRef scheduled, sources;
static CFMutableDictionaryRef repeating;	/* RepeatingPowerOn/Off */
static int next_ps = 1;

#define SCHEDULE_FILE	"/Library/Preferences/SystemConfiguration/com.apple.AutoWake.plist"

static void
save_schedule(void)
{
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFDataRef data;
	FILE *f;

	CFDictionarySetValue(d, CFSTR("ScheduledPowerEvents"), scheduled);
	CFDictionarySetValue(d, CFSTR("RepeatingPowerEvents"), repeating);
	data = CFPropertyListCreateData(NULL, d, kCFPropertyListXMLFormat_v1_0,
	    0, NULL);
	if (data != NULL && (f = fopen(SCHEDULE_FILE ".tmp", "w")) != NULL) {
		fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data),
		    f);
		if (fclose(f) == 0)
			rename(SCHEDULE_FILE ".tmp", SCHEDULE_FILE);
	}
	if (data != NULL)
		CFRelease(data);
	CFRelease(d);
}

static void
load_schedule(void)
{
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
	    (const UInt8 *)SCHEDULE_FILE, (CFIndex)strlen(SCHEDULE_FILE), false);
	CFReadStreamRef rs = CFReadStreamCreateWithFile(NULL, url);
	CFPropertyListRef pl = NULL;

	scheduled = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	repeating = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	if (rs != NULL && CFReadStreamOpen(rs)) {
		pl = CFPropertyListCreateWithStream(NULL, rs, 0,
		    kCFPropertyListImmutable, NULL, NULL);
		CFReadStreamClose(rs);
	}
	if (pl != NULL && CFGetTypeID(pl) == CFDictionaryGetTypeID()) {
		CFTypeRef e = CFDictionaryGetValue(pl, CFSTR("ScheduledPowerEvents"));
		CFTypeRef r = CFDictionaryGetValue(pl,
		    CFSTR("RepeatingPowerEvents"));

		if (e != NULL && CFGetTypeID(e) == CFArrayGetTypeID())
			CFArrayAppendArray(scheduled, e,
			    CFRangeMake(0, CFArrayGetCount(e)));
		if (r != NULL && CFGetTypeID(r) == CFDictionaryGetTypeID()) {
			CFRelease(repeating);
			repeating = CFDictionaryCreateMutableCopy(NULL, 0, r);
		}
	}
	if (pl != NULL)
		CFRelease(pl);
	if (rs != NULL)
		CFRelease(rs);
	CFRelease(url);
	sources = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
}

/* Events in the past are dropped. */
static void
prune_schedule(void)
{
	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
	CFIndex i;

	for (i = CFArrayGetCount(scheduled) - 1; i >= 0; i--) {
		CFDictionaryRef e = CFArrayGetValueAtIndex(scheduled, i);
		CFTypeRef t = CFDictionaryGetValue(e, CFSTR(kIOPMPowerEventTimeKey));

		if (t != NULL && CFGetTypeID(t) == CFDateGetTypeID() &&
		    CFDateGetAbsoluteTime(t) < now)
			CFArrayRemoveValueAtIndex(scheduled, i);
	}
}

static bool
is_root(audit_token_t token)
{
	return (audit_token_to_euid(token) == 0);
}

/* ---- The MIG routines -------------------------------------------------------- */

kern_return_t
_io_pm_assertion_create(mach_port_t server, audit_token_t token,
    vm_offset_t props, mach_msg_type_number_t propsCnt, int *assertion_id,
    int *disableAppSleep, int *enTrIntensity, int *return_code)
{
	CFPropertyListRef pl = unpack(props, propsCnt);
	struct assertion *a;
	long timeout;

	(void)server;
	*assertion_id = 0;
	*disableAppSleep = 0;
	*enTrIntensity = 0;
	if (pl == NULL || CFGetTypeID(pl) != CFDictionaryGetTypeID() ||
	    !CFDictionaryContainsKey(pl, kIOPMAssertionTypeKey)) {
		if (pl != NULL)
			CFRelease(pl);
		*return_code = kIOReturnBadArgument;
		return (KERN_SUCCESS);
	}
	if ((a = calloc(1, sizeof(*a))) == NULL) {
		CFRelease(pl);
		*return_code = kIOReturnNoMemory;
		return (KERN_SUCCESS);
	}
	a->id = next_id++;
	a->pid = audit_token_to_pid(token);
	a->retain = 1;
	a->props = (CFMutableDictionaryRef)pl;
	a->created = time(NULL);
	timeout = dict_long(pl, kIOPMAssertionTimeoutKey, 0);
	if (timeout > 0)
		a->expires = a->created + timeout;
	watch_process(a->pid);
	a->next = assertions;
	assertions = a;
	apply_levels();
	*assertion_id = a->id;
	*return_code = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_assertion_set_properties(mach_port_t server, audit_token_t token,
    int assertion_id, vm_offset_t props, mach_msg_type_number_t propsCnt,
    int *disableAppSleep, int *enableAppSleep, int *enTrIntensity,
    int *return_code)
{
	CFPropertyListRef pl = unpack(props, propsCnt);
	struct assertion *a = find(assertion_id);

	(void)server;
	*disableAppSleep = *enableAppSleep = *enTrIntensity = 0;
	if (a == NULL || pl == NULL || CFGetTypeID(pl) != CFDictionaryGetTypeID()) {
		*return_code = a == NULL ? kIOReturnNotFound : kIOReturnBadArgument;
	} else if (a->pid != audit_token_to_pid(token) && !is_root(token)) {
		*return_code = kIOReturnNotPrivileged;
	} else {
		CFIndex n = CFDictionaryGetCount(pl), i;
		const void **keys = calloc((size_t)n + 1, sizeof(*keys));
		const void **vals = calloc((size_t)n + 1, sizeof(*vals));
		long timeout;

		CFDictionaryGetKeysAndValues(pl, keys, vals);
		for (i = 0; i < n; i++)
			CFDictionarySetValue(a->props, keys[i], vals[i]);
		free(keys);
		free(vals);
		if ((timeout = dict_long(pl, kIOPMAssertionTimeoutKey, -1)) >= 0)
			a->expires = timeout > 0 ? time(NULL) + timeout : 0;
		apply_levels();
		*return_code = kIOReturnSuccess;
	}
	if (pl != NULL)
		CFRelease(pl);
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_assertion_retain_release(mach_port_t server, audit_token_t token,
    int assertion_id, int action, int *retainCnt, int *disableAppSleep,
    int *enableAppSleep, int *return_code)
{
	struct assertion *a = find(assertion_id);

	(void)server;
	*retainCnt = 0;
	*disableAppSleep = *enableAppSleep = 0;
	if (a == NULL) {
		*return_code = kIOReturnNotFound;
		return (KERN_SUCCESS);
	}
	if (a->pid != audit_token_to_pid(token) && !is_root(token)) {
		*return_code = kIOReturnNotPrivileged;
		return (KERN_SUCCESS);
	}
	if (action == kIOPMAssertionMIGDoRetain)
		a->retain++;
	else if (action == kIOPMAssertionMIGDoRelease)
		a->retain--;
	*retainCnt = a->retain;
	if (a->retain <= 0)
		remove_assertion(a);
	apply_levels();
	*return_code = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_assertion_copy_details(mach_port_t server, audit_token_t token,
    int assertion_id, int whichData, vm_offset_t props,
    mach_msg_type_number_t propsCnt, vm_offset_t *assertions_out,
    mach_msg_type_number_t *assertionsCnt, int *return_val)
{
	CFPropertyListRef result = NULL, arg = unpack(props, propsCnt);
	struct assertion *a;

	(void)server;
	(void)token;
	*assertions_out = 0;
	*assertionsCnt = 0;
	expire_check();
	switch (whichData) {
	case kIOPMAssertionMIGCopyOneAssertionProperties:
		if ((a = find(assertion_id)) != NULL)
			result = describe(a);
		break;
	case kIOPMAssertionMIGCopyAll:
		result = by_process(false);
		break;
	case kIOPMAssertionMIGCopyInactive:
		result = by_process(true);
		break;
	case kIOPMAssertionMIGCopyStatus:
		result = status();
		break;
	case kIOPMAssertionMIGCopyByType: {
		CFMutableArrayRef list = CFArrayCreateMutable(NULL, 0,
		    &kCFTypeArrayCallBacks);

		for (a = assertions; a != NULL; a = a->next) {
			CFTypeRef t = CFDictionaryGetValue(a->props,
			    kIOPMAssertionTypeKey);

			if (arg != NULL && t != NULL && CFEqual(t, arg) && active(a)) {
				CFDictionaryRef d = describe(a);

				CFArrayAppendValue(list, d);
				CFRelease(d);
			}
		}
		result = list;
		break;
	}
	case kIOPMPowerEventsMIGCopyScheduledEvents:
		prune_schedule();
		result = CFRetain(scheduled);
		break;
	case kIOPMPowerEventsMIGCopyRepeatEvents:
		result = CFRetain(repeating);
		break;
	}
	if (arg != NULL)
		CFRelease(arg);
	if (result == NULL) {
		*return_val = kIOReturnNotFound;
		return (KERN_SUCCESS);
	}
	*return_val = pack(result, assertions_out, assertionsCnt);
	CFRelease(result);
	return (KERN_SUCCESS);
}

/* Declared activity: an assertion with a timeout, the user's tickling. */
static int
declared(audit_token_t token, CFStringRef type, vm_offset_t props,
    mach_msg_type_number_t propsCnt, int *assertion_id)
{
	CFPropertyListRef pl = unpack(props, propsCnt);
	CFMutableDictionaryRef d = pl != NULL &&
	    CFGetTypeID(pl) == CFDictionaryGetTypeID() ?
	    (CFMutableDictionaryRef)CFRetain(pl) :
	    CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	struct assertion *a = calloc(1, sizeof(*a));

	if (pl != NULL)
		CFRelease(pl);
	if (a == NULL) {
		CFRelease(d);
		return (kIOReturnNoMemory);
	}
	CFDictionarySetValue(d, kIOPMAssertionTypeKey, type);
	if (!CFDictionaryContainsKey(d, kIOPMAssertionNameKey))
		CFDictionarySetValue(d, kIOPMAssertionNameKey, type);
	a->id = next_id++;
	a->pid = audit_token_to_pid(token);
	a->retain = 1;
	a->props = d;
	a->created = time(NULL);
	a->expires = a->created + 30;
	watch_process(a->pid);
	a->next = assertions;
	assertions = a;
	*assertion_id = a->id;
	apply_levels();
	return (kIOReturnSuccess);
}

kern_return_t
_io_pm_declare_user_active(mach_port_t server, audit_token_t token,
    int user_type, vm_offset_t props, mach_msg_type_number_t propsCnt,
    int *assertion_id, int *disableAppSleep, int *return_code)
{
	(void)server;
	(void)user_type;
	*disableAppSleep = 0;
	user_active();
	*return_code = declared(token, CFSTR("UserIsActive"), props, propsCnt,
	    assertion_id);
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_declare_system_active(mach_port_t server, audit_token_t token,
    int *state, vm_offset_t props, mach_msg_type_number_t propsCnt,
    int *assertion_id, int *return_code)
{
	(void)server;
	*state = kIOPMSystemSleepNotReverted;
	*return_code = declared(token, CFSTR("DeclaredSystemActivity"), props,
	    propsCnt, assertion_id);
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_declare_network_client_active(mach_port_t server, audit_token_t token,
    vm_offset_t props, mach_msg_type_number_t propsCnt, int *assertion_id,
    int *disableAppSleep, int *return_code)
{
	(void)server;
	*disableAppSleep = 0;
	*return_code = declared(token, CFSTR("NetworkClientActive"), props,
	    propsCnt, assertion_id);
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_schedule_power_event(mach_port_t server, audit_token_t token,
    vm_offset_t package, mach_msg_type_number_t packageCnt, int action,
    int *return_val)
{
	CFPropertyListRef e = unpack(package, packageCnt);
	CFIndex i;

	(void)server;
	if (!is_root(token)) {
		*return_val = kIOReturnNotPrivileged;
	} else if (e == NULL || CFGetTypeID(e) != CFDictionaryGetTypeID()) {
		*return_val = kIOReturnBadArgument;
	} else if (action == kIOPMScheduleEvent) {
		CFArrayAppendValue(scheduled, e);
		save_schedule();
		*return_val = kIOReturnSuccess;
	} else {
		*return_val = kIOReturnNotFound;
		for (i = CFArrayGetCount(scheduled) - 1; i >= 0; i--) {
			CFDictionaryRef s = CFArrayGetValueAtIndex(scheduled, i);

			if (CFEqual(CFDictionaryGetValue(s,
			    CFSTR(kIOPMPowerEventTimeKey)), CFDictionaryGetValue(e,
			    CFSTR(kIOPMPowerEventTimeKey))) &&
			    CFEqual(CFDictionaryGetValue(s,
			    CFSTR(kIOPMPowerEventTypeKey)), CFDictionaryGetValue(e,
			    CFSTR(kIOPMPowerEventTypeKey)))) {
				CFArrayRemoveValueAtIndex(scheduled, i);
				*return_val = kIOReturnSuccess;
			}
		}
		save_schedule();
	}
	if (e != NULL)
		CFRelease(e);
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_schedule_repeat_event(mach_port_t server, audit_token_t token,
    vm_offset_t package, mach_msg_type_number_t packageCnt, int action,
    int *return_val)
{
	CFPropertyListRef e = unpack(package, packageCnt);

	(void)server;
	(void)action;
	if (!is_root(token)) {
		*return_val = kIOReturnNotPrivileged;
	} else if (e == NULL || CFGetTypeID(e) != CFDictionaryGetTypeID()) {
		*return_val = kIOReturnBadArgument;
	} else {
		/* The power-on and power-off events given replace the old. */
		CFIndex n = CFDictionaryGetCount(e), i;
		const void **keys = calloc((size_t)n + 1, sizeof(*keys));
		const void **vals = calloc((size_t)n + 1, sizeof(*vals));

		CFDictionaryGetKeysAndValues(e, keys, vals);
		for (i = 0; i < n; i++)
			CFDictionarySetValue(repeating, keys[i], vals[i]);
		free(keys);
		free(vals);
		save_schedule();
		*return_val = kIOReturnSuccess;
	}
	if (e != NULL)
		CFRelease(e);
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_cancel_repeat_events(mach_port_t server, audit_token_t token,
    int *return_val)
{
	(void)server;
	if (!is_root(token)) {
		*return_val = kIOReturnNotPrivileged;
		return (KERN_SUCCESS);
	}
	CFDictionaryRemoveAllValues(repeating);
	save_schedule();
	*return_val = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_ps_new_pspowersource(mach_port_t server, audit_token_t token, int *psid,
    int *return_code)
{
	(void)server;
	(void)token;
	*psid = next_ps++;
	*return_code = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_ps_update_pspowersource(mach_port_t server, audit_token_t token, int psid,
    vm_offset_t psdetails, mach_msg_type_number_t psdetailsCnt,
    int *return_code)
{
	CFPropertyListRef d = unpack(psdetails, psdetailsCnt);
	CFIndex i;

	(void)server;
	(void)token;
	if (d == NULL || CFGetTypeID(d) != CFDictionaryGetTypeID()) {
		if (d != NULL)
			CFRelease(d);
		*return_code = kIOReturnBadArgument;
		return (KERN_SUCCESS);
	}
	dict_set_long((CFMutableDictionaryRef)d, CFSTR("PowerSourceID"), psid);
	for (i = 0; i < CFArrayGetCount(sources); i++)
		if (dict_long(CFArrayGetValueAtIndex(sources, i),
		    CFSTR("PowerSourceID"), -1) == psid) {
			CFArraySetValueAtIndex(sources, i, d);
			break;
		}
	if (i == CFArrayGetCount(sources))
		CFArrayAppendValue(sources, d);
	CFRelease(d);
	notify_post(kIOPSNotifyAnyPowerSource);
	*return_code = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_ps_release_pspowersource(mach_port_t server, audit_token_t token, int psid)
{
	CFIndex i;

	(void)server;
	(void)token;
	for (i = CFArrayGetCount(sources) - 1; i >= 0; i--)
		if (dict_long(CFArrayGetValueAtIndex(sources, i),
		    CFSTR("PowerSourceID"), -1) == psid)
			CFArrayRemoveValueAtIndex(sources, i);
	notify_post(kIOPSNotifyAnyPowerSource);
	return (KERN_SUCCESS);
}

kern_return_t
_io_ps_copy_powersources_info(mach_port_t server, audit_token_t token,
    int pstype, vm_offset_t *powersources, mach_msg_type_number_t *cnt,
    int *return_code)
{
	(void)server;
	(void)token;
	(void)pstype;
	*return_code = pack(sources, powersources, cnt);
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_get_value_int(mach_port_t server, audit_token_t token, int selector,
    int *value)
{
	(void)server;
	(void)token;
	(void)selector;
	*value = 0;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_set_value_int(mach_port_t server, audit_token_t token, int selector,
    int value, int *return_val)
{
	(void)server;
	(void)token;
	(void)selector;
	(void)value;
	*return_val = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_force_active_settings(mach_port_t server, audit_token_t token,
    vm_offset_t profiles, mach_msg_type_number_t profilesCnt, int *return_val)
{
	(void)server;
	(void)profiles;
	(void)profilesCnt;
	if (!is_root(token)) {
		*return_val = kIOReturnNotPrivileged;
		return (KERN_SUCCESS);
	}
	apply_settings();
	*return_val = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_last_wake_time(mach_port_t server, vm_offset_t *wakeData,
    mach_msg_type_number_t *wakeDataCnt, vm_offset_t *deltaData,
    mach_msg_type_number_t *deltaDataCnt, int *return_val)
{
	(void)server;
	*wakeData = *deltaData = 0;
	*wakeDataCnt = *deltaDataCnt = 0;
	*return_val = kIOReturnNotFound;	/* it has never slept */
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_get_uuid(mach_port_t server, int selector, string_t out_uuid,
    int *return_val)
{
	(void)server;
	(void)selector;
	out_uuid[0] = '\0';
	*return_val = kIOReturnNotFound;	/* no sleep/wake cycle yet */
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_connection_create(mach_port_t server, audit_token_t token,
    string_t name, int interests, uint32_t *connection_id, int *return_code)
{
	static uint32_t next_connection = 1;

	(void)server;
	(void)token;
	(void)name;
	(void)interests;
	/* Nothing to tell: the system does not sleep. */
	*connection_id = next_connection++;
	*return_code = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_connection_schedule_notification(mach_port_t server,
    audit_token_t token, uint32_t connection_id, mach_port_t notify_port,
    int disable, int *return_code)
{
	(void)server;
	(void)token;
	(void)connection_id;
	(void)disable;
	if (notify_port != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), notify_port);
	*return_code = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_connection_release(mach_port_t server, uint32_t connection_id,
    int *return_code)
{
	(void)server;
	(void)connection_id;
	*return_code = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_connection_acknowledge_event(mach_port_t server, audit_token_t token,
    uint32_t connection_id, int messageToken, vm_offset_t options,
    mach_msg_type_number_t optionsCnt, int *return_code)
{
	(void)server; (void)token; (void)connection_id; (void)messageToken;
	(void)options; (void)optionsCnt;
	*return_code = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_hid_event_report_activity(mach_port_t server, audit_token_t token,
    int action, int *allow)
{
	(void)server;
	(void)token;
	(void)action;
	*allow = 1;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_get_capability_bits(mach_port_t server, audit_token_t token,
    uint32_t *capBits, int *return_val)
{
	(void)server;
	(void)token;
	/* Always fully awake. */
	*capBits = kIOPMSystemCapabilityCPU | kIOPMSystemCapabilityGraphics |
	    kIOPMSystemCapabilityAudio | kIOPMSystemCapabilityNetwork;
	*return_val = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

/* What this system has no use for yet. */
kern_return_t
_io_ps_copy_chargelog(mach_port_t server, audit_token_t token, double t,
    vm_offset_t *log, mach_msg_type_number_t *logCnt, int *return_code)
{
	(void)server; (void)token; (void)t;
	*log = 0;
	*logCnt = 0;
	*return_code = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_hid_event_copy_history(mach_port_t server, vm_offset_t *eventArray,
    mach_msg_type_number_t *eventArrayCnt, int *return_val)
{
	(void)server;
	*eventArray = 0;
	*eventArrayCnt = 0;
	*return_val = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_set_debug_flags(mach_port_t server, audit_token_t token,
    uint32_t newFlags, uint32_t setMode, uint32_t *oldFlags, int *return_val)
{
	(void)server; (void)token; (void)newFlags; (void)setMode;
	*oldFlags = 0;
	*return_val = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_set_bt_wake_interval(mach_port_t server, audit_token_t token,
    uint32_t newInterval, uint32_t *oldInterval, int *return_val)
{
	(void)server; (void)token; (void)newInterval;
	*oldInterval = 0;
	*return_val = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_set_dw_linger_interval(mach_port_t server, audit_token_t token,
    uint32_t newInterval, int *return_val)
{
	(void)server; (void)token; (void)newInterval;
	*return_val = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_change_sa_assertion_behavior(mach_port_t server, audit_token_t token,
    uint32_t newFlags, uint32_t *oldFlags, int *return_val)
{
	(void)server; (void)token; (void)newFlags;
	*oldFlags = 0;
	*return_val = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_set_sleepservice_wake_time_cap(mach_port_t server, audit_token_t token,
    int cap_time, int *return_val)
{
	(void)server; (void)token; (void)cap_time;
	*return_val = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_ctl_assertion_type(mach_port_t server, audit_token_t token,
    string_t assertion, int action, int *return_code)
{
	(void)server; (void)token; (void)assertion; (void)action;
	*return_code = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_assertion_notify(mach_port_t server, audit_token_t token,
    string_t name, int req_type, int *return_code)
{
	(void)server; (void)token; (void)name; (void)req_type;
	*return_code = kIOReturnSuccess;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_assertion_activity_log(mach_port_t server, audit_token_t token,
    vm_offset_t *log, mach_msg_type_number_t *logCnt, uint32_t *entryCnt,
    uint32_t *overflow, int *return_code)
{
	(void)server; (void)token;
	*log = 0;
	*logCnt = 0;
	*entryCnt = 0;
	*overflow = 0;
	*return_code = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_assertion_activity_aggregate(mach_port_t server, audit_token_t token,
    vm_offset_t *statsData, mach_msg_type_number_t *statsDataCnt,
    int *return_code)
{
	(void)server; (void)token;
	*statsData = 0;
	*statsDataCnt = 0;
	*return_code = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

kern_return_t
_io_pm_set_exception_limits(mach_port_t server, audit_token_t token,
    vm_offset_t props, mach_msg_type_number_t propsCnt, int *return_code)
{
	(void)server; (void)token; (void)props; (void)propsCnt;
	*return_code = kIOReturnUnsupported;
	return (KERN_SUCCESS);
}

/*
 * A platform that cannot sleep says so (IOSleepSupported false): sleep is
 * then disabled in the root domain, so that neither idle sleep nor a
 * request stops the machine.
 */
static void
disable_unsupported_sleep(void)
{
	io_service_t root = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceNameMatching("IOPMrootDomain"));
	CFTypeRef supported;

	if (root == MACH_PORT_NULL)
		return;
	supported = IORegistryEntryCreateCFProperty(root,
	    CFSTR("IOSleepSupported"), NULL, 0);
	if (supported == kCFBooleanFalse) {
		IORegistryEntrySetCFProperty(root, CFSTR("SleepDisabled"),
		    kCFBooleanTrue);
		syslog(LOG_NOTICE, "this machine cannot sleep: sleep disabled");
	}
	if (supported != NULL)
		CFRelease(supported);
	IOObjectRelease(root);
}

/* ---- main ---------------------------------------------------------------------- */

int
main(int argc, char *argv[])
{
	mach_port_t port = MACH_PORT_NULL;
	dispatch_source_t mig, timer;
	kern_return_t kr;
	int token;

	(void)argc;
	(void)argv;
	openlog("powerd", LOG_PID, LOG_DAEMON);
	kr = bootstrap_check_in(bootstrap_port, SERVICE, &port);
	if (kr != KERN_SUCCESS)
		errx(1, "can't check in %s: %s", SERVICE, mach_error_string(kr));
	queue = dispatch_queue_create("powerd", NULL);

	dispatch_sync(queue, ^{
		disable_unsupported_sleep();
		load_schedule();
		apply_settings();
		apply_levels();
	});
	notify_register_dispatch(kIOPMPrefsChangeNotify, &token, queue,
	    ^(int t) {
		(void)t;
		apply_settings();
	});

	mig = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, port, 0,
	    queue);
	dispatch_source_set_event_handler(mig, ^{
		dispatch_mig_server(mig, MAX_MSG, powermanagement_server);
	});
	dispatch_resume(mig);

	/* Assertions' timeouts. */
	timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
	dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW,
	    NSEC_PER_SEC), NSEC_PER_SEC, NSEC_PER_SEC / 4);
	dispatch_source_set_event_handler(timer, ^{
		expire_check();
	});
	dispatch_resume(timer);

	syslog(LOG_NOTICE, "serving %s", SERVICE);
	dispatch_main();
}
