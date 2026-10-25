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
 * defaults: read and write user preferences (CFPreferences).
 *
 *   defaults [-currentHost] read [domain [key]]
 *   defaults [-currentHost] read-type domain key
 *   defaults [-currentHost] write domain { 'plist' | key 'value' |
 *                                         key -type value... }
 *   defaults [-currentHost] delete domain [key]
 *   defaults [-currentHost] rename domain old_key new_key
 *   defaults [-currentHost] import domain path | -
 *   defaults [-currentHost] export domain path | -
 *   defaults [-currentHost] domains
 *   defaults [-currentHost] find word
 *
 * A domain is a name (com.example.app), -g or NSGlobalDomain for the
 * global domain, or the path of a property list file. Types for write:
 * -string, -data (hex), -int[eger], -float, -bool[ean], -date,
 * -array, -array-add, -dict, -dict-add. A value that begins with {, (
 * or < is read as a property list.
 */

#include <CoreFoundation/CoreFoundation.h>

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

static CFStringRef host = NULL;		/* kCFPreferencesAnyHost by default */

static void usage(void) __dead2;

/* ---------------------------------------------------------------------
 * Strings
 */

static CFStringRef
cfstr(const char *s)
{
	CFStringRef r = CFStringCreateWithCString(NULL, s,
	    kCFStringEncodingUTF8);

	if (r == NULL)
		errx(EX_DATAERR, "not valid UTF-8: %s", s);
	return (r);
}

/* A C string for a CFString; the caller frees it. */
static char *
cstr(CFStringRef s)
{
	CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
	    kCFStringEncodingUTF8) + 1;
	char *buf = malloc((size_t)len);

	if (buf == NULL)
		err(EX_OSERR, "malloc");
	if (!CFStringGetCString(s, buf, len, kCFStringEncodingUTF8))
		buf[0] = '\0';
	return (buf);
}

/* ---------------------------------------------------------------------
 * Printing, in the old-style property list form macOS's defaults uses
 */

static void print_value(CFTypeRef v, int indent);

static void
print_indent(int indent)
{

	for (int i = 0; i < indent; i++)
		fputs("    ", stdout);
}

/* Quote unless every character is one plists leave bare. */
static void
print_string(CFStringRef s)
{
	char *c = cstr(s);
	bool bare = c[0] != '\0';

	for (const char *p = c; *p != '\0'; p++) {
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		    (*p >= '0' && *p <= '9') || *p == '_' || *p == '.' ||
		    *p == '$' || *p == '/' || *p == ':' || *p == '-')) {
			bare = false;
			break;
		}
	}
	if (bare) {
		fputs(c, stdout);
	} else {
		putchar('"');
		for (const char *p = c; *p != '\0'; p++) {
			switch (*p) {
			case '"':
				fputs("\\\"", stdout);
				break;
			case '\\':
				fputs("\\\\", stdout);
				break;
			case '\n':
				fputs("\\n", stdout);
				break;
			case '\t':
				fputs("\\t", stdout);
				break;
			default:
				putchar(*p);
			}
		}
		putchar('"');
	}
	free(c);
}

static void
print_number(CFNumberRef n)
{
	if (CFNumberIsFloatType(n)) {
		double d;

		CFNumberGetValue(n, kCFNumberDoubleType, &d);
		printf("%.15g", d);
	} else {
		long long l;

		CFNumberGetValue(n, kCFNumberLongLongType, &l);
		printf("%lld", l);
	}
}

static void
print_date(CFDateRef d)
{
	time_t t = (time_t)(CFDateGetAbsoluteTime(d) +
	    kCFAbsoluteTimeIntervalSince1970);
	struct tm tm;
	char buf[64];

	gmtime_r(&t, &tm);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S +0000", &tm);
	fputs(buf, stdout);
}

static void
print_data(CFDataRef d)
{
	const UInt8 *p = CFDataGetBytePtr(d);
	CFIndex len = CFDataGetLength(d);

	printf("{length = %ld, bytes = 0x", (long)len);
	for (CFIndex i = 0; i < len; i++)
		printf("%02x", p[i]);
	putchar('}');
}

struct dict_entry {
	CFStringRef	key;
	CFTypeRef	value;
};

static int
compare_entries(const void *a, const void *b)
{
	const struct dict_entry *x = a, *y = b;

	return ((int)CFStringCompare(x->key, y->key, 0));
}

static void
print_dict(CFDictionaryRef dict, int indent)
{
	CFIndex n = CFDictionaryGetCount(dict);
	const void **keys = calloc((size_t)n + 1, sizeof(void *));
	const void **vals = calloc((size_t)n + 1, sizeof(void *));
	struct dict_entry *e = calloc((size_t)n + 1, sizeof(*e));
	CFIndex m = 0;

	if (keys == NULL || vals == NULL || e == NULL)
		err(EX_OSERR, "calloc");
	CFDictionaryGetKeysAndValues(dict, keys, vals);
	for (CFIndex i = 0; i < n; i++) {
		if (CFGetTypeID(keys[i]) != CFStringGetTypeID())
			continue;
		e[m].key = keys[i];
		e[m].value = vals[i];
		m++;
	}
	qsort(e, (size_t)m, sizeof(*e), compare_entries);
	puts("{");
	for (CFIndex i = 0; i < m; i++) {
		print_indent(indent + 1);
		print_string(e[i].key);
		fputs(" = ", stdout);
		print_value(e[i].value, indent + 1);
		puts(";");
	}
	print_indent(indent);
	putchar('}');
	free(keys);
	free(vals);
	free(e);
}

static void
print_array(CFArrayRef a, int indent)
{
	CFIndex n = CFArrayGetCount(a);

	puts("(");
	for (CFIndex i = 0; i < n; i++) {
		print_indent(indent + 1);
		print_value(CFArrayGetValueAtIndex(a, i), indent + 1);
		puts(i + 1 < n ? "," : "");
	}
	print_indent(indent);
	putchar(')');
}

static void
print_value(CFTypeRef v, int indent)
{
	CFTypeID t = CFGetTypeID(v);

	if (t == CFStringGetTypeID())
		print_string(v);
	else if (t == CFBooleanGetTypeID())
		fputs(CFBooleanGetValue(v) ? "1" : "0", stdout);
	else if (t == CFNumberGetTypeID())
		print_number(v);
	else if (t == CFDateGetTypeID())
		print_date(v);
	else if (t == CFDataGetTypeID())
		print_data(v);
	else if (t == CFArrayGetTypeID()) {
		/* An array or dictionary inside another starts aligned. */
		if (indent > 0)
			fputs("    ", stdout);
		print_array(v, indent);
	} else if (t == CFDictionaryGetTypeID()) {
		if (indent > 0)
			fputs("    ", stdout);
		print_dict(v, indent);
	} else
		fputs("?", stdout);
}

static const char *
type_name(CFTypeRef v)
{
	CFTypeID t = CFGetTypeID(v);

	if (t == CFStringGetTypeID())
		return ("string");
	if (t == CFBooleanGetTypeID())
		return ("boolean");
	if (t == CFNumberGetTypeID())
		return (CFNumberIsFloatType(v) ? "float" : "integer");
	if (t == CFDateGetTypeID())
		return ("date");
	if (t == CFDataGetTypeID())
		return ("data");
	if (t == CFArrayGetTypeID())
		return ("array");
	if (t == CFDictionaryGetTypeID())
		return ("dictionary");
	return ("unknown");
}

/* ---------------------------------------------------------------------
 * Domains: a preferences domain, or a property list file
 */

struct domain {
	CFStringRef	name;		/* app ID, or NULL for a file */
	char		*path;		/* plist file, or NULL */
	CFMutableDictionaryRef file;	/* contents of path */
	bool		is_global;
};

static bool
is_plist_path(const char *s)
{

	return (s[0] == '/' || strncmp(s, "./", 2) == 0 ||
	    strncmp(s, "~/", 2) == 0);
}

static CFPropertyListRef
read_plist_file(const char *path, bool must_exist)
{
	CFPropertyListRef pl;
	CFErrorRef error = NULL;
	CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
	FILE *f;
	UInt8 buf[8192];
	size_t n;

	if (strcmp(path, "-") == 0)
		f = stdin;
	else if ((f = fopen(path, "r")) == NULL) {
		if (!must_exist && errno == ENOENT) {
			CFRelease(data);
			return (NULL);
		}
		err(EX_NOINPUT, "%s", path);
	}
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		CFDataAppendBytes(data, buf, (CFIndex)n);
	if (f != stdin)
		fclose(f);
	pl = CFPropertyListCreateWithData(NULL, data,
	    kCFPropertyListMutableContainersAndLeaves, NULL, &error);
	CFRelease(data);
	if (pl == NULL)
		errx(EX_DATAERR, "%s: not a property list", path);
	return (pl);
}

static void
write_plist_file(const char *path, CFPropertyListRef pl,
    CFPropertyListFormat format)
{
	CFErrorRef error = NULL;
	CFDataRef data = CFPropertyListCreateData(NULL, pl, format, 0, &error);
	FILE *f;

	if (data == NULL)
		errx(EX_SOFTWARE, "cannot encode the property list");
	if (strcmp(path, "-") == 0)
		f = stdout;
	else if ((f = fopen(path, "w")) == NULL)
		err(EX_CANTCREAT, "%s", path);
	if (fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data),
	    f) != (size_t)CFDataGetLength(data))
		err(EX_IOERR, "%s", path);
	if (f != stdout && fclose(f) != 0)
		err(EX_IOERR, "%s", path);
	CFRelease(data);
}

static void
open_domain(struct domain *d, const char *arg)
{
	memset(d, 0, sizeof(*d));
	if (strcmp(arg, "-g") == 0 || strcmp(arg, "NSGlobalDomain") == 0 ||
	    strcmp(arg, "-globalDomain") == 0) {
		d->name = kCFPreferencesAnyApplication;
		d->is_global = true;
		return;
	}
	if (is_plist_path(arg)) {
		CFPropertyListRef pl;

		if (strncmp(arg, "~/", 2) == 0) {
			const char *home = getenv("HOME");

			if (asprintf(&d->path, "%s/%s", home ? home : "",
			    arg + 2) == -1)
				err(EX_OSERR, "asprintf");
		} else if ((d->path = strdup(arg)) == NULL)
			err(EX_OSERR, "strdup");
		/* "defaults read path" works without the .plist too. */
		if (access(d->path, F_OK) == -1 &&
		    strstr(d->path, ".plist") == NULL) {
			char *p;

			if (asprintf(&p, "%s.plist", d->path) == -1)
				err(EX_OSERR, "asprintf");
			free(d->path);
			d->path = p;
		}
		pl = read_plist_file(d->path, false);
		if (pl != NULL && CFGetTypeID(pl) != CFDictionaryGetTypeID())
			errx(EX_DATAERR, "%s: not a dictionary", d->path);
		d->file = pl != NULL ? (CFMutableDictionaryRef)pl :
		    CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		return;
	}
	d->name = cfstr(arg);
}

static CFStringRef
domain_host(void)
{

	return (host != NULL ? host : kCFPreferencesAnyHost);
}

/* All of a domain, as a dictionary (empty if it has nothing). */
static CFDictionaryRef
copy_domain(struct domain *d)
{
	CFArrayRef keys;
	CFDictionaryRef all;

	if (d->file != NULL)
		return (CFRetain(d->file));
	keys = CFPreferencesCopyKeyList(d->name, kCFPreferencesCurrentUser,
	    domain_host());
	if (keys == NULL)
		return (NULL);
	all = CFPreferencesCopyMultiple(keys, d->name,
	    kCFPreferencesCurrentUser, domain_host());
	CFRelease(keys);
	return (all);
}

static CFTypeRef
copy_value(struct domain *d, CFStringRef key)
{
	if (d->file != NULL) {
		CFTypeRef v = CFDictionaryGetValue(d->file, key);

		return (v != NULL ? CFRetain(v) : NULL);
	}
	return (CFPreferencesCopyValue(key, d->name, kCFPreferencesCurrentUser,
	    domain_host()));
}

static void
set_value(struct domain *d, CFStringRef key, CFTypeRef value)
{
	if (d->file != NULL) {
		if (value != NULL)
			CFDictionarySetValue(d->file, key, value);
		else
			CFDictionaryRemoveValue(d->file, key);
		return;
	}
	CFPreferencesSetValue(key, value, d->name, kCFPreferencesCurrentUser,
	    domain_host());
}

static void
sync_domain(struct domain *d)
{
	if (d->file != NULL) {
		write_plist_file(d->path, d->file,
		    kCFPropertyListBinaryFormat_v1_0);
		return;
	}
	if (!CFPreferencesSynchronize(d->name, kCFPreferencesCurrentUser,
	    domain_host()))
		errx(EX_IOERR, "could not write the preferences");
}

static void
domain_label(struct domain *d, char *buf, size_t len)
{
	if (d->file != NULL)
		snprintf(buf, len, "%s", d->path);
	else if (d->is_global)
		snprintf(buf, len, "NSGlobalDomain");
	else {
		char *c = cstr(d->name);

		snprintf(buf, len, "%s", c);
		free(c);
	}
}

/* ---------------------------------------------------------------------
 * Values from the command line
 */

static CFPropertyListRef
parse_plist_arg(const char *s)
{
	CFDataRef data = CFDataCreate(NULL, (const UInt8 *)s,
	    (CFIndex)strlen(s));
	CFErrorRef error = NULL;
	CFPropertyListRef pl = CFPropertyListCreateWithData(NULL, data,
	    kCFPropertyListMutableContainersAndLeaves, NULL, &error);

	CFRelease(data);
	if (pl == NULL)
		errx(EX_DATAERR, "could not parse: %s", s);
	return (pl);
}

/* A plain value: a property list if it looks like one, else a string. */
static CFPropertyListRef
plain_value(const char *s)
{
	if (s[0] == '{' || s[0] == '(' || s[0] == '<')
		return (parse_plist_arg(s));
	return (cfstr(s));
}

static bool
parse_bool(const char *s)
{
	if (strcasecmp(s, "true") == 0 || strcasecmp(s, "yes") == 0 ||
	    strcmp(s, "1") == 0)
		return (true);
	if (strcasecmp(s, "false") == 0 || strcasecmp(s, "no") == 0 ||
	    strcmp(s, "0") == 0)
		return (false);
	errx(EX_USAGE, "not a boolean: %s", s);
}

static CFDataRef
parse_hex(const char *s)
{
	size_t len = strlen(s);
	CFMutableDataRef d = CFDataCreateMutable(NULL, 0);

	if (len % 2 != 0)
		errx(EX_USAGE, "odd number of hex digits: %s", s);
	for (size_t i = 0; i < len; i += 2) {
		unsigned int b;
		char pair[3] = { s[i], s[i + 1], '\0' };
		UInt8 byte;

		if (sscanf(pair, "%2x", &b) != 1)
			errx(EX_USAGE, "not hex: %s", s);
		byte = (UInt8)b;
		CFDataAppendBytes(d, &byte, 1);
	}
	return (d);
}

static CFDateRef
parse_date(const char *s)
{
	struct tm tm;
	time_t t;

	memset(&tm, 0, sizeof(tm));
	if (strptime(s, "%Y-%m-%d %H:%M:%S", &tm) == NULL &&
	    strptime(s, "%Y-%m-%dT%H:%M:%SZ", &tm) == NULL &&
	    strptime(s, "%Y-%m-%d", &tm) == NULL)
		errx(EX_USAGE, "not a date (YYYY-MM-DD [HH:MM:SS]): %s", s);
	t = timegm(&tm);
	return (CFDateCreate(NULL, (CFAbsoluteTime)t -
	    kCFAbsoluteTimeIntervalSince1970));
}

/* One typed value: -type value. */
static CFPropertyListRef
typed_scalar(const char *type, const char *val)
{
	if (strcmp(type, "-string") == 0)
		return (cfstr(val));
	if (strcmp(type, "-data") == 0)
		return (parse_hex(val));
	if (strcmp(type, "-int") == 0 || strcmp(type, "-integer") == 0) {
		char *end;
		long long l;

		errno = 0;
		l = strtoll(val, &end, 0);
		if (errno != 0 || *end != '\0')
			errx(EX_USAGE, "not an integer: %s", val);
		return (CFNumberCreate(NULL, kCFNumberLongLongType, &l));
	}
	if (strcmp(type, "-float") == 0) {
		char *end;
		double d;

		errno = 0;
		d = strtod(val, &end);
		if (errno != 0 || *end != '\0')
			errx(EX_USAGE, "not a number: %s", val);
		return (CFNumberCreate(NULL, kCFNumberDoubleType, &d));
	}
	if (strcmp(type, "-bool") == 0 || strcmp(type, "-boolean") == 0)
		return (CFRetain(parse_bool(val) ? kCFBooleanTrue :
		    kCFBooleanFalse));
	if (strcmp(type, "-date") == 0)
		return (parse_date(val));
	errx(EX_USAGE, "unknown type: %s", type);
}

/* An element of -array or -dict: "-type value" or a plain value. */
static CFPropertyListRef
element(int *ip, int argc, char **argv)
{
	int i = *ip;

	if (argv[i][0] == '-' && argv[i][1] != '\0' && i + 1 < argc &&
	    strcmp(argv[i], "-array") != 0 && strcmp(argv[i], "-dict") != 0) {
		*ip = i + 2;
		return (typed_scalar(argv[i], argv[i + 1]));
	}
	*ip = i + 1;
	return (plain_value(argv[i]));
}

static CFPropertyListRef
write_value(struct domain *d, CFStringRef key, int argc, char **argv)
{
	const char *type = argv[0];
	int i;

	if (type[0] != '-' || type[1] == '\0') {
		if (argc != 1)
			usage();
		return (plain_value(type));
	}
	if (strcmp(type, "-array") == 0 || strcmp(type, "-array-add") == 0) {
		CFMutableArrayRef a;

		a = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		if (strcmp(type, "-array-add") == 0) {
			CFTypeRef old = copy_value(d, key);

			if (old != NULL &&
			    CFGetTypeID(old) == CFArrayGetTypeID())
				CFArrayAppendArray(a, old,
				    CFRangeMake(0, CFArrayGetCount(old)));
			if (old != NULL)
				CFRelease(old);
		}
		for (i = 1; i < argc;) {
			CFPropertyListRef e = element(&i, argc, argv);

			CFArrayAppendValue(a, e);
			CFRelease(e);
		}
		return (a);
	}
	if (strcmp(type, "-dict") == 0 || strcmp(type, "-dict-add") == 0) {
		CFMutableDictionaryRef dict;

		dict = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		if (strcmp(type, "-dict-add") == 0) {
			CFTypeRef old = copy_value(d, key);

			if (old != NULL &&
			    CFGetTypeID(old) == CFDictionaryGetTypeID()) {
				CFRelease(dict);
				dict = CFDictionaryCreateMutableCopy(NULL, 0,
				    old);
			}
			if (old != NULL)
				CFRelease(old);
		}
		for (i = 1; i < argc;) {
			CFStringRef k;
			CFPropertyListRef v;

			if (i + 1 >= argc)
				errx(EX_USAGE, "-dict needs key value pairs");
			k = cfstr(argv[i++]);
			v = element(&i, argc, argv);
			CFDictionarySetValue(dict, k, v);
			CFRelease(k);
			CFRelease(v);
		}
		return (dict);
	}
	if (argc != 2)
		usage();
	return (typed_scalar(type, argv[1]));
}

/* ---------------------------------------------------------------------
 * Commands
 */

static int
cmd_read(int argc, char **argv)
{
	struct domain d;
	char label[1024];

	if (argc == 0) {
		/* Every domain the user has. */
		CFArrayRef apps = CFPreferencesCopyApplicationList(
		    kCFPreferencesCurrentUser, domain_host());
		CFIndex n = apps != NULL ? CFArrayGetCount(apps) : 0;

		puts("{");
		for (CFIndex i = 0; i < n; i++) {
			CFStringRef app = CFArrayGetValueAtIndex(apps, i);
			CFDictionaryRef all;

			d.name = app;
			d.file = NULL;
			d.path = NULL;
			all = copy_domain(&d);
			print_indent(1);
			print_string(app);
			fputs(" = ", stdout);
			if (all != NULL) {
				print_value(all, 1);
				CFRelease(all);
			} else
				fputs("{}", stdout);
			puts(";");
		}
		puts("}");
		return (0);
	}
	open_domain(&d, argv[0]);
	domain_label(&d, label, sizeof(label));
	if (argc == 1) {
		CFDictionaryRef all = copy_domain(&d);

		if (all == NULL || CFDictionaryGetCount(all) == 0) {
			fprintf(stderr, "\nDomain %s does not exist\n", label);
			return (1);
		}
		print_value(all, 0);
		putchar('\n');
		CFRelease(all);
		return (0);
	}
	if (argc == 2) {
		CFStringRef key = cfstr(argv[1]);
		CFTypeRef v = copy_value(&d, key);

		if (v == NULL) {
			fprintf(stderr,
			    "\nThe domain/default pair of (%s, %s) does not "
			    "exist\n", label, argv[1]);
			return (1);
		}
		print_value(v, 0);
		putchar('\n');
		CFRelease(v);
		CFRelease(key);
		return (0);
	}
	usage();
}

static int
cmd_read_type(int argc, char **argv)
{
	struct domain d;
	CFStringRef key;
	CFTypeRef v;
	char label[1024];

	if (argc != 2)
		usage();
	open_domain(&d, argv[0]);
	domain_label(&d, label, sizeof(label));
	key = cfstr(argv[1]);
	if ((v = copy_value(&d, key)) == NULL) {
		fprintf(stderr, "\nThe domain/default pair of (%s, %s) does not "
		    "exist\n", label, argv[1]);
		return (1);
	}
	printf("Type is %s\n", type_name(v));
	CFRelease(v);
	CFRelease(key);
	return (0);
}

static int
cmd_write(int argc, char **argv)
{
	struct domain d;

	if (argc < 2)
		usage();
	open_domain(&d, argv[0]);
	if (argc == 2) {
		/* defaults write domain '{ key = value; ... }' */
		CFPropertyListRef pl = parse_plist_arg(argv[1]);
		CFDictionaryRef old;
		CFIndex n;

		if (CFGetTypeID(pl) != CFDictionaryGetTypeID())
			errx(EX_USAGE, "a domain must be written as a "
			    "dictionary");
		/* It replaces the whole domain. */
		if ((old = copy_domain(&d)) != NULL) {
			n = CFDictionaryGetCount(old);
			const void **keys = calloc((size_t)n + 1, sizeof(void *));

			CFDictionaryGetKeysAndValues(old, keys, NULL);
			for (CFIndex i = 0; i < n; i++)
				set_value(&d, keys[i], NULL);
			free(keys);
			CFRelease(old);
		}
		n = CFDictionaryGetCount(pl);
		const void **keys = calloc((size_t)n + 1, sizeof(void *));
		const void **vals = calloc((size_t)n + 1, sizeof(void *));

		CFDictionaryGetKeysAndValues(pl, keys, vals);
		for (CFIndex i = 0; i < n; i++)
			set_value(&d, keys[i], vals[i]);
		free(keys);
		free(vals);
		CFRelease(pl);
	} else {
		CFStringRef key = cfstr(argv[1]);
		CFPropertyListRef v = write_value(&d, key, argc - 2, argv + 2);

		set_value(&d, key, v);
		CFRelease(v);
		CFRelease(key);
	}
	sync_domain(&d);
	return (0);
}

static int
cmd_delete(int argc, char **argv)
{
	struct domain d;
	char label[1024];

	if (argc < 1 || argc > 2)
		usage();
	open_domain(&d, argv[0]);
	domain_label(&d, label, sizeof(label));
	if (argc == 1) {
		CFDictionaryRef all = copy_domain(&d);
		CFIndex n;

		if (all == NULL || CFDictionaryGetCount(all) == 0) {
			fprintf(stderr, "\nDomain (%s) not found.\n", label);
			return (1);
		}
		n = CFDictionaryGetCount(all);
		const void **keys = calloc((size_t)n + 1, sizeof(void *));

		CFDictionaryGetKeysAndValues(all, keys, NULL);
		for (CFIndex i = 0; i < n; i++)
			set_value(&d, keys[i], NULL);
		free(keys);
		CFRelease(all);
	} else {
		CFStringRef key = cfstr(argv[1]);
		CFTypeRef v = copy_value(&d, key);

		if (v == NULL) {
			fprintf(stderr, "\nDomain (%s) not found.\nDefaults "
			    "have not been changed.\n", label);
			return (1);
		}
		CFRelease(v);
		set_value(&d, key, NULL);
		CFRelease(key);
	}
	sync_domain(&d);
	return (0);
}

static int
cmd_rename(int argc, char **argv)
{
	struct domain d;
	CFStringRef from, to;
	CFTypeRef v;

	if (argc != 3)
		usage();
	open_domain(&d, argv[0]);
	from = cfstr(argv[1]);
	to = cfstr(argv[2]);
	if ((v = copy_value(&d, from)) == NULL)
		errx(1, "key %s does not exist", argv[1]);
	set_value(&d, to, v);
	set_value(&d, from, NULL);
	sync_domain(&d);
	CFRelease(v);
	return (0);
}

static int
cmd_import(int argc, char **argv)
{
	struct domain d;
	CFPropertyListRef pl;
	CFIndex n;

	if (argc != 2)
		usage();
	open_domain(&d, argv[0]);
	pl = read_plist_file(argv[1], true);
	if (CFGetTypeID(pl) != CFDictionaryGetTypeID())
		errx(EX_DATAERR, "%s: not a dictionary", argv[1]);
	n = CFDictionaryGetCount(pl);
	const void **keys = calloc((size_t)n + 1, sizeof(void *));
	const void **vals = calloc((size_t)n + 1, sizeof(void *));

	CFDictionaryGetKeysAndValues(pl, keys, vals);
	for (CFIndex i = 0; i < n; i++)
		set_value(&d, keys[i], vals[i]);
	free(keys);
	free(vals);
	sync_domain(&d);
	CFRelease(pl);
	return (0);
}

static int
cmd_export(int argc, char **argv)
{
	struct domain d;
	CFDictionaryRef all;
	char label[1024];

	if (argc != 2)
		usage();
	open_domain(&d, argv[0]);
	domain_label(&d, label, sizeof(label));
	if ((all = copy_domain(&d)) == NULL ||
	    CFDictionaryGetCount(all) == 0) {
		fprintf(stderr, "\nDomain %s does not exist\n", label);
		return (1);
	}
	write_plist_file(argv[1], all, strcmp(argv[1], "-") == 0 ?
	    kCFPropertyListXMLFormat_v1_0 : kCFPropertyListBinaryFormat_v1_0);
	CFRelease(all);
	return (0);
}

static int
cmd_domains(int argc, char **argv)
{
	CFArrayRef apps;
	CFIndex n;
	bool first = true;

	(void)argv;
	if (argc != 0)
		usage();
	apps = CFPreferencesCopyApplicationList(kCFPreferencesCurrentUser,
	    domain_host());
	n = apps != NULL ? CFArrayGetCount(apps) : 0;
	for (CFIndex i = 0; i < n; i++) {
		CFStringRef app = CFArrayGetValueAtIndex(apps, i);

		if (CFEqual(app, kCFPreferencesAnyApplication))
			continue;
		if (!first)
			fputs(", ", stdout);
		print_string(app);
		first = false;
	}
	putchar('\n');
	return (0);
}

/* Does a key or value (deep) mention word? Case-insensitive. */
static bool
mentions(CFTypeRef v, CFStringRef word)
{
	CFTypeID t = CFGetTypeID(v);

	if (t == CFStringGetTypeID())
		return (CFStringFind(v, word, kCFCompareCaseInsensitive).location
		    != kCFNotFound);
	if (t == CFArrayGetTypeID()) {
		for (CFIndex i = 0; i < CFArrayGetCount(v); i++)
			if (mentions(CFArrayGetValueAtIndex(v, i), word))
				return (true);
		return (false);
	}
	if (t == CFDictionaryGetTypeID()) {
		CFIndex n = CFDictionaryGetCount(v);
		const void **k = calloc((size_t)n + 1, sizeof(void *));
		const void **x = calloc((size_t)n + 1, sizeof(void *));
		bool found = false;

		CFDictionaryGetKeysAndValues(v, k, x);
		for (CFIndex i = 0; i < n && !found; i++)
			found = mentions(k[i], word) || mentions(x[i], word);
		free(k);
		free(x);
		return (found);
	}
	return (false);
}

static int
cmd_find(int argc, char **argv)
{
	CFArrayRef apps;
	CFStringRef word;
	CFIndex n;

	if (argc != 1)
		usage();
	word = cfstr(argv[0]);
	apps = CFPreferencesCopyApplicationList(kCFPreferencesCurrentUser,
	    domain_host());
	n = apps != NULL ? CFArrayGetCount(apps) : 0;
	for (CFIndex i = 0; i < n; i++) {
		struct domain d = { .name = CFArrayGetValueAtIndex(apps, i) };
		CFDictionaryRef all = copy_domain(&d);
		CFMutableDictionaryRef hits;
		CFIndex m;
		char *name;

		if (all == NULL)
			continue;
		hits = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		m = CFDictionaryGetCount(all);
		const void **k = calloc((size_t)m + 1, sizeof(void *));
		const void **x = calloc((size_t)m + 1, sizeof(void *));

		CFDictionaryGetKeysAndValues(all, k, x);
		for (CFIndex j = 0; j < m; j++)
			if (mentions(k[j], word) || mentions(x[j], word))
				CFDictionarySetValue(hits, k[j], x[j]);
		if (CFDictionaryGetCount(hits) > 0 ||
		    mentions(d.name, word)) {
			name = cstr(d.name);
			printf("Found %ld keys in domain '%s': ",
			    (long)CFDictionaryGetCount(hits), name);
			print_value(hits, 0);
			putchar('\n');
			free(name);
		}
		free(k);
		free(x);
		CFRelease(hits);
		CFRelease(all);
	}
	return (0);
}

int
main(int argc, char *argv[])
{
	const char *cmd;

	argc--;
	argv++;
	while (argc > 0 && argv[0][0] == '-') {
		if (strcmp(argv[0], "-currentHost") == 0)
			host = kCFPreferencesCurrentHost;
		else if (strcmp(argv[0], "-host") == 0 && argc > 1) {
			host = cfstr(argv[1]);
			argc--;
			argv++;
		} else
			break;
		argc--;
		argv++;
	}
	if (argc == 0)
		usage();
	cmd = argv[0];
	argc--;
	argv++;

	if (strcmp(cmd, "read") == 0)
		return (cmd_read(argc, argv));
	if (strcmp(cmd, "read-type") == 0)
		return (cmd_read_type(argc, argv));
	if (strcmp(cmd, "write") == 0)
		return (cmd_write(argc, argv));
	if (strcmp(cmd, "delete") == 0 || strcmp(cmd, "remove") == 0)
		return (cmd_delete(argc, argv));
	if (strcmp(cmd, "rename") == 0)
		return (cmd_rename(argc, argv));
	if (strcmp(cmd, "import") == 0)
		return (cmd_import(argc, argv));
	if (strcmp(cmd, "export") == 0)
		return (cmd_export(argc, argv));
	if (strcmp(cmd, "domains") == 0)
		return (cmd_domains(argc, argv));
	if (strcmp(cmd, "find") == 0)
		return (cmd_find(argc, argv));
	if (strcmp(cmd, "help") == 0) {
		usage();
	}
	usage();
}

static void
usage(void)
{

	fprintf(stderr,
"usage: defaults [-currentHost | -host hostname] command\n"
"\n"
"  read [domain [key]]            show a domain, a key, or everything\n"
"  read-type domain key           show the type of a key's value\n"
"  write domain key value         write a value (a string, or a plist\n"
"                                 if it begins with {, ( or <)\n"
"  write domain key -type value   types: -string -data -int -float\n"
"                                 -bool -date -array[-add] -dict[-add]\n"
"  write domain 'plist'           replace the domain with a dictionary\n"
"  delete domain [key]            remove a key, or the whole domain\n"
"  rename domain old new          rename a key\n"
"  import domain path | -         add the keys of a plist file\n"
"  export domain path | -         save the domain as a plist\n"
"  domains                        list the domains\n"
"  find word                      search keys and values\n"
"\n"
"A domain is a name (com.example.app), -g or NSGlobalDomain, or the\n"
"path of a property list file.\n");
	exit(EX_USAGE);
}
