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
 * pmset: power management settings, through IOKit's power functions and
 * powerd. -g shows them (the active ones, the custom ones per power
 * source, what this machine can do, power sources, assertions, scheduled
 * events, thermal state); "pmset [-a | -c | -b | -u] setting value ..."
 * changes them; and sleepnow, displaysleepnow, schedule, repeat,
 * disablesleep, restoredefaults, touch and noidle act now.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

#include <sys/types.h>

#include <err.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* IOKit's (private on macOS too). */
CFMutableDictionaryRef IOPMCopyPMPreferences(void);
CFDictionaryRef IOPMCopyActivePMPreferences(void);
IOReturn IOPMSetPMPreferences(CFDictionaryRef);
bool IOPMFeatureIsAvailable(CFStringRef, CFStringRef);
IOReturn IOPMScheduleRepeatingPowerEvent(CFDictionaryRef);
CFDictionaryRef IOPMCopyRepeatingPowerEvents(void);
IOReturn IOPMCancelAllRepeatingPowerEvents(void);

/* Repeating events' keys (IOPMLibPrivate on macOS). */
#ifndef kIOPMRepeatingPowerOnKey
#define kIOPMRepeatingPowerOnKey	"RepeatingPowerOn"
#define kIOPMRepeatingPowerOffKey	"RepeatingPowerOff"
#define kIOPMDaysOfWeekKey		"weekdays"
#endif

#define AC	"AC Power"
#define BATT	"Battery Power"
#define UPS	"UPS Power"

/* pmset's names for the settings' keys. */
static const struct setting {
	const char	*name;
	const char	*key;
	bool		 minutes;	/* a timer: "never" is 0 */
	bool		 string;
} settings[] = {
	{ "displaysleep",	"Display Sleep Timer",	true,	false },
	{ "disksleep",		"Disk Sleep Timer",	true,	false },
	{ "sleep",		"System Sleep Timer",	true,	false },
	{ "womp",		"Wake On LAN",		false,	false },
	{ "ring",		"Wake On Modem Ring",	false,	false },
	{ "autorestart",	"Automatic Restart On Power Loss", false, false },
	{ "lidwake",		"Wake On Clamshell Open", false, false },
	{ "acwake",		"Wake On AC Change",	false,	false },
	{ "lessbright",		"ReduceBrightness",	false,	false },
	{ "halfdim",		"Display Sleep Uses Dim", false, false },
	{ "ttyskeepawake",	"TTYSPreventSleep",	false,	false },
	{ "hibernatemode",	"Hibernate Mode",	false,	false },
	{ "hibernatefile",	"Hibernate File",	false,	true },
	{ "standby",		"Standby Enabled",	false,	false },
	{ "standbydelay",	"Standby Delay",	false,	false },
	{ "autopoweroff",	"AutoPowerOff Enabled",	false,	false },
	{ "autopoweroffdelay",	"AutoPowerOff Delay",	false,	false },
	{ "powernap",		"DarkWakeBackgroundTasks", false, false },
	{ "tcpkeepalive",	"TCPKeepAlivePref",	false,	false },
	{ "networkoversleep",	"PrioritizeNetworkReachabilityOverSleep",
	    false, false },
	{ "proximitywake",	"ProximityDarkWake",	false,	false },
	{ "gpuswitch",		"GPUSwitch",		false,	false },
	{ "dps",		"Dynamic Power Step",	false,	false },
	{ "Sleep On Power Button", "Sleep On Power Button", false, false },
};
#define NSETTINGS	(sizeof(settings) / sizeof(settings[0]))

static io_registry_entry_t
root_domain(void)
{
	return (IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceNameMatching("IOPMrootDomain")));
}

static bool
root_bool(const char *key, bool dflt)
{
	io_registry_entry_t root = root_domain();
	CFStringRef k = CFStringCreateWithCString(NULL, key,
	    kCFStringEncodingUTF8);
	CFTypeRef v = root ? IORegistryEntryCreateCFProperty(root, k, NULL, 0) :
	    NULL;
	bool b = dflt;

	CFRelease(k);
	if (v != NULL) {
		if (CFGetTypeID(v) == CFBooleanGetTypeID())
			b = CFBooleanGetValue(v);
		CFRelease(v);
	}
	if (root)
		IOObjectRelease(root);
	return (b);
}

static bool
sleep_supported(void)
{
	return (root_bool("IOSleepSupported", true));
}

static bool
sleep_disabled(void)
{
	return (root_bool("SleepDisabled", false));
}

static const struct setting *
find_setting(const char *name)
{
	size_t i;

	for (i = 0; i < NSETTINGS; i++)
		if (strcasecmp(settings[i].name, name) == 0)
			return (&settings[i]);
	return (NULL);
}

static void
print_value(CFTypeRef v)
{
	char buf[1024];

	if (CFGetTypeID(v) == CFNumberGetTypeID()) {
		long n = 0;

		CFNumberGetValue(v, kCFNumberLongType, &n);
		printf("%ld", n);
	} else if (CFGetTypeID(v) == CFStringGetTypeID() &&
	    CFStringGetCString(v, buf, sizeof(buf), kCFStringEncodingUTF8)) {
		printf("%s", buf);
	} else if (CFGetTypeID(v) == CFBooleanGetTypeID()) {
		printf("%d", CFBooleanGetValue(v) ? 1 : 0);
	}
}

/* The settings of one power source, as macOS lists them. */
static void
print_profile(CFDictionaryRef d, bool live)
{
	size_t i;

	for (i = 0; i < NSETTINGS; i++) {
		CFStringRef key = CFStringCreateWithCString(NULL, settings[i].key,
		    kCFStringEncodingUTF8);
		CFTypeRef v = CFDictionaryGetValue(d, key);

		CFRelease(key);
		if (v == NULL)
			continue;
		printf(" %-20s ", settings[i].name);
		print_value(v);
		if (live && strcmp(settings[i].name, "sleep") == 0) {
			if (!sleep_supported())
				printf(" (sleep is not supported on this machine)");
			else if (sleep_disabled())
				printf(" (sleep disabled)");
		}
		printf("\n");
	}
}

static int
get_live(void)
{
	CFDictionaryRef all = IOPMCopyActivePMPreferences();
	CFTypeRef ps = IOPSCopyPowerSourcesInfo();
	CFStringRef src = ps ? IOPSGetProvidingPowerSourceType(ps) : NULL;
	CFDictionaryRef cur;

	printf("System-wide power settings:\n");
	if (sleep_disabled())
		printf(" SleepDisabled\t\t1\n");
	printf("Currently in use:\n");
	if (src == NULL)
		src = CFSTR(AC);
	cur = all ? CFDictionaryGetValue(all, src) : NULL;
	if (cur == NULL && all != NULL)
		cur = CFDictionaryGetValue(all, CFSTR(AC));
	if (cur != NULL)
		print_profile(cur, true);
	if (all != NULL)
		CFRelease(all);
	if (ps != NULL)
		CFRelease(ps);
	return (0);
}

static int
get_custom(void)
{
	CFDictionaryRef all = IOPMCopyActivePMPreferences();
	static const char *const sources[] = { BATT, AC, UPS };
	size_t i;

	for (i = 0; all != NULL && i < 3; i++) {
		CFStringRef k = CFStringCreateWithCString(NULL, sources[i],
		    kCFStringEncodingUTF8);
		CFDictionaryRef d = CFDictionaryGetValue(all, k);

		CFRelease(k);
		/* Only the sources this machine has. */
		if (d == NULL || (i != 1 && !IOPMFeatureIsAvailable(
		    CFSTR("Display Sleep Timer"), i == 0 ? CFSTR(BATT) : CFSTR(UPS))))
			continue;
		printf("%s:\n", sources[i]);
		print_profile(d, false);
	}
	if (all != NULL)
		CFRelease(all);
	return (0);
}

static int
get_cap(void)
{
	size_t i;

	printf("Capabilities for AC Power:\n");
	for (i = 0; i < NSETTINGS; i++) {
		CFStringRef key = CFStringCreateWithCString(NULL, settings[i].key,
		    kCFStringEncodingUTF8);

		if (IOPMFeatureIsAvailable(key, CFSTR(AC)))
			printf(" %s\n", settings[i].name);
		CFRelease(key);
	}
	return (0);
}

static int
get_batt(void)
{
	CFTypeRef ps = IOPSCopyPowerSourcesInfo();
	CFArrayRef list = ps ? IOPSCopyPowerSourcesList(ps) : NULL;
	CFStringRef src = ps ? IOPSGetProvidingPowerSourceType(ps) : NULL;
	char buf[128] = "AC Power";
	CFIndex i;

	if (src != NULL)
		CFStringGetCString(src, buf, sizeof(buf), kCFStringEncodingUTF8);
	printf("Now drawing from '%s'\n", buf);
	for (i = 0; list != NULL && i < CFArrayGetCount(list); i++) {
		CFDictionaryRef d = IOPSGetPowerSourceDescription(ps,
		    CFArrayGetValueAtIndex(list, i));
		char name[128] = "";
		int cur = 0, max = 100;
		CFTypeRef v;

		if (d == NULL)
			continue;
		if ((v = CFDictionaryGetValue(d, CFSTR(kIOPSNameKey))) != NULL)
			CFStringGetCString(v, name, sizeof(name),
			    kCFStringEncodingUTF8);
		if ((v = CFDictionaryGetValue(d, CFSTR(kIOPSCurrentCapacityKey))))
			CFNumberGetValue(v, kCFNumberIntType, &cur);
		if ((v = CFDictionaryGetValue(d, CFSTR(kIOPSMaxCapacityKey))))
			CFNumberGetValue(v, kCFNumberIntType, &max);
		printf(" -%s\t%d%%; %s; present: true\n", name,
		    max ? cur * 100 / max : 0, CFDictionaryGetValue(d,
		    CFSTR(kIOPSIsChargingKey)) == kCFBooleanTrue ? "charging" :
		    "discharging");
	}
	if (list != NULL)
		CFRelease(list);
	if (ps != NULL)
		CFRelease(ps);
	return (0);
}

static int
get_assertions(void)
{
	CFDictionaryRef status = NULL, byproc = NULL;
	CFIndex n, i, j;
	char buf[256], when[64];
	time_t now = time(NULL);

	if (IOPMCopyAssertionsStatus(&status) != kIOReturnSuccess ||
	    status == NULL) {
		fprintf(stderr, "pmset: can't read the assertions (is powerd "
		    "running?)\n");
		return (1);
	}
	strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S %z", localtime(&now));
	printf("%s \nAssertion status system-wide:\n", when);
	n = CFDictionaryGetCount(status);
	{
		const void **keys = calloc((size_t)n + 1, sizeof(*keys));
		const void **vals = calloc((size_t)n + 1, sizeof(*vals));
		CFMutableArrayRef sorted = CFArrayCreateMutable(NULL, 0,
		    &kCFTypeArrayCallBacks);

		CFDictionaryGetKeysAndValues(status, keys, vals);
		for (i = 0; i < n; i++)
			CFArrayAppendValue(sorted, keys[i]);
		CFArraySortValues(sorted, CFRangeMake(0, n),
		    (CFComparatorFunction)CFStringCompare, NULL);
		for (i = 0; i < n; i++) {
			CFStringRef k = CFArrayGetValueAtIndex(sorted, i);
			int v = 0;

			CFNumberGetValue(CFDictionaryGetValue(status, k),
			    kCFNumberIntType, &v);
			CFStringGetCString(k, buf, sizeof(buf),
			    kCFStringEncodingUTF8);
			printf("   %-30s %d\n", buf, v);
		}
		CFRelease(sorted);
		free(keys);
		free(vals);
	}
	CFRelease(status);

	printf("Listed by owning process:\n");
	if (IOPMCopyAssertionsByProcess(&byproc) == kIOReturnSuccess &&
	    byproc != NULL) {
		n = CFDictionaryGetCount(byproc);
		const void **keys = calloc((size_t)n + 1, sizeof(*keys));
		const void **vals = calloc((size_t)n + 1, sizeof(*vals));

		CFDictionaryGetKeysAndValues(byproc, keys, vals);
		for (i = 0; i < n; i++) {
			CFArrayRef list = vals[i];
			int pid = 0;

			CFNumberGetValue(keys[i], kCFNumberIntType, &pid);
			for (j = 0; j < CFArrayGetCount(list); j++) {
				CFDictionaryRef a = CFArrayGetValueAtIndex(list, j);
				char type[128] = "", name[256] = "", proc[64] = "?";
				CFTypeRef t = CFDictionaryGetValue(a,
				    kIOPMAssertionTypeKey);
				CFTypeRef nm = CFDictionaryGetValue(a,
				    kIOPMAssertionNameKey);
				CFTypeRef id = CFDictionaryGetValue(a,
				    CFSTR("AssertionId"));
				CFTypeRef cd = CFDictionaryGetValue(a,
				    CFSTR("AssertStartWhen"));
				long age = 0;
				int aid = 0;
				FILE *ps;
				char cmd[64];

				if (t)
					CFStringGetCString(t, type, sizeof(type),
					    kCFStringEncodingUTF8);
				if (nm)
					CFStringGetCString(nm, name, sizeof(name),
					    kCFStringEncodingUTF8);
				if (id)
					CFNumberGetValue(id, kCFNumberIntType, &aid);
				if (cd && CFGetTypeID(cd) == CFDateGetTypeID())
					age = (long)(CFAbsoluteTimeGetCurrent() -
					    CFDateGetAbsoluteTime(cd));
				snprintf(cmd, sizeof(cmd), "/bin/ps -o comm= -p %d",
				    pid);
				if ((ps = popen(cmd, "r")) != NULL) {
					if (fgets(proc, sizeof(proc), ps) != NULL)
						proc[strcspn(proc, "\n")] = '\0';
					pclose(ps);
				}
				printf("   pid %d(%s): [0x%016x] %02ld:%02ld:%02ld %s "
				    "named: \"%s\" \n", pid, strrchr(proc, '/') ?
				    strrchr(proc, '/') + 1 : proc, aid, age / 3600,
				    age / 60 % 60, age % 60, type, name);
			}
		}
		free(keys);
		free(vals);
		CFRelease(byproc);
	}
	printf("No kernel assertions.\n");
	return (0);
}

static void
print_event(CFDictionaryRef e, int idx)
{
	char type[64] = "", by[128] = "", when[64] = "";
	CFTypeRef t = CFDictionaryGetValue(e, CFSTR(kIOPMPowerEventTypeKey));
	CFTypeRef b = CFDictionaryGetValue(e, CFSTR(kIOPMPowerEventAppNameKey));
	CFTypeRef d = CFDictionaryGetValue(e, CFSTR(kIOPMPowerEventTimeKey));

	if (t)
		CFStringGetCString(t, type, sizeof(type), kCFStringEncodingUTF8);
	if (b)
		CFStringGetCString(b, by, sizeof(by), kCFStringEncodingUTF8);
	if (d && CFGetTypeID(d) == CFDateGetTypeID()) {
		time_t secs = (time_t)(CFDateGetAbsoluteTime(d) +
		    kCFAbsoluteTimeIntervalSince1970);

		strftime(when, sizeof(when), "%m/%d/%y %H:%M:%S",
		    localtime(&secs));
	}
	printf(" [%d]  %s at %s by '%s'\n", idx, type, when, by);
}

static int
get_sched(void)
{
	CFArrayRef ev = IOPMCopyScheduledPowerEvents();
	CFDictionaryRef rep = IOPMCopyRepeatingPowerEvents();
	CFIndex i;

	if (ev != NULL && CFArrayGetCount(ev) > 0) {
		printf("Scheduled power events:\n");
		for (i = 0; i < CFArrayGetCount(ev); i++)
			print_event(CFArrayGetValueAtIndex(ev, i), (int)i);
	}
	if (rep != NULL && CFDictionaryGetCount(rep) > 0) {
		static const char *const which[] = {
			kIOPMRepeatingPowerOnKey, kIOPMRepeatingPowerOffKey };

		printf("Repeating power events:\n");
		for (i = 0; i < 2; i++) {
			CFStringRef k = CFStringCreateWithCString(NULL, which[i],
			    kCFStringEncodingUTF8);
			CFDictionaryRef r = CFDictionaryGetValue(rep, k);
			char type[64] = "";
			int mins = 0, days = 0;
			CFTypeRef v;

			CFRelease(k);
			if (r == NULL)
				continue;
			if ((v = CFDictionaryGetValue(r,
			    CFSTR(kIOPMPowerEventTypeKey))) != NULL)
				CFStringGetCString(v, type, sizeof(type),
				    kCFStringEncodingUTF8);
			if ((v = CFDictionaryGetValue(r,
			    CFSTR(kIOPMPowerEventTimeKey))) != NULL)
				CFNumberGetValue(v, kCFNumberIntType, &mins);
			if ((v = CFDictionaryGetValue(r,
			    CFSTR(kIOPMDaysOfWeekKey))) != NULL)
				CFNumberGetValue(v, kCFNumberIntType, &days);
			printf("  %s at %d:%02d%s on days 0x%x\n", type,
			    mins / 60 % 12 ? mins / 60 % 12 : 12, mins % 60,
			    mins / 60 >= 12 ? "PM" : "AM", days);
		}
	}
	if ((ev == NULL || CFArrayGetCount(ev) == 0) &&
	    (rep == NULL || CFDictionaryGetCount(rep) == 0))
		printf("No scheduled events.\n");
	if (ev != NULL)
		CFRelease(ev);
	if (rep != NULL)
		CFRelease(rep);
	return (0);
}

/* ---- Changes ------------------------------------------------------------------- */

static void
need_root(void)
{
	if (geteuid() != 0)
		errx(1, "Must be run as root to modify settings");
}

static int
set_settings(int argc, char **argv, const char *const *sources, int nsources)
{
	CFMutableDictionaryRef all = IOPMCopyPMPreferences();
	int i, s;

	need_root();
	if (all == NULL)
		all = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
	if (argc % 2 != 0)
		errx(1, "Usage: pmset [-a | -b | -c | -u] <setting> <value> ...");
	for (s = 0; s < nsources; s++) {
		CFStringRef sk = CFStringCreateWithCString(NULL, sources[s],
		    kCFStringEncodingUTF8);
		CFDictionaryRef old = CFDictionaryGetValue(all, sk);
		CFMutableDictionaryRef prof = old ?
		    CFDictionaryCreateMutableCopy(NULL, 0, old) :
		    CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);

		for (i = 0; i < argc; i += 2) {
			const struct setting *st = find_setting(argv[i]);
			CFStringRef key;
			CFTypeRef v;
			char *end;
			long n;

			if (st == NULL)
				errx(1, "Unrecognized setting: %s", argv[i]);
			key = CFStringCreateWithCString(NULL, st->key,
			    kCFStringEncodingUTF8);
			if (st->string) {
				v = CFStringCreateWithCString(NULL, argv[i + 1],
				    kCFStringEncodingUTF8);
			} else {
				if (st->minutes && strcmp(argv[i + 1], "never") == 0)
					n = 0;
				else {
					n = strtol(argv[i + 1], &end, 10);
					if (*end != '\0' || n < 0)
						errx(1, "Bad value for %s: %s",
						    argv[i], argv[i + 1]);
				}
				v = CFNumberCreate(NULL, kCFNumberLongType, &n);
			}
			CFDictionarySetValue(prof, key, v);
			CFRelease(key);
			CFRelease(v);
		}
		CFDictionarySetValue(all, sk, prof);
		CFRelease(prof);
		CFRelease(sk);
	}
	if (IOPMSetPMPreferences(all) != kIOReturnSuccess)
		errx(1, "Error writing the settings");
	CFRelease(all);
	return (0);
}

static int
sleep_now(void)
{
	io_connect_t pm;
	IOReturn r;

	if (!sleep_supported()) {
		fprintf(stderr, "Sleep is not supported on this machine.\n");
		return (1);
	}
	if (sleep_disabled()) {
		fprintf(stderr, "Sleep is disabled (pmset disablesleep).\n");
		return (1);
	}
	printf("Sleeping now...\n");
	fflush(stdout);
	pm = IOPMFindPowerManagement(MACH_PORT_NULL);
	r = IOPMSleepSystem(pm);
	IOServiceClose(pm);
	if (r != kIOReturnSuccess) {
		fprintf(stderr, "pmset: sleep was refused: %s\n",
		    r == kIOReturnNotPrivileged ? "Must be run as root" :
		    "the system did not allow it");
		return (1);
	}
	return (0);
}

/* As macOS does: the display wrangler is asked to go idle. */
static int
display_sleep_now(void)
{
	io_registry_entry_t w = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceNameMatching("IODisplayWrangler"));
	kern_return_t kr;

	if (w == MACH_PORT_NULL) {
		fprintf(stderr, "Display sleep is not supported on this "
		    "machine.\n");
		return (1);
	}
	kr = IORegistryEntrySetCFProperty(w, CFSTR("IORequestIdle"),
	    kCFBooleanTrue);
	IOObjectRelease(w);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "pmset: the display could not sleep\n");
		return (1);
	}
	return (0);
}

static int
disable_sleep(const char *arg)
{
	io_registry_entry_t root;
	bool on;
	kern_return_t kr;

	need_root();
	if (arg == NULL || (strcmp(arg, "0") != 0 && strcmp(arg, "1") != 0))
		errx(1, "Usage: pmset disablesleep 0 | 1");
	on = strcmp(arg, "1") == 0;
	if (!on && !sleep_supported())
		errx(1, "Sleep is not supported on this machine: it stays "
		    "disabled.");
	root = root_domain();
	kr = IORegistryEntrySetCFProperty(root, CFSTR("SleepDisabled"),
	    on ? kCFBooleanTrue : kCFBooleanFalse);
	IOObjectRelease(root);
	if (kr != KERN_SUCCESS)
		errx(1, "Error setting disablesleep");
	return (0);
}

static CFDateRef
parse_date(const char *s)
{
	struct tm tm;
	time_t t;

	memset(&tm, 0, sizeof(tm));
	if (strptime(s, "%m/%d/%y %H:%M:%S", &tm) == NULL &&
	    strptime(s, "%m/%d/%Y %H:%M:%S", &tm) == NULL)
		return (NULL);
	tm.tm_isdst = -1;
	if ((t = mktime(&tm)) == (time_t)-1)
		return (NULL);
	return (CFDateCreate(NULL, (CFAbsoluteTime)t -
	    kCFAbsoluteTimeIntervalSince1970));
}

static CFStringRef
event_type(const char *s)
{
	if (strcmp(s, "sleep") == 0)
		return (CFSTR(kIOPMAutoSleep));
	if (strcmp(s, "wake") == 0)
		return (CFSTR(kIOPMAutoWake));
	if (strcmp(s, "poweron") == 0)
		return (CFSTR(kIOPMAutoPowerOn));
	if (strcmp(s, "shutdown") == 0)
		return (CFSTR(kIOPMAutoShutdown));
	if (strcmp(s, "wakeorpoweron") == 0)
		return (CFSTR(kIOPMAutoWakeOrPowerOn));
	if (strcmp(s, "restart") == 0)
		return (CFSTR(kIOPMAutoRestart));
	return (NULL);
}

static int
schedule(int argc, char **argv)
{
	bool cancel = false;
	CFStringRef type, owner;
	CFDateRef when;
	IOReturn r;

	need_root();
	if (argc > 0 && strcmp(argv[0], "cancel") == 0) {
		cancel = true;
		argc--;
		argv++;
	}
	if (argc < 2 || (type = event_type(argv[0])) == NULL)
		errx(1, "Usage: pmset schedule [cancel] <type> \"MM/dd/yy "
		    "HH:mm:ss\" [owner]");
	if ((when = parse_date(argv[1])) == NULL)
		errx(1, "Bad date: %s (MM/dd/yy HH:mm:ss)", argv[1]);
	owner = CFStringCreateWithCString(NULL, argc > 2 ? argv[2] : "pmset",
	    kCFStringEncodingUTF8);
	r = cancel ? IOPMCancelScheduledPowerEvent(when, owner, type) :
	    IOPMSchedulePowerEvent(when, owner, type);
	CFRelease(owner);
	CFRelease(when);
	if (r != kIOReturnSuccess)
		errx(1, "Error %s the event (0x%x)", cancel ? "cancelling" :
		    "scheduling", r);
	return (0);
}

/* "repeat cancel" or "repeat <type> <days> <HH:mm:ss>" */
static int
repeat(int argc, char **argv)
{
	CFMutableDictionaryRef all, ev;
	CFStringRef type;
	CFNumberRef n;
	int h, m, s = 0, days = 0, mins;
	const char *p;
	bool off;

	need_root();
	if (argc == 1 && strcmp(argv[0], "cancel") == 0) {
		if (IOPMCancelAllRepeatingPowerEvents() != kIOReturnSuccess)
			errx(1, "Error cancelling the repeating events");
		return (0);
	}
	if (argc != 3 || (type = event_type(argv[0])) == NULL ||
	    sscanf(argv[2], "%d:%d:%d", &h, &m, &s) < 2)
		errx(1, "Usage: pmset repeat cancel | <type> <weekdays> "
		    "<HH:mm:ss>   (weekdays: subset of MTWRFSU)");
	for (p = argv[1]; *p != '\0'; p++) {
		const char *const order = "MTWRFSU";
		const char *c = strchr(order, *p);

		if (c == NULL)
			errx(1, "Bad weekdays: %s (subset of MTWRFSU)", argv[1]);
		days |= 1 << (c - order);
	}
	mins = h * 60 + m;
	off = CFEqual(type, CFSTR(kIOPMAutoSleep)) ||
	    CFEqual(type, CFSTR(kIOPMAutoShutdown)) ||
	    CFEqual(type, CFSTR(kIOPMAutoRestart));
	ev = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(ev, CFSTR(kIOPMPowerEventTypeKey), type);
	n = CFNumberCreate(NULL, kCFNumberIntType, &mins);
	CFDictionarySetValue(ev, CFSTR(kIOPMPowerEventTimeKey), n);
	CFRelease(n);
	n = CFNumberCreate(NULL, kCFNumberIntType, &days);
	CFDictionarySetValue(ev, CFSTR(kIOPMDaysOfWeekKey), n);
	CFRelease(n);
	all = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(all, off ? CFSTR(kIOPMRepeatingPowerOffKey) :
	    CFSTR(kIOPMRepeatingPowerOnKey), ev);
	CFRelease(ev);
	if (IOPMScheduleRepeatingPowerEvent(all) != kIOReturnSuccess)
		errx(1, "Error scheduling the repeating event");
	CFRelease(all);
	return (0);
}

static volatile sig_atomic_t stop;

static void
on_signal(int sig)
{
	(void)sig;
	stop = 1;
}

static int
noidle(void)
{
	IOPMAssertionID id;

	if (IOPMAssertionCreateWithName(kIOPMAssertionTypePreventUserIdleSystemSleep,
	    kIOPMAssertionLevelOn, CFSTR("pmset prevent sleep"), &id) !=
	    kIOReturnSuccess)
		errx(1, "Error creating the assertion");
	printf("Preventing idle sleep (^C to exit)...\n");
	fflush(stdout);
	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	while (!stop)
		pause();
	IOPMAssertionRelease(id);
	return (0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: pmset <options>\n"
	    "See pmset(1) for details: \"man pmset\"\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	if (argc < 2)
		usage();
	if (strcmp(argv[1], "-g") == 0) {
		const char *what = argc > 2 ? argv[2] : "live";

		if (strcmp(what, "live") == 0)
			return (get_live());
		if (strcmp(what, "custom") == 0)
			return (get_custom());
		if (strcmp(what, "cap") == 0)
			return (get_cap());
		if (strcmp(what, "batt") == 0 || strcmp(what, "ps") == 0)
			return (get_batt());
		if (strcmp(what, "assertions") == 0)
			return (get_assertions());
		if (strcmp(what, "sched") == 0)
			return (get_sched());
		if (strcmp(what, "therm") == 0) {
			printf("Note: No thermal warning level has been "
			    "recorded\nNote: No performance warning level has "
			    "been recorded\nNote: No CPU power status has been "
			    "recorded\n");
			return (0);
		}
		if (strcmp(what, "uuid") == 0)
			return (0);	/* no sleep/wake cycle yet */
		if (strcmp(what, "log") == 0) {
			printf("No power management log yet.\n");
			return (0);
		}
		fprintf(stderr, "pmset: unknown -g argument: %s\n", what);
		return (1);
	}
	if (strcmp(argv[1], "-a") == 0 || strcmp(argv[1], "-c") == 0 ||
	    strcmp(argv[1], "-b") == 0 || strcmp(argv[1], "-u") == 0) {
		static const char *const allsrc[] = { AC, BATT, UPS };
		static const char *const ac[] = { AC };
		static const char *const batt[] = { BATT };
		static const char *const ups[] = { UPS };

		switch (argv[1][1]) {
		case 'a': return (set_settings(argc - 2, argv + 2, allsrc, 3));
		case 'c': return (set_settings(argc - 2, argv + 2, ac, 1));
		case 'b': return (set_settings(argc - 2, argv + 2, batt, 1));
		default:  return (set_settings(argc - 2, argv + 2, ups, 1));
		}
	}
	if (find_setting(argv[1]) != NULL) {
		static const char *const allsrc[] = { AC, BATT, UPS };

		return (set_settings(argc - 1, argv + 1, allsrc, 3));
	}
	if (strcmp(argv[1], "restoredefaults") == 0) {
		need_root();
		return (IOPMSetPMPreferences(NULL) == kIOReturnSuccess ? 0 : 1);
	}
	if (strcmp(argv[1], "touch") == 0) {
		CFMutableDictionaryRef all = IOPMCopyPMPreferences();

		need_root();
		return (all != NULL && IOPMSetPMPreferences(all) ==
		    kIOReturnSuccess ? 0 : 1);
	}
	if (strcmp(argv[1], "sleepnow") == 0)
		return (sleep_now());
	if (strcmp(argv[1], "displaysleepnow") == 0)
		return (display_sleep_now());
	if (strcmp(argv[1], "disablesleep") == 0)
		return (disable_sleep(argc > 2 ? argv[2] : NULL));
	if (strcmp(argv[1], "schedule") == 0)
		return (schedule(argc - 2, argv + 2));
	if (strcmp(argv[1], "repeat") == 0)
		return (repeat(argc - 2, argv + 2));
	if (strcmp(argv[1], "noidle") == 0)
		return (noidle());
	usage();
}
