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
 * nvram: read and set firmware variables, as on macOS, through the
 * "options" entry of the I/O Kit device tree (IODeviceTree:/options),
 * which a firmware NVRAM driver publishes: print them (-p, or -x as XML),
 * one (name), set (name=value, or from a file with -f), delete (-d) or
 * clear them all (-c). Values print with non-printing bytes as %xx.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>
#include <mach/mach_error.h>

#include <ctype.h>
#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	OPTIONS	"IODeviceTree:/options"

static io_registry_entry_t options;
static int errors;

static void
usage(void)
{

	fprintf(stderr,
	    "usage: nvram [-x] [-p] [-f filename] [-d name] [-c] "
	    "name[=value] ...\n"
	    "\t-x         use XML format for printing or reading variables\n"
	    "\t-p         print all firmware variables\n"
	    "\t-f         set firmware variables from a text file\n"
	    "\t-d         delete the named variable\n"
	    "\t-c         delete all variables\n"
	    "\tname=value set named variable\n"
	    "\tname       print variable\n");
	exit(1);
}

static void
printvalue(CFTypeRef v)
{
	CFTypeID t = CFGetTypeID(v);

	if (t == CFStringGetTypeID()) {
		char buf[4096];

		if (CFStringGetCString(v, buf, sizeof(buf),
		    kCFStringEncodingUTF8))
			printf("%s", buf);
	} else if (t == CFDataGetTypeID()) {
		const UInt8 *p = CFDataGetBytePtr(v);
		CFIndex i, n = CFDataGetLength(v);

		for (i = 0; i < n; i++)
			if (isprint(p[i]) && p[i] != '%')
				putchar(p[i]);
			else
				printf("%%%02x", p[i]);
	} else if (t == CFBooleanGetTypeID()) {
		printf("%s", CFBooleanGetValue(v) ? "true" : "false");
	} else if (t == CFNumberGetTypeID()) {
		long long n = 0;

		CFNumberGetValue(v, kCFNumberLongLongType, &n);
		printf("0x%llx", n);
	}
}

static void
printone(const void *key, const void *val, void *ctx)
{
	char name[1024];

	(void)ctx;
	if (!CFStringGetCString(key, name, sizeof(name), kCFStringEncodingUTF8))
		return;
	printf("%s\t", name);
	printvalue(val);
	printf("\n");
}

static void
printall(bool xml)
{
	CFMutableDictionaryRef d = NULL;

	if (IORegistryEntryCreateCFProperties(options, &d, NULL, 0) !=
	    KERN_SUCCESS || d == NULL)
		errx(1, "Error getting the firmware variables");
	if (xml) {
		CFDataRef x = CFPropertyListCreateData(NULL, d,
		    kCFPropertyListXMLFormat_v1_0, 0, NULL);

		if (x != NULL) {
			fwrite(CFDataGetBytePtr(x), 1,
			    (size_t)CFDataGetLength(x), stdout);
			CFRelease(x);
		}
	} else
		CFDictionaryApplyFunction(d, printone, NULL);
	CFRelease(d);
}

static void
printvar(const char *name)
{
	CFStringRef k = CFStringCreateWithCString(NULL, name,
	    kCFStringEncodingUTF8);
	CFTypeRef v = IORegistryEntryCreateCFProperty(options, k, NULL, 0);

	CFRelease(k);
	if (v == NULL) {
		warnx("Error getting variable - '%s': (iokit/common) data was "
		    "not found", name);
		errors++;
		return;
	}
	printf("%s\t", name);
	printvalue(v);
	printf("\n");
	CFRelease(v);
}

/* A value as data, with %xx for any byte. */
static CFDataRef
decode(const char *s)
{
	CFMutableDataRef d = CFDataCreateMutable(NULL, 0);
	UInt8 b;

	while (*s != '\0') {
		if (s[0] == '%' && isxdigit((unsigned char)s[1]) &&
		    isxdigit((unsigned char)s[2])) {
			char hex[3] = { s[1], s[2], '\0' };

			b = (UInt8)strtoul(hex, NULL, 16);
			s += 3;
		} else
			b = (UInt8)*s++;
		CFDataAppendBytes(d, &b, 1);
	}
	return (d);
}

static void
setvar(const char *name, const char *value)
{
	CFStringRef k = CFStringCreateWithCString(NULL, name,
	    kCFStringEncodingUTF8);
	CFDataRef v = decode(value);
	kern_return_t kr;

	kr = IORegistryEntrySetCFProperty(options, k, v);
	if (kr != KERN_SUCCESS) {
		warnx("Error setting variable - '%s': %s", name,
		    mach_error_string(kr));
		errors++;
	}
	CFRelease(k);
	CFRelease(v);
}

static void
delvar(const char *name)
{
	CFStringRef k = CFStringCreateWithCString(NULL, name,
	    kCFStringEncodingUTF8);
	kern_return_t kr;

	/* Setting this property of the options entry deletes the variable. */
	kr = IORegistryEntrySetCFProperty(options,
	    CFSTR(kIONVRAMDeletePropertyKey), k);
	if (kr != KERN_SUCCESS) {
		warnx("Error deleting variable - '%s': %s", name,
		    mach_error_string(kr));
		errors++;
	}
	CFRelease(k);
}

static void
delall(const void *key, const void *val, void *ctx)
{
	char name[1024];

	(void)val;
	(void)ctx;
	if (CFStringGetCString(key, name, sizeof(name), kCFStringEncodingUTF8))
		delvar(name);
}

static void
fromfile(const char *path)
{
	FILE *f = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
	char *line = NULL, *eq;
	size_t cap = 0;
	ssize_t n;

	if (f == NULL)
		err(1, "%s", path);
	while ((n = getline(&line, &cap, f)) > 0) {
		if (line[n - 1] == '\n')
			line[--n] = '\0';
		if (line[0] == '\0' || line[0] == '#')
			continue;
		if ((eq = strchr(line, '=')) == NULL) {
			warnx("%s: no '=' in \"%s\"", path, line);
			errors++;
			continue;
		}
		*eq = '\0';
		setvar(line, eq + 1);
	}
	free(line);
	if (f != stdin)
		fclose(f);
}

int
main(int argc, char *argv[])
{
	bool xml = false;
	int ch, i;
	char *eq;

	options = IORegistryEntryFromPath(kIOMasterPortDefault, OPTIONS);
	if (options == 0)
		errx(1, "nvram is not supported on this system");
	while ((ch = getopt(argc, argv, "cd:f:px")) != -1)
		switch (ch) {
		case 'c': {
			CFMutableDictionaryRef d = NULL;

			if (IORegistryEntryCreateCFProperties(options, &d, NULL,
			    0) == KERN_SUCCESS && d != NULL) {
				CFDictionaryApplyFunction(d, delall, NULL);
				CFRelease(d);
			}
			break;
		}
		case 'd':
			delvar(optarg);
			break;
		case 'f':
			fromfile(optarg);
			break;
		case 'p':
			printall(xml);
			break;
		case 'x':
			xml = true;
			break;
		default:
			usage();
		}
	for (i = optind; i < argc; i++) {
		if ((eq = strchr(argv[i], '=')) != NULL) {
			*eq = '\0';
			setvar(argv[i], eq + 1);
		} else
			printvar(argv[i]);
	}
	if (argc == 1)
		usage();
	IOObjectRelease(options);
	return (errors ? 1 : 0);
}
