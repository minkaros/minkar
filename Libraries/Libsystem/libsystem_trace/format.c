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
 * os_log messages: their text from the format and its arguments, which
 * come as a va_list (_os_log_internal) or encoded in a buffer
 * (_os_log_impl, as the compiler lays them out for os_log), and where the
 * text goes: syslogd, and stderr when OS_ACTIVITY_DT_MODE is set. What ASL
 * hands over (os_log_with_args, from syslog and asl_log) already went to
 * syslogd: only stderr.
 *
 * The format is printf's, with os_log's annotations: "%{public}s",
 * "%{private}@" and the like. In the system log, strings and objects are
 * shown only when marked public, as on the reference system; on stderr
 * everything is shown.
 */

#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <pthread.h>

#include "format.h"

/* The encoded buffer's items (the compiler's os_log layout). */
#define ITEM_SCALAR	0
#define ITEM_COUNT	1
#define ITEM_STRING	2
#define ITEM_POINTER	3
#define ITEM_OBJECT	4
#define ITEM_ERRNO	6
#define ITEM_PRIVATE	0x1
#define ITEM_PUBLIC	0x2

struct args {
	va_list *ap;			/* or: */
	const uint8_t *buf, *end;	/* the encoded items */
	unsigned left;
	int saved_errno;
};

struct item {
	int kind;
	int flags;
	uint64_t bits;			/* scalars, pointers */
	unsigned size;
};

static bool
next_item(struct args *a, struct item *it)
{
	if (a->left == 0 || a->buf + 2 > a->end)
		return false;
	it->kind = a->buf[0] >> 4;
	it->flags = a->buf[0] & 0xf;
	it->size = a->buf[1];
	a->buf += 2;
	if (a->buf + it->size > a->end)
		return false;
	it->bits = 0;
	memcpy(&it->bits, a->buf, it->size > 8 ? 8 : it->size);
	a->buf += it->size;
	a->left--;
	return true;
}

/* CoreFoundation, if the program has it: an object's description. */
typedef const void *(*copydesc_t)(const void *);
typedef bool (*getcstr_t)(const void *, char *, long, uint32_t);
typedef void (*release_t)(const void *);

static void
describe(const void *obj, char *out, size_t size)
{
	static copydesc_t copydesc;
	static getcstr_t getcstr;
	static release_t release;
	const void *desc;

	if (obj == NULL) {
		strlcpy(out, "(null)", size);
		return;
	}
	if (copydesc == NULL) {
		copydesc = (copydesc_t)dlsym(RTLD_DEFAULT, "CFCopyDescription");
		getcstr = (getcstr_t)dlsym(RTLD_DEFAULT, "CFStringGetCString");
		release = (release_t)dlsym(RTLD_DEFAULT, "CFRelease");
	}
	if (copydesc == NULL || getcstr == NULL || release == NULL ||
	    (desc = copydesc(obj)) == NULL) {
		snprintf(out, size, "<%p>", obj);
		return;
	}
	if (!getcstr(desc, out, (long)size, 0x08000100 /* UTF-8 */))
		snprintf(out, size, "<%p>", obj);
	release(desc);
}

struct out {
	char *p;
	size_t left;
};

static void
put(struct out *o, const char *s, size_t n)
{
	if (n >= o->left)
		n = o->left ? o->left - 1 : 0;
	memcpy(o->p, s, n);
	o->p += n;
	o->left -= n;
	*o->p = '\0';
}

/*
 * The text of a message. redact: strings and objects not marked public
 * become "<private>", as the system log shows them.
 */
static void
render(char *buf, size_t size, const char *fmt, struct args *a, bool redact)
{
	struct out o = { buf, size };
	char spec[64], piece[1024], ann[64];
	const char *f = fmt, *start;

	*buf = '\0';
	while (*f) {
		if (*f != '%') {
			start = f;
			while (*f && *f != '%')
				f++;
			put(&o, start, (size_t)(f - start));
			continue;
		}
		if (f[1] == '%') {
			put(&o, "%", 1);
			f += 2;
			continue;
		}
		/* The annotation, then printf's own spec. */
		f++;
		ann[0] = '\0';
		if (*f == '{') {
			const char *close = strchr(f, '}');
			size_t n;

			if (close == NULL)
				break;
			n = (size_t)(close - f - 1);
			if (n >= sizeof(ann))
				n = sizeof(ann) - 1;
			memcpy(ann, f + 1, n);
			ann[n] = '\0';
			f = close + 1;
		}
		size_t sl = 0;
		int stars = 0, longs = 0, wide = 0;
		spec[sl++] = '%';
		while (*f && strchr("-+ #0'", *f) && sl < sizeof(spec) - 8)
			spec[sl++] = *f++;
		while (*f && (strchr("0123456789.", *f) || *f == '*') &&
		    sl < sizeof(spec) - 8) {
			if (*f == '*')
				stars++;
			spec[sl++] = *f++;
		}
		while (*f && strchr("hlLqjzt", *f) && sl < sizeof(spec) - 8) {
			if (*f == 'l' || *f == 'q' || *f == 'j' || *f == 'z' ||
			    *f == 't')
				longs++;
			if (*f == 'L')
				wide = 1;
			if (*f == 'q') {
				spec[sl++] = 'l';
				spec[sl++] = 'l';
				f++;
				continue;
			}
			spec[sl++] = *f++;
		}
		char conv = *f ? *f++ : '\0';
		spec[sl++] = conv;
		spec[sl] = '\0';

		int counts[2] = { 0, 0 };
		for (int i = 0; i < stars && i < 2; i++) {
			if (a->ap)
				counts[i] = va_arg(*a->ap, int);
			else {
				struct item it;
				if (next_item(a, &it))
					counts[i] = (int)it.bits;
			}
		}

		bool private_item = strstr(ann, "private") != NULL;
		bool public_item = strstr(ann, "public") != NULL;
		piece[0] = '\0';
		switch (conv) {
		case 'd': case 'i': case 'u': case 'o': case 'x': case 'X':
		case 'c': {
			long long v = 0;
			struct item it;

			if (a->ap) {
				if (longs >= 1)
					v = va_arg(*a->ap, long long);
				else
					v = va_arg(*a->ap, int);
			} else if (next_item(a, &it)) {
				v = (long long)it.bits;
				if (it.size == 4)
					v = (conv == 'd' || conv == 'i') ?
					    (long long)(int32_t)it.bits :
					    (long long)(uint32_t)it.bits;
			}
			/* printf with a long long, whatever the length was */
			char s2[64];
			size_t k = 0, j;
			for (j = 0; j < sl - 1 && k < sizeof(s2) - 4; j++)
				if (!strchr("hlLqjzt", spec[j]))
					s2[k++] = spec[j];
			if (conv != 'c') {
				s2[k++] = 'l';
				s2[k++] = 'l';
			}
			s2[k++] = conv;
			s2[k] = '\0';
			if (stars == 2)
				snprintf(piece, sizeof(piece), s2, counts[0],
				    counts[1], v);
			else if (stars == 1)
				snprintf(piece, sizeof(piece), s2, counts[0], v);
			else
				snprintf(piece, sizeof(piece), s2, v);
			if (strstr(ann, "bool") != NULL)
				strlcpy(piece, v ? "true" : "false",
				    sizeof(piece));
			if (strstr(ann, "errno") != NULL)
				snprintf(piece, sizeof(piece), "[%lld: %s]", v,
				    strerror((int)v));
			if (redact && private_item)
				strlcpy(piece, "<private>", sizeof(piece));
			break;
		}
		case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
		case 'a': case 'A': {
			double d = 0;
			struct item it;

			if (a->ap) {
				if (wide)
					d = (double)va_arg(*a->ap, long double);
				else
					d = va_arg(*a->ap, double);
			} else if (next_item(a, &it))
				memcpy(&d, &it.bits, sizeof(d));
			char s2[64];
			size_t k = 0, j;
			for (j = 0; j < sl - 1 && k < sizeof(s2) - 2; j++)
				if (!strchr("hlLqjzt", spec[j]))
					s2[k++] = spec[j];
			s2[k++] = conv;
			s2[k] = '\0';
			if (stars == 2)
				snprintf(piece, sizeof(piece), s2, counts[0],
				    counts[1], d);
			else if (stars == 1)
				snprintf(piece, sizeof(piece), s2, counts[0], d);
			else
				snprintf(piece, sizeof(piece), s2, d);
			if (redact && private_item)
				strlcpy(piece, "<private>", sizeof(piece));
			break;
		}
		case 's': case 'p': case '@': case 'P': {
			const void *p = NULL;
			struct item it;

			if (a->ap)
				p = va_arg(*a->ap, const void *);
			else if (next_item(a, &it))
				p = (const void *)(uintptr_t)it.bits;
			if (conv == 's')
				snprintf(piece, sizeof(piece), "%s",
				    p ? (const char *)p : "(null)");
			else if (conv == '@')
				describe(p, piece, sizeof(piece));
			else if (conv == 'P')
				snprintf(piece, sizeof(piece), "<%d bytes>",
				    counts[stars > 1 ? 1 : 0]);
			else
				snprintf(piece, sizeof(piece), "%p", p);
			if (redact && (conv == 's' || conv == '@' ||
			    conv == 'P') && !public_item)
				strlcpy(piece, "<private>", sizeof(piece));
			break;
		}
		case 'm':
			strlcpy(piece, strerror(a->saved_errno), sizeof(piece));
			break;
		case 'n':
			if (a->ap)
				(void)va_arg(*a->ap, void *);
			break;
		default:
			put(&o, spec, sl);
			continue;
		}
		put(&o, piece, strlen(piece));
	}
}

static const char *
type_name(os_log_type_t type)
{
	switch (type) {
	case OS_LOG_TYPE_INFO:	return "Info";
	case OS_LOG_TYPE_DEBUG:	return "Debug";
	case OS_LOG_TYPE_ERROR:	return "Error";
	case OS_LOG_TYPE_FAULT:	return "Fault";
	default:		return "Default";
	}
}

static int
syslog_level(os_log_type_t type)
{
	switch (type) {
	case OS_LOG_TYPE_INFO:	return LOG_INFO;
	case OS_LOG_TYPE_DEBUG:	return LOG_DEBUG;
	case OS_LOG_TYPE_ERROR:	return LOG_ERR;
	case OS_LOG_TYPE_FAULT:	return LOG_CRIT;
	default:		return LOG_NOTICE;
	}
}

static bool
dt_mode(void)
{
	const char *v = getenv("OS_ACTIVITY_DT_MODE");

	return v != NULL && *v != '\0' && strcmp(v, "0") != 0 &&
	    strcasecmp(v, "NO") != 0;
}

/*
 * One message out: to syslogd (info and debug are not kept, as on the
 * reference system), and to stderr in DT mode.
 */
/* Set while a message goes out: syslog may call os_log back (ASL does). */
static __thread int emitting;

static void
emit(const char *subsystem, const char *category, os_log_type_t type,
    const char *fmt, struct args *a, struct args *again, bool to_syslog)
{
	char text[2048], scope[256] = "";

	if (emitting)
		return;
	emitting = 1;

	if (subsystem != NULL && *subsystem != '\0')
		snprintf(scope, sizeof(scope), "[%s:%s] ", subsystem,
		    category ? category : "");
	if (to_syslog && type != OS_LOG_TYPE_INFO && type != OS_LOG_TYPE_DEBUG) {
		render(text, sizeof(text), fmt, a, true);
		syslog(syslog_level(type), "%s%s", scope, text);
	}
	if (dt_mode()) {
		uint64_t tid = 0;

		pthread_threadid_np(NULL, &tid);
		render(text, sizeof(text), fmt, again, false);
		fprintf(stderr, "%s[%d:%llx] (%s) %s%s\n", getprogname(),
		    (int)getpid(), (unsigned long long)tid, type_name(type),
		    scope, text);
	}
	emitting = 0;
}

void
_os_log_emit_va(const char *subsystem, const char *category,
    os_log_type_t type, const char *fmt, va_list ap, bool to_syslog)
{
	va_list ap1, ap2;
	struct args a = { 0 }, b = { 0 };

	if (fmt == NULL)
		return;
	va_copy(ap1, ap);
	va_copy(ap2, ap);
	a.ap = &ap1;
	b.ap = &ap2;
	a.saved_errno = b.saved_errno = errno;
	emit(subsystem, category, type, fmt, &a, &b, to_syslog);
	va_end(ap1);
	va_end(ap2);
}

void
_os_log_emit_buffer(const char *subsystem, const char *category,
    os_log_type_t type, const char *fmt, const uint8_t *buf, uint32_t size)
{
	struct args a = { 0 }, b;

	if (fmt == NULL)
		return;
	a.saved_errno = errno;
	if (buf != NULL && size >= 2) {
		a.buf = buf + 2;
		a.end = buf + size;
		a.left = buf[1];
	}
	b = a;
	emit(subsystem, category, type, fmt, &a, &b, true);
}
