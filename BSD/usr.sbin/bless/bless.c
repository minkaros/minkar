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
 * from).
 *
 * --setBoot (with --mount, --device or --folder: a volume) makes that
 * volume start: Minkar's loader and kernel cache, as the volume the system
 * started from has them in its disk's EFI System Partition (or the loader
 * --bootefi names), go into the EFI System Partition of the volume's disk
 * (/EFI/BOOT/BOOTX64.EFI, the firmware's default path, and
 * /Minkar/kernelcache), with /Minkar/org.minkar.boot.plist naming the
 * volume (boot-uuid) and keeping the rest of the running system's boot
 * settings. NVRAM variables can't be written on this system yet: the
 * firmware starts the disk by its default path.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>

#include <sys/param.h>
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
#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>

extern char **environ;

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

#define	EFI_TYPE	"C12A7328-F81F-11D2-BA4B-00A0C93EC93B"

/* A disk's slice that is an EFI System Partition: diskN -> diskNsM. */
static bool
esp_of(const char *whole, char *esp, size_t len)
{
	for (int n = 1; n <= 32; n++) {
		char bsd[64];
		io_service_t media;
		CFTypeRef content;
		bool found = false;

		snprintf(bsd, sizeof(bsd), "%ss%d", whole, n);
		media = IOServiceGetMatchingService(kIOMasterPortDefault,
		    IOBSDNameMatching(kIOMasterPortDefault, 0, bsd));
		if (media == IO_OBJECT_NULL)
			continue;
		content = IORegistryEntryCreateCFProperty(media, CFSTR(kIOMediaContentKey), NULL, 0);
		if (content != NULL && CFGetTypeID(content) == CFStringGetTypeID() &&
		    CFStringCompare(content, CFSTR(EFI_TYPE), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
			found = true;
		if (content != NULL)
			CFRelease(content);
		IOObjectRelease(media);
		if (found) {
			strlcpy(esp, bsd, len);
			return (true);
		}
	}
	return (false);
}

/* diskNsM -> diskN */
static void
whole_of(const char *bsd, char *whole, size_t len)
{
	const char *s;

	strlcpy(whole, bsd, len);
	if ((s = strchr(bsd + 4, 's')) != NULL && (size_t)(s - bsd) < len)
		whole[s - bsd] = '\0';
}

static int
run(const char *const argv[])
{
	pid_t pid;
	int status;

	if (posix_spawn(&pid, argv[0], NULL, NULL, (char *const *)argv, environ) != 0)
		return (-1);
	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
			return (-1);
	return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

/* Where an EFI System Partition is mounted: where it already is, or a new
 * directory (*mounted says to unmount it after). */
static bool
mount_esp(const char *esp, char *dir, size_t len, bool *mounted)
{
	struct statfs *mnt;
	char dev[80];
	int n;

	snprintf(dev, sizeof(dev), "/dev/%s", esp);
	n = getmntinfo(&mnt, MNT_NOWAIT);
	for (int i = 0; i < n; i++)
		if (strcmp(mnt[i].f_mntfromname, dev) == 0) {
			strlcpy(dir, mnt[i].f_mntonname, len);
			*mounted = false;
			return (true);
		}
	strlcpy(dir, "/tmp/bless.XXXXXX", len);
	if (mkdtemp(dir) == NULL)
		return (false);
	{
		const char *argv[] = { "/sbin/mount_msdos", dev, dir, NULL };

		if (run(argv) != 0) {
			rmdir(dir);
			return (false);
		}
	}
	*mounted = true;
	return (true);
}

static void
unmount_esp(const char *dir, bool mounted)
{
	if (!mounted)
		return;
	(void)unmount(dir, 0);
	rmdir(dir);
}

static bool
copy_file(const char *from, const char *to)
{
	char buf[65536], tmp[MAXPATHLEN];
	ssize_t n;
	int in, out;
	bool ok = true;

	if ((in = open(from, O_RDONLY)) < 0)
		return (false);
	snprintf(tmp, sizeof(tmp), "%s.new", to);
	if ((out = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
		close(in);
		return (false);
	}
	while ((n = read(in, buf, sizeof(buf))) > 0)
		if (write(out, buf, (size_t)n) != n) {
			ok = false;
			break;
		}
	if (n < 0 || fsync(out) != 0)
		ok = false;
	close(in);
	close(out);
	if (!ok || rename(tmp, to) != 0) {
		unlink(tmp);
		return (false);
	}
	return (true);
}

static void
make_dir(const char *root, const char *sub)
{
	char path[MAXPATHLEN];

	snprintf(path, sizeof(path), "%s/%s", root, sub);
	(void)mkdir(path, 0755);
}

/* A volume's UUID, as diskarbitrationd says it (the loader's boot-uuid). */
static bool
volume_uuid(const char *bsd, char *buf, size_t len)
{
	DASessionRef session = DASessionCreate(kCFAllocatorDefault);
	DADiskRef disk;
	CFDictionaryRef d;
	CFUUIDRef uuid;
	bool ok = false;

	if (session == NULL)
		return (false);
	if ((disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, bsd)) != NULL) {
		if ((d = DADiskCopyDescription(disk)) != NULL) {
			uuid = CFDictionaryGetValue(d, kDADiskDescriptionVolumeUUIDKey);
			if (uuid != NULL && CFGetTypeID(uuid) == CFUUIDGetTypeID()) {
				CFStringRef s = CFUUIDCreateString(kCFAllocatorDefault, uuid);

				ok = CFStringGetCString(s, buf, len, kCFStringEncodingUTF8);
				CFRelease(s);
			}
			CFRelease(d);
		}
		CFRelease(disk);
	}
	CFRelease(session);
	return (ok);
}

static int
set_boot(const char *volume, const char *bootefi)
{
	struct statfs tsf, rsf;
	char tbsd[64], twhole[64], tesp[64], rwhole[64], resp[64];
	char tdir[MAXPATHLEN], rdir[MAXPATHLEN], from[MAXPATHLEN], to[MAXPATHLEN];
	char uuid[64];
	bool tmounted = false, rmounted = false, same;
	CFMutableDictionaryRef plist = NULL;
	int rc = 1;

	if (volume == NULL)
		volume = "/";
	if (statfs(volume, &tsf) != 0)
		err(1, "%s", volume);
	if (strncmp(tsf.f_mntfromname, "/dev/disk", 9) != 0 || strcmp(tsf.f_fstypename, "hfs") != 0)
		errx(1, "%s is not an HFS+ volume on a disk", volume);
	strlcpy(tbsd, tsf.f_mntfromname + 5, sizeof(tbsd));
	whole_of(tbsd, twhole, sizeof(twhole));
	if (!esp_of(twhole, tesp, sizeof(tesp)))
		errx(1, "%s has no EFI System Partition", twhole);
	if (!volume_uuid(tbsd, uuid, sizeof(uuid)))
		errx(1, "%s: no volume UUID", tbsd);
	if (statfs("/", &rsf) != 0)
		err(1, "/");
	whole_of(rsf.f_mntfromname + 5, rwhole, sizeof(rwhole));
	if (!esp_of(rwhole, resp, sizeof(resp)))
		errx(1, "%s (the system's disk) has no EFI System Partition", rwhole);
	same = strcmp(resp, tesp) == 0;

	if (!mount_esp(tesp, tdir, sizeof(tdir), &tmounted))
		errx(1, "Could not mount %s", tesp);
	if (same)
		strlcpy(rdir, tdir, sizeof(rdir));
	else if (!mount_esp(resp, rdir, sizeof(rdir), &rmounted)) {
		warnx("Could not mount %s", resp);
		goto out;
	}

	make_dir(tdir, "EFI");
	make_dir(tdir, "EFI/BOOT");
	make_dir(tdir, "Minkar");
	/* The loader, at the firmware's default path. */
	if (bootefi != NULL)
		strlcpy(from, bootefi, sizeof(from));
	else
		snprintf(from, sizeof(from), "%s/EFI/BOOT/BOOTX64.EFI", rdir);
	snprintf(to, sizeof(to), "%s/EFI/BOOT/BOOTX64.EFI", tdir);
	if (!same || bootefi != NULL) {
		if (!copy_file(from, to)) {
			warn("%s", from);
			goto out;
		}
		snprintf(from, sizeof(from), "%s/Minkar/kernelcache", rdir);
		snprintf(to, sizeof(to), "%s/Minkar/kernelcache", tdir);
		if (!copy_file(from, to)) {
			warn("%s", from);
			goto out;
		}
	}
	/* The boot settings: the running system's, the volume this one. */
	snprintf(from, sizeof(from), "%s/Minkar/org.minkar.boot.plist", rdir);
	{
		CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)from, strlen(from), false);
		CFReadStreamRef in = CFReadStreamCreateWithFile(NULL, url);
		CFPropertyListRef old = NULL;

		if (in != NULL && CFReadStreamOpen(in)) {
			old = CFPropertyListCreateWithStream(NULL, in, 0, kCFPropertyListImmutable, NULL, NULL);
			CFReadStreamClose(in);
		}
		if (in != NULL)
			CFRelease(in);
		CFRelease(url);
		if (old != NULL && CFGetTypeID(old) == CFDictionaryGetTypeID())
			plist = CFDictionaryCreateMutableCopy(NULL, 0, old);
		else
			plist = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
			    &kCFTypeDictionaryValueCallBacks);
		if (old != NULL)
			CFRelease(old);
	}
	{
		CFStringRef u = CFStringCreateWithCString(NULL, uuid, kCFStringEncodingUTF8);
		CFDataRef data;

		CFDictionarySetValue(plist, CFSTR("boot-uuid"), u);
		CFDictionarySetValue(plist, CFSTR("boot-file"), CFSTR("\\Minkar\\kernelcache"));
		CFRelease(u);
		data = CFPropertyListCreateData(NULL, plist, kCFPropertyListXMLFormat_v1_0, 0, NULL);
		snprintf(to, sizeof(to), "%s/Minkar/org.minkar.boot.plist", tdir);
		{
			int fd = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			bool ok = fd >= 0 && data != NULL &&
			    write(fd, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data)) == CFDataGetLength(data) &&
			    fsync(fd) == 0;

			if (fd >= 0)
				close(fd);
			if (data != NULL)
				CFRelease(data);
			if (!ok) {
				warn("%s", to);
				goto out;
			}
		}
	}
	if (!quiet)
		printf("%s (%s) starts from %s's EFI System Partition (%s)\n", volume, uuid, twhole, tesp);
	if (verbose)
		printf("NVRAM variables can't be written on this system yet: the firmware starts "
		    "%s by its default path\n", twhole);
	rc = 0;
out:
	if (plist != NULL)
		CFRelease(plist);
	if (!same)
		unmount_esp(rdir, rmounted);
	unmount_esp(tdir, tmounted);
	return (rc);
}

/* Where a device ("disk2s2", "/dev/disk2s2") is mounted. */
static const char *
device_mount(const char *device)
{
	static char dir[MAXPATHLEN];
	struct statfs *mnt;
	char dev[80];
	int n;

	snprintf(dev, sizeof(dev), "%s%s", strncmp(device, "/dev/", 5) == 0 ? "" : "/dev/", device);
	n = getmntinfo(&mnt, MNT_NOWAIT);
	for (int i = 0; i < n; i++)
		if (strcmp(mnt[i].f_mntfromname, dev) == 0) {
			strlcpy(dir, mnt[i].f_mntonname, sizeof(dir));
			return (dir);
		}
	errx(1, "%s is not mounted", device);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: bless [--info] [mountpoint]\n"
	    "       bless --folder directory [--file file] [--setBoot]\n"
	    "       bless --mount mountpoint | --device device --setBoot [--bootefi file]\n"
	    "       bless --unbless directory\n"
	    "       bless --getBoot\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	enum { O_INFO = 1, O_FOLDER, O_FILE, O_MOUNT, O_DEVICE, O_SETBOOT,
	    O_GETBOOT, O_UNBLESS, O_VERBOSE, O_QUIET, O_IGNORED, O_IGNORED_ARG, O_BOOTEFI };
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
		{ "bootefi",	required_argument,	NULL, O_BOOTEFI },
		{ NULL, 0, NULL, 0 }
	};
	const char *folder = NULL, *file = NULL, *unblessed = NULL;
	const char *info_path = NULL, *volume = NULL, *bootefi = NULL;
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
			target = true;
			volume = optarg;
			break;
		case O_DEVICE:
			target = true;
			volume = device_mount(optarg);
			break;
		case O_BOOTEFI:
			bootefi = optarg;
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
		rc = set_boot(volume != NULL ? volume : folder, bootefi);
	return (rc);
}
