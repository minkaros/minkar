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
 * hdiutil: disk images. The kernel's MinkarDiskImage driver makes an
 * image file a disk (/dev/diskN, its partitions published as on any
 * disk); hdiutil attaches and detaches images through it, mounting their
 * volumes in /Volumes, and creates images: a raw file, a GUID partition
 * table with one partition (or none), formatted with newfs_hfs or
 * newfs_msdos, optionally filled from a folder.
 *
 * With diskarbitrationd running, attach mounts through DiskArbitration,
 * as on macOS (and keeps it from mounting the image's volumes itself, for
 * -nomount or another mount point); without it, hdiutil mounts them.
 *
 * Images are raw (what "hdiutil create" writes, and UDTO/.cdr/.iso) or
 * UDIF images (udif.c): read/write (UDRW, the disk as it is), or
 * read-only and compressed in chunks (UDRO, UDCO, UDZO, UDBZ, ULFO),
 * attached read-only with their chunk table, the driver decompressing a
 * chunk when it is read. "hdiutil convert" writes them, as "create
 * -format" does. LZMA images (ULMO) are converted but not attached.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <DiskArbitration/DiskArbitrationPrivate.h>	/* DARegisterIdleCallback */
#include <IOKit/IOKitLib.h>

#include <sys/types.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libkern/OSByteOrder.h>
#include <mach/mach_error.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <zlib.h>

#include "udif.h"

extern char **environ;

/* The driver's user client (MinkarDiskImage.h). */
#define kMinkarDiskImageAttach		0
#define kMinkarDiskImageDetach		1
#define kMinkarDiskImageAttachChunks	2
#define kMinkarDiskImageReadOnly	0x1

#define GPT_ENTRIES	128
#define GPT_ENTRY_SIZE	128
#define FIRST_LBA	40		/* where macOS's single partition starts */

static bool quiet;

/* ---- Helpers ------------------------------------------------------------ */

/* Runs argv; with out, collects its standard output (len bytes). */
static int
run(const char *const argv[], char *out, size_t len, bool silent)
{
	posix_spawn_file_actions_t fa;
	int fds[2] = { -1, -1 }, status;
	pid_t pid;

	posix_spawn_file_actions_init(&fa);
	if (out != NULL) {
		if (pipe(fds) != 0)
			err(1, "pipe");
		posix_spawn_file_actions_adddup2(&fa, fds[1], STDOUT_FILENO);
		posix_spawn_file_actions_addclose(&fa, fds[0]);
		posix_spawn_file_actions_addclose(&fa, fds[1]);
	} else if (silent) {
		posix_spawn_file_actions_addopen(&fa, STDOUT_FILENO, "/dev/null",
		    O_WRONLY, 0);
	}
	if (silent)
		posix_spawn_file_actions_addopen(&fa, STDERR_FILENO, "/dev/null",
		    O_WRONLY, 0);
	if (posix_spawn(&pid, argv[0], &fa, NULL, (char *const *)argv,
	    environ) != 0) {
		posix_spawn_file_actions_destroy(&fa);
		return (-1);
	}
	posix_spawn_file_actions_destroy(&fa);
	if (out != NULL) {
		size_t got = 0;
		ssize_t n;

		close(fds[1]);
		while (got + 1 < len &&
		    (n = read(fds[0], out + got, len - 1 - got)) > 0)
			got += (size_t)n;
		out[got] = '\0';
		close(fds[0]);
	}
	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
			return (-1);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

static uint64_t
parse_size(const char *s)
{
	char *end;
	double v = strtod(s, &end);
	uint64_t mult = 1;

	switch (tolower((unsigned char)*end)) {
	case 'b': mult = SECTOR; end++; break;	/* sectors, as macOS */
	case 'k': mult = 1ULL << 10; end++; break;
	case 'm': mult = 1ULL << 20; end++; break;
	case 'g': mult = 1ULL << 30; end++; break;
	case 't': mult = 1ULL << 40; end++; break;
	}
	if (v <= 0 || *end != '\0')
		errx(1, "%s: not a size", s);
	return ((uint64_t)(v * (double)mult) / SECTOR * SECTOR);
}

/* ---- The driver ----------------------------------------------------------- */

static io_connect_t
driver(void)
{
	io_service_t svc = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOServiceMatching("MinkarDiskImageController"));
	io_connect_t c = MACH_PORT_NULL;
	kern_return_t kr;

	if (svc == MACH_PORT_NULL)
		errx(1, "the disk image driver is not loaded");
	kr = IOServiceOpen(svc, mach_task_self(), 0, &c);
	IOObjectRelease(svc);
	/* The driver takes administrators only. */
	if (kr != KERN_SUCCESS && geteuid() != 0)
		errx(1, "using disk images needs root");
	if (kr != KERN_SUCCESS)
		errx(1, "can't use the disk image driver: %s",
		    mach_error_string(kr));
	return (c);
}

/*
 * The image device the driver made, by its registry ID. Its registration
 * goes on as the attach returns (on another processor with SMP), and a
 * matching lookup finds it only once registered: wait for it, up to 5 s.
 */
static io_service_t
image_device(uint64_t id)
{
	io_service_t dev;
	int tries;

	for (tries = 0; tries < 50; tries++) {
		dev = IOServiceGetMatchingService(kIOMasterPortDefault,
		    IORegistryEntryIDMatching(id));
		if (dev != MACH_PORT_NULL)
			return (dev);
		usleep(100000);
	}
	return (MACH_PORT_NULL);
}

/* The BSD name of the whole medium on an image device (waits for it). */
static bool
device_disk(io_service_t dev, char *name, size_t len, int tries)
{
	for (; tries > 0; tries--) {
		io_iterator_t it;
		io_registry_entry_t e;
		bool found = false;

		if (IORegistryEntryCreateIterator(dev, kIOServicePlane,
		    kIORegistryIterateRecursively, &it) == KERN_SUCCESS) {
			while (!found && (e = IOIteratorNext(it)) != 0) {
				CFTypeRef whole = IORegistryEntryCreateCFProperty(e,
				    CFSTR("Whole"), NULL, 0);
				CFTypeRef bsd = IORegistryEntryCreateCFProperty(e,
				    CFSTR("BSD Name"), NULL, 0);

				if (whole == kCFBooleanTrue && bsd != NULL)
					found = CFStringGetCString(bsd, name,
					    (CFIndex)len, kCFStringEncodingUTF8);
				if (whole != NULL)
					CFRelease(whole);
				if (bsd != NULL)
					CFRelease(bsd);
				IOObjectRelease(e);
			}
			IOObjectRelease(it);
		}
		if (found)
			return (true);
		usleep(100000);
	}
	return (false);
}

/* The image device whose disk is diskN. */
static io_service_t
find_device(const char *disk)
{
	io_iterator_t it;
	io_service_t dev, found = MACH_PORT_NULL;
	char name[64];

	if (IOServiceGetMatchingServices(kIOMasterPortDefault,
	    IOServiceMatching("MinkarDiskImageDevice"), &it) != KERN_SUCCESS)
		return (MACH_PORT_NULL);
	while (found == MACH_PORT_NULL && (dev = IOIteratorNext(it)) != 0) {
		if (device_disk(dev, name, sizeof(name), 1) &&
		    strcmp(name, disk) == 0)
			found = dev;
		else
			IOObjectRelease(dev);
	}
	IOObjectRelease(it);
	return (found);
}

static bool
string_property(io_registry_entry_t e, const char *key, char *buf, size_t len)
{
	CFStringRef k = CFStringCreateWithCString(NULL, key,
	    kCFStringEncodingUTF8);
	CFTypeRef v = IORegistryEntryCreateCFProperty(e, k, NULL, 0);
	bool ok = false;

	CFRelease(k);
	buf[0] = '\0';
	if (v != NULL && CFGetTypeID(v) == CFStringGetTypeID())
		ok = CFStringGetCString(v, buf, (CFIndex)len,
		    kCFStringEncodingUTF8);
	if (v != NULL)
		CFRelease(v);
	return (ok);
}

/* ---- Image formats -------------------------------------------------------- */

/* ---- GUID partition table ------------------------------------------------- */

static void
guid_bytes(const char *s, uint8_t out[16])
{
	uuid_t u;

	if (uuid_parse(s, u) != 0)
		errx(1, "bad GUID %s", s);
	/* The first three fields are little-endian on disk. */
	out[0] = u[3]; out[1] = u[2]; out[2] = u[1]; out[3] = u[0];
	out[4] = u[5]; out[5] = u[4];
	out[6] = u[7]; out[7] = u[6];
	memcpy(out + 8, u + 8, 8);
}

static void
random_guid(uint8_t out[16])
{
	uuid_t u;

	uuid_generate_random(u);
	memcpy(out, u, 16);
}

static void
put32(uint8_t *p, uint32_t v)
{
	OSWriteLittleInt32(p, 0, v);
}

static void
put64(uint8_t *p, uint64_t v)
{
	OSWriteLittleInt64(p, 0, v);
}

/* One partition of the given type over the disk, as GPTSPUD lays it. */
static void
write_gpt(int fd, uint64_t bytes, const char *type, uint64_t *first,
    uint64_t *last)
{
	uint64_t lbas = bytes / SECTOR, last_usable = lbas - 34;
	uint8_t mbr[SECTOR], hdr[SECTOR], entries[GPT_ENTRIES * GPT_ENTRY_SIZE];
	uint8_t disk_guid[16];
	const char *name = "disk image";
	uint32_t ecrc;
	int i;

	*first = FIRST_LBA;
	*last = last_usable;
	memset(mbr, 0, sizeof(mbr));
	mbr[446 + 4] = 0xee;			/* the protective partition */
	mbr[446 + 1] = 0x00; mbr[446 + 2] = 0x02; mbr[446 + 3] = 0x00;
	mbr[446 + 5] = 0xff; mbr[446 + 6] = 0xff; mbr[446 + 7] = 0xff;
	put32(mbr + 446 + 8, 1);
	put32(mbr + 446 + 12, lbas - 1 > 0xffffffffULL ? 0xffffffffU :
	    (uint32_t)(lbas - 1));
	mbr[510] = 0x55;
	mbr[511] = 0xaa;

	memset(entries, 0, sizeof(entries));
	guid_bytes(type, entries);
	random_guid(entries + 16);
	put64(entries + 32, *first);
	put64(entries + 40, *last);
	for (i = 0; name[i] != '\0'; i++)
		entries[56 + 2 * i] = (uint8_t)name[i];
	ecrc = (uint32_t)crc32(0, entries, sizeof(entries));
	random_guid(disk_guid);

	for (i = 0; i < 2; i++) {
		uint64_t self = i == 0 ? 1 : lbas - 1;
		uint64_t other = i == 0 ? lbas - 1 : 1;
		uint64_t table = i == 0 ? 2 : lbas - 33;

		memset(hdr, 0, sizeof(hdr));
		memcpy(hdr, "EFI PART", 8);
		put32(hdr + 8, 0x00010000);
		put32(hdr + 12, 92);
		put64(hdr + 24, self);
		put64(hdr + 32, other);
		put64(hdr + 40, 34);
		put64(hdr + 48, last_usable);
		memcpy(hdr + 56, disk_guid, 16);
		put64(hdr + 72, table);
		put32(hdr + 80, GPT_ENTRIES);
		put32(hdr + 84, GPT_ENTRY_SIZE);
		put32(hdr + 88, ecrc);
		put32(hdr + 16, (uint32_t)crc32(0, hdr, 92));
		if (pwrite(fd, entries, sizeof(entries), (off_t)(table * SECTOR)) !=
		    (ssize_t)sizeof(entries) ||
		    pwrite(fd, hdr, sizeof(hdr), (off_t)(self * SECTOR)) !=
		    (ssize_t)sizeof(hdr))
			err(1, "writing the partition table");
	}
	if (pwrite(fd, mbr, sizeof(mbr), 0) != (ssize_t)sizeof(mbr))
		err(1, "writing the partition table");
}

/* ---- attach and detach ---------------------------------------------------- */

static const char *
content_name(const char *content)
{
	static const struct {
		const char	*guid, *name;
	} names[] = {
		{ "48465300-0000-11AA-AA11-00306543ECAC", "Apple_HFS" },
		{ "7C3457EF-0000-11AA-AA11-00306543ECAC", "Apple_APFS" },
		{ "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", "Microsoft Basic Data" },
		{ "C12A7328-F81F-11D2-BA4B-00A0C93EC93B", "EFI" },
		{ "4C616265-6C00-11AA-AA11-00306543ECAC", "Apple_Label" },
	};
	size_t i;

	for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		if (strcasecmp(content, names[i].guid) == 0)
			return (names[i].name);
	return (content);
}

struct media {
	char	name[64];		/* diskNsM */
	char	content[64];
	bool	leaf;			/* no partitions on it */
};

/* The disk and its partitions, in order. */
static int
list_media(const char *disk, struct media *m, int max)
{
	io_service_t whole = IOServiceGetMatchingService(kIOMasterPortDefault,
	    IOBSDNameMatching(kIOMasterPortDefault, 0, disk));
	io_iterator_t it;
	io_registry_entry_t e;
	int n = 0;

	if (whole == MACH_PORT_NULL)
		return (0);
	strlcpy(m[n].name, disk, sizeof(m[n].name));
	string_property(whole, "Content", m[n].content, sizeof(m[n].content));
	m[n].leaf = true;
	n++;
	if (IORegistryEntryCreateIterator(whole, kIOServicePlane,
	    kIORegistryIterateRecursively, &it) == KERN_SUCCESS) {
		while (n < max && (e = IOIteratorNext(it)) != 0) {
			char name[64];

			if (IOObjectConformsTo(e, "IOMedia") &&
			    string_property(e, "BSD Name", name, sizeof(name))) {
				strlcpy(m[n].name, name, sizeof(m[n].name));
				string_property(e, "Content", m[n].content,
				    sizeof(m[n].content));
				m[n].leaf = true;
				m[0].leaf = false;
				n++;
			}
			IOObjectRelease(e);
		}
		IOObjectRelease(it);
	}
	IOObjectRelease(whole);
	/* A partition with partitions of its own is not a leaf. */
	for (int i = 1; i < n; i++)
		for (int j = 1; j < n; j++)
			if (j != i && strncmp(m[j].name, m[i].name,
			    strlen(m[i].name)) == 0 &&
			    m[j].name[strlen(m[i].name)] == 's')
				m[i].leaf = false;
	return (n);
}

/* The mount point a device is mounted on, if any. */
static bool
mounted_on(const char *dev, char *mp, size_t len)
{
	struct statfs *sf;
	int n = getmntinfo(&sf, MNT_NOWAIT), i;

	for (i = 0; i < n; i++)
		if (strcmp(sf[i].f_mntfromname, dev) == 0) {
			strlcpy(mp, sf[i].f_mntonname, len);
			return (true);
		}
	return (false);
}

struct volname {
	uint32_t	length;
	attrreference_t	name;
	char		buf[NAME_MAX * 3 + 1];
} __attribute__((packed));

static void
volume_name(const char *dev, const char *fstype, char *label, size_t len)
{
	struct attrlist al = { .bitmapcount = ATTR_BIT_MAP_COUNT,
	    .volattr = ATTR_VOL_INFO | ATTR_VOL_NAME };
	const char *argv[] = { "/sbin/mount", "-r", "-t", fstype, dev, NULL,
	    NULL };
	struct volname vn;
	char tmp[] = "/tmp/hdiutil.XXXXXX";

	if (mkdtemp(tmp) == NULL)
		return;
	argv[5] = tmp;
	if (run(argv, NULL, 0, true) == 0) {
		if (getattrlist(tmp, &al, &vn, sizeof(vn), 0) == 0 &&
		    vn.name.attr_length > 0)
			strlcpy(label, (char *)&vn.name + vn.name.attr_dataoffset,
			    len);
		unmount(tmp, 0);
	}
	rmdir(tmp);
}

/* Mounts a volume in /Volumes under its name (or the given point). */
static bool
mount_volume(const char *disk, const char *at, bool readonly, char *mp,
    size_t len)
{
	char dev[80], type[256] = "", label[256] = "", *nl;
	const char *argv[8];
	const char *fstype;
	int argc = 0, i;

	snprintf(dev, sizeof(dev), "/dev/%s", disk);
	argv[0] = "/usr/sbin/fstyp"; argv[1] = "-l"; argv[2] = dev; argv[3] = NULL;
	if (run(argv, type, sizeof(type), true) != 0)
		return (false);
	if ((nl = strchr(type, '\n')) != NULL)
		*nl = '\0';
	if (strncmp(type, "hfs", 3) == 0)
		fstype = "hfs";
	else if (strncmp(type, "msdos", 5) == 0)
		fstype = "msdos";
	else
		return (false);
	/* fstyp -l: "type label" */
	if (strchr(type, ' ') != NULL)
		strlcpy(label, strchr(type, ' ') + 1, sizeof(label));
	/* fstyp does not read HFS+'s name: a first mount tells it. */
	if (label[0] == '\0' && strcmp(fstype, "hfs") == 0)
		volume_name(dev, fstype, label, sizeof(label));
	if (label[0] == '\0' || strchr(label, '/') != NULL)
		strlcpy(label, "Untitled", sizeof(label));
	if (at != NULL) {
		strlcpy(mp, at, len);
	} else {
		snprintf(mp, len, "/Volumes/%s", label);
		for (i = 1; access(mp, F_OK) == 0; i++)
			snprintf(mp, len, "/Volumes/%s %d", label, i);
	}
	mkdir("/Volumes", 0755);
	if (mkdir(mp, 0755) != 0 && errno != EEXIST)
		return (false);
	argv[argc++] = "/sbin/mount";
	argv[argc++] = "-t";
	argv[argc++] = fstype;
	if (readonly)
		argv[argc++] = "-r";
	argv[argc++] = dev;
	argv[argc++] = mp;
	argv[argc] = NULL;
	if (run(argv, NULL, 0, true) != 0) {
		if (at == NULL)
			rmdir(mp);
		return (false);
	}
	return (true);
}

/* ---- DiskArbitration ------------------------------------------------------ */

static struct {
	DASessionRef	session;
	char		disk[64];	/* the attached image's whole disk */
	bool		approve;	/* our own mounts */
	bool		appeared;	/* DiskArbitration has the disk */
	bool		idle;		/* and is done with it */
} da;

static bool
da_ours(DADiskRef disk)
{
	const char *bsd = DADiskGetBSDName(disk);
	size_t n = strlen(da.disk);

	return (bsd != NULL && n > 0 && strncmp(bsd, da.disk, n) == 0 &&
	    (bsd[n] == '\0' || bsd[n] == 's'));
}

/* diskarbitrationd's own mounts of the image's volumes are ours to make. */
static DADissenterRef
da_approve_mount(DADiskRef disk, void *context)
{
	(void)context;
	if (!da.approve && da_ours(disk))
		return (DADissenterCreate(kCFAllocatorDefault,
		    kDAReturnExclusiveAccess, CFSTR("hdiutil")));
	return (NULL);
}

static void
da_appeared(DADiskRef disk, void *context)
{
	(void)context;
	if (da_ours(disk)) {
		da.appeared = true;
		da.idle = false;
	}
}

static void
da_idle(void *context)
{
	(void)context;
	if (da.appeared)
		da.idle = true;
}

static bool
da_open(void)
{
	da.session = DASessionCreate(kCFAllocatorDefault);
	if (da.session == NULL)
		return (false);
	DARegisterDiskMountApprovalCallback(da.session, NULL, da_approve_mount,
	    NULL);
	DARegisterDiskAppearedCallback(da.session, NULL, da_appeared, NULL);
	DARegisterIdleCallback(da.session, da_idle, NULL);
	DASessionScheduleWithRunLoop(da.session, CFRunLoopGetCurrent(),
	    kCFRunLoopDefaultMode);
	return (true);
}

/* Runs the session until cond, or seconds pass. */
static bool
da_wait(bool *cond, double seconds)
{
	CFAbsoluteTime end = CFAbsoluteTimeGetCurrent() + seconds;

	while (!*cond && CFAbsoluteTimeGetCurrent() < end)
		CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
	return (*cond);
}

struct da_mount_result {
	bool	done;
	bool	ok;
};

static void
da_mounted(DADiskRef disk, DADissenterRef dissenter, void *context)
{
	struct da_mount_result *r = context;

	(void)disk;
	r->ok = (dissenter == NULL);
	r->done = true;
}

/* Mounts a volume through DiskArbitration (in /Volumes, or at). */
static bool
da_mount(const char *bsd, const char *at, bool readonly, char *mp, size_t len)
{
	struct da_mount_result r = { false, false };
	CFStringRef args[2] = { readonly ? CFSTR("rdonly") : NULL, NULL };
	CFDictionaryRef desc;
	CFURLRef url = NULL, path;
	DADiskRef disk;
	bool ok = false;

	disk = DADiskCreateFromBSDName(kCFAllocatorDefault, da.session, bsd);
	if (disk == NULL)
		return (false);
	if (at != NULL)
		url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
		    (const UInt8 *)at, (CFIndex)strlen(at), true);
	da.approve = true;
	DADiskMountWithArguments(disk, url, kDADiskMountOptionDefault,
	    da_mounted, &r, args);
	da_wait(&r.done, 60);
	da.approve = false;
	if (r.ok && (desc = DADiskCopyDescription(disk)) != NULL) {
		path = CFDictionaryGetValue(desc, kDADiskDescriptionVolumePathKey);
		ok = path != NULL && CFURLGetFileSystemRepresentation(path, true,
		    (UInt8 *)mp, (CFIndex)len);
		CFRelease(desc);
	}
	if (url != NULL)
		CFRelease(url);
	CFRelease(disk);
	return (ok);
}

static void
print_media(const struct media *m, const char *mp)
{
	printf("/dev/%-15s\t%-31s\t%s\n", m->name,
	    m->content[0] ? content_name(m->content) : "", mp ? mp : "");
}

static int
attach(const char *path, bool readonly, bool nomount, const char *mountpoint)
{
	char abspath[PATH_MAX], disk[64], mp[PATH_MAX];
	uint64_t in[3], out[1] = { 0 };
	uint32_t outcnt = 1;
	struct media m[32];
	struct image img;
	const char *why;
	io_connect_t c;
	io_service_t dev;
	kern_return_t kr;
	int n, i, mounted = 0;
	bool usingda;

	if (realpath(path, abspath) == NULL)
		err(1, "%s", path);
	if (!image_layout(abspath, &img, &why)) {
		fprintf(stderr, "hdiutil: attach failed - %s\n", why);
		return (1);
	}
	/* Before the disk appears: its volumes are not to be mounted for us. */
	usingda = da_open();
	c = driver();
	if (!img.flat) {
		/* Compressed: its chunk table, read-only. */
		struct { uint32_t pathLength, chunkCount; uint64_t sectorCount; } hdr;
		size_t plen = strlen(abspath) + 1, pspace = (plen + 7) & ~(size_t)7;
		size_t size = sizeof(hdr) + pspace + img.nchunks * sizeof(struct udif_chunk);
		uint8_t *table = calloc(1, size);
		uint64_t flags = 0;

		if (strcmp(img.format, "ULMO") == 0) {
			fprintf(stderr, "hdiutil: attach failed - an lzma-compressed image (ULMO) "
			    "is not attached yet; hdiutil convert makes it UDZO\n");
			IOServiceClose(c);
			return (1);
		}
		if (table == NULL)
			err(1, "attach");
		hdr.pathLength = (uint32_t)plen;
		hdr.chunkCount = img.nchunks;
		hdr.sectorCount = img.sectors;
		memcpy(table, &hdr, sizeof(hdr));
		memcpy(table + sizeof(hdr), abspath, plen);
		memcpy(table + sizeof(hdr) + pspace, img.chunks, img.nchunks * sizeof(struct udif_chunk));
		kr = IOConnectCallMethod(c, kMinkarDiskImageAttachChunks, &flags, 1, table, size,
		    out, &outcnt, NULL, NULL);
		free(table);
		readonly = true;
		image_free(&img);
		goto attached;
	}
	in[0] = img.offset;
	in[1] = img.length;
	in[2] = readonly ? kMinkarDiskImageReadOnly : 0;
	kr = IOConnectCallMethod(c, kMinkarDiskImageAttach, in, 3, abspath,
	    strlen(abspath) + 1, out, &outcnt, NULL, NULL);
	if (kr == kIOReturnNotWritable && !readonly) {
		/* A read-only file attaches read-only, as on macOS. */
		readonly = true;
		in[2] = kMinkarDiskImageReadOnly;
		kr = IOConnectCallMethod(c, kMinkarDiskImageAttach, in, 3,
		    abspath, strlen(abspath) + 1, out, &outcnt, NULL, NULL);
	}
attached:
	IOServiceClose(c);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "hdiutil: attach failed - %s\n",
		    kr == kIOReturnNotPrivileged ? "Permission denied" :
		    kr == kIOReturnUnsupported ? "the image's compression is not supported" :
		    mach_error_string(kr));
		return (1);
	}
	dev = image_device(out[0]);
	if (dev == MACH_PORT_NULL || !device_disk(dev, disk, sizeof(disk), 50)) {
		fprintf(stderr, "hdiutil: attach failed - no disk appeared\n");
		return (1);
	}
	IOObjectRelease(dev);
	if (usingda) {
		/* diskarbitrationd probes the volumes: let it finish. */
		strlcpy(da.disk, disk, sizeof(da.disk));
		if (da_wait(&da.appeared, 10))
			da_wait(&da.idle, 30);
	} else
		usleep(500000);	/* the partitions are published after */
	n = list_media(disk, m, 32);
	{
		char mps[32][PATH_MAX];
		bool ok[32];

		for (i = 0; i < n; i++) {
			ok[i] = false;
			if (!nomount && m[i].leaf && usingda)
				ok[i] = da_mount(m[i].name, mounted == 0 ? mountpoint :
				    NULL, readonly, mps[i], sizeof(mps[i]));
			else if (!nomount && m[i].leaf)
				ok[i] = mount_volume(m[i].name, mounted == 0 ?
				    mountpoint : NULL, readonly, mps[i], sizeof(mps[i]));
			if (ok[i])
				mounted++;
		}
		/* Nothing mounted when asked to (a mount point that is not
		 * there, a volume that will not mount): the image goes. */
		if (!nomount && mounted == 0) {
			uint64_t id = out[0];

			c = driver();
			IOConnectCallMethod(c, kMinkarDiskImageDetach, &id, 1, NULL, 0,
			    NULL, NULL, NULL, NULL);
			IOServiceClose(c);
			fprintf(stderr, "hdiutil: attach failed - no mountable file systems\n");
			return (1);
		}
		for (i = 0; i < n; i++)
			print_media(&m[i], ok[i] ? mps[i] : NULL);
	}
	(void)mp;
	return (0);
}

/* "/dev/disk3s1", "disk3", a mount point -> "disk3" */
static bool
whole_disk(const char *arg, char *disk, size_t len)
{
	struct statfs sf;
	const char *p = arg;
	size_t n;

	if (strncmp(p, "/dev/", 5) == 0)
		p += 5;
	else if (strncmp(p, "disk", 4) != 0) {
		if (statfs(arg, &sf) != 0 ||
		    strncmp(sf.f_mntfromname, "/dev/", 5) != 0)
			return (false);
		p = sf.f_mntfromname + 5;
	}
	if (*p == 'r')
		p++;
	if (strncmp(p, "disk", 4) != 0)
		return (false);
	n = 4 + strspn(p + 4, "0123456789");
	if (n == 4 || n >= len)
		return (false);
	memcpy(disk, p, n);
	disk[n] = '\0';
	return (true);
}

static int
detach(const char *arg, bool force)
{
	char disk[64], mp[PATH_MAX], dev[80];
	struct media m[32];
	io_service_t svc;
	io_connect_t c;
	uint64_t id = 0;
	kern_return_t kr;
	int n, i;

	if (!whole_disk(arg, disk, sizeof(disk)) ||
	    (svc = find_device(disk)) == MACH_PORT_NULL) {
		fprintf(stderr, "hdiutil: detach failed - No such file or "
		    "directory\n");
		return (1);
	}
	n = list_media(disk, m, 32);
	for (i = n - 1; i >= 0; i--) {
		snprintf(dev, sizeof(dev), "/dev/%s", m[i].name);
		if (mounted_on(dev, mp, sizeof(mp))) {
			if (unmount(mp, force ? MNT_FORCE : 0) != 0) {
				fprintf(stderr, "hdiutil: couldn't unmount "
				    "\"%s\" - Resource busy\n", m[i].name);
				IOObjectRelease(svc);
				return (1);
			}
			if (strncmp(mp, "/Volumes/", 9) == 0)
				rmdir(mp);
		}
	}
	IORegistryEntryGetRegistryEntryID(svc, &id);
	IOObjectRelease(svc);
	c = driver();
	/* A prober (diskarbitrationd's) may hold the disk open a moment. */
	for (i = 0; i < 25; i++) {
		kr = IOConnectCallMethod(c, kMinkarDiskImageDetach, &id, 1, NULL,
		    0, NULL, NULL, NULL, NULL);
		if (kr != kIOReturnBusy)
			break;
		usleep(200000);
	}
	IOServiceClose(c);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "hdiutil: detach failed - %s\n",
		    kr == kIOReturnBusy ? "Resource busy" : mach_error_string(kr));
		return (1);
	}
	printf("\"%s\" ejected.\n", disk);
	return (0);
}

/* ---- info and imageinfo ---------------------------------------------------- */

static int
info(void)
{
	io_iterator_t it;
	io_service_t dev;
	char path[PATH_MAX], disk[64], mp[PATH_MAX], dev_path[80];
	struct media m[32];

	printf("framework       : 1.0\ndriver          : 1.0\n");
	if (IOServiceGetMatchingServices(kIOMasterPortDefault,
	    IOServiceMatching("MinkarDiskImageDevice"), &it) != KERN_SUCCESS)
		return (0);
	while ((dev = IOIteratorNext(it)) != 0) {
		bool ro = false;
		int n, i;

		string_property(dev, "image-path", path, sizeof(path));
		printf("================================================\n");
		printf("image-path      : %s\n", path);
		if (device_disk(dev, disk, sizeof(disk), 10)) {
			io_service_t whole = IOServiceGetMatchingService(
			    kIOMasterPortDefault, IOBSDNameMatching(
			    kIOMasterPortDefault, 0, disk));

			if (whole != MACH_PORT_NULL) {
				CFTypeRef w = IORegistryEntryCreateCFProperty(whole,
				    CFSTR("Writable"), NULL, 0);
				CFTypeRef s = IORegistryEntryCreateCFProperty(whole,
				    CFSTR("Size"), NULL, 0);
				long long size = 0;

				ro = w == kCFBooleanFalse;
				if (s != NULL)
					CFNumberGetValue(s, kCFNumberLongLongType,
					    &size);
				printf("writeable       : %s\n", ro ? "FALSE" :
				    "TRUE");
				printf("blockcount      : %lld\n", size / SECTOR);
				printf("blocksize       : %d\n", SECTOR);
				if (w != NULL)
					CFRelease(w);
				if (s != NULL)
					CFRelease(s);
				IOObjectRelease(whole);
			}
			n = list_media(disk, m, 32);
			for (i = 0; i < n; i++) {
				snprintf(dev_path, sizeof(dev_path), "/dev/%s",
				    m[i].name);
				print_media(&m[i], mounted_on(dev_path, mp,
				    sizeof(mp)) ? mp : NULL);
			}
		}
		IOObjectRelease(dev);
	}
	IOObjectRelease(it);
	return (0);
}

static int
imageinfo(const char *path)
{
	struct image img;
	const char *why;

	if (!image_layout(path, &img, &why)) {
		fprintf(stderr, "hdiutil: imageinfo failed - %s\n", why);
		return (1);
	}
	printf("Format Description: %s\n", image_format_description(img.format));
	printf("Format: %s\n", img.format);
	printf("Size Information:\n");
	printf("\tTotal Bytes: %llu\n", (unsigned long long)(img.sectors * SECTOR));
	printf("\tSector Count: %llu\n", (unsigned long long)img.sectors);
	if (img.flat)
		printf("\tData Offset: %llu\n", (unsigned long long)img.offset);
	else {
		uint64_t stored = 0;

		for (uint32_t i = 0; i < img.nchunks; i++)
			stored += img.chunks[i].length;
		printf("\tCompressed Bytes: %llu\n", (unsigned long long)stored);
		printf("\tChunk Count: %u\n", img.nchunks);
	}
	image_free(&img);
	return (0);
}

/* ---- create ---------------------------------------------------------------- */

static uint64_t
folder_size(const char *dir)
{
	const char *argv[] = { "/usr/bin/du", "-sk", dir, NULL };
	char out[256];

	if (run(argv, out, sizeof(out), true) != 0)
		errx(1, "%s: can't measure the folder", dir);
	/* Room for the file system's own structures. */
	return ((uint64_t)strtoull(out, NULL, 10) * 1024 * 5 / 4 + (8 << 20));
}

static int
create(const char *image, uint64_t size, const char *fs, const char *volname,
    const char *layout, const char *srcfolder, bool overwrite, const char *format, int level)
{
	char path[PATH_MAX], disk[64], part[80], mp[PATH_MAX];
	const char *type;
	bool hfs, gpt, journaled;
	uint64_t first = 0, last = 0;
	struct media m[8];
	io_connect_t c;
	io_service_t dev;
	uint64_t in[3], out[1];
	uint32_t outcnt = 1;
	int fd, rc = 0;
	bool usingda;

	if (fs == NULL)
		fs = "HFS+";
	hfs = strcasecmp(fs, "HFS+") == 0 || strcasecmp(fs, "HFS+J") == 0 ||
	    strcasecmp(fs, "Journaled HFS+") == 0 ||
	    strcasecmp(fs, "Case-sensitive HFS+") == 0 ||
	    strcasecmp(fs, "Case-sensitive Journaled HFS+") == 0;
	journaled = strcasestr(fs, "Journaled") != NULL ||
	    strcasecmp(fs, "HFS+J") == 0;
	if (!hfs && strcasecmp(fs, "MS-DOS") != 0 &&
	    strcasecmp(fs, "FAT32") != 0 && strcasecmp(fs, "MS-DOS FAT32") != 0 &&
	    strcasecmp(fs, "MS-DOS FAT16") != 0)
		errx(1, "%s: file system not supported (HFS+, Journaled HFS+, "
		    "MS-DOS)", fs);
	if (layout == NULL)
		layout = "GPTSPUD";
	if (strcasecmp(layout, "GPTSPUD") == 0)
		gpt = true;
	else if (strcasecmp(layout, "NONE") == 0)
		gpt = false;
	else
		errx(1, "%s: layout not supported (GPTSPUD, NONE)", layout);
	if (volname == NULL)
		volname = "untitled";
	if (srcfolder != NULL && size == 0)
		size = folder_size(srcfolder);
	if (size == 0)
		errx(1, "create: -size or -srcfolder is needed");
	if (size < (1 << 20))
		size = 1 << 20;

	/* The name gets .dmg unless it has an extension. */
	strlcpy(path, image, sizeof(path));
	if (strchr(strrchr(path, '/') ? strrchr(path, '/') : path, '.') == NULL)
		strlcat(path, ".dmg", sizeof(path));
	if (!overwrite && access(path, F_OK) == 0) {
		fprintf(stderr, "hdiutil: create failed - File exists\n");
		return (1);
	}
	if ((fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0)
		err(1, "%s", path);
	if (ftruncate(fd, (off_t)size) != 0)
		err(1, "%s", path);
	type = hfs ? "48465300-0000-11AA-AA11-00306543ECAC" :
	    "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7";
	if (gpt)
		write_gpt(fd, size, type, &first, &last);
	close(fd);

	/* Attach it bare and format the partition (or the whole disk). */
	if (realpath(path, mp) == NULL)
		err(1, "%s", path);
	strlcpy(path, mp, sizeof(path));
	/* diskarbitrationd is not to mount the new volume on its own. */
	usingda = da_open();
	c = driver();
	in[0] = 0; in[1] = 0; in[2] = 0;
	if (IOConnectCallMethod(c, kMinkarDiskImageAttach, in, 3, path,
	    strlen(path) + 1, out, &outcnt, NULL, NULL) != KERN_SUCCESS)
		errx(1, "create failed - can't attach the new image");
	IOServiceClose(c);
	dev = image_device(out[0]);
	if (dev == MACH_PORT_NULL || !device_disk(dev, disk, sizeof(disk), 50))
		errx(1, "create failed - no disk appeared");
	IOObjectRelease(dev);
	if (usingda) {
		/* It probes the bare disk first: let it finish. */
		strlcpy(da.disk, disk, sizeof(da.disk));
		if (da_wait(&da.appeared, 10))
			da_wait(&da.idle, 30);
	}
	if (gpt) {
		int tries;

		/* Wait for the partition. */
		for (tries = 0; tries < 50 && list_media(disk, m, 8) < 2; tries++)
			usleep(100000);
		snprintf(part, sizeof(part), "/dev/r%ss1", disk);
	} else {
		snprintf(part, sizeof(part), "/dev/r%s", disk);
	}
	if (hfs) {
		const char *argv[] = { "/sbin/newfs_hfs", "-v", volname,
		    journaled ? "-J" : NULL, NULL, NULL };

		if (journaled)
			argv[4] = part;
		else
			argv[3] = part;
		if (strcasestr(fs, "Case-sensitive") != NULL)
			errx(1, "case-sensitive HFS+ is made with newfs_hfs -s");
		rc = run(argv, NULL, 0, true);
	} else {
		char label[12];
		const char *argv[] = { "/sbin/newfs_msdos", "-v", label, part,
		    NULL };
		size_t i;

		strlcpy(label, volname, sizeof(label));
		for (i = 0; label[i] != '\0'; i++)
			label[i] = (char)toupper((unsigned char)label[i]);
		rc = run(argv, NULL, 0, true);
	}
	if (rc != 0) {
		detach(disk, true);
		errx(1, "create failed - the file system could not be made");
	}
	if (srcfolder != NULL) {
		char src[PATH_MAX + 2], vol[80];
		const char *argv[] = { "/bin/cp", "-pR", src, mp, NULL };

		snprintf(vol, sizeof(vol), "%ss1", disk);
		if (!(usingda ? da_mount(gpt ? vol : disk, NULL, false, mp,
		    sizeof(mp)) : mount_volume(gpt ? vol : disk, NULL, false, mp,
		    sizeof(mp)))) {
			detach(disk, true);
			errx(1, "create failed - can't mount the new volume");
		}
		snprintf(src, sizeof(src), "%s/.", srcfolder);
		rc = run(argv, NULL, 0, false);
	}
	{
		int saved = dup(STDOUT_FILENO), null = open("/dev/null", O_WRONLY);

		/* detach's "ejected" is not create's to say */
		fflush(stdout);
		dup2(null, STDOUT_FILENO);
		detach(disk, false);
		fflush(stdout);
		dup2(saved, STDOUT_FILENO);
		close(null);
		close(saved);
	}
	if (rc != 0)
		errx(1, "create failed - copying %s", srcfolder);
	/* A read-only or compressed format: the raw image made, converted. */
	if (format != NULL && (strcmp(format, "UDZO") == 0 || strcmp(format, "UDBZ") == 0 ||
	    strcmp(format, "ULFO") == 0 || strcmp(format, "UDRO") == 0)) {
		char raw[PATH_MAX];

		snprintf(raw, sizeof(raw), "%s.raw-%d", path, (int)getpid());
		if (rename(path, raw) != 0)
			err(1, "%s", path);
		rc = image_convert(raw, path, format, level);
		unlink(raw);
		if (rc != 0)
			return (1);
	}
	if (!quiet)
		printf("created: %s\n", path);
	return (0);
}

/* Name with the extension a format's images take, unless it has one. */
static void
image_name(const char *name, const char *format, char *out, size_t len)
{
	const char *base = strrchr(name, '/') ? strrchr(name, '/') + 1 : name;

	strlcpy(out, name, len);
	if (strchr(base, '.') == NULL)
		strlcat(out, strcmp(format, "UDTO") == 0 ? ".cdr" : ".dmg", len);
}

static int
convert(const char *src, const char *dst, const char *format, bool overwrite, int level)
{
	char path[PATH_MAX];

	image_name(dst, format, path, sizeof(path));
	if (!overwrite && access(path, F_OK) == 0) {
		fprintf(stderr, "hdiutil: convert failed - File exists\n");
		return (1);
	}
	if (image_convert(src, path, format, level) != 0)
		return (1);
	if (!quiet)
		printf("created: %s\n", path);
	return (0);
}

/* -imagekey zlib-level=N */
static int
zlib_level(const char *key)
{
	if (strncmp(key, "zlib-level=", 11) == 0)
		return (atoi(key + 11));
	return (0);
}

/* ---- Commands ---------------------------------------------------------------- */

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: hdiutil <verb> <options>\n"
	    "<verb> is one of the following:\n"
	    "help\n"
	    "attach\n"
	    "detach\n"
	    "eject\n"
	    "create\n"
	    "convert\n"
	    "info\n"
	    "imageinfo\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *verb;
	int i;

	if (argc < 2)
		usage();
	verb = argv[1];
	for (i = 2; i < argc; i++)
		if (strcmp(argv[i], "-quiet") == 0)
			quiet = true;
	if (strcmp(verb, "help") == 0 || strcmp(verb, "-help") == 0)
		usage();
	if (strcmp(verb, "info") == 0)
		return (info());
	if (strcmp(verb, "attach") == 0 || strcmp(verb, "mount") == 0) {
		const char *image = NULL, *mountpoint = NULL;
		bool ro = false, nomount = false;

		for (i = 2; i < argc; i++) {
			if (strcmp(argv[i], "-readonly") == 0)
				ro = true;
			else if (strcmp(argv[i], "-nomount") == 0)
				nomount = true;
			else if (strcmp(argv[i], "-mountpoint") == 0 && i + 1 < argc)
				mountpoint = argv[++i];
			else if (argv[i][0] == '-')
				continue;	/* -nobrowse, -noverify... */
			else
				image = argv[i];
		}
		if (image == NULL)
			usage();
		return (attach(image, ro, nomount, mountpoint));
	}
	if (strcmp(verb, "detach") == 0 || strcmp(verb, "eject") == 0) {
		const char *target = NULL;
		bool force = false;

		for (i = 2; i < argc; i++) {
			if (strcmp(argv[i], "-force") == 0)
				force = true;
			else if (argv[i][0] != '-')
				target = argv[i];
		}
		if (target == NULL)
			usage();
		return (detach(target, force));
	}
	if (strcmp(verb, "imageinfo") == 0) {
		for (i = 2; i < argc; i++)
			if (argv[i][0] != '-')
				return (imageinfo(argv[i]));
		usage();
	}
	if (strcmp(verb, "create") == 0) {
		const char *image = NULL, *fs = NULL, *volname = NULL;
		const char *layout = NULL, *src = NULL, *format = NULL;
		uint64_t size = 0;
		bool ov = false;
		int level = 0;

		for (i = 2; i < argc; i++) {
			const char *a = argv[i];
			const char *v = i + 1 < argc ? argv[i + 1] : NULL;

			if (strcmp(a, "-size") == 0 && v) {
				size = parse_size(v); i++;
			} else if (strcmp(a, "-sectors") == 0 && v) {
				size = strtoull(v, NULL, 10) * SECTOR; i++;
			} else if (strcmp(a, "-fs") == 0 && v) {
				fs = v; i++;
			} else if (strcmp(a, "-volname") == 0 && v) {
				volname = v; i++;
			} else if (strcmp(a, "-layout") == 0 && v) {
				layout = v; i++;
			} else if (strcmp(a, "-srcfolder") == 0 && v) {
				src = v; i++;
			} else if (strcmp(a, "-format") == 0 && v) {
				/* UDZO, UDBZ, ULFO, UDRO: converted when made;
				   the others are raw images */
				format = v; i++;
			} else if (strcmp(a, "-type") == 0 && v) {
				i++;		/* SPARSE, UDIF: raw images */
			} else if (strcmp(a, "-imagekey") == 0 && v) {
				level = zlib_level(v); i++;
			} else if (strcmp(a, "-ov") == 0) {
				ov = true;
			} else if (a[0] == '-') {
				continue;
			} else {
				image = a;
			}
		}
		if (image == NULL)
			usage();
		return (create(image, size, fs, volname, layout, src, ov, format, level));
	}
	if (strcmp(verb, "convert") == 0) {
		const char *src = NULL, *dst = NULL, *format = NULL;
		bool ov = false;
		int level = 0;

		for (i = 2; i < argc; i++) {
			const char *a = argv[i];
			const char *v = i + 1 < argc ? argv[i + 1] : NULL;

			if (strcmp(a, "-format") == 0 && v) {
				format = v; i++;
			} else if (strcmp(a, "-o") == 0 && v) {
				dst = v; i++;
			} else if (strcmp(a, "-imagekey") == 0 && v) {
				level = zlib_level(v); i++;
			} else if (strcmp(a, "-ov") == 0) {
				ov = true;
			} else if (a[0] == '-') {
				continue;
			} else {
				src = a;
			}
		}
		if (src == NULL || dst == NULL || format == NULL)
			usage();
		return (convert(src, dst, format, ov, level));
	}
	fprintf(stderr, "hdiutil: %s: verb not recognized\n", verb);
	usage();
}
