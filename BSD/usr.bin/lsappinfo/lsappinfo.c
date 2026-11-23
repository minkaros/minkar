/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * lsappinfo: the running applications, as LaunchServices' session
 * (launchservicesd) keeps them, in the reference's command's words and
 * layout. Each application is named by its ASN, "ASN:0x0-0x<serial>:"
 * (the serial the session gave it at check-in). Commands:
 *
 *   list                         every application and what is known of it
 *   processlist                  their ASNs, in check-in order (list too)
 *   visibleProcessList [-includeHidden]
 *                                the ASNs of the shown (foreground) ones,
 *                                front first
 *   front                        the front application's ASN
 *   info [-only key]... app      one application (its keys: -only)
 *   find key=value...            the ASNs of those that match
 *   setfront app                 makes it the front application
 *
 * An application is given by its ASN, its pid, its bundle identifier or
 * its name. With no command: list.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <LaunchServices/LSSession.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

/* NSApplicationActivateIgnoringOtherApps. */
#define kActivateIgnoringOtherApps	(1u << 1)

/* The session's applications, front to back, and the same in the order
   they checked in. */
static LSSessionApp *apps, *started;
static uint32_t appCount;

static int
bySerial(const void *a, const void *b)
{
	uint32_t x = ((const LSSessionApp *)a)->serial, y = ((const LSSessionApp *)b)->serial;

	return x < y ? -1 : x > y;
}

static void
loadApps(void)
{
	if (LSSessionCopyApps(&apps, &appCount) != kLSSessionSuccess)
		errx(1, "cannot reach LaunchServices' session");
	if ((started = calloc(appCount ? appCount : 1, sizeof(*started))) == NULL)
		err(1, NULL);
	memcpy(started, apps, appCount * sizeof(*started));
	qsort(started, appCount, sizeof(*started), bySerial);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: lsappinfo [ command [ command-args ] ]...\n"
	    "  commands: list, processlist, visibleProcessList [ -includeHidden ], front,\n"
	    "            info [ -only key ]... <app>, find [ key=value ]..., setfront <app>\n"
	    "  <app>: an ASN (ASN:0x0-0x...:), a pid, a bundle identifier or a name\n");
	exit(2);
}

static const char *
typeOf(const LSSessionApp *app)
{
	switch (app->activationPolicy) {
	case 0:
		return "Foreground";
	case 1:
		return "UIElement";
	default:
		return "BackgroundOnly";
	}
}

/* The ASN, as the session numbers the application. */
static void
printASN(const LSSessionApp *app, const char *suffix)
{
	printf("ASN:0x0-0x%x:%s", app->serial, suffix);
}

/* The name for ASN lists: spaces as underscores. */
static void
printListed(const LSSessionApp *app)
{
	printf("ASN:0x0-0x%x-\"", app->serial);
	for (const char *c = app->name; *c != '\0'; c++)
		putchar(*c == ' ' ? '_' : *c);
	printf("\": ");
}

/* What the application's bundle says (NULL when it is not a bundle). */
static CFDictionaryRef
bundleInfo(const LSSessionApp *app)
{
	CFURLRef url;
	CFBundleRef bundle;
	CFDictionaryRef info = NULL;
	size_t length = strlen(app->path);

	if (length < 5 || strcmp(app->path + length - 4, ".app") != 0)
		return NULL;
	url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)app->path,
	    (CFIndex)length, true);
	if (url == NULL)
		return NULL;
	bundle = CFBundleCreate(NULL, url);
	CFRelease(url);
	if (bundle == NULL)
		return NULL;
	info = CFBundleGetInfoDictionary(bundle);
	if (info != NULL)
		CFRetain(info);
	CFRelease(bundle);
	return info;
}

static void
copyInfoString(CFDictionaryRef info, CFStringRef key, char *buffer, size_t size)
{
	CFTypeRef value = info != NULL ? CFDictionaryGetValue(info, key) : NULL;

	buffer[0] = '\0';
	if (value != NULL && CFGetTypeID(value) == CFStringGetTypeID())
		CFStringGetCString(value, buffer, (CFIndex)size, kCFStringEncodingUTF8);
}

/* The executable: the bundle's, or the path itself. */
static void
executablePath(const LSSessionApp *app, CFDictionaryRef info, char *buffer, size_t size)
{
	char executable[256];

	copyInfoString(info, CFSTR("CFBundleExecutable"), executable, sizeof(executable));
	if (info != NULL && executable[0] != '\0')
		snprintf(buffer, size, "%s/Contents/MacOS/%s", app->path, executable);
	else
		snprintf(buffer, size, "%s", app->path);
}

static void
printQuotedOrNull(const char *label, const char *value)
{
	if (value[0] != '\0')
		printf("%s=\"%s\"", label, value);
	else
		printf("%s=[ NULL ] ", label);
}

static void
printCheckIn(const LSSessionApp *app)
{
	time_t when = (time_t)app->launchTime;
	struct tm tm;
	char date[32];
	long ago = (long)(time(NULL) - when);

	localtime_r(&when, &tm);
	strftime(date, sizeof(date), "%Y/%m/%d %H:%M:%S", &tm);
	if (ago < 0)
		ago = 0;
	printf("    checkin time = %s ( ", date);
	if (ago >= 86400)
		printf("%ld day%s, ", ago / 86400, ago / 86400 == 1 ? "" : "s");
	if (ago >= 3600)
		printf("%ld hour%s, ", ago % 86400 / 3600, ago % 86400 / 3600 == 1 ? "" : "s");
	if (ago >= 60)
		printf("%ld minute%s, ", ago % 3600 / 60, ago % 3600 / 60 == 1 ? "" : "s");
	printf("%ld seconds ago )\n", ago % 60);
}

/* An application as list and info show it. */
static void
printApp(const LSSessionApp *app, int number)
{
	CFDictionaryRef info = bundleInfo(app);
	char executable[1024], version[64], fileType[16], creator[16];

	executablePath(app, info, executable, sizeof(executable));
	copyInfoString(info, CFSTR("CFBundleShortVersionString"), version, sizeof(version));
	if (version[0] == '\0')
		copyInfoString(info, CFSTR("CFBundleVersion"), version, sizeof(version));
	copyInfoString(info, CFSTR("CFBundlePackageType"), fileType, sizeof(fileType));
	copyInfoString(info, CFSTR("CFBundleSignature"), creator, sizeof(creator));

	if (number > 0)
		printf("%2d) ", number);
	printf("\"%s\" ", app->name);
	printASN(app, " \n");
	printf("    ");
	printQuotedOrNull("bundleID", app->bundleIdentifier);
	printf("\n    bundle path=\"%s\"\n", app->path);
	printf("    executable path=\"%s\"\n", executable);
	printf("    pid = %d type=\"%s\"%s ", app->pid, typeOf(app),
	    (app->flags & kLSSessionAppHiddenState) ? " isHidden" : "");
	printQuotedOrNull("Version", version);
	printf(" fileType=\"%s\" creator=\"%s\" Arch=x86_64 \n",
	    fileType[0] != '\0' ? fileType : "????", creator[0] != '\0' ? creator : "????");
	printCheckIn(app);
	if (info != NULL)
		CFRelease(info);
}

/* An application from its specifier, or NULL. */
static const LSSessionApp *
findApp(const char *specifier)
{
	unsigned serial;
	char *end;
	long pid;

	if (sscanf(specifier, "ASN:0x0-0x%x", &serial) == 1) {
		for (uint32_t i = 0; i < appCount; i++)
			if (apps[i].serial == serial)
				return &apps[i];
		return NULL;
	}
	pid = strtol(specifier, &end, 10);
	if (*specifier != '\0' && *end == '\0') {
		for (uint32_t i = 0; i < appCount; i++)
			if (apps[i].pid == pid)
				return &apps[i];
		return NULL;
	}
	for (uint32_t i = 0; i < appCount; i++)
		if (strcasecmp(apps[i].bundleIdentifier, specifier) == 0)
			return &apps[i];
	for (uint32_t i = 0; i < appCount; i++)
		if (strcasecmp(apps[i].name, specifier) == 0)
			return &apps[i];
	return NULL;
}

/* info -only: a key by its short name or its own, and its value. */
static void
printKey(const LSSessionApp *app, const char *key)
{
	CFDictionaryRef info = bundleInfo(app);
	char buffer[1024];

	if (strcasecmp(key, "bundleid") == 0 || strcmp(key, "CFBundleIdentifier") == 0)
		printf("\"CFBundleIdentifier\"=\"%s\"\n", app->bundleIdentifier);
	else if (strcasecmp(key, "pid") == 0)
		printf("\"pid\"=%d\n", app->pid);
	else if (strcasecmp(key, "name") == 0 || strcmp(key, "LSDisplayName") == 0)
		printf("\"LSDisplayName\"=\"%s\"\n", app->name);
	else if (strcasecmp(key, "bundlepath") == 0 || strcmp(key, "LSBundlePath") == 0)
		printf("\"LSBundlePath\"=\"%s\"\n", app->path);
	else if (strcasecmp(key, "executablepath") == 0 || strcmp(key, "CFBundleExecutablePath") == 0) {
		executablePath(app, info, buffer, sizeof(buffer));
		printf("\"CFBundleExecutablePath\"=\"%s\"\n", buffer);
	} else if (strcasecmp(key, "type") == 0 || strcmp(key, "ApplicationType") == 0)
		printf("\"ApplicationType\"=\"%s\"\n", typeOf(app));
	else if (strcasecmp(key, "version") == 0) {
		copyInfoString(info, CFSTR("CFBundleVersion"), buffer, sizeof(buffer));
		printf("\"CFBundleVersion\"=\"%s\"\n", buffer);
	} else if (strcasecmp(key, "hidden") == 0 || strcmp(key, "IsHiddenAttr") == 0)
		printf("\"IsHiddenAttr\"=%s\n", (app->flags & kLSSessionAppHiddenState) ? "true" : "false");
	else if (strcasecmp(key, "asn") == 0) {
		printf("\"ASN\"=");
		printASN(app, "\n");
	} else
		printf("\"%s\"=[ NULL ]\n", key);
	if (info != NULL)
		CFRelease(info);
}

static const LSSessionApp *
frontApp(void)
{
	for (uint32_t i = 0; i < appCount; i++)
		if (apps[i].flags & kLSSessionAppActive)
			return &apps[i];
	return NULL;
}

/* find: each key=value must match (bundleid, pid, name, type). */
static int
matches(const LSSessionApp *app, int argc, char **argv)
{
	for (int i = 0; i < argc; i++) {
		const char *equals = strchr(argv[i], '=');
		size_t keyLength;
		const char *value;

		if (equals == NULL)
			return 0;
		keyLength = (size_t)(equals - argv[i]);
		value = equals + 1;
		if (strncasecmp(argv[i], "bundleid", keyLength) == 0 && keyLength == 8) {
			if (strcasecmp(app->bundleIdentifier, value) != 0)
				return 0;
		} else if (strncasecmp(argv[i], "pid", keyLength) == 0 && keyLength == 3) {
			if (app->pid != atoi(value))
				return 0;
		} else if (strncasecmp(argv[i], "name", keyLength) == 0 && keyLength == 4) {
			if (strcasecmp(app->name, value) != 0)
				return 0;
		} else if (strncasecmp(argv[i], "type", keyLength) == 0 && keyLength == 4) {
			if (strcasecmp(typeOf(app), value) != 0)
				return 0;
		} else
			return 0;
	}
	return 1;
}

int
main(int argc, char *argv[])
{
	int i = 1;

	/* The reference's options that change nothing here. */
	while (i < argc && argv[i][0] == '-' && strcmp(argv[i], "-only") != 0) {
		if (strcmp(argv[i], "--version") == 0) {
			printf("lsappinfo (Minkar)\n");
			return 0;
		}
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			usage();
		i++;
	}
	loadApps();
	if (i >= argc) {
		for (uint32_t n = 0; n < appCount; n++) {
			printApp(&started[n], (int)n + 1);
			putchar('\n');
		}
		return 0;
	}

	const char *command = argv[i++];

	if (strcmp(command, "list") == 0) {
		for (uint32_t n = 0; n < appCount; n++) {
			printApp(&started[n], (int)n + 1);
			putchar('\n');
		}
	} else if (strcmp(command, "processlist") == 0) {
		for (uint32_t n = 0; n < appCount; n++)
			printListed(&started[n]);
		putchar('\n');
	} else if (strcasecmp(command, "visibleProcessList") == 0) {
		int hidden = i < argc && strcmp(argv[i], "-includeHidden") == 0;
		const LSSessionApp *front = frontApp();

		if (front != NULL && front->activationPolicy == 0)
			printListed(front);
		for (uint32_t n = 0; n < appCount; n++)
			if (&apps[n] != front && apps[n].activationPolicy == 0 &&
			    (hidden || !(apps[n].flags & kLSSessionAppHiddenState)))
				printListed(&apps[n]);
		putchar('\n');
	} else if (strcmp(command, "front") == 0) {
		const LSSessionApp *front = frontApp();

		if (front != NULL)
			printASN(front, "\n");
		else
			printf("[ NULL ]\n");
	} else if (strcmp(command, "info") == 0) {
		const char *only[16];
		int onlyCount = 0;
		const LSSessionApp *app;

		while (i + 1 < argc && strcmp(argv[i], "-only") == 0 && onlyCount < 16) {
			only[onlyCount++] = argv[i + 1];
			i += 2;
		}
		if (i >= argc)
			usage();
		if ((app = findApp(argv[i])) == NULL) {
			printf("[ NULL ]\n");
			return 1;
		}
		if (onlyCount == 0)
			printApp(app, 0);
		for (int k = 0; k < onlyCount; k++)
			printKey(app, only[k]);
	} else if (strcmp(command, "find") == 0) {
		for (uint32_t n = 0; n < appCount; n++)
			if (matches(&apps[n], argc - i, argv + i))
				printListed(&apps[n]);
		putchar('\n');
	} else if (strcmp(command, "setfront") == 0) {
		const LSSessionApp *app;

		if (i >= argc)
			usage();
		if ((app = findApp(argv[i])) == NULL)
			errx(1, "%s: no such application", argv[i]);
		if (LSSessionActivate(app->pid, kActivateIgnoringOtherApps) != kLSSessionSuccess)
			errx(1, "%s: cannot make it the front application", argv[i]);
	} else
		errx(2, "Unrecognized command: %s", command);
	free(apps);
	free(started);
	return 0;
}
