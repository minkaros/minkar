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
 * ioreg: show the I/O Kit registry, as on macOS: the tree of objects in a
 * plane (IOService by default), each with its class, id, busy count and
 * retain count, and with -l their properties; or, with -a, the same as an
 * XML property list. All through IOKit.framework's public calls.
 */

#include <sys/ioctl.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <err.h>
#include <stdarg.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool aflag, iflag, lflag, rflag, sflag, tflag, xflag;
static const char *cname, *nname, *kname;
static char plane[128] = kIOServicePlane;
static long depth = -1;
static long width = -1;
static bool found;

static void
usage(void)
{

	fprintf(stderr, "usage: ioreg [-abilrstx] [-c class] [-d depth] "
	    "[-k key] [-n name] [-p plane] [-w width]\n");
	exit(1);
}

/* Output, cut to the width. */
static char line[65536];
static size_t used;

static void
put(const char *fmt, ...) __printflike(1, 2);

static void
put(const char *fmt, ...)
{
	va_list ap;
	int n;

	va_start(ap, fmt);
	n = vsnprintf(line + used, sizeof(line) - used, fmt, ap);
	va_end(ap);
	if (n > 0)
		used += (size_t)n < sizeof(line) - used ? (size_t)n :
		    sizeof(line) - used - 1;
}

static void
endline(void)
{

	if (width > 0 && used > (size_t)width)
		used = (size_t)width;
	fwrite(line, 1, used, stdout);
	fputc('\n', stdout);
	used = 0;
}

static char *
cfstr(CFStringRef s)
{
	CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
	    kCFStringEncodingUTF8) + 1;
	char *buf = malloc((size_t)len);

	if (buf == NULL)
		err(1, "malloc");
	if (!CFStringGetCString(s, buf, len, kCFStringEncodingUTF8))
		buf[0] = '\0';
	return (buf);
}

static void value(CFTypeRef v);

static void
dictentry(const void *key, const void *val, void *ctx)
{
	bool *first = ctx;
	char *k = cfstr(key);

	put("%s\"%s\"=", *first ? "" : ",", k);
	free(k);
	value(val);
	*first = false;
}

/* A property value, as ioreg writes them. */
static void
value(CFTypeRef v)
{
	CFTypeID t = CFGetTypeID(v);

	if (t == CFStringGetTypeID()) {
		char *s = cfstr(v);

		put("\"%s\"", s);
		free(s);
	} else if (t == CFNumberGetTypeID()) {
		long long n = 0;

		CFNumberGetValue(v, kCFNumberLongLongType, &n);
		put(xflag ? "0x%llx" : "%lld", n);
	} else if (t == CFBooleanGetTypeID()) {
		put("%s", CFBooleanGetValue(v) ? "Yes" : "No");
	} else if (t == CFDataGetTypeID()) {
		const UInt8 *p = CFDataGetBytePtr(v);
		CFIndex i, n = CFDataGetLength(v);

		put("<");
		for (i = 0; i < n; i++)
			put("%02x", p[i]);
		put(">");
	} else if (t == CFArrayGetTypeID()) {
		CFIndex i, n = CFArrayGetCount(v);

		put("(");
		for (i = 0; i < n; i++) {
			if (i > 0)
				put(",");
			value(CFArrayGetValueAtIndex(v, i));
		}
		put(")");
	} else if (t == CFDictionaryGetTypeID()) {
		bool first = true;

		put("{");
		CFDictionaryApplyFunction(v, dictentry, &first);
		put("}");
	} else {
		put("<%s>", "?");
	}
}

/* The tree's left margin for an object at this depth. */
static void
margin(const bool *more, int level)
{
	int i;

	for (i = 0; i < level; i++)
		put("%s", more[i] ? "| " : "  ");
}

struct propctx {
	const bool	*more;
	int		 level;
	bool		 last;
};

static int
keycmp(const void *a, const void *b)
{

	return (CFStringCompare(*(CFStringRef const *)a,
	    *(CFStringRef const *)b, 0));
}

static void
properties(io_registry_entry_t e, const bool *more, int level, bool hasmore)
{
	CFMutableDictionaryRef props = NULL;
	CFIndex n, i;
	const void **keys, **vals;

	if (IORegistryEntryCreateCFProperties(e, &props, kCFAllocatorDefault,
	    0) != KERN_SUCCESS || props == NULL)
		return;
	n = CFDictionaryGetCount(props);
	keys = calloc((size_t)n + 1, sizeof(*keys));
	vals = calloc((size_t)n + 1, sizeof(*vals));
	if (keys == NULL || vals == NULL)
		err(1, "calloc");
	CFDictionaryGetKeysAndValues(props, keys, vals);
	qsort(keys, (size_t)n, sizeof(*keys), keycmp);
	margin(more, level);
	put("%s {", hasmore ? "|" : " ");
	endline();
	for (i = 0; i < n; i++) {
		char *k;

		if (kname != NULL) {
			CFStringRef want = CFStringCreateWithCString(NULL,
			    kname, kCFStringEncodingUTF8);
			bool same = CFEqual(want, keys[i]);

			CFRelease(want);
			if (!same)
				continue;
		}
		k = cfstr(keys[i]);
		margin(more, level);
		put("%s   \"%s\" = ", hasmore ? "|" : " ", k);
		free(k);
		value(CFDictionaryGetValue(props, keys[i]));
		endline();
	}
	margin(more, level);
	put("%s }", hasmore ? "|" : " ");
	endline();
	margin(more, level);
	put("%s", hasmore ? "|" : " ");
	endline();
	free(keys);
	free(vals);
	CFRelease(props);
}

/* Whether e is one -c, -n or -k asked for (all when none). */
static bool
wanted(io_registry_entry_t e)
{
	io_name_t name;
	CFTypeRef p;

	if (cname != NULL && !IOObjectConformsTo(e, cname))
		return (false);
	if (nname != NULL) {
		if (IORegistryEntryGetNameInPlane(e, plane, name) != KERN_SUCCESS)
			return (false);
		if (strcmp(name, nname) != 0) {
			io_name_t loc;
			char full[sizeof(name) + sizeof(loc) + 1];

			if (IORegistryEntryGetLocationInPlane(e, plane, loc) !=
			    KERN_SUCCESS)
				return (false);
			snprintf(full, sizeof(full), "%s@%s", name, loc);
			if (strcmp(full, nname) != 0)
				return (false);
		}
	}
	if (kname != NULL) {
		CFStringRef k = CFStringCreateWithCString(NULL, kname,
		    kCFStringEncodingUTF8);

		p = IORegistryEntryCreateCFProperty(e, k, NULL, 0);
		CFRelease(k);
		if (p == NULL)
			return (false);
		CFRelease(p);
	}
	return (true);
}

static void
object(io_registry_entry_t e, const bool *more, int level, bool hasmore,
    bool matched)
{
	io_name_t name, cls, loc;
	uint64_t id = 0;
	uint32_t busy = 0;

	found = true;
	if (IORegistryEntryGetNameInPlane(e, plane, name) != KERN_SUCCESS)
		strlcpy(name, "?", sizeof(name));
	if (IOObjectGetClass(e, cls) != KERN_SUCCESS)
		strlcpy(cls, "?", sizeof(cls));
	(void)IORegistryEntryGetRegistryEntryID(e, &id);
	margin(more, level);
	put("+-o %s", name);
	if (IORegistryEntryGetLocationInPlane(e, plane, loc) == KERN_SUCCESS)
		put("@%s", loc);
	put("  <class %s, id 0x%llx", cls, (unsigned long long)id);
	if (strcmp(plane, kIOServicePlane) == 0 &&
	    IOObjectConformsTo(e, "IOService") &&
	    IOServiceGetBusyState(e, &busy) == KERN_SUCCESS)
		put(", busy %u", busy);
	put(", retain %u>", IOObjectGetRetainCount(e));
	endline();
	if (iflag) {
		CFStringRef sup;
		CFStringRef c = CFStringCreateWithCString(NULL, cls,
		    kCFStringEncodingUTF8);

		margin(more, level);
		put("%s   class %s", hasmore ? "|" : " ", cls);
		while ((sup = IOObjectCopySuperclassForClass(c)) != NULL) {
			char *s = cfstr(sup);

			put(":%s", s);
			free(s);
			CFRelease(c);
			c = sup;
		}
		CFRelease(c);
		endline();
	}
	/* As macOS's: -c, -n and -k show the matching objects' properties. */
	if (lflag || matched)
		properties(e, more, level, hasmore);
}

/*
 * The tree from e down, e at this level; more[i] says whether the object
 * at depth i has siblings after it (for the "|" lines). With -c, -n or -k
 * (filtering), only the matching objects are shown, each as a root, and
 * with -r the subtrees under them.
 */
static void
tree(io_registry_entry_t e, bool *more, int level, bool showing)
{
	io_iterator_t it;
	io_registry_entry_t child, next;
	bool filtering = cname || nname || kname;
	bool show = showing;

	if (!showing && wanted(e)) {
		show = true;
		level = 0;
	}
	if (show)
		object(e, more, level, false, filtering && wanted(e));
	if (show && (filtering && !showing) && !rflag)
		show = false;	/* a match without -r: not its children */
	else if (show && depth >= 0 && level + 1 >= depth)
		return;
	if (IORegistryEntryGetChildIterator(e, plane, &it) != KERN_SUCCESS)
		return;
	child = IOIteratorNext(it);
	while (child != 0) {
		next = IOIteratorNext(it);
		if (level + 1 < 256)
			more[level + 1] = show && next != 0;
		tree(child, more, show ? level + 1 : level, show);
		IOObjectRelease(child);
		child = next;
	}
	IOObjectRelease(it);
}

static CFMutableDictionaryRef archive(io_registry_entry_t e, int level);

static CFMutableDictionaryRef
archive(io_registry_entry_t e, int level)
{
	CFMutableDictionaryRef d = NULL;
	CFMutableArrayRef kids;
	io_iterator_t it;
	io_registry_entry_t child;
	io_name_t name, cls;
	CFStringRef s;

	if (IORegistryEntryCreateCFProperties(e, &d, kCFAllocatorDefault, 0) !=
	    KERN_SUCCESS || d == NULL)
		d = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
	if (IORegistryEntryGetNameInPlane(e, plane, name) == KERN_SUCCESS) {
		s = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
		CFDictionarySetValue(d, CFSTR("IORegistryEntryName"), s);
		CFRelease(s);
	}
	if (IOObjectGetClass(e, cls) == KERN_SUCCESS) {
		s = CFStringCreateWithCString(NULL, cls, kCFStringEncodingUTF8);
		CFDictionarySetValue(d, CFSTR("IOObjectClass"), s);
		CFRelease(s);
	}
	if (depth >= 0 && level + 1 >= depth)
		return (d);
	if (IORegistryEntryGetChildIterator(e, plane, &it) != KERN_SUCCESS)
		return (d);
	kids = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	while ((child = IOIteratorNext(it)) != 0) {
		CFMutableDictionaryRef c = archive(child, level + 1);

		CFArrayAppendValue(kids, c);
		CFRelease(c);
		IOObjectRelease(child);
	}
	IOObjectRelease(it);
	if (CFArrayGetCount(kids) > 0)
		CFDictionarySetValue(d, CFSTR("IORegistryEntryChildren"), kids);
	CFRelease(kids);
	return (d);
}

int
main(int argc, char *argv[])
{
	io_registry_entry_t root;
	bool more[256] = { false };
	struct winsize ws;
	char *end;
	int ch;

	while ((ch = getopt(argc, argv, "abc:d:ik:ln:p:rstw:x")) != -1)
		switch (ch) {
		case 'a':
			aflag = true;
			break;
		case 'b':
			break;		/* bold names: plain text here */
		case 'c':
			cname = optarg;
			break;
		case 'd':
			depth = strtol(optarg, &end, 10);
			if (*optarg == '\0' || *end != '\0' || depth < 0)
				usage();
			break;
		case 'i':
			iflag = true;
			break;
		case 'k':
			kname = optarg;
			break;
		case 'l':
			lflag = true;
			break;
		case 'n':
			nname = optarg;
			break;
		case 'p':
			strlcpy(plane, optarg, sizeof(plane));
			break;
		case 'r':
			rflag = true;
			break;
		case 's':
			sflag = true;
			break;
		case 't':
			tflag = true;
			break;
		case 'w':
			width = strtol(optarg, &end, 10);
			if (*optarg == '\0' || *end != '\0' || width < 0)
				usage();
			break;
		case 'x':
			xflag = true;
			break;
		default:
			usage();
		}
	if (optind != argc)
		usage();
	if (width < 0)
		width = isatty(STDOUT_FILENO) &&
		    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 ?
		    ws.ws_col : 0;

	root = IORegistryGetRootEntry(kIOMasterPortDefault);
	if (root == 0)
		errx(1, "cannot reach the I/O Kit registry");
	if (aflag) {
		CFMutableDictionaryRef d = archive(root, 0);
		CFDataRef x = CFPropertyListCreateData(NULL, d,
		    kCFPropertyListXMLFormat_v1_0, 0, NULL);

		if (x == NULL)
			errx(1, "cannot write the property list");
		fwrite(CFDataGetBytePtr(x), 1, (size_t)CFDataGetLength(x),
		    stdout);
		CFRelease(x);
		CFRelease(d);
		return (0);
	}
	(void)sflag;
	(void)tflag;
	tree(root, more, 0, !(cname || nname || kname));	/* all when no filter */
	IOObjectRelease(root);
	return (cname || nname || kname) && !found ? 1 : 0;
}
