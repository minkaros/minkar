/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Checking a compiled profile (sbblob.h): its shape, and the action it
 * gives an operation. Used by libsystem_sandbox (sandbox_check, tests)
 * and by the kernel's sandbox policy; plain C, also valid C++, with no
 * allocation (the kernel passes a regex workspace in its query).
 */

#include "sbblob.h"

#ifdef KERNEL
#include <libkern/libkern.h>
#else
#include <string.h>
#endif

static uint32_t rd32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd16(const uint8_t *p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}

static const struct sb_header *header(const uint8_t *blob) {
	return (const struct sb_header *)(const void *)blob;
}

static const char *string_at(const uint8_t *blob, uint32_t offset) {
	return (const char *)blob + header(blob)->strings_offset + offset;
}

/* Walks one filter, checking it stays inside [p, end) and its strings are
   inside the string table; returns the byte after it, or NULL. */
static const uint8_t *check_filter(const uint8_t *p, const uint8_t *end, const uint8_t *blob, int depth) {
	const struct sb_header *h = header(blob);
	if (p >= end || depth > 64)
		return NULL;
	uint8_t type = *p++;
	switch (type) {
	case SBF_TRUE:
	case SBF_NEVER:
		return p;
	case SBF_ALL:
	case SBF_ANY: {
		if (end - p < 2)
			return NULL;
		uint16_t n = rd16(p);
		p += 2;
		for (uint16_t i = 0; i < n && p != NULL; i++)
			p = check_filter(p, end, blob, depth + 1);
		return p;
	}
	case SBF_NOT:
		return check_filter(p, end, blob, depth + 1);
	case SBF_PATH:
	case SBF_ADDR:
	case SBF_NAME: {
		size_t operands = type == SBF_NAME ? 2 : 1;
		if ((size_t)(end - p) < operands + 4)
			return NULL;
		uint32_t s = rd32(p + operands);
		if (s >= h->size - h->strings_offset)
			return NULL;
		return p + operands + 4;
	}
	case SBF_INT:
		if (end - p < 5)
			return NULL;
		return p + 5;
	default:
		return NULL;
	}
}

int sb_blob_valid(const uint8_t *blob, size_t size) {
	if (blob == NULL || size < sizeof(struct sb_header) || size > SB_MAX_BLOB)
		return 0;
	const struct sb_header *h = header(blob);
	if (h->magic != SB_MAGIC || h->version != SB_VERSION || h->size != size)
		return 0;
	if (h->rules_offset < sizeof(struct sb_header) ||
	    h->rules_offset + (uint64_t)h->nrules * sizeof(struct sb_rule) > h->filters_offset ||
	    h->filters_offset > h->strings_offset || h->strings_offset >= size)
		return 0;
	/* The string table ends with a NUL, so every string in it does. */
	if (blob[size - 1] != '\0')
		return 0;
	const struct sb_rule *rules = (const struct sb_rule *)(const void *)(blob + h->rules_offset);
	for (uint32_t i = 0; i < h->nrules; i++) {
		if (rules[i].operation >= size - h->strings_offset)
			return 0;
		if (rules[i].filter != SB_NO_FILTER &&
		    (rules[i].filter >= h->strings_offset - h->filters_offset ||
		    check_filter(blob + h->filters_offset + rules[i].filter, blob + h->strings_offset, blob, 0) == NULL))
			return 0;
	}
	return 1;
}

/* An operation pattern ("file-read*", "default") covers an operation. */
static int operation_matches(const char *pattern, const char *operation) {
	size_t n = strlen(pattern);
	if (n > 0 && pattern[n - 1] == '*')
		return strncmp(pattern, operation, n - 1) == 0;
	return strcmp(pattern, operation) == 0;
}

static int string_matches(int match, const char *pattern, const char *subject, void *workspace) {
	if (subject == NULL)
		return 0;
	size_t n = strlen(pattern);
	switch (match) {
	case SBM_LITERAL:
		return strcmp(pattern, subject) == 0;
	case SBM_PREFIX:
		return strncmp(pattern, subject, n) == 0;
	case SBM_SUBPATH:
		/* The path itself or anything under it; an empty one is none. */
		if (n == 0)
			return 0;
		if (n > 1 && pattern[n - 1] == '/')
			n--;
		return strncmp(pattern, subject, n) == 0 && (subject[n] == '\0' || subject[n] == '/' || n == 1);
	case SBM_REGEX:
		if (workspace != NULL)
			return sb_regex_search_ws(pattern, subject, workspace) == 1;
#ifdef KERNEL
		return 0;	/* the kernel always passes one */
#else
		return sb_regex_search(pattern, subject) == 1;
#endif
	}
	return 0;
}

static const char *last_colon(const char *s) {
	const char *last = NULL;
	for (; *s != '\0'; s++)
		if (*s == ':')
			last = s;
	return last;
}

/* An address filter: "ip HOST:PORT" or "unix PATH", with '*' for any
   host or port; the query's address is "ip HOST:PORT" / "unix PATH". */
static int address_matches(const char *pattern, const char *address) {
	if (address == NULL)
		return 0;
	if (strncmp(pattern, "unix ", 5) == 0)
		return strncmp(address, "unix ", 5) == 0 && (strcmp(pattern + 5, "*") == 0 || strcmp(pattern + 5, address + 5) == 0);
	if (strncmp(pattern, "ip ", 3) != 0 || strncmp(address, "ip ", 3) != 0)
		return 0;
	const char *pc = last_colon(pattern + 3), *ac = last_colon(address + 3);
	if (pc == NULL || ac == NULL)
		return 0;
	size_t ph = (size_t)(pc - (pattern + 3)), ah = (size_t)(ac - (address + 3));
	int host = (ph == 1 && pattern[3] == '*') || (ph == ah && strncmp(pattern + 3, address + 3, ph) == 0) ||
	    (ph == 9 && strncmp(pattern + 3, "localhost", 9) == 0 &&
	    ((ah == 9 && strncmp(address + 3, "127.0.0.1", 9) == 0) || (ah == 3 && strncmp(address + 3, "::1", 3) == 0)));
	int port = strcmp(pc + 1, "*") == 0 || strcmp(pc + 1, ac + 1) == 0;
	return host && port;
}

struct eval {
	const uint8_t *blob;
	const struct sb_query *query;
	void *workspace;
};

static const uint8_t *skip_filter(const uint8_t *p) {
	uint8_t type = *p++;
	switch (type) {
	case SBF_ALL:
	case SBF_ANY: {
		uint16_t n = rd16(p);
		p += 2;
		for (uint16_t i = 0; i < n; i++)
			p = skip_filter(p);
		return p;
	}
	case SBF_NOT:
		return skip_filter(p);
	case SBF_PATH:
	case SBF_ADDR:
		return p + 5;
	case SBF_NAME:
		return p + 6;
	case SBF_INT:
		return p + 5;
	default:
		return p;
	}
}

static int filter_matches(struct eval *e, const uint8_t *p) {
	const struct sb_query *q = e->query;
	switch (*p++) {
	case SBF_TRUE:
		return 1;
	case SBF_NEVER:
		return 0;
	case SBF_ALL:
	case SBF_ANY: {
		int any = p[-1] == SBF_ANY;
		uint16_t n = rd16(p);
		p += 2;
		for (uint16_t i = 0; i < n; i++) {
			int m = filter_matches(e, p);
			if (any && m)
				return 1;
			if (!any && !m)
				return 0;
			p = skip_filter(p);
		}
		return !any;
	}
	case SBF_NOT:
		return !filter_matches(e, p);
	case SBF_PATH:
		return string_matches(p[0], string_at(e->blob, rd32(p + 1)), q->path, e->workspace);
	case SBF_NAME: {
		int kind = p[0];
		if (kind <= 0 || kind >= SBN_KINDS)
			return 0;
		return string_matches(p[1], string_at(e->blob, rd32(p + 2)), q->names[kind], e->workspace);
	}
	case SBF_INT: {
		int kind = p[0];
		uint32_t value = rd32(p + 1);
		if (kind == SBI_VNODE_TYPE)
			return q->vnode_type != 0 && (uint32_t)q->vnode_type == value;
		if (kind <= 0 || kind >= SBI_KINDS || !q->has_ints[kind])
			return 0;
		if (kind == SBI_FILE_MODE)
			return ((uint32_t)q->ints[kind] & value) == value;
		return (uint32_t)q->ints[kind] == value;
	}
	case SBF_ADDR:
		return address_matches(string_at(e->blob, rd32(p + 1)), p[0] == SBA_LOCAL ? q->local_addr : q->remote_addr);
	}
	return 0;
}

int sb_evaluate(const uint8_t *blob, const char *operation, const struct sb_query *query, int *flags) {
	const struct sb_header *h = header(blob);
	const struct sb_rule *rules = (const struct sb_rule *)(const void *)(blob + h->rules_offset);
	struct eval e;
	e.blob = blob;
	e.query = query;
	e.workspace = query->regex_workspace;
	/* The last rule that matches decides. */
	for (uint32_t i = h->nrules; i-- > 0;) {
		if (!operation_matches(string_at(blob, rules[i].operation), operation))
			continue;
		if (rules[i].filter != SB_NO_FILTER && !filter_matches(&e, blob + h->filters_offset + rules[i].filter))
			continue;
		if (flags != NULL)
			*flags = rules[i].flags;
		return rules[i].action;
	}
	if (flags != NULL)
		*flags = (int)h->default_flags;
	return (int)h->default_action;
}
