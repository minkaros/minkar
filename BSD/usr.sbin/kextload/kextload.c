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
 * kextload: load kernel extensions, given by path or (-b) by identifier,
 * through IOKit's OSKext functions, which link them and their
 * dependencies for the running kernel and start them. The extensions of
 * /System/Library/Extensions and /Library/Extensions are read first, so
 * identifiers and dependencies are found there.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/kext/OSKext.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* IOKit's private hook (kextd and macOS's kextload set it to record each
 * load); loading refuses to start without one. */
typedef Boolean (*OSKextLoadAuditFnPtr)(OSKextRef);
void _OSKextSetLoadAuditFunction(OSKextLoadAuditFnPtr);

static Boolean
audit(OSKextRef kext)
{
	(void)kext;
	return (true);
}

static void
read_extensions(void)
{
	static const char *dirs[] = {
		"/System/Library/Extensions", "/Library/Extensions"
	};
	/* Kept for the whole run: a kext is known by identifier, and its
	 * dependencies resolve, only while something holds it. */
	static CFArrayRef kexts[sizeof(dirs) / sizeof(dirs[0])];
	unsigned i;

	for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
		    (const UInt8 *)dirs[i], (CFIndex)strlen(dirs[i]), true);

		kexts[i] = OSKextCreateKextsFromURL(NULL, url);
		CFRelease(url);
	}
}

static int
load(OSKextRef kext, const char *name)
{
	OSReturn r;

	if (kext == NULL) {
		warnx("%s: no such kext", name);
		return (1);
	}
	/* IOKit reports a failure itself, with the reason. */
	r = OSKextLoad(kext);
	return (r == kOSReturnSuccess ? 0 : 1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: kextload [-qv] [-b bundle_id] ... [kext] ...\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	CFMutableArrayRef ids = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	int ch, failed = 0, i, verbose = 0;

	while ((ch = getopt(argc, argv, "b:qv")) != -1) {
		switch (ch) {
		case 'b': {
			CFStringRef s = CFStringCreateWithCString(NULL, optarg,
			    kCFStringEncodingUTF8);

			CFArrayAppendValue(ids, s);
			CFRelease(s);
			break;
		}
		case 'q':
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0 && CFArrayGetCount(ids) == 0)
		usage();

	_OSKextSetLoadAuditFunction(audit);
	/* The kernel warns of each dependency it already has loaded; only its
	 * errors are wanted unless -v. */
	if (!verbose)
		OSKextSetLogFilter(kOSKextLogErrorLevel |
		    kOSKextLogVerboseFlagsMask, true);
	read_extensions();
	for (i = 0; i < argc; i++) {
		CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
		    (const UInt8 *)argv[i], (CFIndex)strlen(argv[i]), true);
		OSKextRef kext = OSKextCreate(NULL, url);

		failed |= load(kext, argv[i]);
		if (kext != NULL)
			CFRelease(kext);
		CFRelease(url);
	}
	for (i = 0; i < CFArrayGetCount(ids); i++) {
		CFStringRef id = CFArrayGetValueAtIndex(ids, i);
		char name[256];

		CFStringGetCString(id, name, sizeof(name), kCFStringEncodingUTF8);
		failed |= load(OSKextGetKextWithIdentifier(id), name);
	}
	return (failed);
}
