/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 ravynOS contributors
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
 * plutil: check, print, convert and edit property lists.
 *
 *   plutil -lint file...
 *   plutil -p file...
 *   plutil -convert xml1|binary1|json [-r] [-o out | -e ext] file...
 *   plutil -insert keypath -type value [-append] file...
 *   plutil -replace keypath -type value file...
 *   plutil -remove keypath file...
 *   plutil -extract keypath xml1|binary1|json|raw [-o out] file...
 *   plutil -type keypath file...
 *
 * Types: -bool, -integer, -float, -string, -date, -data (base64),
 * -xml and -json (a value written in that form), -array and
 * -dictionary (empty). A keypath is keys and array indexes joined by
 * dots; "\." is a dot inside a key. "-" as a file is standard input
 * (and output). Files may be XML, binary, old-style or JSON.
 */

#include <CoreFoundation/CoreFoundation.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

enum fmt { FMT_NONE, FMT_XML, FMT_BINARY, FMT_JSON, FMT_RAW };

static bool silent;
static bool readable;		/* -r: indented JSON */

static void usage(void) __dead2;

/* ---------------------------------------------------------------------
 * Helpers
 */

static CFStringRef
cfstr_n(const char *s, size_t len)
{
	CFStringRef r = CFStringCreateWithBytes(NULL, (const UInt8 *)s,
	    (CFIndex)len, kCFStringEncodingUTF8, false);

	if (r == NULL)
		errx(EX_DATAERR, "not valid UTF-8");
	return (r);
}

static CFStringRef
cfstr(const char *s)
{

	return (cfstr_n(s, strlen(s)));
}

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

static CFDataRef
read_file(const char *path)
{
	CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
	UInt8 buf[16384];
	size_t n;
	FILE *f;

	if (strcmp(path, "-") == 0)
		f = stdin;
	else if ((f = fopen(path, "r")) == NULL)
		return (NULL);
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		CFDataAppendBytes(data, buf, (CFIndex)n);
	if (f != stdin)
		fclose(f);
	return (data);
}

static bool
write_file(const char *path, const void *bytes, size_t len)
{
	FILE *f;
	char *tmp = NULL;
	bool ok;

	if (strcmp(path, "-") == 0)
		return (fwrite(bytes, 1, len, stdout) == len &&
		    fflush(stdout) == 0);
	/* Write beside the file and rename, so a failure loses nothing. */
	if (asprintf(&tmp, "%s.plutil.%d", path, (int)getpid()) == -1)
		err(EX_OSERR, "asprintf");
	if ((f = fopen(tmp, "w")) == NULL) {
		warn("%s", tmp);
		free(tmp);
		return (false);
	}
	ok = fwrite(bytes, 1, len, f) == len;
	ok = (fclose(f) == 0) && ok;
	if (ok && rename(tmp, path) == -1) {
		warn("%s", path);
		ok = false;
	}
	if (!ok)
		unlink(tmp);
	free(tmp);
	return (ok);
}

/* ---------------------------------------------------------------------
 * JSON
 */

struct jparse {
	const char	*p, *end;
	const char	*error;
};

static CFTypeRef json_value(struct jparse *j, int depth);

static void
json_space(struct jparse *j)
{

	while (j->p < j->end && (*j->p == ' ' || *j->p == '\t' ||
	    *j->p == '\n' || *j->p == '\r'))
		j->p++;
}

static int
hexval(char c)
{
	if (c >= '0' && c <= '9')
		return (c - '0');
	if (c >= 'a' && c <= 'f')
		return (c - 'a' + 10);
	if (c >= 'A' && c <= 'F')
		return (c - 'A' + 10);
	return (-1);
}

static bool
json_hex4(struct jparse *j, unsigned *out)
{
	unsigned v = 0;

	if (j->end - j->p < 4)
		return (false);
	for (int i = 0; i < 4; i++) {
		int h = hexval(j->p[i]);

		if (h < 0)
			return (false);
		v = v << 4 | (unsigned)h;
	}
	j->p += 4;
	*out = v;
	return (true);
}

static void
put_utf8(char **b, size_t *len, size_t *cap, unsigned cp)
{
	char tmp[4];
	size_t n;

	if (cp < 0x80) {
		tmp[0] = (char)cp;
		n = 1;
	} else if (cp < 0x800) {
		tmp[0] = (char)(0xc0 | cp >> 6);
		tmp[1] = (char)(0x80 | (cp & 0x3f));
		n = 2;
	} else if (cp < 0x10000) {
		tmp[0] = (char)(0xe0 | cp >> 12);
		tmp[1] = (char)(0x80 | (cp >> 6 & 0x3f));
		tmp[2] = (char)(0x80 | (cp & 0x3f));
		n = 3;
	} else {
		tmp[0] = (char)(0xf0 | cp >> 18);
		tmp[1] = (char)(0x80 | (cp >> 12 & 0x3f));
		tmp[2] = (char)(0x80 | (cp >> 6 & 0x3f));
		tmp[3] = (char)(0x80 | (cp & 0x3f));
		n = 4;
	}
	if (*len + n + 1 > *cap) {
		*cap = (*cap + n) * 2;
		if ((*b = realloc(*b, *cap)) == NULL)
			err(EX_OSERR, "realloc");
	}
	memcpy(*b + *len, tmp, n);
	*len += n;
}

static CFStringRef
json_string(struct jparse *j)
{
	char *b = NULL;
	size_t len = 0, cap = 0;
	CFStringRef s;

	j->p++;		/* the opening quote */
	while (j->p < j->end && *j->p != '"') {
		unsigned c = (unsigned char)*j->p++;

		if (c < 0x20) {
			j->error = "control character in a string";
			free(b);
			return (NULL);
		}
		if (c == '\\') {
			if (j->p >= j->end)
				break;
			switch (*j->p++) {
			case '"': c = '"'; break;
			case '\\': c = '\\'; break;
			case '/': c = '/'; break;
			case 'b': c = '\b'; break;
			case 'f': c = '\f'; break;
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			case 'u': {
				unsigned lo;

				if (!json_hex4(j, &c))
					goto bad;
				if (c >= 0xd800 && c < 0xdc00) {
					if (j->end - j->p < 6 || j->p[0] != '\\' ||
					    j->p[1] != 'u')
						goto bad;
					j->p += 2;
					if (!json_hex4(j, &lo) || lo < 0xdc00 ||
					    lo > 0xdfff)
						goto bad;
					c = 0x10000 + ((c - 0xd800) << 10) +
					    (lo - 0xdc00);
				}
				break;
			}
			default:
				goto bad;
			}
			put_utf8(&b, &len, &cap, c);
			continue;
		}
		/* UTF-8 bytes pass through. */
		if (len + 2 > cap) {
			cap = (cap + 2) * 2;
			if ((b = realloc(b, cap)) == NULL)
				err(EX_OSERR, "realloc");
		}
		b[len++] = (char)c;
	}
	if (j->p >= j->end) {
		j->error = "unterminated string";
		free(b);
		return (NULL);
	}
	j->p++;		/* the closing quote */
	s = CFStringCreateWithBytes(NULL, (const UInt8 *)(b ? b : ""),
	    (CFIndex)len, kCFStringEncodingUTF8, false);
	free(b);
	if (s == NULL)
		j->error = "not valid UTF-8";
	return (s);
bad:
	j->error = "bad escape in a string";
	free(b);
	return (NULL);
}

static CFNumberRef
json_number(struct jparse *j)
{
	const char *start = j->p;
	bool is_float = false;
	char buf[64];
	size_t n;

	if (j->p < j->end && *j->p == '-')
		j->p++;
	while (j->p < j->end && (isdigit((unsigned char)*j->p) ||
	    *j->p == '.' || *j->p == 'e' || *j->p == 'E' || *j->p == '+' ||
	    *j->p == '-')) {
		if (*j->p == '.' || *j->p == 'e' || *j->p == 'E')
			is_float = true;
		j->p++;
	}
	n = (size_t)(j->p - start);
	if (n == 0 || n >= sizeof(buf)) {
		j->error = "bad number";
		return (NULL);
	}
	memcpy(buf, start, n);
	buf[n] = '\0';
	if (!is_float) {
		char *end;
		long long l;

		errno = 0;
		l = strtoll(buf, &end, 10);
		if (errno == 0 && *end == '\0')
			return (CFNumberCreate(NULL, kCFNumberLongLongType, &l));
	}
	{
		char *end;
		double d = strtod(buf, &end);

		if (*end != '\0') {
			j->error = "bad number";
			return (NULL);
		}
		return (CFNumberCreate(NULL, kCFNumberDoubleType, &d));
	}
}

static bool
json_word(struct jparse *j, const char *w)
{
	size_t n = strlen(w);

	if ((size_t)(j->end - j->p) >= n && memcmp(j->p, w, n) == 0) {
		j->p += n;
		return (true);
	}
	return (false);
}

static CFTypeRef
json_value(struct jparse *j, int depth)
{
	if (depth > 512) {
		j->error = "nested too deeply";
		return (NULL);
	}
	json_space(j);
	if (j->p >= j->end) {
		j->error = "unexpected end";
		return (NULL);
	}
	switch (*j->p) {
	case '{': {
		CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);

		j->p++;
		json_space(j);
		if (j->p < j->end && *j->p == '}') {
			j->p++;
			return (d);
		}
		for (;;) {
			CFStringRef k;
			CFTypeRef v;

			json_space(j);
			if (j->p >= j->end || *j->p != '"') {
				j->error = "expected a key";
				CFRelease(d);
				return (NULL);
			}
			if ((k = json_string(j)) == NULL) {
				CFRelease(d);
				return (NULL);
			}
			json_space(j);
			if (j->p >= j->end || *j->p != ':') {
				j->error = "expected ':'";
				CFRelease(k);
				CFRelease(d);
				return (NULL);
			}
			j->p++;
			if ((v = json_value(j, depth + 1)) == NULL) {
				CFRelease(k);
				CFRelease(d);
				return (NULL);
			}
			CFDictionarySetValue(d, k, v);
			CFRelease(k);
			CFRelease(v);
			json_space(j);
			if (j->p < j->end && *j->p == ',') {
				j->p++;
				continue;
			}
			if (j->p < j->end && *j->p == '}') {
				j->p++;
				return (d);
			}
			j->error = "expected ',' or '}'";
			CFRelease(d);
			return (NULL);
		}
	}
	case '[': {
		CFMutableArrayRef a = CFArrayCreateMutable(NULL, 0,
		    &kCFTypeArrayCallBacks);

		j->p++;
		json_space(j);
		if (j->p < j->end && *j->p == ']') {
			j->p++;
			return (a);
		}
		for (;;) {
			CFTypeRef v = json_value(j, depth + 1);

			if (v == NULL) {
				CFRelease(a);
				return (NULL);
			}
			CFArrayAppendValue(a, v);
			CFRelease(v);
			json_space(j);
			if (j->p < j->end && *j->p == ',') {
				j->p++;
				continue;
			}
			if (j->p < j->end && *j->p == ']') {
				j->p++;
				return (a);
			}
			j->error = "expected ',' or ']'";
			CFRelease(a);
			return (NULL);
		}
	}
	case '"':
		return (json_string(j));
	case 't':
		if (json_word(j, "true"))
			return (CFRetain(kCFBooleanTrue));
		break;
	case 'f':
		if (json_word(j, "false"))
			return (CFRetain(kCFBooleanFalse));
		break;
	case 'n':
		if (json_word(j, "null")) {
			j->error = "null has no property list form";
			return (NULL);
		}
		break;
	default:
		if (*j->p == '-' || isdigit((unsigned char)*j->p))
			return (json_number(j));
		break;
	}
	j->error = "unexpected character";
	return (NULL);
}

static CFTypeRef
json_parse(const char *s, size_t len, const char **error)
{
	struct jparse j = { s, s + len, NULL };
	CFTypeRef v = json_value(&j, 0);

	if (v != NULL) {
		json_space(&j);
		if (j.p != j.end) {
			CFRelease(v);
			v = NULL;
			j.error = "garbage after the value";
		}
	}
	*error = j.error;
	return (v);
}

struct buf {
	char	*b;
	size_t	len, cap;
};

static void
bput(struct buf *b, const char *s, size_t n)
{
	if (b->len + n + 1 > b->cap) {
		b->cap = (b->cap + n + 1) * 2;
		if ((b->b = realloc(b->b, b->cap)) == NULL)
			err(EX_OSERR, "realloc");
	}
	memcpy(b->b + b->len, s, n);
	b->len += n;
	b->b[b->len] = '\0';
}

static void
bputs(struct buf *b, const char *s)
{

	bput(b, s, strlen(s));
}

static void
json_put_string(struct buf *b, CFStringRef s)
{
	char *c = cstr(s);

	bputs(b, "\"");
	for (const unsigned char *p = (unsigned char *)c; *p; p++) {
		char esc[8];

		switch (*p) {
		case '"': bputs(b, "\\\""); break;
		case '\\': bputs(b, "\\\\"); break;
		case '\n': bputs(b, "\\n"); break;
		case '\r': bputs(b, "\\r"); break;
		case '\t': bputs(b, "\\t"); break;
		case '\b': bputs(b, "\\b"); break;
		case '\f': bputs(b, "\\f"); break;
		case '/': bputs(b, "\\/"); break;
		default:
			if (*p < 0x20) {
				snprintf(esc, sizeof(esc), "\\u%04x", *p);
				bputs(b, esc);
			} else
				bput(b, (const char *)p, 1);
		}
	}
	bputs(b, "\"");
	free(c);
}

static void
json_indent(struct buf *b, int level)
{
	if (!readable)
		return;
	bputs(b, "\n");
	for (int i = 0; i < level; i++)
		bputs(b, "  ");
}

static int
compare_keys(const void *a, const void *b)
{

	return ((int)CFStringCompare(*(CFStringRef const *)a,
	    *(CFStringRef const *)b, 0));
}

/* Returns a reason when v has no JSON form (data, dates), or NULL. */
static const char *
json_put(struct buf *b, CFTypeRef v, int level)
{
	CFTypeID t = CFGetTypeID(v);
	const char *why;

	if (t == CFStringGetTypeID()) {
		json_put_string(b, v);
	} else if (t == CFBooleanGetTypeID()) {
		bputs(b, CFBooleanGetValue(v) ? "true" : "false");
	} else if (t == CFNumberGetTypeID()) {
		char num[64];

		if (CFNumberIsFloatType(v)) {
			double d;

			CFNumberGetValue(v, kCFNumberDoubleType, &d);
			snprintf(num, sizeof(num), "%.17g", d);
		} else {
			long long l;

			CFNumberGetValue(v, kCFNumberLongLongType, &l);
			snprintf(num, sizeof(num), "%lld", l);
		}
		bputs(b, num);
	} else if (t == CFArrayGetTypeID()) {
		CFIndex n = CFArrayGetCount(v);

		bputs(b, "[");
		for (CFIndex i = 0; i < n; i++) {
			if (i > 0)
				bputs(b, ",");
			json_indent(b, level + 1);
			if ((why = json_put(b, CFArrayGetValueAtIndex(v, i),
			    level + 1)) != NULL)
				return (why);
		}
		if (n > 0)
			json_indent(b, level);
		bputs(b, "]");
	} else if (t == CFDictionaryGetTypeID()) {
		CFIndex n = CFDictionaryGetCount(v);
		const void **keys = calloc((size_t)n + 1, sizeof(void *));

		CFDictionaryGetKeysAndValues(v, keys, NULL);
		qsort(keys, (size_t)n, sizeof(void *), compare_keys);
		bputs(b, "{");
		for (CFIndex i = 0; i < n; i++) {
			if (i > 0)
				bputs(b, ",");
			json_indent(b, level + 1);
			json_put_string(b, keys[i]);
			bputs(b, readable ? " : " : ":");
			if ((why = json_put(b, CFDictionaryGetValue(v, keys[i]),
			    level + 1)) != NULL) {
				free(keys);
				return (why);
			}
		}
		if (n > 0)
			json_indent(b, level);
		bputs(b, "}");
		free(keys);
	} else if (t == CFDateGetTypeID())
		return ("invalid object in plist for destination format");
	else if (t == CFDataGetTypeID())
		return ("invalid object in plist for destination format");
	else
		return ("unknown object");
	return (NULL);
}

/* ---------------------------------------------------------------------
 * Reading and writing property lists
 */

static CFPropertyListRef
parse_data(CFDataRef data, const char **why)
{
	CFErrorRef error = NULL;
	CFPropertyListRef pl;
	const UInt8 *p = CFDataGetBytePtr(data);
	CFIndex len = CFDataGetLength(data), i = 0;

	*why = NULL;
	/* JSON starts with { or [ after blanks; old-style plists may too. */
	while (i < len && isspace(p[i]))
		i++;
	pl = CFPropertyListCreateWithData(NULL, data,
	    kCFPropertyListMutableContainersAndLeaves, NULL, &error);
	if (pl != NULL)
		return (pl);
	if (i < len && (p[i] == '{' || p[i] == '[' || p[i] == '"')) {
		const char *jerr;

		pl = json_parse((const char *)p, (size_t)len, &jerr);
		if (pl != NULL)
			return (pl);
		*why = jerr;
	}
	if (*why == NULL)
		*why = "not a property list";
	if (error != NULL)
		CFRelease(error);
	return (NULL);
}

static CFPropertyListRef
load(const char *path)
{
	CFDataRef data = read_file(path);
	CFPropertyListRef pl;
	const char *why;

	if (data == NULL) {
		warnx("%s: file does not exist or is not readable", path);
		return (NULL);
	}
	pl = parse_data(data, &why);
	CFRelease(data);
	if (pl == NULL)
		warnx("%s: %s", path, why);
	return (pl);
}

static bool
save(const char *path, CFPropertyListRef pl, enum fmt fmt)
{
	if (fmt == FMT_JSON) {
		struct buf b = { NULL, 0, 0 };
		const char *why = json_put(&b, pl, 0);
		bool ok;

		if (why != NULL) {
			warnx("%s: %s", path, why);
			free(b.b);
			return (false);
		}
		if (readable)
			bputs(&b, "\n");
		ok = write_file(path, b.b, b.len);
		free(b.b);
		return (ok);
	} else {
		CFErrorRef error = NULL;
		CFDataRef data = CFPropertyListCreateData(NULL, pl,
		    fmt == FMT_BINARY ? kCFPropertyListBinaryFormat_v1_0 :
		    kCFPropertyListXMLFormat_v1_0, 0, &error);
		bool ok;

		if (data == NULL) {
			warnx("%s: cannot encode the property list", path);
			return (false);
		}
		ok = write_file(path, CFDataGetBytePtr(data),
		    (size_t)CFDataGetLength(data));
		CFRelease(data);
		return (ok);
	}
}

static enum fmt
parse_fmt(const char *s)
{
	if (strcmp(s, "xml1") == 0)
		return (FMT_XML);
	if (strcmp(s, "binary1") == 0)
		return (FMT_BINARY);
	if (strcmp(s, "json") == 0)
		return (FMT_JSON);
	if (strcmp(s, "raw") == 0)
		return (FMT_RAW);
	errx(EX_USAGE, "unknown format: %s (xml1, binary1, json, raw)", s);
}

/* ---------------------------------------------------------------------
 * Printing (-p)
 */

static void
pprint(CFTypeRef v, int level)
{
	CFTypeID t = CFGetTypeID(v);

	if (t == CFStringGetTypeID()) {
		char *c = cstr(v);

		printf("\"%s\"", c);
		free(c);
	} else if (t == CFBooleanGetTypeID()) {
		fputs(CFBooleanGetValue(v) ? "true" : "false", stdout);
	} else if (t == CFNumberGetTypeID()) {
		if (CFNumberIsFloatType(v)) {
			double d;

			CFNumberGetValue(v, kCFNumberDoubleType, &d);
			printf("%.15g", d);
		} else {
			long long l;

			CFNumberGetValue(v, kCFNumberLongLongType, &l);
			printf("%lld", l);
		}
	} else if (t == CFDateGetTypeID()) {
		time_t tt = (time_t)(CFDateGetAbsoluteTime(v) +
		    kCFAbsoluteTimeIntervalSince1970);
		struct tm tm;
		char buf[64];

		gmtime_r(&tt, &tm);
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S +0000", &tm);
		fputs(buf, stdout);
	} else if (t == CFDataGetTypeID()) {
		const UInt8 *p = CFDataGetBytePtr(v);
		CFIndex n = CFDataGetLength(v);

		printf("{length = %ld, bytes = 0x", (long)n);
		for (CFIndex i = 0; i < n && i < 32; i++)
			printf("%02x", p[i]);
		if (n > 32)
			fputs("...", stdout);
		putchar('}');
	} else if (t == CFArrayGetTypeID()) {
		CFIndex n = CFArrayGetCount(v);

		puts("[");
		for (CFIndex i = 0; i < n; i++) {
			printf("%*s%ld => ", (level + 1) * 2, "", (long)i);
			pprint(CFArrayGetValueAtIndex(v, i), level + 1);
			putchar('\n');
		}
		printf("%*s]", level * 2, "");
	} else if (t == CFDictionaryGetTypeID()) {
		CFIndex n = CFDictionaryGetCount(v);
		const void **keys = calloc((size_t)n + 1, sizeof(void *));

		CFDictionaryGetKeysAndValues(v, keys, NULL);
		qsort(keys, (size_t)n, sizeof(void *), compare_keys);
		puts("{");
		for (CFIndex i = 0; i < n; i++) {
			char *k = cstr(keys[i]);

			printf("%*s\"%s\" => ", (level + 1) * 2, "", k);
			free(k);
			pprint(CFDictionaryGetValue(v, keys[i]), level + 1);
			putchar('\n');
		}
		printf("%*s}", level * 2, "");
		free(keys);
	}
}

/* ---------------------------------------------------------------------
 * Key paths
 */

/* Split "a.b\.c.0" into components; returns the count. */
static int
split_keypath(const char *kp, char ***out)
{
	char **parts = NULL;
	int n = 0;
	const char *p = kp;

	for (;;) {
		char *comp = malloc(strlen(p) + 1), *q = comp;

		if (comp == NULL)
			err(EX_OSERR, "malloc");
		while (*p != '\0' && *p != '.') {
			if (*p == '\\' && p[1] == '.')
				p++;
			*q++ = *p++;
		}
		*q = '\0';
		if ((parts = realloc(parts, sizeof(char *) * (size_t)(n + 1)))
		    == NULL)
			err(EX_OSERR, "realloc");
		parts[n++] = comp;
		if (*p == '\0')
			break;
		p++;
	}
	*out = parts;
	return (n);
}

static bool
array_index(const char *s, CFIndex count, CFIndex *idx)
{
	char *end;
	long l;

	if (*s == '\0')
		return (false);
	errno = 0;
	l = strtol(s, &end, 10);
	if (errno != 0 || *end != '\0' || l < 0 || l >= count)
		return (false);
	*idx = l;
	return (true);
}

/* The value at a key path, or NULL. */
static CFTypeRef
lookup(CFTypeRef root, char **parts, int n)
{
	CFTypeRef cur = root;

	for (int i = 0; i < n && cur != NULL; i++) {
		if (CFGetTypeID(cur) == CFDictionaryGetTypeID()) {
			CFStringRef k = cfstr(parts[i]);

			cur = CFDictionaryGetValue(cur, k);
			CFRelease(k);
		} else if (CFGetTypeID(cur) == CFArrayGetTypeID()) {
			CFIndex idx;

			if (!array_index(parts[i], CFArrayGetCount(cur), &idx))
				return (NULL);
			cur = CFArrayGetValueAtIndex(cur, idx);
		} else
			return (NULL);
	}
	return (cur);
}

enum edit { EDIT_INSERT, EDIT_REPLACE, EDIT_REMOVE };

/* Apply an edit at a key path; returns a reason on failure. */
static const char *
edit(CFTypeRef root, char **parts, int n, enum edit op, CFTypeRef value,
    bool append)
{
	CFTypeRef parent = lookup(root, parts, n - 1);
	const char *last = parts[n - 1];

	if (parent == NULL)
		return ("No value at that key path or invalid key path");
	if (CFGetTypeID(parent) == CFDictionaryGetTypeID()) {
		CFMutableDictionaryRef d = (CFMutableDictionaryRef)parent;
		CFStringRef k = cfstr(last);
		bool exists = CFDictionaryContainsKey(d, k);

		if (append) {
			CFRelease(k);
			return ("-append needs an array at the key path");
		}
		switch (op) {
		case EDIT_INSERT:
			if (exists) {
				CFRelease(k);
				return ("Value already exists at key path");
			}
			CFDictionarySetValue(d, k, value);
			break;
		case EDIT_REPLACE:
			CFDictionarySetValue(d, k, value);
			break;
		case EDIT_REMOVE:
			if (!exists) {
				CFRelease(k);
				return ("No value to remove at key path");
			}
			CFDictionaryRemoveValue(d, k);
			break;
		}
		CFRelease(k);
		return (NULL);
	}
	if (CFGetTypeID(parent) == CFArrayGetTypeID()) {
		CFMutableArrayRef a = (CFMutableArrayRef)parent;
		CFIndex count = CFArrayGetCount(a), idx;

		/* -append: the key path names the array itself. */
		if (append) {
			CFTypeRef target = lookup(root, parts, n);

			if (target == NULL ||
			    CFGetTypeID(target) != CFArrayGetTypeID())
				return ("-append needs an array at the key path");
			CFArrayAppendValue((CFMutableArrayRef)target, value);
			return (NULL);
		}
		switch (op) {
		case EDIT_INSERT:
			if (!array_index(last, count + 1, &idx))
				return ("Invalid array index");
			CFArrayInsertValueAtIndex(a, idx, value);
			break;
		case EDIT_REPLACE:
			if (array_index(last, count, &idx))
				CFArraySetValueAtIndex(a, idx, value);
			else if (array_index(last, count + 1, &idx))
				CFArrayAppendValue(a, value);
			else
				return ("Invalid array index");
			break;
		case EDIT_REMOVE:
			if (!array_index(last, count, &idx))
				return ("Invalid array index");
			CFArrayRemoveValueAtIndex(a, idx);
			break;
		}
		return (NULL);
	}
	return ("No value at that key path or invalid key path");
}

/* ---------------------------------------------------------------------
 * Values from the command line
 */

static CFDataRef
base64_decode(const char *s)
{
	static const char tbl[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	CFMutableDataRef d = CFDataCreateMutable(NULL, 0);
	unsigned acc = 0;
	int bits = 0;

	for (; *s != '\0'; s++) {
		const char *p;
		UInt8 byte;

		if (*s == '=' || isspace((unsigned char)*s))
			continue;
		if ((p = strchr(tbl, *s)) == NULL)
			errx(EX_USAGE, "not base64: %c", *s);
		acc = acc << 6 | (unsigned)(p - tbl);
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			byte = (UInt8)(acc >> bits);
			CFDataAppendBytes(d, &byte, 1);
		}
	}
	return (d);
}

static CFTypeRef
make_value(const char *type, const char *val)
{
	if (strcmp(type, "-string") == 0)
		return (cfstr(val));
	if (strcmp(type, "-bool") == 0) {
		if (strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0 ||
		    strcmp(val, "1") == 0)
			return (CFRetain(kCFBooleanTrue));
		if (strcasecmp(val, "false") == 0 || strcasecmp(val, "no") == 0 ||
		    strcmp(val, "0") == 0)
			return (CFRetain(kCFBooleanFalse));
		errx(EX_USAGE, "not a boolean: %s", val);
	}
	if (strcmp(type, "-integer") == 0) {
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
		double d = strtod(val, &end);

		if (*end != '\0')
			errx(EX_USAGE, "not a number: %s", val);
		return (CFNumberCreate(NULL, kCFNumberDoubleType, &d));
	}
	if (strcmp(type, "-date") == 0) {
		struct tm tm;

		memset(&tm, 0, sizeof(tm));
		if (strptime(val, "%Y-%m-%dT%H:%M:%SZ", &tm) == NULL &&
		    strptime(val, "%Y-%m-%d %H:%M:%S", &tm) == NULL)
			errx(EX_USAGE, "not a date (YYYY-MM-DDTHH:MM:SSZ): %s",
			    val);
		return (CFDateCreate(NULL, (CFAbsoluteTime)timegm(&tm) -
		    kCFAbsoluteTimeIntervalSince1970));
	}
	if (strcmp(type, "-data") == 0)
		return (base64_decode(val));
	if (strcmp(type, "-xml") == 0) {
		CFDataRef d = CFDataCreate(NULL, (const UInt8 *)val,
		    (CFIndex)strlen(val));
		CFErrorRef error = NULL;
		CFTypeRef v = CFPropertyListCreateWithData(NULL, d,
		    kCFPropertyListMutableContainersAndLeaves, NULL, &error);

		CFRelease(d);
		if (v == NULL)
			errx(EX_USAGE, "not a property list: %s", val);
		return (v);
	}
	if (strcmp(type, "-json") == 0) {
		const char *why;
		CFTypeRef v = json_parse(val, strlen(val), &why);

		if (v == NULL)
			errx(EX_USAGE, "not JSON (%s): %s", why, val);
		return (v);
	}
	errx(EX_USAGE, "unknown type: %s", type);
}

static void
print_raw(CFTypeRef v)
{
	CFTypeID t = CFGetTypeID(v);

	if (t == CFStringGetTypeID()) {
		char *c = cstr(v);

		puts(c);
		free(c);
	} else if (t == CFBooleanGetTypeID() || t == CFNumberGetTypeID() ||
	    t == CFDateGetTypeID()) {
		pprint(v, 0);
		putchar('\n');
	} else if (t == CFDataGetTypeID()) {
		static const char tbl[] =
		    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
		    "0123456789+/";
		const UInt8 *p = CFDataGetBytePtr(v);
		CFIndex n = CFDataGetLength(v);

		for (CFIndex i = 0; i < n; i += 3) {
			unsigned w = (unsigned)p[i] << 16 |
			    (i + 1 < n ? (unsigned)p[i + 1] << 8 : 0) |
			    (i + 2 < n ? p[i + 2] : 0);

			putchar(tbl[w >> 18 & 63]);
			putchar(tbl[w >> 12 & 63]);
			putchar(i + 1 < n ? tbl[w >> 6 & 63] : '=');
			putchar(i + 2 < n ? tbl[w & 63] : '=');
		}
		putchar('\n');
	} else if (t == CFArrayGetTypeID())
		printf("%ld\n", (long)CFArrayGetCount(v));
	else if (t == CFDictionaryGetTypeID()) {
		CFIndex n = CFDictionaryGetCount(v);
		const void **keys = calloc((size_t)n + 1, sizeof(void *));

		CFDictionaryGetKeysAndValues(v, keys, NULL);
		qsort(keys, (size_t)n, sizeof(void *), compare_keys);
		for (CFIndex i = 0; i < n; i++) {
			char *k = cstr(keys[i]);

			puts(k);
			free(k);
		}
		free(keys);
	}
}

static const char *
type_name(CFTypeRef v)
{
	CFTypeID t = CFGetTypeID(v);

	if (t == CFStringGetTypeID())
		return ("string");
	if (t == CFBooleanGetTypeID())
		return ("bool");
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
 * main
 */

int
main(int argc, char *argv[])
{
	const char *command = NULL, *keypath = NULL, *out = NULL, *ext = NULL;
	const char *vtype = NULL, *vstr = NULL;
	enum fmt fmt = FMT_NONE;
	bool append = false;
	int i, nfiles = 0, status = 0;
	char **files;

	if ((files = calloc((size_t)argc, sizeof(char *))) == NULL)
		err(EX_OSERR, "calloc");
	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "--") == 0) {
			for (i++; i < argc; i++)
				files[nfiles++] = argv[i];
			break;
		}
		if (strcmp(a, "-help") == 0 || strcmp(a, "-h") == 0)
			usage();
		else if (strcmp(a, "-lint") == 0 || strcmp(a, "-p") == 0)
			command = a;
		else if (strcmp(a, "-convert") == 0 && i + 1 < argc) {
			command = a;
			fmt = parse_fmt(argv[++i]);
		} else if ((strcmp(a, "-insert") == 0 ||
		    strcmp(a, "-replace") == 0) && i + 1 < argc) {
			command = a;
			keypath = argv[++i];
			if (i + 1 < argc && (strcmp(argv[i + 1], "-array") == 0 ||
			    strcmp(argv[i + 1], "-dictionary") == 0))
				vtype = argv[++i];
			else if (i + 2 < argc) {
				vtype = argv[++i];
				vstr = argv[++i];
			} else
				usage();
		} else if (strcmp(a, "-remove") == 0 && i + 1 < argc) {
			command = a;
			keypath = argv[++i];
		} else if (strcmp(a, "-extract") == 0 && i + 2 < argc) {
			command = a;
			keypath = argv[++i];
			fmt = parse_fmt(argv[++i]);
		} else if (strcmp(a, "-type") == 0 && i + 1 < argc) {
			command = a;
			keypath = argv[++i];
		} else if (strcmp(a, "-append") == 0)
			append = true;
		else if (strcmp(a, "-o") == 0 && i + 1 < argc)
			out = argv[++i];
		else if (strcmp(a, "-e") == 0 && i + 1 < argc)
			ext = argv[++i];
		else if (strcmp(a, "-r") == 0)
			readable = true;
		else if (strcmp(a, "-s") == 0)
			silent = true;
		else if (a[0] == '-' && a[1] != '\0')
			errx(EX_USAGE, "unknown option: %s (try -help)", a);
		else
			files[nfiles++] = argv[i];
	}
	if (command == NULL || nfiles == 0)
		usage();
	if (out != NULL && nfiles > 1)
		errx(EX_USAGE, "-o works with one file only");

	for (int f = 0; f < nfiles; f++) {
		const char *path = files[f];
		CFPropertyListRef pl = load(path);
		char **parts = NULL;
		int nparts = 0;

		if (pl == NULL) {
			status = 1;
			continue;
		}
		if (keypath != NULL)
			nparts = split_keypath(keypath, &parts);

		if (strcmp(command, "-lint") == 0) {
			if (!silent)
				printf("%s: OK\n", path);
		} else if (strcmp(command, "-p") == 0) {
			pprint(pl, 0);
			putchar('\n');
		} else if (strcmp(command, "-type") == 0) {
			CFTypeRef v = lookup(pl, parts, nparts);

			if (v == NULL) {
				warnx("%s: no value at key path %s", path,
				    keypath);
				status = 1;
			} else
				puts(type_name(v));
		} else if (strcmp(command, "-extract") == 0) {
			CFTypeRef v = lookup(pl, parts, nparts);

			if (v == NULL) {
				warnx("%s: no value at key path %s", path,
				    keypath);
				status = 1;
			} else if (fmt == FMT_RAW)
				print_raw(v);
			else if (!save(out != NULL ? out : "-", v, fmt))
				status = 1;
		} else if (strcmp(command, "-convert") == 0) {
			char *dest = NULL;

			if (out != NULL)
				dest = strdup(out);
			else if (ext != NULL) {
				const char *dot = strrchr(path, '.');
				const char *slash = strrchr(path, '/');
				size_t base = (dot != NULL && (slash == NULL ||
				    dot > slash)) ? (size_t)(dot - path) :
				    strlen(path);

				if (asprintf(&dest, "%.*s.%s", (int)base, path,
				    ext) == -1)
					err(EX_OSERR, "asprintf");
			} else
				dest = strdup(path);
			if (!save(dest, pl, fmt))
				status = 1;
			free(dest);
		} else {
			/* -insert, -replace, -remove */
			enum edit op = strcmp(command, "-insert") == 0 ?
			    EDIT_INSERT : strcmp(command, "-replace") == 0 ?
			    EDIT_REPLACE : EDIT_REMOVE;
			CFTypeRef value = NULL;
			const char *why;
			CFDataRef raw;
			CFPropertyListFormat orig = kCFPropertyListXMLFormat_v1_0;

			if (op != EDIT_REMOVE) {
				if (strcmp(vtype, "-array") == 0)
					value = CFArrayCreateMutable(NULL, 0,
					    &kCFTypeArrayCallBacks);
				else if (strcmp(vtype, "-dictionary") == 0)
					value = CFDictionaryCreateMutable(NULL,
					    0, &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
				else
					value = make_value(vtype, vstr);
			}
			why = edit(pl, parts, nparts, op, value, append);
			if (value != NULL)
				CFRelease(value);
			if (why != NULL) {
				warnx("%s: %s", path, why);
				status = 1;
			} else {
				/* Keep the file's own format. */
				if ((raw = read_file(path)) != NULL) {
					CFPropertyListRef chk;
					CFErrorRef e = NULL;

					chk = CFPropertyListCreateWithData(NULL,
					    raw, 0, &orig, &e);
					if (chk != NULL)
						CFRelease(chk);
					else
						orig = (CFPropertyListFormat)-1;
					CFRelease(raw);
				}
				if (!save(out != NULL ? out : path, pl,
				    orig == kCFPropertyListBinaryFormat_v1_0 ?
				    FMT_BINARY : orig == (CFPropertyListFormat)-1 ?
				    FMT_JSON : FMT_XML))
					status = 1;
			}
		}
		for (int k = 0; k < nparts; k++)
			free(parts[k]);
		free(parts);
		CFRelease(pl);
	}
	free(files);
	return (status);
}

static void
usage(void)
{

	fprintf(stderr,
"usage: plutil <command> [options] file...\n"
"\n"
"  -lint                         check that files are property lists\n"
"  -p                            print in a readable form\n"
"  -convert xml1|binary1|json    rewrite in that format\n"
"  -insert keypath -type value   add a value (-append adds to an array)\n"
"  -replace keypath -type value  set a value\n"
"  -remove keypath               delete a value\n"
"  -extract keypath fmt          write a value (fmt: xml1 binary1 json raw)\n"
"  -type keypath                 print the type of a value\n"
"\n"
"Options: -o path (output), -e ext (output extension), -r (readable\n"
"JSON), -s (silent). Types: -bool -integer -float -string -date -data\n"
"-xml -json -array -dictionary. '-' is standard input/output.\n");
	exit(EX_USAGE);
}
