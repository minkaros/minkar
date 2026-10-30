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
 * diskutil: disks, partitions and volumes, through DiskArbitration (as
 * on macOS: diskarbitrationd mounts, unmounts, ejects and renames, and
 * describes each disk) and the file system bundles in
 * /System/Library/Filesystems (to verify, repair and erase).
 *
 *	diskutil list [disk]
 *	diskutil info [-plist] disk | mount point
 *	diskutil mount [readOnly] [-mountPoint path] disk
 *	diskutil mountDisk disk
 *	diskutil unmount [force] disk | mount point
 *	diskutil unmountDisk [force] disk
 *	diskutil eject disk
 *	diskutil rename disk name
 *	diskutil verifyVolume | repairVolume disk | mount point
 *	diskutil eraseVolume format name disk
 *	diskutil listFilesystems
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <spawn.h>
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

extern char **environ;

static DASessionRef session;

/* ---- helpers --------------------------------------------------------------- */

static void
usage(void)
{
	fprintf(stderr,
	    "usage: diskutil list [disk]\n"
	    "       diskutil info [-plist] disk | mountPoint\n"
	    "       diskutil mount [readOnly] [-mountPoint path] disk\n"
	    "       diskutil mountDisk disk\n"
	    "       diskutil unmount [force] disk | mountPoint\n"
	    "       diskutil unmountDisk [force] disk\n"
	    "       diskutil eject disk\n"
	    "       diskutil rename disk name\n"
	    "       diskutil verifyVolume | repairVolume disk | mountPoint\n"
	    "       diskutil eraseVolume format name disk\n"
	    "       diskutil listFilesystems\n");
	exit(1);
}

static bool
cfstr(CFTypeRef s, char *buf, size_t len)
{
	buf[0] = '\0';
	if (s == NULL || CFGetTypeID(s) != CFStringGetTypeID())
		return (false);
	return (CFStringGetCString(s, buf, (CFIndex)len, kCFStringEncodingUTF8));
}

static bool
cfbool(CFDictionaryRef d, CFStringRef key)
{
	return (CFDictionaryGetValue(d, key) == kCFBooleanTrue);
}

static uint64_t
cfnum(CFDictionaryRef d, CFStringRef key)
{
	CFTypeRef n = CFDictionaryGetValue(d, key);
	int64_t v = 0;

	if (n != NULL && CFGetTypeID(n) == CFNumberGetTypeID())
		CFNumberGetValue(n, kCFNumberSInt64Type, &v);
	return ((uint64_t)v);
}

static bool
cfpath(CFDictionaryRef d, CFStringRef key, char *buf, size_t len)
{
	CFTypeRef u = CFDictionaryGetValue(d, key);

	buf[0] = '\0';
	if (u == NULL || CFGetTypeID(u) != CFURLGetTypeID())
		return (false);
	return (CFURLGetFileSystemRepresentation(u, true, (UInt8 *)buf,
	    (CFIndex)len));
}

static bool
cfuuid(CFDictionaryRef d, CFStringRef key, char *buf, size_t len)
{
	CFTypeRef u = CFDictionaryGetValue(d, key);
	CFStringRef s;
	bool ok;

	buf[0] = '\0';
	if (u == NULL || CFGetTypeID(u) != CFUUIDGetTypeID())
		return (false);
	s = CFUUIDCreateString(kCFAllocatorDefault, u);
	ok = cfstr(s, buf, len);
	CFRelease(s);
	return (ok);
}

/* Sizes as macOS shows them: decimal units, three significant digits. */
static void
human(uint64_t bytes, char *buf, size_t len)
{
	static const char *unit[] = { "B", "KB", "MB", "GB", "TB", "PB" };
	double v = (double)bytes;
	int i = 0;

	while (v >= 1000.0 && i < 5) {
		v /= 1000.0;
		i++;
	}
	if (i == 0)
		snprintf(buf, len, "%llu B", (unsigned long long)bytes);
	else
		snprintf(buf, len, "%.1f %s", v, unit[i]);
}

/* "disk3s1", "/dev/disk3s1", "/dev/rdisk3s1" or a mount point -> "disk3s1" */
static bool
bsd_name(const char *arg, char *name, size_t len)
{
	struct statfs sf;

	if (strncmp(arg, "/dev/r", 6) == 0 && strncmp(arg + 6, "disk", 4) == 0)
		arg += 6;
	else if (strncmp(arg, "/dev/", 5) == 0)
		arg += 5;
	if (strncmp(arg, "disk", 4) == 0 && strchr(arg, '/') == NULL) {
		strlcpy(name, arg, len);
		return (true);
	}
	if (statfs(arg, &sf) == 0 && strncmp(sf.f_mntfromname, "/dev/disk", 9) == 0) {
		strlcpy(name, sf.f_mntfromname + 5, len);
		return (true);
	}
	return (false);
}

static DADiskRef
disk_arg(const char *arg)
{
	char name[64];
	DADiskRef d;

	CFDictionaryRef desc = NULL;

	/* The disk is there when diskarbitrationd describes it. */
	if (!bsd_name(arg, name, sizeof(name)) ||
	    (d = DADiskCreateFromBSDName(kCFAllocatorDefault, session, name)) == NULL ||
	    (desc = DADiskCopyDescription(d)) == NULL) {
		fprintf(stderr, "Could not find disk: %s\n", arg);
		exit(1);
	}
	CFRelease(desc);
	return (d);
}

/* The whole disk a disk belongs to ("disk3s2" -> "disk3"). */
static void
whole_of(const char *bsd, char *whole, size_t len)
{
	char tmp[64];
	char *s;

	strlcpy(tmp, bsd, sizeof(tmp));	/* whole may be bsd */
	if ((s = strchr(tmp + 4, 's')) != NULL)
		*s = '\0';
	strlcpy(whole, tmp, len);
}

/* ---- running DiskArbitration requests ------------------------------------- */

struct result {
	bool		done;
	DAReturn	status;
	char		message[256];
};

static void
done_cb(DADiskRef disk, DADissenterRef dissenter, void *context)
{
	struct result *r = context;

	(void)disk;
	r->status = dissenter ? DADissenterGetStatus(dissenter) : 0;
	if (dissenter && DADissenterGetStatusString(dissenter))
		cfstr(DADissenterGetStatusString(dissenter), r->message,
		    sizeof(r->message));
	r->done = true;
}

static void
wait_for(struct result *r)
{
	while (!r->done)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, true);
}

static const char *
why(const struct result *r)
{
	static char buf[300];

	if (r->message[0] != '\0')
		return (r->message);
	switch (r->status) {
	case kDAReturnBusy:		return ("Resource busy");
	case kDAReturnNotPrivileged:	return ("Not privileged");
	case kDAReturnNotPermitted:	return ("Not permitted");
	case kDAReturnUnsupported:	return ("Unsupported");
	case kDAReturnNotFound:		return ("Not found");
	case kDAReturnNotMounted:	return ("Not mounted");
	case kDAReturnExclusiveAccess:	return ("Exclusive access");
	default:
		snprintf(buf, sizeof(buf), "error 0x%08x", (unsigned)r->status);
		return (buf);
	}
}

static void
volume_name(DADiskRef d, char *buf, size_t len)
{
	CFDictionaryRef desc = DADiskCopyDescription(d);

	buf[0] = '\0';
	if (desc != NULL) {
		cfstr(CFDictionaryGetValue(desc, kDADiskDescriptionVolumeNameKey),
		    buf, len);
		CFRelease(desc);
	}
}

/* ---- list ------------------------------------------------------------------ */

/* Partition types by their GUID or name, as diskutil shows them. */
static const char *
content_name(const char *c)
{
	static const struct { const char *content, *name; } names[] = {
		{ "C12A7328-F81F-11D2-BA4B-00A0C93EC93B", "EFI" },
		{ "48465300-0000-11AA-AA11-00306543ECAC", "Apple_HFS" },
		{ "7C3457EF-0000-11AA-AA11-00306543ECAC", "Apple_APFS" },
		{ "426F6F74-0000-11AA-AA11-00306543ECAC", "Apple_Boot" },
		{ "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", "Microsoft Basic Data" },
		{ "E3C9E316-0B5C-4DB8-817D-F92DF00215AE", "Microsoft Reserved" },
		{ "0FC63DAF-8483-4772-8E79-3D69D8477DE4", "Linux Filesystem" },
		{ "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F", "Linux Swap" },
		{ "516E7CB4-6ECF-11D6-8FF8-00022D09712B", "FreeBSD UFS" },
	};
	size_t i;

	for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if (strcasecmp(c, names[i].content) == 0)
			return (names[i].name);
	return (c);
}

struct entry {
	char		bsd[64];
	unsigned	part;		/* 0: the whole disk */
};

static int
entry_cmp(const void *a, const void *b)
{
	const struct entry *x = a, *y = b;
	char wx[64], wy[64];
	int ux, uy;

	whole_of(x->bsd, wx, sizeof(wx));
	whole_of(y->bsd, wy, sizeof(wy));
	ux = atoi(wx + 4);
	uy = atoi(wy + 4);
	if (ux != uy)
		return (ux < uy ? -1 : 1);
	return (x->part < y->part ? -1 : x->part > y->part);
}

static int
collect(struct entry *e, int max)
{
	io_iterator_t it;
	io_service_t m;
	int n = 0;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault,
	    IOServiceMatching(kIOMediaClass), &it) != KERN_SUCCESS)
		return (0);
	while ((m = IOIteratorNext(it)) != IO_OBJECT_NULL) {
		CFTypeRef name = IORegistryEntryCreateCFProperty(m,
		    CFSTR(kIOBSDNameKey), kCFAllocatorDefault, 0);
		const char *s;

		if (name != NULL && n < max && cfstr(name, e[n].bsd,
		    sizeof(e[n].bsd))) {
			s = strchr(e[n].bsd + 4, 's');
			e[n].part = s ? (unsigned)atoi(s + 1) : 0;
			n++;
		}
		if (name != NULL)
			CFRelease(name);
		IOObjectRelease(m);
	}
	IOObjectRelease(it);
	qsort(e, (size_t)n, sizeof(*e), entry_cmp);
	return (n);
}

static void
list_header(CFDictionaryRef d, const char *bsd)
{
	CFTypeRef proto = CFDictionaryGetValue(d,
	    kDADiskDescriptionDeviceProtocolKey);
	char p[64];

	cfstr(proto, p, sizeof(p));
	if (strcmp(p, "Virtual Interface") == 0)
		printf("/dev/%s (disk image):\n", bsd);
	else
		printf("/dev/%s (%s, physical):\n", bsd,
		    cfbool(d, kDADiskDescriptionDeviceInternalKey) ?
		    "internal" : "external");
	printf("   #:%27s %-23s %-10s %s\n", "TYPE", "NAME", "SIZE",
	    "IDENTIFIER");
}

static void
list_line(CFDictionaryRef d, const char *bsd, unsigned part)
{
	char content[128], name[128], size[32], sz[40];
	const char *type;

	cfstr(CFDictionaryGetValue(d, kDADiskDescriptionMediaContentKey),
	    content, sizeof(content));
	cfstr(CFDictionaryGetValue(d, kDADiskDescriptionVolumeNameKey), name,
	    sizeof(name));
	type = content_name(content);
	human(cfnum(d, kDADiskDescriptionMediaSizeKey), size, sizeof(size));
	snprintf(sz, sizeof(sz), "%s%s", part == 0 ? "*" : "", size);
	printf("%4u:%27.27s %-23.23s %10s %s\n", part, type, name, sz, bsd);
}

static int
cmd_list(int argc, char **argv)
{
	static struct entry e[256];
	char only[64] = "", whole[64];
	int n, i;
	bool first = true;

	if (argc > 1)
		usage();
	if (argc == 1 && !bsd_name(argv[0], only, sizeof(only))) {
		fprintf(stderr, "Could not find disk: %s\n", argv[0]);
		return (1);
	}
	if (only[0] != '\0')
		whole_of(only, only, sizeof(only));
	n = collect(e, 256);
	for (i = 0; i < n; i++) {
		DADiskRef disk;
		CFDictionaryRef d;

		whole_of(e[i].bsd, whole, sizeof(whole));
		if (only[0] != '\0' && strcmp(whole, only) != 0)
			continue;
		disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session,
		    e[i].bsd);
		if (disk == NULL || (d = DADiskCopyDescription(disk)) == NULL) {
			if (disk != NULL)
				CFRelease(disk);
			continue;
		}
		if (e[i].part == 0) {
			if (!first)
				printf("\n");
			first = false;
			list_header(d, e[i].bsd);
		}
		list_line(d, e[i].bsd, e[i].part);
		CFRelease(d);
		CFRelease(disk);
	}
	if (only[0] != '\0' && first) {
		fprintf(stderr, "Could not find disk: %s\n", argv[0]);
		return (1);
	}
	return (0);
}

/* ---- info ------------------------------------------------------------------ */

static void
row(const char *key, const char *value)
{
	char k[64];

	snprintf(k, sizeof(k), "%s:", key);
	printf("   %-27s%s\n", k, value);
}

static void
plist_value(CFMutableDictionaryRef p, const char *key, CFTypeRef v)
{
	CFStringRef k = CFStringCreateWithCString(kCFAllocatorDefault, key,
	    kCFStringEncodingUTF8);

	if (v != NULL)
		CFDictionarySetValue(p, k, v);
	CFRelease(k);
}

static void
plist_str(CFMutableDictionaryRef p, const char *key, const char *s)
{
	CFStringRef v = CFStringCreateWithCString(kCFAllocatorDefault, s,
	    kCFStringEncodingUTF8);

	plist_value(p, key, v);
	CFRelease(v);
}

static void
plist_num(CFMutableDictionaryRef p, const char *key, uint64_t n)
{
	CFNumberRef v = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type,
	    &n);

	plist_value(p, key, v);
	CFRelease(v);
}

static int
cmd_info(int argc, char **argv)
{
	char bsd[64] = "", whole[64], s[512], v[600], mp[MAXPATHLEN], sz[32];
	char kind[64], type[128], name[256], uuid[64], proto[64];
	CFMutableDictionaryRef p = NULL;
	CFDictionaryRef d;
	DADiskRef disk;
	uint64_t size, block;
	bool plist = false, isWhole, mounted, writable, ro = false;
	struct statfs sf;

	if (argc > 0 && strcmp(argv[0], "-plist") == 0) {
		plist = true;
		argc--;
		argv++;
	}
	if (argc != 1)
		usage();
	disk = disk_arg(argv[0]);
	if ((d = DADiskCopyDescription(disk)) == NULL) {
		fprintf(stderr, "Could not find disk: %s\n", argv[0]);
		return (1);
	}
	cfstr(CFDictionaryGetValue(d, kDADiskDescriptionMediaBSDNameKey), bsd,
	    sizeof(bsd));
	whole_of(bsd, whole, sizeof(whole));
	isWhole = cfbool(d, kDADiskDescriptionMediaWholeKey);
	mounted = cfpath(d, kDADiskDescriptionVolumePathKey, mp, sizeof(mp));
	if (mounted && statfs(mp, &sf) == 0)
		ro = (sf.f_flags & MNT_RDONLY) != 0;
	cfstr(CFDictionaryGetValue(d, kDADiskDescriptionVolumeKindKey), kind,
	    sizeof(kind));
	cfstr(CFDictionaryGetValue(d, kDADiskDescriptionVolumeTypeKey), type,
	    sizeof(type));
	cfstr(CFDictionaryGetValue(d, kDADiskDescriptionVolumeNameKey), name,
	    sizeof(name));
	cfstr(CFDictionaryGetValue(d, kDADiskDescriptionDeviceProtocolKey),
	    proto, sizeof(proto));
	size = cfnum(d, kDADiskDescriptionMediaSizeKey);
	block = cfnum(d, kDADiskDescriptionMediaBlockSizeKey);
	writable = cfbool(d, kDADiskDescriptionMediaWritableKey);
	cfstr(CFDictionaryGetValue(d, kDADiskDescriptionMediaContentKey), s,
	    sizeof(s));

	if (plist) {
		p = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		plist_str(p, "DeviceIdentifier", bsd);
		snprintf(v, sizeof(v), "/dev/%s", bsd);
		plist_str(p, "DeviceNode", v);
		plist_value(p, "WholeDisk", isWhole ? kCFBooleanTrue : kCFBooleanFalse);
		plist_str(p, "ParentWholeDisk", whole);
		plist_str(p, "Content", s);
		plist_str(p, "VolumeName", name);
		plist_str(p, "MountPoint", mounted ? mp : "");
		plist_str(p, "FilesystemType", kind);
		plist_str(p, "FilesystemUserVisibleName", type);
		plist_num(p, "Size", size);
		plist_num(p, "TotalSize", size);
		plist_num(p, "DeviceBlockSize", block);
		plist_str(p, "BusProtocol", proto);
		plist_value(p, "Internal",
		    cfbool(d, kDADiskDescriptionDeviceInternalKey) ?
		    kCFBooleanTrue : kCFBooleanFalse);
		plist_value(p, "Removable",
		    cfbool(d, kDADiskDescriptionMediaRemovableKey) ?
		    kCFBooleanTrue : kCFBooleanFalse);
		plist_value(p, "Ejectable",
		    cfbool(d, kDADiskDescriptionMediaEjectableKey) ?
		    kCFBooleanTrue : kCFBooleanFalse);
		plist_value(p, "WritableMedia", writable ? kCFBooleanTrue :
		    kCFBooleanFalse);
		plist_value(p, "WritableVolume", mounted && !ro ? kCFBooleanTrue :
		    kCFBooleanFalse);
		if (cfuuid(d, kDADiskDescriptionVolumeUUIDKey, uuid, sizeof(uuid)))
			plist_str(p, "VolumeUUID", uuid);
		if (cfuuid(d, kDADiskDescriptionMediaUUIDKey, uuid, sizeof(uuid)))
			plist_str(p, "DiskUUID", uuid);
		{
			CFDataRef x = CFPropertyListCreateData(kCFAllocatorDefault,
			    p, kCFPropertyListXMLFormat_v1_0, 0, NULL);

			if (x != NULL) {
				fwrite(CFDataGetBytePtr(x), 1,
				    (size_t)CFDataGetLength(x), stdout);
				CFRelease(x);
			}
		}
		CFRelease(p);
		CFRelease(d);
		CFRelease(disk);
		return (0);
	}

	row("Device Identifier", bsd);
	snprintf(v, sizeof(v), "/dev/%s", bsd);
	row("Device Node", v);
	row("Whole", isWhole ? "Yes" : "No");
	row("Part of Whole", whole);
	if (CFDictionaryGetValue(d, kDADiskDescriptionMediaNameKey) != NULL) {
		cfstr(CFDictionaryGetValue(d, kDADiskDescriptionMediaNameKey), v,
		    sizeof(v));
		row("Device / Media Name", v);
	}
	printf("\n");
	if (kind[0] != '\0' || name[0] != '\0') {
		row("Volume Name", name[0] ? name : "Not applicable (no file system)");
		row("Mounted", mounted ? "Yes" : "No");
		if (mounted)
			row("Mount Point", mp);
		printf("\n");
	}
	row(isWhole ? "Content (IOContent)" : "Partition Type",
	    content_name(s));
	if (kind[0] != '\0') {
		row("File System Personality", strcmp(kind, "hfs") == 0 ?
		    (strstr(type, "Journaled") ? "Journaled HFS+" : "HFS+") :
		    strcmp(kind, "msdos") == 0 ? (strstr(type, "FAT32") ?
		    "MS-DOS FAT32" : strstr(type, "FAT16") ? "MS-DOS FAT16" :
		    strstr(type, "FAT12") ? "MS-DOS FAT12" : "MS-DOS") : kind);
		row("Type (Bundle)", kind);
		row("Name (User Visible)", type[0] ? type : kind);
	}
	printf("\n");
	if (cfuuid(d, kDADiskDescriptionMediaUUIDKey, uuid, sizeof(uuid)))
		row("Disk / Partition UUID", uuid);
	if (cfuuid(d, kDADiskDescriptionVolumeUUIDKey, uuid, sizeof(uuid)))
		row("Volume UUID", uuid);
	human(size, sz, sizeof(sz));
	snprintf(v, sizeof(v), "%s (%llu Bytes) (exactly %llu 512-Byte-Units)",
	    sz, (unsigned long long)size, (unsigned long long)(size / 512));
	row("Disk Size", v);
	snprintf(v, sizeof(v), "%llu Bytes", (unsigned long long)block);
	row("Device Block Size", v);
	if (mounted && statfs(mp, &sf) == 0) {
		uint64_t total = (uint64_t)sf.f_blocks * sf.f_bsize;
		uint64_t avail = (uint64_t)sf.f_bavail * sf.f_bsize;

		human(total, sz, sizeof(sz));
		snprintf(v, sizeof(v), "%s (%llu Bytes)", sz,
		    (unsigned long long)total);
		row("Volume Total Space", v);
		human(avail, sz, sizeof(sz));
		snprintf(v, sizeof(v), "%s (%llu Bytes)", sz,
		    (unsigned long long)avail);
		row("Volume Free Space", v);
	}
	printf("\n");
	row("Media Read-Only", writable ? "No" : "Yes");
	if (mounted)
		row("Volume Read-Only", ro ? "Yes" : "No");
	printf("\n");
	row("Device Location", cfbool(d, kDADiskDescriptionDeviceInternalKey) ?
	    "Internal" : "External");
	row("Removable Media", cfbool(d, kDADiskDescriptionMediaRemovableKey) ?
	    "Removable" : "Fixed");
	if (proto[0] != '\0')
		row("Protocol", strcmp(proto, "Virtual Interface") == 0 ?
		    "Disk Image" : proto);
	if (CFDictionaryGetValue(d, kDADiskDescriptionDeviceModelKey) != NULL) {
		cfstr(CFDictionaryGetValue(d, kDADiskDescriptionDeviceModelKey), v,
		    sizeof(v));
		row("Device Model", v);
	}
	CFRelease(d);
	CFRelease(disk);
	return (0);
}

/* ---- mount, unmount, eject, rename ---------------------------------------- */

static int
cmd_mount(int argc, char **argv, bool wholeDisk)
{
	struct result r = { false, 0, "" };
	CFStringRef args[2] = { NULL, NULL };
	const char *at = NULL;
	CFURLRef url = NULL;
	DADiskRef disk;
	char name[256], mp[MAXPATHLEN], bsd[64];
	CFDictionaryRef d;

	while (argc > 1) {
		if (strcasecmp(argv[0], "readOnly") == 0) {
			args[0] = CFSTR("rdonly");
			argc--;
			argv++;
		} else if (strcasecmp(argv[0], "-mountPoint") == 0 && argc > 2) {
			at = argv[1];
			argc -= 2;
			argv += 2;
		} else
			usage();
	}
	if (argc != 1)
		usage();
	disk = disk_arg(argv[0]);
	if (at != NULL)
		url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		    (const UInt8 *)at, (CFIndex)strlen(at), true);
	DADiskMountWithArguments(disk, url, wholeDisk ? kDADiskMountOptionWhole :
	    kDADiskMountOptionDefault, done_cb, &r, args);
	wait_for(&r);
	strlcpy(bsd, DADiskGetBSDName(disk), sizeof(bsd));
	if (wholeDisk) {
		if (r.status != 0) {
			fprintf(stderr, "Volume(s) mounted on disk %s failed: %s\n",
			    bsd, why(&r));
			return (1);
		}
		printf("Volume(s) mounted successfully\n");
		return (0);
	}
	volume_name(disk, name, sizeof(name));
	if (r.status != 0) {
		fprintf(stderr, "Volume on %s failed to mount: %s\n", bsd, why(&r));
		return (1);
	}
	mp[0] = '\0';
	if ((d = DADiskCopyDescription(disk)) != NULL) {
		cfpath(d, kDADiskDescriptionVolumePathKey, mp, sizeof(mp));
		CFRelease(d);
	}
	if (at != NULL)
		printf("Volume %s on %s mounted at %s\n", name, bsd, mp);
	else
		printf("Volume %s on %s mounted\n", name, bsd);
	if (url != NULL)
		CFRelease(url);
	CFRelease(disk);
	return (0);
}

static int
cmd_unmount(int argc, char **argv, bool wholeDisk)
{
	struct result r = { false, 0, "" };
	DADiskUnmountOptions opt = wholeDisk ? kDADiskUnmountOptionWhole :
	    kDADiskUnmountOptionDefault;
	DADiskRef disk;
	char name[256], bsd[64];

	if (argc == 2 && strcasecmp(argv[0], "force") == 0) {
		opt |= kDADiskUnmountOptionForce;
		argc--;
		argv++;
	}
	if (argc != 1)
		usage();
	disk = disk_arg(argv[0]);
	if (wholeDisk) {
		char whole[64];

		whole_of(DADiskGetBSDName(disk), whole, sizeof(whole));
		CFRelease(disk);
		disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session,
		    whole);
	}
	strlcpy(bsd, DADiskGetBSDName(disk), sizeof(bsd));
	volume_name(disk, name, sizeof(name));
	DADiskUnmount(disk, opt, done_cb, &r);
	wait_for(&r);
	if (r.status != 0) {
		if (wholeDisk)
			fprintf(stderr, "Unmount of %s failed: %s\n", bsd, why(&r));
		else
			fprintf(stderr, "Unmount of %s failed: %s\n",
			    name[0] ? name : bsd, why(&r));
		return (1);
	}
	if (wholeDisk)
		printf("Unmount of all volumes on %s was successful\n", bsd);
	else
		printf("Volume %s on %s unmounted\n", name, bsd);
	CFRelease(disk);
	return (0);
}

static int
cmd_eject(int argc, char **argv)
{
	struct result r = { false, 0, "" };
	DADiskRef disk, whole;
	char w[64];

	if (argc != 1)
		usage();
	disk = disk_arg(argv[0]);
	whole_of(DADiskGetBSDName(disk), w, sizeof(w));
	whole = DADiskCreateFromBSDName(kCFAllocatorDefault, session, w);
	/* As on macOS: unmount the volumes first, then eject. */
	DADiskUnmount(whole, kDADiskUnmountOptionWhole, done_cb, &r);
	wait_for(&r);
	if (r.status != 0 && r.status != kDAReturnNotMounted) {
		fprintf(stderr, "Disk %s failed to eject: %s\n", w, why(&r));
		return (1);
	}
	memset(&r, 0, sizeof(r));
	DADiskEject(whole, kDADiskEjectOptionDefault, done_cb, &r);
	wait_for(&r);
	if (r.status != 0) {
		fprintf(stderr, "Disk %s failed to eject: %s\n", w, why(&r));
		return (1);
	}
	printf("Disk %s ejected\n", w);
	CFRelease(whole);
	CFRelease(disk);
	return (0);
}

static int
cmd_rename(int argc, char **argv)
{
	struct result r = { false, 0, "" };
	CFStringRef name;
	DADiskRef disk;
	char old[256];

	if (argc != 2)
		usage();
	disk = disk_arg(argv[0]);
	volume_name(disk, old, sizeof(old));
	name = CFStringCreateWithCString(kCFAllocatorDefault, argv[1],
	    kCFStringEncodingUTF8);
	DADiskRename(disk, name, kDADiskRenameOptionDefault, done_cb, &r);
	wait_for(&r);
	if (r.status != 0) {
		fprintf(stderr, "Volume on %s failed to rename: %s\n",
		    DADiskGetBSDName(disk), why(&r));
		return (1);
	}
	printf("Volume on %s renamed to %s\n", DADiskGetBSDName(disk), argv[1]);
	CFRelease(name);
	CFRelease(disk);
	return (0);
}

/* ---- the file system bundles: verify, repair, erase ------------------------ */

static int
run(const char *const argv[])
{
	pid_t pid;
	int st;

	if (posix_spawn(&pid, argv[0], NULL, NULL, __DECONST(char *const *, argv),
	    environ) != 0) {
		warn("%s", argv[0]);
		return (-1);
	}
	while (waitpid(pid, &st, 0) < 0)
		if (errno != EINTR)
			return (-1);
	return (WIFEXITED(st) ? WEXITSTATUS(st) : -1);
}

static void
description(DADiskRef disk, char *kind, size_t klen, char *mp, size_t mlen)
{
	CFDictionaryRef d = DADiskCopyDescription(disk);

	kind[0] = mp[0] = '\0';
	if (d != NULL) {
		cfstr(CFDictionaryGetValue(d, kDADiskDescriptionVolumeKindKey),
		    kind, klen);
		cfpath(d, kDADiskDescriptionVolumePathKey, mp, mlen);
		CFRelease(d);
	}
}

static int
cmd_fsck(int argc, char **argv, bool repair)
{
	char kind[64], mp[MAXPATHLEN], fsck[MAXPATHLEN], raw[80];
	const char *args[8];
	struct result r = { false, 0, "" };
	DADiskRef disk;
	int n = 0, rc;

	if (argc != 1)
		usage();
	disk = disk_arg(argv[0]);
	description(disk, kind, sizeof(kind), mp, sizeof(mp));
	if (kind[0] == '\0')
		errx(1, "%s does not have a file system", DADiskGetBSDName(disk));
	snprintf(fsck, sizeof(fsck), "/sbin/fsck_%s", kind);
	if (access(fsck, X_OK) != 0)
		errx(1, "%s volumes cannot be %s", kind, repair ? "repaired" :
		    "verified");
	snprintf(raw, sizeof(raw), "/dev/r%s", DADiskGetBSDName(disk));
	printf("Started file system %s on %s\n", repair ? "repair" :
	    "verification", DADiskGetBSDName(disk));
	fflush(stdout);
	/* A mounted volume is repaired unmounted; HFS+ verifies it live. */
	if (mp[0] != '\0' && (repair || strcmp(kind, "hfs") != 0)) {
		DADiskUnmount(disk, kDADiskUnmountOptionDefault, done_cb, &r);
		wait_for(&r);
		if (r.status != 0)
			errx(1, "Unable to unmount volume for %s: %s",
			    repair ? "repair" : "verification", why(&r));
	}
	args[n++] = fsck;
	if (mp[0] != '\0' && !repair && strcmp(kind, "hfs") == 0)
		args[n++] = "-l";
	if (strcmp(kind, "hfs") == 0)
		args[n++] = "-f";	/* even when the journal says it is clean */
	args[n++] = repair ? "-y" : "-n";
	args[n++] = raw;
	args[n] = NULL;
	rc = run(args);
	if (mp[0] != '\0' && r.done) {
		memset(&r, 0, sizeof(r));
		DADiskMount(disk, NULL, kDADiskMountOptionDefault, done_cb, &r);
		wait_for(&r);
	}
	if (rc != 0) {
		fprintf(stderr, "File system %s failed on %s\n",
		    repair ? "repair" : "verification", DADiskGetBSDName(disk));
		return (1);
	}
	printf("Finished file system %s on %s\n", repair ? "repair" :
	    "verification", DADiskGetBSDName(disk));
	CFRelease(disk);
	return (0);
}

/* The formats eraseVolume makes, by the names macOS's diskutil takes. */
static const struct format {
	const char	*names[4];
	const char	*kind;
	const char	*args[4];
} formats[] = {
	{ { "JHFS+", "Journaled HFS+", "HFS+J", NULL }, "hfs", { "-J", NULL } },
	{ { "HFS+", "Mac OS Extended", NULL }, "hfs", { NULL } },
	{ { "JHFSX", "Case-sensitive Journaled HFS+", NULL }, "hfs",
	  { "-J", "-s", NULL } },
	{ { "HFSX", "Case-sensitive HFS+", NULL }, "hfs", { "-s", NULL } },
	{ { "FAT32", "MS-DOS FAT32", NULL }, "msdos", { "-F", "32", NULL } },
	{ { "FAT16", "MS-DOS FAT16", NULL }, "msdos", { "-F", "16", NULL } },
	{ { "FAT12", "MS-DOS FAT12", NULL }, "msdos", { "-F", "12", NULL } },
	{ { "MS-DOS", "FAT", "MS-DOS FAT", NULL }, "msdos", { NULL } },
};

static const struct format *
find_format(const char *name)
{
	size_t i, j;

	for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
		for (j = 0; formats[i].names[j] != NULL; j++)
			if (strcasecmp(name, formats[i].names[j]) == 0)
				return (&formats[i]);
	return (NULL);
}

static int
cmd_erase(int argc, char **argv)
{
	struct result r = { false, 0, "" };
	const struct format *f;
	const char *args[16];
	char raw[80], label[256], tool[64];
	DADiskRef disk;
	int n = 0, i;

	if (argc != 3)
		usage();
	if ((f = find_format(argv[0])) == NULL)
		errx(1, "Unrecognized file system: %s (diskutil listFilesystems)",
		    argv[0]);
	disk = disk_arg(argv[2]);
	strlcpy(label, argv[1], sizeof(label));
	if (strcmp(f->kind, "msdos") == 0)
		for (i = 0; label[i] != '\0'; i++)
			label[i] = (char)toupper((unsigned char)label[i]);
	printf("Started erase on %s\n", DADiskGetBSDName(disk));
	printf("Unmounting disk\n");
	fflush(stdout);
	DADiskUnmount(disk, kDADiskUnmountOptionDefault, done_cb, &r);
	wait_for(&r);
	if (r.status != 0 && r.status != kDAReturnNotMounted)
		errx(1, "Unable to unmount %s: %s", DADiskGetBSDName(disk),
		    why(&r));
	snprintf(raw, sizeof(raw), "/dev/r%s", DADiskGetBSDName(disk));
	snprintf(tool, sizeof(tool), "/sbin/newfs_%s", f->kind);
	args[n++] = tool;
	for (i = 0; f->args[i] != NULL; i++)
		args[n++] = f->args[i];
	args[n++] = "-v";
	args[n++] = label;
	args[n++] = raw;
	args[n] = NULL;
	printf("Erasing\n");
	fflush(stdout);
	if (run(args) != 0) {
		/* The volume is left as it was: mounted again. */
		memset(&r, 0, sizeof(r));
		DADiskMount(disk, NULL, kDADiskMountOptionDefault, done_cb, &r);
		wait_for(&r);
		errx(1, "Error: -69888: Couldn't format %s",
		    DADiskGetBSDName(disk));
	}
	printf("Initialized %s as %s\n", raw, argv[0]);
	/* diskarbitrationd probes the new volume again, then mounts it. */
	memset(&r, 0, sizeof(r));
	DADiskMount(disk, NULL, kDADiskMountOptionDefault, done_cb, &r);
	wait_for(&r);
	if (r.status == 0)
		printf("Mounting disk\n");
	printf("Finished erase on %s\n", DADiskGetBSDName(disk));
	CFRelease(disk);
	return (0);
}

static int
cmd_listfs(void)
{
	size_t i;

	printf("Formattable file systems\n\n");
	printf("These file system personalities can be used for erasing and "
	    "partitioning.\nWhen specifying a personality as a parameter to a "
	    "verb, case is not considered.\n\n");
	printf("%-30s %s\n", "PERSONALITY", "USER VISIBLE NAME");
	printf("----------------------------------------------------------------"
	    "-----------\n");
	for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
		printf("%-30s %s\n", formats[i].names[0],
		    formats[i].names[1] ? formats[i].names[1] : "");
	return (0);
}

int
main(int argc, char **argv)
{
	const char *verb;

	if (argc < 2)
		usage();
	verb = argv[1];
	argc -= 2;
	argv += 2;
	if (strcasecmp(verb, "listFilesystems") == 0)
		return (cmd_listfs());
	session = DASessionCreate(kCFAllocatorDefault);
	if (session == NULL)
		errx(1, "Unable to reach diskarbitrationd");
	DASessionScheduleWithRunLoop(session, CFRunLoopGetCurrent(),
	    kCFRunLoopDefaultMode);
	if (strcasecmp(verb, "list") == 0)
		return (cmd_list(argc, argv));
	if (strcasecmp(verb, "info") == 0 || strcasecmp(verb, "information") == 0)
		return (cmd_info(argc, argv));
	if (strcasecmp(verb, "mount") == 0)
		return (cmd_mount(argc, argv, false));
	if (strcasecmp(verb, "mountDisk") == 0)
		return (cmd_mount(argc, argv, true));
	if (strcasecmp(verb, "unmount") == 0 || strcasecmp(verb, "umount") == 0)
		return (cmd_unmount(argc, argv, false));
	if (strcasecmp(verb, "unmountDisk") == 0 ||
	    strcasecmp(verb, "umountDisk") == 0)
		return (cmd_unmount(argc, argv, true));
	if (strcasecmp(verb, "eject") == 0)
		return (cmd_eject(argc, argv));
	if (strcasecmp(verb, "rename") == 0 ||
	    strcasecmp(verb, "renameVolume") == 0)
		return (cmd_rename(argc, argv));
	if (strcasecmp(verb, "verifyVolume") == 0)
		return (cmd_fsck(argc, argv, false));
	if (strcasecmp(verb, "repairVolume") == 0)
		return (cmd_fsck(argc, argv, true));
	if (strcasecmp(verb, "eraseVolume") == 0)
		return (cmd_erase(argc, argv));
	usage();
}
