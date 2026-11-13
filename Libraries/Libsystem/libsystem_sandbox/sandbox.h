/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The sandbox: a process limits itself, from then on, to what a profile
 * allows (files, network, services). Profiles are written in SBPL; the
 * kernel's sandbox policy enforces them, for the process and what it
 * starts.
 */

#ifndef _SANDBOX_H_
#define _SANDBOX_H_

#include <sys/cdefs.h>
#include <sys/types.h>
#include <stdint.h>

__BEGIN_DECLS

/* sandbox_init's flags: the profile is a name (of a built-in profile, or
   of a file in /usr/share/sandbox or /System/Library/Sandbox/Profiles),
   or the profile's text (0). */
#define SANDBOX_NAMED		0x0001
#define SANDBOX_NAMED_BUILTIN	0x0002
#define SANDBOX_NAMED_EXTERNAL	0x0003
#define SANDBOX_NAMED_MASK	0x000f

/* The built-in profiles. */
extern const char kSBXProfileNoInternet[];
extern const char kSBXProfileNoNetwork[];
extern const char kSBXProfileNoWrite[];
extern const char kSBXProfileNoWriteExceptTemporary[];
extern const char kSBXProfilePureComputation[];

/* 0, or -1 with *errorbuf a message to free with sandbox_free_error. */
int sandbox_init(const char *profile, uint64_t flags, char **errorbuf);
/* parameters: key, value, ..., NULL: the profile's (param "KEY"). */
int sandbox_init_with_parameters(const char *profile, uint64_t flags, const char *const parameters[],
    char **errorbuf);
void sandbox_free_error(char *errorbuf);

/* What sandbox_check is asked about. */
enum sandbox_filter_type {
	SANDBOX_FILTER_NONE = 0,
	SANDBOX_FILTER_PATH = 1,
	SANDBOX_FILTER_GLOBAL_NAME = 2,
	SANDBOX_FILTER_LOCAL_NAME = 3,
	SANDBOX_FILTER_APPLEEVENT_DESTINATION = 4,
	SANDBOX_FILTER_RIGHT_NAME = 5,
	SANDBOX_FILTER_PREFERENCE_DOMAIN = 6,
	SANDBOX_FILTER_KEXT_BUNDLE_ID = 7,
	SANDBOX_FILTER_INFO_TYPE = 8,
	SANDBOX_FILTER_NOTIFICATION = 9,
};

/* Ored into the filter type: do not log a denial. */
#define SANDBOX_CHECK_NO_REPORT 0x40000000

/*
 * Whether the process pid would be denied an operation ("mach-lookup",
 * "file-read-data", ...) on a name or path (the argument after type, a
 * const char *, unless type is SANDBOX_FILTER_NONE): 1 denied, 0 allowed
 * (also for a process without a sandbox), -1 error (errno).
 */
int sandbox_check(pid_t pid, const char *operation, enum sandbox_filter_type type, ...);

__END_DECLS

#endif
