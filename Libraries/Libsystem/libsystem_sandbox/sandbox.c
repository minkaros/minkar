/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The sandbox calls: a profile compiled here (sbpl.c) and given to the
 * kernel's sandbox policy, MinkarSandbox, which enforces it; questions
 * about a process's sandbox asked of the policy too.
 */

#include "sandbox.h"
#include "sbpl.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int __mac_syscall(const char *policy, int call, void *arg);

const char kSBXProfileNoInternet[] = "no-internet";
const char kSBXProfileNoNetwork[] = "no-network";
const char kSBXProfileNoWrite[] = "no-write";
const char kSBXProfileNoWriteExceptTemporary[] = "no-write-except-temporary";
const char kSBXProfilePureComputation[] = "pure-computation";

static const struct {
	const char *name;
	const char *profile;
} builtin_profiles[] = {
	{ kSBXProfileNoInternet,
	  "(version 1)\n(allow default)\n(deny network-outbound (remote ip \"*:*\"))\n"
	  "(deny network-inbound (local ip \"*:*\"))\n(deny network-bind (local ip \"*:*\"))\n" },
	{ kSBXProfileNoNetwork,
	  "(version 1)\n(allow default)\n(deny network*)\n" },
	{ kSBXProfileNoWrite,
	  "(version 1)\n(allow default)\n(deny file-write*)\n"
	  "(allow file-write-data (literal \"/dev/null\") (literal \"/dev/zero\") (literal \"/dev/dtracehelper\"))\n" },
	{ kSBXProfileNoWriteExceptTemporary,
	  "(version 1)\n(allow default)\n(deny file-write*)\n"
	  "(allow file-write* (subpath \"/private/tmp\") (subpath \"/private/var/tmp\") (subpath \"/private/var/folders\"))\n"
	  "(allow file-write-data (literal \"/dev/null\") (literal \"/dev/zero\") (literal \"/dev/dtracehelper\"))\n" },
	{ kSBXProfilePureComputation,
	  "(version 1)\n(deny default)\n(allow sysctl-read)\n"
	  "(allow file-read* file-map-executable (subpath \"/usr/lib\") (subpath \"/System/Library/Frameworks\"))\n"
	  "(allow file-read-data file-write-data (literal \"/dev/null\"))\n" },
};

static char *error_copy(const char *message) {
	return strdup(message != NULL ? message : "sandbox error");
}

static int set_error(char **errorbuf, const char *fmt, ...) {
	if (errorbuf != NULL) {
		char buf[512];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		*errorbuf = error_copy(buf);
	}
	return -1;
}

static char *read_file(const char *path) {
	FILE *f = fopen(path, "r");
	if (f == NULL)
		return NULL;
	char *buf = NULL;
	size_t n = 0, cap = 0;
	for (;;) {
		if (n + 4096 + 1 > cap) {
			cap = cap ? cap * 2 : 8192;
			if (cap > SB_MAX_BLOB * 4) {
				free(buf);
				fclose(f);
				return NULL;
			}
			char *b = realloc(buf, cap);
			if (b == NULL) {
				free(buf);
				fclose(f);
				return NULL;
			}
			buf = b;
		}
		size_t got = fread(buf + n, 1, 4096, f);
		n += got;
		if (got < 4096)
			break;
	}
	fclose(f);
	buf[n] = '\0';
	return buf;
}

/* A named profile's text (free it): built in, or a file of that name. */
static char *named_profile(const char *name, uint64_t flags) {
	if ((flags & SANDBOX_NAMED_MASK) == SANDBOX_NAMED_EXTERNAL)
		return read_file(name);
	for (size_t i = 0; i < sizeof(builtin_profiles) / sizeof(builtin_profiles[0]); i++)
		if (strcmp(builtin_profiles[i].name, name) == 0)
			return strdup(builtin_profiles[i].profile);
	if (strchr(name, '/') != NULL)
		return NULL;
	static const char *const dirs[] = { "/System/Library/Sandbox/Profiles", "/usr/share/sandbox" };
	for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		char path[1024];
		snprintf(path, sizeof(path), "%s/%s.sb", dirs[i], name);
		char *text = read_file(path);
		if (text != NULL)
			return text;
	}
	return NULL;
}

int sandbox_init_with_parameters(const char *profile, uint64_t flags, const char *const parameters[],
    char **errorbuf) {
	if (errorbuf != NULL)
		*errorbuf = NULL;
	if (profile == NULL)
		return set_error(errorbuf, "no profile");
	char *text = NULL;
	const char *source = profile;
	if (flags & SANDBOX_NAMED_MASK) {
		text = named_profile(profile, flags);
		if (text == NULL)
			return set_error(errorbuf, "%s: no such profile", profile);
		source = text;
	}
	char error[256] = "";
	size_t size = 0;
	uint8_t *blob = sbpl_compile(source, parameters, &size, error, sizeof(error));
	free(text);
	if (blob == NULL)
		return set_error(errorbuf, "%s", error);
	struct sb_set_profile_args args;
	args.blob = (uint64_t)(uintptr_t)blob;
	args.size = size;
	int r = __mac_syscall(SB_POLICY_NAME, SB_CALL_SET_PROFILE, &args);
	int saved = errno;
	free(blob);
	if (r != 0) {
		if (saved == ENOENT || saved == ENOSYS || saved == EINVAL)
			return set_error(errorbuf, "the system's sandbox policy (%s) is not loaded", SB_POLICY_NAME);
		return set_error(errorbuf, "the sandbox could not be applied: %s", strerror(saved));
	}
	return 0;
}

int sandbox_init(const char *profile, uint64_t flags, char **errorbuf) {
	return sandbox_init_with_parameters(profile, flags, NULL, errorbuf);
}

void sandbox_free_error(char *errorbuf) {
	free(errorbuf);
}

int sandbox_check(pid_t pid, const char *operation, enum sandbox_filter_type type, ...) {
	const char *name = NULL;
	int kind = -1;
	int base = (int)type & ~SANDBOX_CHECK_NO_REPORT;
	if (base != SANDBOX_FILTER_NONE) {
		va_list ap;
		va_start(ap, type);
		name = va_arg(ap, const char *);
		va_end(ap);
	}
	switch (base) {
	case SANDBOX_FILTER_PATH:
		kind = 0;
		break;
	case SANDBOX_FILTER_GLOBAL_NAME:
		kind = SBN_GLOBAL_NAME;
		break;
	case SANDBOX_FILTER_LOCAL_NAME:
		kind = SBN_LOCAL_NAME;
		break;
	case SANDBOX_FILTER_PREFERENCE_DOMAIN:
		kind = SBN_PREFERENCE_DOMAIN;
		break;
	default:
		/* An argument this system has no filter for: none is given. */
		kind = -1;
		name = NULL;
		break;
	}
	if (operation == NULL) {
		errno = EINVAL;
		return -1;
	}
	struct sb_check_args args;
	memset(&args, 0, sizeof(args));
	args.pid = pid;
	args.name_kind = kind;
	args.operation = (uint64_t)(uintptr_t)operation;
	args.name = (uint64_t)(uintptr_t)name;
	if (__mac_syscall(SB_POLICY_NAME, SB_CALL_CHECK, &args) != 0) {
		/* No policy loaded: no process is sandboxed. */
		if (errno == ENOENT || errno == ENOSYS || errno == EINVAL)
			return 0;
		return -1;
	}
	return args.sandboxed && args.result == SB_DENY ? 1 : 0;
}
