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
 * kextstat: the kexts the kernel has loaded (OSKextCopyLoadedKextInfo),
 * one per line in load order, as macOS prints them: index, references,
 * address, sizes, identifier and version, UUID and the indexes of the
 * kexts it is linked against. -b shows only the given identifiers, -l
 * leaves out the header, -k leaves out the built-in kernel components
 * (the kernel, com.apple.kpi.* and com.apple.kernel.*).
 */

#include <CoreFoundation/CoreFoundation.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

CFDictionaryRef OSKextCopyLoadedKextInfo(CFArrayRef, CFArrayRef);	/* IOKit */

struct kext {
	long		tag;
	long		refs;
	long long	addr;
	long long	size;
	long long	wired;
	char		id[256];
	char		version[64];
	char		uuid[40];
	char		deps[512];
};

static long long
num(CFDictionaryRef d, const char *key)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFNumberRef n = CFDictionaryGetValue(d, k);
	long long v = 0;

	CFRelease(k);
	if (n != NULL && CFGetTypeID(n) == CFNumberGetTypeID())
		CFNumberGetValue(n, kCFNumberLongLongType, &v);
	return (v);
}

static void
str(CFDictionaryRef d, const char *key, char *buf, size_t len)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	CFStringRef s = CFDictionaryGetValue(d, k);

	CFRelease(k);
	buf[0] = '\0';
	if (s != NULL && CFGetTypeID(s) == CFStringGetTypeID())
		CFStringGetCString(s, buf, (CFIndex)len, kCFStringEncodingUTF8);
}

static int
cmp_long(const void *a, const void *b)
{
	long x = *(const long *)a, y = *(const long *)b;

	return (x < y ? -1 : x > y);
}

static void
collect(const void *key, const void *value, void *ctx)
{
	CFDictionaryRef d = value;
	CFMutableArrayRef all = ctx;
	struct kext *k;
	CFDataRef u;
	CFArrayRef deps;
	CFStringRef ukey;

	(void)key;
	if (CFGetTypeID(d) != CFDictionaryGetTypeID())
		return;
	if ((k = calloc(1, sizeof(*k))) == NULL)
		err(1, "calloc");
	k->tag = (long)num(d, "OSBundleLoadTag");
	k->refs = (long)num(d, "OSBundleRetainCount");
	k->addr = num(d, "OSBundleLoadAddress");
	k->size = num(d, "OSBundleLoadSize");
	k->wired = num(d, "OSBundleWiredSize");
	str(d, "CFBundleIdentifier", k->id, sizeof(k->id));
	str(d, "CFBundleVersion", k->version, sizeof(k->version));

	ukey = CFSTR("OSBundleUUID");
	u = CFDictionaryGetValue(d, ukey);
	if (u != NULL && CFGetTypeID(u) == CFDataGetTypeID() && CFDataGetLength(u) == 16) {
		const UInt8 *b = CFDataGetBytePtr(u);

		snprintf(k->uuid, sizeof(k->uuid),
		    "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		    b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9],
		    b[10], b[11], b[12], b[13], b[14], b[15]);
	}
	deps = CFDictionaryGetValue(d, CFSTR("OSBundleDependencies"));
	if (deps != NULL && CFGetTypeID(deps) == CFArrayGetTypeID()) {
		CFIndex i, n = CFArrayGetCount(deps);
		long tags[64];
		size_t off = 0;

		if (n > 64)
			n = 64;
		for (i = 0; i < n; i++) {
			CFNumberRef tn = CFArrayGetValueAtIndex(deps, i);

			tags[i] = 0;
			if (CFGetTypeID(tn) == CFNumberGetTypeID())
				CFNumberGetValue(tn, kCFNumberLongType, &tags[i]);
		}
		qsort(tags, (size_t)n, sizeof(tags[0]), cmp_long);
		for (i = 0; i < n && off < sizeof(k->deps) - 8; i++)
			off += snprintf(k->deps + off, sizeof(k->deps) - off,
			    "%s%ld", i ? " " : "", tags[i]);
	}
	CFArrayAppendValue(all, k);
}

static CFComparisonResult
by_tag(const void *a, const void *b, void *ctx)
{
	(void)ctx;
	long x = ((const struct kext *)a)->tag, y = ((const struct kext *)b)->tag;

	return (x < y ? kCFCompareLessThan : x > y ? kCFCompareGreaterThan : kCFCompareEqualTo);
}

static void
usage(void)
{
	fprintf(stderr, "usage: kextstat [-k] [-l] [-b bundle_id] ...\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	CFMutableArrayRef ids = NULL, all;
	CFDictionaryRef info;
	int ch, noheader = 0, nokernel = 0;
	CFIndex i;

	while ((ch = getopt(argc, argv, "b:klv")) != -1) {
		switch (ch) {
		case 'b': {
			CFStringRef s = CFStringCreateWithCString(NULL, optarg,
			    kCFStringEncodingUTF8);

			if (ids == NULL)
				ids = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			CFArrayAppendValue(ids, s);
			CFRelease(s);
			break;
		}
		case 'k':
			nokernel = 1;
			break;
		case 'l':
			noheader = 1;
			break;
		case 'v':
			break;
		default:
			usage();
		}
	}
	if (optind != argc)
		usage();

	info = OSKextCopyLoadedKextInfo(ids, NULL);
	if (info == NULL)
		errx(1, "cannot get the loaded kexts from the kernel");
	all = CFArrayCreateMutable(NULL, 0, NULL);
	CFDictionaryApplyFunction(info, collect, all);
	CFArraySortValues(all, CFRangeMake(0, CFArrayGetCount(all)), by_tag, NULL);

	if (!noheader)
		printf("Index Refs Address            Size       Wired      "
		    "Name (Version) UUID <Linked Against>\n");
	for (i = 0; i < CFArrayGetCount(all); i++) {
		const struct kext *k = CFArrayGetValueAtIndex(all, i);

		if (nokernel && (strncmp(k->id, "com.apple.kpi.", 14) == 0 ||
		    strncmp(k->id, "com.apple.kernel", 16) == 0 ||
		    strcmp(k->id, "__kernel__") == 0))
			continue;
		printf("%5ld %4ld 0x%-16llx 0x%-8llx 0x%-8llx %s (%s) %s <%s>\n",
		    k->tag, k->refs, k->addr, k->size, k->wired, k->id,
		    k->version, k->uuid, k->deps);
	}
	return (0);
}
