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
 * XML property lists as XPC objects (xpc_create_from_plist), and the
 * entitlements of a connection's other end, which the kernel keeps as an
 * XML property list (xpc_connection_copy_entitlement_value). libxpc
 * cannot use CoreFoundation, which is built on it.
 *
 * dict, array, key, string, integer, real, true, false, data (Base64) and
 * date (ISO 8601, UTC); entities and character references in text. Binary
 * property lists are not read.
 */

#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <time.h>
#include <xpc/xpc.h>
#include <xpc/launchd.h>
#include "xpc_internal.h"

/* libxpc's include path has the kernel's string.h ahead of libc's: its
 * own searches. */
static const char *
find(const char *p, const char *end, const char *what, size_t len)
{
	for (; end - p >= (ptrdiff_t)len; p++)
		if (memcmp(p, what, len) == 0)
			return (p);
	return (NULL);
}

static char *
copy(const char *s)
{
	size_t n = strlen(s) + 1;
	char *c = malloc(n);

	if (c != NULL)
		memcpy(c, s, n);
	return (c);
}

struct parser {
	const char	*p;
	const char	*end;
};

/* Skips white space, comments, processing instructions and the DOCTYPE. */
static void
skip(struct parser *ps)
{
	for (;;) {
		while (ps->p < ps->end && isspace((unsigned char)*ps->p))
			ps->p++;
		if (ps->end - ps->p >= 4 && memcmp(ps->p, "<!--", 4) == 0) {
			const char *e = find(ps->p + 4, ps->end, "-->", 3);
			ps->p = e != NULL ? e + 3 : ps->end;
		} else if (ps->end - ps->p >= 2 && (memcmp(ps->p, "<?", 2) == 0 ||
		    memcmp(ps->p, "<!", 2) == 0)) {
			const char *e = find(ps->p, ps->end, ">", 1);
			ps->p = e != NULL ? e + 1 : ps->end;
		} else
			return;
	}
}

/*
 * Reads a tag: its name (at most size - 1 bytes) and whether it closes
 * (</name>) or is empty (<name/>). Attributes are skipped.
 */
static bool
tag(struct parser *ps, char *name, size_t size, bool *closing, bool *empty)
{
	size_t n = 0;

	skip(ps);
	if (ps->p >= ps->end || *ps->p != '<')
		return (false);
	ps->p++;
	*closing = ps->p < ps->end && *ps->p == '/';
	if (*closing)
		ps->p++;
	while (ps->p < ps->end && (isalnum((unsigned char)*ps->p) || *ps->p == '-' ||
	    *ps->p == '_')) {
		if (n + 1 < size)
			name[n++] = *ps->p;
		ps->p++;
	}
	name[n] = '\0';
	const char *e = find(ps->p, ps->end, ">", 1);
	if (e == NULL || n == 0)
		return (false);
	*empty = e > ps->p && e[-1] == '/';
	ps->p = e + 1;
	return (true);
}

/* Appends code point c to out as UTF-8. */
static size_t
utf8(char *out, unsigned long c)
{
	if (c < 0x80) {
		out[0] = (char)c;
		return (1);
	}
	if (c < 0x800) {
		out[0] = (char)(0xC0 | (c >> 6));
		out[1] = (char)(0x80 | (c & 0x3F));
		return (2);
	}
	if (c < 0x10000) {
		out[0] = (char)(0xE0 | (c >> 12));
		out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
		out[2] = (char)(0x80 | (c & 0x3F));
		return (3);
	}
	out[0] = (char)(0xF0 | (c >> 18));
	out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
	out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
	out[3] = (char)(0x80 | (c & 0x3F));
	return (4);
}

/*
 * The text up to the closing tag </name>, entities decoded; NULL when the
 * tag is not there. The caller frees it.
 */
static char *
text(struct parser *ps, const char *name)
{
	char close[32];
	const char *e;
	char *out;
	size_t n = 0;

	snprintf(close, sizeof(close), "</%s>", name);
	e = find(ps->p, ps->end, close, strlen(close));
	if (e == NULL)
		return (NULL);
	/* Decoded text is never longer than its source. */
	out = malloc(e - ps->p + 1);
	if (out == NULL)
		return (NULL);
	while (ps->p < e) {
		if (*ps->p != '&') {
			out[n++] = *ps->p++;
			continue;
		}
		const char *semi = find(ps->p, e, ";", 1);
		size_t len = semi != NULL ? (size_t)(semi - ps->p) + 1 : 0;
		if (len == 4 && memcmp(ps->p, "&lt;", 4) == 0)
			out[n++] = '<';
		else if (len == 4 && memcmp(ps->p, "&gt;", 4) == 0)
			out[n++] = '>';
		else if (len == 5 && memcmp(ps->p, "&amp;", 5) == 0)
			out[n++] = '&';
		else if (len == 6 && memcmp(ps->p, "&quot;", 6) == 0)
			out[n++] = '"';
		else if (len == 6 && memcmp(ps->p, "&apos;", 6) == 0)
			out[n++] = '\'';
		else if (len > 3 && ps->p[1] == '#') {
			unsigned long c = ps->p[2] == 'x' ? strtoul(ps->p + 3, NULL, 16) :
			    strtoul(ps->p + 2, NULL, 10);
			/* "&#N;" is at least 4 bytes; UTF-8 of N at most 4. */
			if (c == 0 || c > 0x10FFFF || utf8(out + n, c) > len) {
				free(out);
				return (NULL);
			}
			n += utf8(out + n, c);
		} else {
			free(out);
			return (NULL);
		}
		ps->p += len;
	}
	out[n] = '\0';
	ps->p = e + strlen(close);
	return (out);
}

static int
base64(char c)
{
	if (c >= 'A' && c <= 'Z')
		return (c - 'A');
	if (c >= 'a' && c <= 'z')
		return (c - 'a' + 26);
	if (c >= '0' && c <= '9')
		return (c - '0' + 52);
	if (c == '+')
		return (62);
	if (c == '/')
		return (63);
	return (-1);
}

static xpc_object_t
data(const char *s)
{
	size_t len = strlen(s), n = 0;
	unsigned int bits = 0, count = 0;
	uint8_t *buf = malloc(len / 4 * 3 + 3);
	xpc_object_t obj;

	if (buf == NULL)
		return (NULL);
	for (; *s != '\0' && *s != '='; s++) {
		int v = base64(*s);
		if (v < 0) {
			if (isspace((unsigned char)*s))
				continue;
			free(buf);
			return (NULL);
		}
		bits = (bits << 6) | (unsigned int)v;
		if (++count == 4) {
			buf[n++] = (uint8_t)(bits >> 16);
			buf[n++] = (uint8_t)(bits >> 8);
			buf[n++] = (uint8_t)bits;
			bits = count = 0;
		}
	}
	if (count == 2)
		buf[n++] = (uint8_t)(bits >> 4);
	else if (count == 3) {
		buf[n++] = (uint8_t)(bits >> 10);
		buf[n++] = (uint8_t)(bits >> 2);
	}
	obj = xpc_data_create(buf, n);
	free(buf);
	return (obj);
}

/* 2026-11-03T12:00:00Z: nanoseconds since 1970, as XPC dates are. */
static xpc_object_t
date(const char *s)
{
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	if (sscanf(s, "%d-%d-%dT%d:%d:%dZ", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
	    &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6)
		return (NULL);
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	return (xpc_date_create((int64_t)timegm(&tm) * 1000000000LL));
}

static xpc_object_t value(struct parser *ps, int depth);

/* The items of a <dict> or <array>, up to its closing tag. */
static xpc_object_t
container(struct parser *ps, bool dict, int depth)
{
	xpc_object_t obj = dict ? xpc_dictionary_create(NULL, NULL, 0) :
	    xpc_array_create(NULL, 0);
	char name[16];
	bool closing, empty;

	for (;;) {
		const char *at = ps->p;
		if (!tag(ps, name, sizeof(name), &closing, &empty))
			goto fail;
		if (closing) {
			if (strcmp(name, dict ? "dict" : "array") != 0)
				goto fail;
			return (obj);
		}
		char *key = NULL;
		if (dict) {
			if (strcmp(name, "key") != 0)
				goto fail;
			key = empty ? copy("") : text(ps, "key");
			if (key == NULL)
				goto fail;
		} else
			ps->p = at;	/* the item's tag, read again by value() */
		xpc_object_t item = value(ps, depth + 1);
		if (item == NULL) {
			free(key);
			goto fail;
		}
		if (dict)
			xpc_dictionary_set_value(obj, key, item);
		else
			xpc_array_append_value(obj, item);
		xpc_release(item);
		free(key);
	}
fail:
	xpc_release(obj);
	return (NULL);
}

static xpc_object_t
value(struct parser *ps, int depth)
{
	char name[16];
	bool closing, empty;
	xpc_object_t obj = NULL;
	char *s;

	if (depth > 64 || !tag(ps, name, sizeof(name), &closing, &empty) || closing)
		return (NULL);
	if (strcmp(name, "true") == 0 || strcmp(name, "false") == 0) {
		if (!empty && !(tag(ps, name + 8, 8, &closing, &empty) && closing))
			return (NULL);
		return (xpc_bool_create(name[0] == 't'));
	}
	if (strcmp(name, "dict") == 0)
		return (empty ? xpc_dictionary_create(NULL, NULL, 0) : container(ps, true, depth));
	if (strcmp(name, "array") == 0)
		return (empty ? xpc_array_create(NULL, 0) : container(ps, false, depth));
	s = empty ? copy("") : text(ps, name);
	if (s == NULL)
		return (NULL);
	if (strcmp(name, "string") == 0)
		obj = xpc_string_create(s);
	else if (strcmp(name, "integer") == 0) {
		char *e;
		long long v = strtoll(s, &e, 0);
		while (isspace((unsigned char)*e))
			e++;
		if (*s != '\0' && *e == '\0')
			obj = xpc_int64_create(v);
	} else if (strcmp(name, "real") == 0)
		obj = xpc_double_create(strtod(s, NULL));
	else if (strcmp(name, "data") == 0)
		obj = data(s);
	else if (strcmp(name, "date") == 0)
		obj = date(s);
	free(s);
	return (obj);
}

xpc_object_t
xpc_create_from_plist(uint8_t *buf, size_t length)
{
	struct parser ps = { (const char *)buf, (const char *)buf + length };
	char name[16];
	bool closing, empty;
	xpc_object_t obj;

	if (buf == NULL || !tag(&ps, name, sizeof(name), &closing, &empty) ||
	    strcmp(name, "plist") != 0 || closing || empty)
		return (NULL);
	obj = value(&ps, 0);
	if (obj != NULL && !(tag(&ps, name, sizeof(name), &closing, &empty) && closing &&
	    strcmp(name, "plist") == 0)) {
		xpc_release(obj);
		return (NULL);
	}
	return (obj);
}

/* sys/codesign.h (private): the kernel's copy of a process's entitlements,
 * a blob header and an XML property list. */
#define CS_OPS_ENTITLEMENTS_BLOB	7
int csops_audittoken(pid_t pid, unsigned int ops, void *useraddr, size_t usersize,
    audit_token_t *token);
xpc_object_t xpc_connection_copy_entitlement_value(xpc_connection_t, const char *);

/*
 * The value of one entitlement of the process at the other end of the
 * connection (the sender of the message that made it, for a peer), as the
 * kernel validated it when the process started; NULL when it has none.
 */
xpc_object_t
xpc_connection_copy_entitlement_value(xpc_connection_t xconn, const char *entitlement)
{
	struct xpc_connection *conn = (struct xpc_connection *)xconn;
	struct { uint32_t magic, length; } header;
	audit_token_t token;
	xpc_object_t all, value = NULL;
	uint32_t length;
	char *blob;

	if (conn == NULL || entitlement == NULL)
		return (NULL);
	token = conn->xc_creds.xc_audit_token;
	pid_t pid = conn->xc_creds.xc_remote_pid;
	if (pid <= 0)
		return (NULL);
	/* The blob's size first (the header comes back with ERANGE), then all. */
	header.length = 0;
	csops_audittoken(pid, CS_OPS_ENTITLEMENTS_BLOB, &header, sizeof(header), &token);
	length = ntohl(header.length);
	if (length <= sizeof(header) || length > (1 << 20))
		return (NULL);
	blob = malloc(length);
	if (blob == NULL)
		return (NULL);
	if (csops_audittoken(pid, CS_OPS_ENTITLEMENTS_BLOB, blob, length, &token) == 0) {
		all = xpc_create_from_plist((uint8_t *)blob + sizeof(header),
		    length - sizeof(header));
		if (all != NULL && xpc_get_type(all) == XPC_TYPE_DICTIONARY) {
			value = xpc_dictionary_get_value(all, entitlement);
			if (value != NULL)
				xpc_retain(value);
		}
		if (all != NULL)
			xpc_release(all);
	}
	free(blob);
	return (value);
}
