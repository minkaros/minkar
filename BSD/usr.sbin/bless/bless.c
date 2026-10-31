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
 * bless: a volume's boot information and the firmware's boot device.
 *
 * An HFS+ volume keeps eight words of boot information in its header (its
 * "Finder info"): the blessed system folder, the blessed booter file, the
 * open-folder list, an alternate OS's folder, an unused word, the OS X
 * blessed folder (each a catalog ID), and the volume's 64-bit ID. They are
 * read and written with getattrlist and setattrlist of ATTR_CMN_FNDRINFO
 * with ATTR_VOL_INFO on the volume's mount point. --info shows them,
 * --folder and --file bless, --unbless takes a folder's blessing away.
 *
 * The firmware's boot device is the efi-boot-device NVRAM variable;
 * --getBoot reads it (and, without it, says the volume the system started
 * from). NVRAM variables can't be written on this system yet, so
 * --setBoot says so.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <sys/types.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <libkern/OSByteOrder.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

ssize_t fsgetpath(char *, size_t, fsid_t *, uint64_t);

struct bootinfo {
	uint32_t	length;
	uint32_t	words[8];	/* big-endian on the volume */
} __attribute__((packed));

static bool verbose, quiet;

/* ---- The volume's boot information ---------------------------------------- */

static void
volume_of(const char *path, struct statfs *sf)
{
	if (statfs(path, sf) != 0)
		err(1, "%s", path);
	if (strcmp(sf->f_fstypename, "hfs") != 0)
		errx(1, "%s: %s is not an HFS+ volume (%s)", path,
		    sf->f_mntonname, sf->f_fstypename);
}

static void
get_info(const char *mnt, uint32_t words[8])
{
	struct attrlist al = { .bitmapcount = ATTR_BIT_MAP_COUNT,
	    .commonattr = ATTR_CMN_FNDRINFO, .volattr = ATTR_VOL_INFO };
	struct bootinfo bi;
	int i;

	if (getattrlist(mnt, &al, &bi, sizeof(bi), 0) != 0)
		err(1, "%s: can't read the boot information", mnt);
	for (i = 0; i < 8; i++)
		words[i] = OSSwapBigToHostInt32(bi.words[i]);
}

static void
set_info(const char *mnt, const uint32_t words[8])
{
	struct attrlist al = { .bitmapcount = ATTR_BIT_MAP_COUNT,
	    .commonattr = ATTR_CMN_FNDRINFO, .volattr = ATTR_VOL_INFO };
	uint32_t be[8];
	int i;

	for (i = 0; i < 8; i++)
		be[i] = OSSwapHostToBigInt32(words[i]);
	if (setattrlist(mnt, &al, be, sizeof(be), 0) != 0)
		err(1, "%s: can't write the boot information", mnt);
}

/* A catalog ID's path, from the volume's root. */
static bool
id_path(const struct statfs *sf, uint32_t id, char *buf, size_t len)
{
	fsid_t fsid = sf->f_fsid;

	return (id != 0 && fsgetpath(buf, len, &fsid, id) > 0);
}

static int
info(const char *path)
{
	static const struct {
		const char	*is;
		const char	*none;
	} what[6] = {
		{ "Blessed System Folder is", "No Blessed System Folder" },
		{ "Blessed System File is", "No Blessed System File" },
		{ "Open-folder linked list starts with",
		    "Open-folder linked list empty" },
		{ "Alternate OS blessed file/folder is",
		    "No alternate OS blessed file/folder" },
		{ "Unused field set to", "Unused field unset" },
		{ "OS X blessed folder is", "No OS X blessed folder" },
	};
	struct statfs sf;
	uint32_t w[8];
	char p[PATH_MAX];
	int i;

	volume_of(path, &sf);
	get_info(sf.f_mntonname, w);
	for (i = 0; i < 6; i++) {
		if (w[i] == 0)
			printf("finderinfo[%d]: %6u => %s\n", i, w[i], what[i].none);
		else if (i != 4 && id_path(&sf, w[i], p, sizeof(p)))
			printf("finderinfo[%d]: %6u => %s %s\n", i, w[i],
			    what[i].is, p);
		else
			printf("finderinfo[%d]: %6u => %s %u\n", i, w[i],
			    what[i].is, w[i]);
	}
	printf("64-bit VSDB volume id:  0x%08X%08X\n", w[6], w[7]);
	return (0);
}

/* What a path is on its volume: its catalog ID (the inode number). */
static uint32_t
catalog_id(const char *path, bool dir, struct statfs *sf)
{
	struct stat st;

	if (stat(path, &st) != 0)
		err(1, "%s", path);
	if (dir != S_ISDIR(st.st_mode))
		errx(1, "%s: not a %s", path, dir ? "folder" : "file");
	volume_of(path, sf);
	if (st.st_ino > UINT32_MAX)
		errx(1, "%s: its ID does not fit the boot information", path);
	return ((uint32_t)st.st_ino);
}

static int
bless(const char *folder, const char *file)
{
	struct statfs sf, sf2;
	uint32_t w[8], dir = 0, fid = 0;

	if (getuid() != 0 && geteuid() != 0)
		errx(1, "blessing a volume needs root");
	if (folder != NULL)
		dir = catalog_id(folder, true, &sf);
	if (file != NULL) {
		fid = catalog_id(file, false, folder ? &sf2 : &sf);
		if (folder != NULL && sf.f_fsid.val[0] != sf2.f_fsid.val[0])
			errx(1, "%s and %s are on different volumes", folder,
			    file);
	}
	get_info(sf.f_mntonname, w);
	if (folder != NULL)
		w[0] = w[5] = dir;
	if (file != NULL)
		w[1] = fid;
	set_info(sf.f_mntonname, w);
	if (verbose)
		printf("Blessed %s%s%s on %s\n", folder ? folder : "",
		    folder && file ? " and " : "", file ? file : "",
		    sf.f_mntonname);
	return (0);
}

static int
unbless(const char *folder)
{
	struct statfs sf;
	uint32_t w[8], dir;
	int i;

	if (getuid() != 0 && geteuid() != 0)
		errx(1, "unblessing a volume needs root");
	dir = catalog_id(folder, true, &sf);
	get_info(sf.f_mntonname, w);
	for (i = 0; i < 6; i++)
		if (i != 4 && w[i] == dir)
			w[i] = 0;
	/* The booter goes with its folder. */
	if (w[1] != 0) {
		char fp[PATH_MAX], dp[PATH_MAX];
		size_t n;

		if (id_path(&sf, w[1], fp, sizeof(fp)) &&
		    id_path(&sf, dir, dp, sizeof(dp)) &&
		    strncmp(fp, dp, (n = strlen(dp))) == 0 && fp[n] == '/')
			w[1] = 0;
	}
	set_info(sf.f_mntonname, w);
	return (0);
}

/* ---- The firmware's boot device ------------------------------------------- */

/* The NVRAM variable, as a string; NULL when it is not set. */
static char *
nvram_string(const char *name)
{
	io_registry_entry_t options = IORegistryEntryFromPath(
	    kIOMasterPortDefault, "IODeviceTree:/options");
	CFStringRef key;
	CFTypeRef v;
	char *s = NULL;

	if (options == MACH_PORT_NULL)
		return (NULL);
	key = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
	v = IORegistryEntryCreateCFProperty(options, key, NULL, 0);
	CFRelease(key);
	IOObjectRelease(options);
	if (v == NULL)
		return (NULL);
	if (CFGetTypeID(v) == CFDataGetTypeID()) {
		size_t n = (size_t)CFDataGetLength(v);

		if ((s = malloc(n + 1)) != NULL) {
			memcpy(s, CFDataGetBytePtr(v), n);
			s[n] = '\0';
		}
	} else if (CFGetTypeID(v) == CFStringGetTypeID()) {
		CFIndex n = CFStringGetMaximumSizeForEncoding(CFStringGetLength(v),
		    kCFStringEncodingUTF8) + 1;

		if ((s = malloc((size_t)n)) != NULL)
			CFStringGetCString(v, s, n, kCFStringEncodingUTF8);
	}
	CFRelease(v);
	return (s);
}

/* The BSD name in efi-boot-device's IOMatch, or the media its UUID names. */
static bool
boot_device_name(const char *xml, char *buf, size_t len)
{
	CFDataRef data = CFDataCreate(NULL, (const UInt8 *)xml,
	    (CFIndex)strlen(xml));
	CFPropertyListRef pl = CFPropertyListCreateWithData(NULL, data, 0, NULL,
	    NULL);
	CFTypeRef match = NULL, bsd = NULL, uuid = NULL, props = NULL;
	bool ok = false;

	CFRelease(data);
	if (pl == NULL)
		return (false);
	if (CFGetTypeID(pl) == CFArrayGetTypeID() && CFArrayGetCount(pl) > 0)
		match = CFArrayGetValueAtIndex(pl, 0);
	else if (CFGetTypeID(pl) == CFDictionaryGetTypeID())
		match = pl;
	if (match != NULL && CFGetTypeID(match) == CFDictionaryGetTypeID())
		match = CFDictionaryGetValue(match, CFSTR("IOMatch"));
	if (match != NULL && CFGetTypeID(match) == CFDictionaryGetTypeID()) {
		bsd = CFDictionaryGetValue(match, CFSTR("BSD Name"));
		props = CFDictionaryGetValue(match, CFSTR("IOPropertyMatch"));
		if (props != NULL && CFGetTypeID(props) == CFDictionaryGetTypeID())
			uuid = CFDictionaryGetValue(props, CFSTR("UUID"));
	}
	if (bsd != NULL && CFGetTypeID(bsd) == CFStringGetTypeID()) {
		ok = CFStringGetCString(bsd, buf, (CFIndex)len,
		    kCFStringEncodingUTF8);
	} else if (uuid != NULL && CFGetTypeID(uuid) == CFStringGetTypeID()) {
		CFMutableDictionaryRef m = IOServiceMatching("IOMedia");
		CFMutableDictionaryRef pm = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		io_service_t media;

		CFDictionarySetValue(pm, CFSTR("UUID"), uuid);
		CFDictionarySetValue(m, CFSTR(kIOPropertyMatchKey), pm);
		CFRelease(pm);
		media = IOServiceGetMatchingService(kIOMasterPortDefault, m);
		if (media != MACH_PORT_NULL) {
			CFTypeRef n = IORegistryEntryCreateCFProperty(media,
			    CFSTR("BSD Name"), NULL, 0);

			if (n != NULL) {
				ok = CFStringGetCString(n, buf, (CFIndex)len,
				    kCFStringEncodingUTF8);
				CFRelease(n);
			}
			IOObjectRelease(media);
		}
	}
	CFRelease(pl);
	return (ok);
}

static int
get_boot(void)
{
	char *xml = nvram_string("efi-boot-device"), name[64];
	struct statfs sf;

	if (xml != NULL && boot_device_name(xml, name, sizeof(name))) {
		printf("/dev/%s\n", name);
		free(xml);
		return (0);
	}
	free(xml);
	/* No boot device set: the firmware started this system's volume. */
	if (statfs("/", &sf) != 0)
		err(1, "/");
	printf("%s\n", sf.f_mntfromname);
	return (0);
}

static int
set_boot(void)
{
	fprintf(stderr, "bless: can't set the firmware's boot device: NVRAM "
	    "variables can't be written on this system yet\n");
	return (3);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: bless [--info] [mountpoint]\n"
	    "       bless --folder directory [--file file] [--setBoot]\n"
	    "       bless --mount mountpoint | --device device --setBoot\n"
	    "       bless --unbless directory\n"
	    "       bless --getBoot\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	enum { O_INFO = 1, O_FOLDER, O_FILE, O_MOUNT, O_DEVICE, O_SETBOOT,
	    O_GETBOOT, O_UNBLESS, O_VERBOSE, O_QUIET, O_IGNORED, O_IGNORED_ARG };
	static const struct option longopts[] = {
		{ "info",	optional_argument,	NULL, O_INFO },
		{ "folder",	required_argument,	NULL, O_FOLDER },
		{ "file",	required_argument,	NULL, O_FILE },
		{ "mount",	required_argument,	NULL, O_MOUNT },
		{ "device",	required_argument,	NULL, O_DEVICE },
		{ "setBoot",	no_argument,		NULL, O_SETBOOT },
		{ "getBoot",	no_argument,		NULL, O_GETBOOT },
		{ "unbless",	required_argument,	NULL, O_UNBLESS },
		{ "verbose",	no_argument,		NULL, O_VERBOSE },
		{ "quiet",	no_argument,		NULL, O_QUIET },
		{ "nextonly",	no_argument,		NULL, O_IGNORED },
		{ "legacy",	no_argument,		NULL, O_IGNORED },
		{ "label",	required_argument,	NULL, O_IGNORED_ARG },
		{ "bootefi",	optional_argument,	NULL, O_IGNORED_ARG },
		{ NULL, 0, NULL, 0 }
	};
	const char *folder = NULL, *file = NULL, *unblessed = NULL;
	const char *info_path = NULL;
	bool do_info = false, setboot = false, getboot = false, target = false;
	int ch, rc = 0;

	while ((ch = getopt_long_only(argc, argv, "", longopts, NULL)) != -1) {
		switch (ch) {
		case O_INFO:
			do_info = true;
			info_path = optarg;
			break;
		case O_FOLDER:
			folder = optarg;
			break;
		case O_FILE:
			file = optarg;
			break;
		case O_MOUNT:
		case O_DEVICE:
			target = true;
			break;
		case O_SETBOOT:
			setboot = true;
			break;
		case O_GETBOOT:
			getboot = true;
			break;
		case O_UNBLESS:
			unblessed = optarg;
			break;
		case O_VERBOSE:
			verbose = true;
			break;
		case O_QUIET:
			quiet = true;
			break;
		case O_IGNORED:
		case O_IGNORED_ARG:
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (do_info && info_path == NULL && argc > 0)
		info_path = argv[0];
	if (getboot)
		return (get_boot());
	if (do_info)
		return (info(info_path != NULL ? info_path : "/"));
	if (unblessed != NULL)
		return (unbless(unblessed));
	if (folder != NULL || file != NULL)
		rc = bless(folder, file);
	else if (!target && !setboot)
		usage();
	if (rc == 0 && setboot)
		rc = set_boot();
	return (rc);
}
