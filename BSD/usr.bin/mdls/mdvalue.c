/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "mdvalue.h"

#include <time.h>

const char *
md_cstring(CFStringRef string, char *buffer, size_t size)
{
	buffer[0] = '\0';
	if (string != NULL)
		CFStringGetCString(string, buffer, (CFIndex)size, kCFStringEncodingUTF8);
	return buffer;
}

void
md_print_value(FILE *out, CFTypeRef value, int indent, int raw)
{
	char buffer[4096];

	if (value == NULL) {
		fputs("(null)", out);
		return;
	}
	CFTypeID type = CFGetTypeID(value);
	if (type == CFStringGetTypeID()) {
		md_cstring(value, buffer, sizeof(buffer));
		fprintf(out, raw ? "%s" : "\"%s\"", buffer);
	} else if (type == CFBooleanGetTypeID())
		fputs(CFBooleanGetValue(value) ? "1" : "0", out);
	else if (type == CFNumberGetTypeID()) {
		if (CFNumberIsFloatType(value)) {
			double d = 0;
			CFNumberGetValue(value, kCFNumberDoubleType, &d);
			fprintf(out, "%g", d);
		} else {
			long long n = 0;
			CFNumberGetValue(value, kCFNumberLongLongType, &n);
			fprintf(out, "%lld", n);
		}
	} else if (type == CFDateGetTypeID()) {
		time_t t = (time_t)(CFDateGetAbsoluteTime(value) + kCFAbsoluteTimeIntervalSince1970);
		struct tm tm;
		gmtime_r(&t, &tm);
		strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S +0000", &tm);
		fputs(buffer, out);
	} else if (type == CFArrayGetTypeID()) {
		CFIndex count = CFArrayGetCount(value);
		fputs("(\n", out);
		for (CFIndex i = 0; i < count; i++) {
			fprintf(out, "%*s", indent + 4, "");
			md_print_value(out, CFArrayGetValueAtIndex(value, i), indent + 4, 0);
			fputs(i + 1 < count ? ",\n" : "\n", out);
		}
		fprintf(out, "%*s)", indent, "");
	} else {
		CFStringRef description = CFCopyDescription(value);
		fputs(md_cstring(description, buffer, sizeof(buffer)), out);
		CFRelease(description);
	}
}
