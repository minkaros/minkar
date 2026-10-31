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
 * kextunload: unload kernel extensions, given by path or (-b) by
 * identifier, through IOKit's OSKext functions: their services are
 * terminated and their personalities removed first, as on macOS.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/kext/OSKext.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int
unload(CFStringRef id, const char *name)
{
	OSReturn r;

	if (id == NULL) {
		warnx("%s: not a kext", name);
		return (1);
	}
	/* IOKit reports a failure itself, with the kernel's reason. */
	r = OSKextUnloadKextWithIdentifier(id, true);
	return (r == kOSReturnSuccess ? 0 : 1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: kextunload [-qv] [-b bundle_id] ... [kext] ...\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	CFMutableArrayRef ids = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	int ch, failed = 0, i;

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
		case 'v':
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0 && CFArrayGetCount(ids) == 0)
		usage();

	for (i = 0; i < argc; i++) {
		CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
		    (const UInt8 *)argv[i], (CFIndex)strlen(argv[i]), true);
		OSKextRef kext = OSKextCreate(NULL, url);

		failed |= unload(kext ? OSKextGetIdentifier(kext) : NULL, argv[i]);
		if (kext != NULL)
			CFRelease(kext);
		CFRelease(url);
	}
	for (i = 0; i < CFArrayGetCount(ids); i++) {
		CFStringRef id = CFArrayGetValueAtIndex(ids, i);
		char name[256];

		CFStringGetCString(id, name, sizeof(name), kCFStringEncodingUTF8);
		failed |= unload(id, name);
	}
	return (failed);
}
