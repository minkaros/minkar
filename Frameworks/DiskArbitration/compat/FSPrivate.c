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
 * The name of a volume's format, read from its header (HFS Plus and
 * HFSX, FAT12/16/32, exFAT, ISO 9660) or from the mounted volume.
 */

#include <sys/param.h>
#include <sys/mount.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <FSPrivate.h>

#define HFS_JOURNALED_BIT	13	/* in the volume header's attributes */

static uint16_t
be16(const uint8_t *p)
{
	return ((uint16_t)(p[0] << 8 | p[1]));
}

static uint32_t
be32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
	    (uint32_t)p[2] << 8 | p[3]);
}

static uint16_t
le16(const uint8_t *p)
{
	return ((uint16_t)(p[0] | p[1] << 8));
}

static uint32_t
le32(const uint8_t *p)
{
	return ((uint32_t)p[0] | (uint32_t)p[1] << 8 |
	    (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24);
}

static const char *
hfs_name(bool caseSensitive, bool journaled)
{
	if (caseSensitive)
		return (journaled ? "Mac OS Extended (Case-sensitive, Journaled)" :
		    "Mac OS Extended (Case-sensitive)");
	return (journaled ? "Mac OS Extended (Journaled)" : "Mac OS Extended");
}

/* FAT's type is its count of clusters (Microsoft's FAT specification). */
static const char *
fat_name(const uint8_t *b)
{
	uint32_t bps = le16(b + 11), spc = b[13], rsvd = le16(b + 14);
	uint32_t nfats = b[16], rootents = le16(b + 17);
	uint32_t total = le16(b + 19), fatsz = le16(b + 22);
	uint32_t rootsecs, data, clusters;

	if (b[510] != 0x55 || b[511] != 0xaa)
		return (NULL);
	if (bps < 512 || bps > 4096 || (bps & (bps - 1)) != 0 || spc == 0 ||
	    (spc & (spc - 1)) != 0 || nfats == 0 || rsvd == 0)
		return (NULL);
	if (total == 0)
		total = le32(b + 32);
	if (fatsz == 0)
		fatsz = le32(b + 36);
	rootsecs = (rootents * 32 + bps - 1) / bps;
	if (total <= rsvd + nfats * fatsz + rootsecs)
		return (NULL);
	data = total - (rsvd + nfats * fatsz + rootsecs);
	clusters = data / spc;
	if (clusters < 4085)
		return ("MS-DOS (FAT12)");
	if (clusters < 65525)
		return ("MS-DOS (FAT16)");
	return ("MS-DOS (FAT32)");
}

static CFStringRef
copy_name(const char *name)
{
	if (name == NULL)
		return (NULL);
	return (CFStringCreateWithCString(kCFAllocatorDefault, name,
	    kCFStringEncodingUTF8));
}

CFStringRef
_FSCopyNameForVolumeFormatAtNode(CFStringRef devicePath)
{
	char path[MAXPATHLEN];
	uint8_t *buf;
	const char *name = NULL;
	ssize_t n;
	int fd;

	if (devicePath == NULL || !CFStringGetFileSystemRepresentation(
	    devicePath, path, sizeof(path)))
		return (NULL);
	if ((fd = open(path, O_RDONLY)) < 0)
		return (NULL);
	/* Raw devices take whole blocks: 36 KiB holds ISO 9660's first descriptor. */
	if ((buf = calloc(1, 36864)) == NULL) {
		close(fd);
		return (NULL);
	}
	n = pread(fd, buf, 36864, 0);
	close(fd);
	if (n >= 2048) {
		uint16_t sig = be16(buf + 1024);

		if (sig == 0x482b || sig == 0x4858)	/* "H+", "HX" */
			name = hfs_name(sig == 0x4858,
			    (be32(buf + 1028) & (1u << HFS_JOURNALED_BIT)) != 0);
		else if (memcmp(buf + 3, "EXFAT   ", 8) == 0)
			name = "ExFAT";
		else if (n >= 32774 && memcmp(buf + 32769, "CD001", 5) == 0)
			name = "ISO 9660";
		else
			name = fat_name(buf);
	}
	free(buf);
	return (copy_name(name));
}

CFStringRef
_FSCopyNameForVolumeFormatAtURL(CFURLRef url)
{
	char path[MAXPATHLEN];
	struct statfs sfs;

	if (url == NULL || !CFURLGetFileSystemRepresentation(url, true,
	    (UInt8 *)path, sizeof(path)) || statfs(path, &sfs) != 0)
		return (NULL);
	if (strcmp(sfs.f_fstypename, "hfs") == 0)
		return (copy_name(hfs_name(pathconf(path,
		    _PC_CASE_SENSITIVE) == 1, (sfs.f_flags & MNT_JOURNALED) != 0)));
	if (strcmp(sfs.f_fstypename, "msdos") == 0) {
		char dev[MAXPATHLEN];

		/* The type is in the boot sector, on the raw device. */
		if (strncmp(sfs.f_mntfromname, "/dev/disk", 9) == 0) {
			CFStringRef d, s;

			snprintf(dev, sizeof(dev), "/dev/r%s", sfs.f_mntfromname + 5);
			d = CFStringCreateWithCString(kCFAllocatorDefault, dev,
			    kCFStringEncodingUTF8);
			s = d ? _FSCopyNameForVolumeFormatAtNode(d) : NULL;
			if (d)
				CFRelease(d);
			if (s)
				return (s);
		}
		return (copy_name("MS-DOS"));
	}
	if (strcmp(sfs.f_fstypename, "cd9660") == 0)
		return (copy_name("ISO 9660"));
	if (strcmp(sfs.f_fstypename, "exfat") == 0)
		return (copy_name("ExFAT"));
	return (NULL);
}
