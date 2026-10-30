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
 * autodiskmount: mounts, through DiskArbitration, the volumes that are
 * not mounted, by the rules diskarbitrationd follows when a disk appears
 * (its "automatic" mount: the volumes it mounts on its own, where it
 * mounts them; an EFI partition stays unmounted). macOS keeps the command
 * for compatibility, with no options; -v tells what was mounted.
 */

#include <sys/param.h>

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>

static bool verbose;
static int pending;

static void
mounted(DADiskRef disk, DADissenterRef dissenter, void *context)
{
	char path[MAXPATHLEN];
	CFDictionaryRef d;
	CFURLRef url;

	(void)context;
	pending--;
	if (dissenter != NULL || !verbose)
		return;
	if ((d = DADiskCopyDescription(disk)) == NULL)
		return;
	url = CFDictionaryGetValue(d, kDADiskDescriptionVolumePathKey);
	if (url != NULL && CFURLGetFileSystemRepresentation(url, true,
	    (UInt8 *)path, sizeof(path)))
		printf("%s mounted at %s\n", DADiskGetBSDName(disk), path);
	CFRelease(d);
}

/* A volume diskarbitrationd can mount and has not. */
static bool
unmounted_volume(DADiskRef disk)
{
	CFDictionaryRef d = DADiskCopyDescription(disk);
	bool r;

	if (d == NULL)
		return (false);
	r = CFDictionaryGetValue(d, kDADiskDescriptionVolumeKindKey) != NULL &&
	    CFDictionaryGetValue(d, kDADiskDescriptionVolumeMountableKey) !=
	    kCFBooleanFalse &&
	    CFDictionaryGetValue(d, kDADiskDescriptionVolumePathKey) == NULL;
	CFRelease(d);
	return (r);
}

int
main(int argc, char **argv)
{
	CFStringRef args[] = { CFSTR("automatic"), NULL };
	DASessionRef session;
	io_iterator_t it;
	io_service_t m;
	int ch;

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
		case 'v':
			verbose = true;
			break;
		default:
			fprintf(stderr, "usage: autodiskmount [-v]\n");
			return (1);
		}
	}
	if ((session = DASessionCreate(kCFAllocatorDefault)) == NULL)
		errx(1, "unable to reach diskarbitrationd");
	DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(),
	    kCFRunLoopDefaultMode);
	if (IOServiceGetMatchingServices(kIOMasterPortDefault,
	    IOServiceMatching(kIOMediaClass), &it) != KERN_SUCCESS)
		errx(1, "unable to list the disks");
	while ((m = IOIteratorNext(it)) != IO_OBJECT_NULL) {
		DADiskRef disk = DADiskCreateFromIOMedia(kCFAllocatorDefault,
		    session, m);

		if (disk != NULL && unmounted_volume(disk)) {
			pending++;
			DADiskMountWithArguments(disk, NULL,
			    kDADiskMountOptionDefault, mounted, NULL, args);
		}
		if (disk != NULL)
			CFRelease(disk);
		IOObjectRelease(m);
	}
	IOObjectRelease(it);
	while (pending > 0)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, true);
	return (0);
}
