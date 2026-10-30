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
 * SMJobIsEnabled for the NFS daemons: a job is loaded when launchd has it
 * (LAUNCH_KEY_GETJOB), and disabled when launchd's overrides (what
 * "launchctl load -w" and "unload -w" write) or, without an override, its
 * own property list say Disabled.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <launch.h>
#include <stdio.h>
#include <string.h>

#include <ServiceManagement/ServiceManagement.h>

#define OVERRIDES	"/var/db/launchd/com.apple.launchd/overrides.plist"
#define DAEMONS		"/System/Library/LaunchDaemons"

const CFStringRef kSMDomainSystemLaunchd = CFSTR("kSMDomainSystemLaunchd");
const CFStringRef kSMDomainUserLaunchd = CFSTR("kSMDomainUserLaunchd");

static CFDictionaryRef
read_plist(const char *path)
{
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
	    (const UInt8 *)path, (CFIndex)strlen(path), false);
	CFReadStreamRef rs = CFReadStreamCreateWithFile(NULL, url);
	CFPropertyListRef pl = NULL;

	if (rs != NULL && CFReadStreamOpen(rs)) {
		pl = CFPropertyListCreateWithStream(NULL, rs, 0,
		    kCFPropertyListImmutable, NULL, NULL);
		CFReadStreamClose(rs);
	}
	if (rs != NULL)
		CFRelease(rs);
	CFRelease(url);
	if (pl != NULL && CFGetTypeID(pl) != CFDictionaryGetTypeID()) {
		CFRelease(pl);
		pl = NULL;
	}
	return (pl);
}

static bool
is_loaded(const char *label)
{
	launch_data_t msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY), resp;
	bool loaded = false;

	if (msg == NULL)
		return (false);
	launch_data_dict_insert(msg, launch_data_new_string(label),
	    LAUNCH_KEY_GETJOB);
	resp = launch_msg(msg);
	if (resp != NULL) {
		loaded = launch_data_get_type(resp) == LAUNCH_DATA_DICTIONARY;
		launch_data_free(resp);
	}
	launch_data_free(msg);
	return (loaded);
}

static bool
is_disabled(const char *label, CFStringRef cflabel)
{
	CFDictionaryRef overrides = read_plist(OVERRIDES), job;
	CFTypeRef v;
	char path[PATH_MAX];
	bool disabled = false, decided = false;

	if (overrides != NULL) {
		job = CFDictionaryGetValue(overrides, cflabel);
		if (job != NULL && CFGetTypeID(job) == CFDictionaryGetTypeID() &&
		    (v = CFDictionaryGetValue(job, CFSTR(LAUNCH_JOBKEY_DISABLED)))
		    != NULL && CFGetTypeID(v) == CFBooleanGetTypeID()) {
			disabled = CFBooleanGetValue(v);
			decided = true;
		}
		CFRelease(overrides);
	}
	if (decided)
		return (disabled);
	snprintf(path, sizeof(path), "%s/%s.plist", DAEMONS, label);
	if ((job = read_plist(path)) != NULL) {
		v = CFDictionaryGetValue(job, CFSTR(LAUNCH_JOBKEY_DISABLED));
		if (v != NULL && CFGetTypeID(v) == CFBooleanGetTypeID())
			disabled = CFBooleanGetValue(v);
		CFRelease(job);
	}
	return (disabled);
}

Boolean
SMJobIsEnabled(CFStringRef domain, CFStringRef label, Boolean *persistence)
{
	char name[256];
	bool loaded, enabled;

	(void)domain;
	if (!CFStringGetCString(label, name, sizeof(name),
	    kCFStringEncodingUTF8))
		return (false);
	loaded = is_loaded(name);
	enabled = !is_disabled(name, label);
	/* As macOS returns it: disabled == loaded XOR persistence. */
	if (persistence != NULL)
		*persistence = enabled ? loaded : !loaded;
	return (loaded);
}
