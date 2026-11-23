/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * mdls: a file's metadata attributes, as the reference's command lists
 * them: each name and its value, the names in order and aligned; with
 * -name, only those (one missing shows "(null)"); with -raw, the values
 * alone, separated by NULs (-nullMarker: what a missing one shows). The
 * attributes are CoreServices' MDItem's, read from the file and its type
 * (there is no index).
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mdvalue.h"

static void
usage(void)
{
	fprintf(stderr, "Usage: mdls [-name attributeName] [-raw [-nullMarker markerString]] file ...\n"
	    "List the metadata attributes for the specified file\n"
	    "  -name      print only the matching metadata attribute value.  Can be used multiple times\n"
	    "  -raw       print raw attribute data in the order that was requested.\n"
	    "             Fields will be separated with a ASCII NUL character, suitable for piping to xargs(1) -0.\n"
	    "  -nullMarker  sets a marker string to be used when a requested attribute is null.\n"
	    "               Only used in -raw mode.  Default is \"(null)\".\n");
	exit(1);
}

static CFComparisonResult
byName(const void *a, const void *b, void *context)
{
	return CFStringCompare(a, b, 0);
}

static int
listFile(const char *path, CFArrayRef wanted, int raw, const char *nullMarker)
{
	char absolute[PATH_MAX];
	CFStringRef cfPath;
	MDItemRef item;
	CFMutableArrayRef names;
	size_t width = 0;
	char name[256];

	if (realpath(path, absolute) == NULL) {
		fprintf(stderr, "%s: could not find %s.\n", path, path);
		return 1;
	}
	cfPath = CFStringCreateWithFileSystemRepresentation(NULL, absolute);
	item = cfPath != NULL ? MDItemCreate(NULL, cfPath) : NULL;
	if (cfPath != NULL)
		CFRelease(cfPath);
	if (item == NULL) {
		fprintf(stderr, "%s: could not find %s.\n", path, path);
		return 1;
	}
	if (wanted != NULL)
		names = CFArrayCreateMutableCopy(NULL, 0, wanted);
	else {
		CFArrayRef all = MDItemCopyAttributeNames(item);
		names = CFArrayCreateMutableCopy(NULL, 0, all);
		CFRelease(all);
		/* Not listed: it is the file named. */
		CFIndex at = CFArrayGetFirstIndexOfValue(names, CFRangeMake(0, CFArrayGetCount(names)), kMDItemPath);
		if (at != kCFNotFound)
			CFArrayRemoveValueAtIndex(names, at);
		CFArraySortValues(names, CFRangeMake(0, CFArrayGetCount(names)), byName, NULL);
	}
	for (CFIndex i = 0; i < CFArrayGetCount(names); i++) {
		size_t length = strlen(md_cstring(CFArrayGetValueAtIndex(names, i), name, sizeof(name)));
		if (length > width)
			width = length;
	}
	for (CFIndex i = 0; i < CFArrayGetCount(names); i++) {
		CFStringRef attribute = CFArrayGetValueAtIndex(names, i);
		CFTypeRef value = MDItemCopyAttribute(item, attribute);

		if (raw) {
			if (i > 0)
				putchar('\0');
			if (value == NULL)
				fputs(nullMarker, stdout);
			else
				md_print_value(stdout, value, 0, 1);
		} else {
			printf("%-*s = ", (int)width, md_cstring(attribute, name, sizeof(name)));
			md_print_value(stdout, value, 0, 0);
			putchar('\n');
		}
		if (value != NULL)
			CFRelease(value);
	}
	CFRelease(names);
	CFRelease(item);
	return 0;
}

int
main(int argc, char *argv[])
{
	CFMutableArrayRef wanted = NULL;
	const char *nullMarker = "(null)";
	int raw = 0, status = 0, i;

	for (i = 1; i < argc && argv[i][0] == '-'; i++) {
		if (strcmp(argv[i], "-name") == 0 && i + 1 < argc) {
			CFStringRef name = CFStringCreateWithCString(NULL, argv[++i], kCFStringEncodingUTF8);
			if (wanted == NULL)
				wanted = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			CFArrayAppendValue(wanted, name);
			CFRelease(name);
		} else if (strcmp(argv[i], "-raw") == 0 || strcmp(argv[i], "-r") == 0)
			raw = 1;
		else if (strcmp(argv[i], "-nullMarker") == 0 && i + 1 < argc)
			nullMarker = argv[++i];
		else
			usage();
	}
	if (i >= argc)
		usage();
	for (; i < argc; i++)
		status |= listFile(argv[i], wanted, raw, nullMarker);
	if (wanted != NULL)
		CFRelease(wanted);
	return status;
}
