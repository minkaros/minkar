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
 * newfs_exfat: makes an exFAT file system, after Microsoft's published
 * specification: the boot region and its backup, one FAT, the cluster
 * heap with the allocation bitmap, the up-case table and the root
 * directory (the volume label, the bitmap's and the table's entries).
 *
 *	newfs_exfat [-N] [-v name] [-b bytes-per-sector] [-c bytes-per-cluster]
 *	    [-I volume-id] special
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <sys/disk.h>
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "exfat_format.h"
#include "exfat_upcase.h"

static int fd;
static unsigned bps;		/* bytes per sector */

static void
usage(void)
{
	fprintf(stderr, "usage: newfs_exfat [-N] [-v name] [-b bytes-per-sector] "
	    "[-c bytes-per-cluster] [-I volume-id] special\n");
	exit(1);
}

static bool
power_of_two(unsigned long v)
{
	return (v != 0 && (v & (v - 1)) == 0);
}

static unsigned
shift_of(unsigned long v)
{
	unsigned s = 0;

	while ((1ul << s) < v)
		s++;
	return (s);
}

static void
write_at(uint64_t sector, const void *buf, size_t len)
{
	if (pwrite(fd, buf, len, (off_t)(sector * bps)) != (ssize_t)len)
		err(1, "write at sector %llu", (unsigned long long)sector);
}

/* Zeroes [sector, sector + count), a megabyte at a time. */
static void
zero_sectors(uint64_t sector, uint64_t count)
{
	static uint8_t zeros[1 << 20];
	uint64_t per = sizeof(zeros) / bps;

	while (count > 0) {
		uint64_t n = count < per ? count : per;

		write_at(sector, zeros, (size_t)(n * bps));
		sector += n;
		count -= n;
	}
}

/* A UTF-8 label as UTF-16: returns its length in characters. */
static unsigned
label_utf16(const char *s, uint16_t *out)
{
	const unsigned char *p = (const unsigned char *)s;
	unsigned n = 0;

	while (*p != '\0') {
		uint32_t c;

		if (*p < 0x80)
			c = *p++;
		else if ((*p & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) {
			c = (uint32_t)(*p & 0x1f) << 6 | (p[1] & 0x3f);
			p += 2;
		} else if ((*p & 0xf0) == 0xe0 && (p[1] & 0xc0) == 0x80 &&
		    (p[2] & 0xc0) == 0x80) {
			c = (uint32_t)(*p & 0x0f) << 12 |
			    (uint32_t)(p[1] & 0x3f) << 6 | (p[2] & 0x3f);
			p += 3;
		} else
			errx(1, "the volume name is not valid UTF-8 (or not in "
			    "the Basic Multilingual Plane)");
		if (n == EXFAT_LABEL_MAX)
			errx(1, "the volume name is longer than %d characters",
			    EXFAT_LABEL_MAX);
		if (c < 0x20 || strchr("\"*/:<>?\\|", (int)c) != NULL)
			errx(1, "the volume name cannot have %s",
			    c < 0x20 ? "control characters" :
			    "any of \" * / : < > ? \\ |");
		out[n++] = (uint16_t)c;
	}
	return (n);
}

static uint64_t
device_sectors(const char *path, unsigned *sector_size)
{
	struct stat st;

	if (fstat(fd, &st) != 0)
		err(1, "%s", path);
	*sector_size = 512;
	if (S_ISREG(st.st_mode))
		return ((uint64_t)st.st_size / 512);
#ifdef __APPLE__
	{
		uint32_t bs = 0;
		uint64_t count = 0;

		if (ioctl(fd, DKIOCGETBLOCKSIZE, &bs) != 0 ||
		    ioctl(fd, DKIOCGETBLOCKCOUNT, &count) != 0)
			err(1, "%s: cannot get its size", path);
		*sector_size = bs;
		return (count);
	}
#else
	errx(1, "%s: not a regular file", path);
#endif
}

int
main(int argc, char **argv)
{
	const char *name = "", *special;
	unsigned long cluster_bytes = 0, sector_arg = 0;
	uint32_t serial = 0, spc, count, fat_offset, fat_length, heap;
	uint32_t bitmap_clusters, upcase_clusters, used, c, *fat;
	uint64_t total, bitmap_bytes;
	uint16_t label[EXFAT_LABEL_MAX], *table, *packed;
	unsigned label_len, sector_size, i;
	size_t packed_len, fat_bytes;
	bool dry = false, have_serial = false;
	uint8_t *region, *bitmap, *cluster;
	struct exfat_boot *boot;
	int ch;

	while ((ch = getopt(argc, argv, "Nv:b:c:I:")) != -1) {
		switch (ch) {
		case 'N':
			dry = true;
			break;
		case 'v':
			name = optarg;
			break;
		case 'b':
			sector_arg = strtoul(optarg, NULL, 0);
			break;
		case 'c':
			cluster_bytes = strtoul(optarg, NULL, 0);
			break;
		case 'I':
			serial = (uint32_t)strtoul(optarg, NULL, 0);
			have_serial = true;
			break;
		default:
			usage();
		}
	}
	if (argc - optind != 1)
		usage();
	special = argv[optind];
	label_len = label_utf16(name, label);

	if ((fd = open(special, dry ? O_RDONLY : O_RDWR)) < 0)
		err(1, "%s", special);
	total = device_sectors(special, &sector_size);
	bps = sector_size;
	if (sector_arg != 0) {
		if (!power_of_two(sector_arg) || sector_arg < 512 ||
		    sector_arg > 4096 || sector_arg < sector_size)
			errx(1, "bytes per sector: 512 to 4096, a power of two, "
			    "not under the device's %u", sector_size);
		total = total * sector_size / sector_arg;
		bps = (unsigned)sector_arg;
	}
	if (cluster_bytes == 0) {
		uint64_t bytes = total * bps;

		cluster_bytes = bytes <= (256ull << 20) ? 4096 :
		    bytes <= (32ull << 30) ? 32768 : 131072;
	}
	if (!power_of_two(cluster_bytes) || cluster_bytes < bps ||
	    cluster_bytes > (32ul << 20))
		errx(1, "bytes per cluster: a power of two from %u to 32 MB", bps);
	spc = (uint32_t)(cluster_bytes / bps);

	/* The layout: the FAT after both boot regions, the heap aligned. */
	fat_offset = 24;
	if (fat_offset % spc != 0)
		fat_offset += spc - fat_offset % spc;
	count = (uint32_t)((total - fat_offset) / spc);
	for (;;) {
		uint32_t next;

		fat_length = (uint32_t)(((uint64_t)count + 2) * 4 / bps +
		    (((uint64_t)count + 2) * 4 % bps != 0));
		if (fat_length % spc != 0)
			fat_length += spc - fat_length % spc;
		heap = fat_offset + fat_length;
		if (total <= heap)
			errx(1, "%s is too small for exFAT", special);
		next = (uint32_t)((total - heap) / spc);
		if (next >= count)
			break;
		count = next;
	}
	if (count > 0xfffffff5u)
		count = 0xfffffff5u;

	/* The cluster heap: the bitmap, the up-case table, the root. */
	bitmap_bytes = ((uint64_t)count + 7) / 8;
	bitmap_clusters = (uint32_t)((bitmap_bytes + cluster_bytes - 1) /
	    cluster_bytes);
	if ((table = calloc(EXFAT_UPCASE_CHARS, sizeof(*table))) == NULL ||
	    (packed = calloc(EXFAT_UPCASE_CHARS, sizeof(*packed))) == NULL)
		err(1, NULL);
	exfat_upcase_default(table);
	packed_len = exfat_upcase_compress(table, packed);
	upcase_clusters = (uint32_t)((packed_len * 2 + cluster_bytes - 1) /
	    cluster_bytes);
	used = bitmap_clusters + upcase_clusters + 1;
	if (used >= count)
		errx(1, "%s is too small for exFAT", special);
	if (!have_serial)
		serial = (uint32_t)time(NULL) ^ (uint32_t)getpid() << 16;

	printf("Volume name      : %s\n", name);
	printf("Volume size      : %llu sectors (%llu bytes)\n",
	    (unsigned long long)total, (unsigned long long)total * bps);
	printf("Bytes per sector : %u\n", bps);
	printf("Bytes per cluster: %lu\n", cluster_bytes);
	printf("FAT offset       : %u sectors (%llu bytes)\n", fat_offset,
	    (unsigned long long)fat_offset * bps);
	printf("# FAT sectors    : %u\n", fat_length);
	printf("Number of FATs   : 1\n");
	printf("Cluster offset   : %u sectors (%llu bytes)\n", heap,
	    (unsigned long long)heap * bps);
	printf("# Clusters       : %u\n", count);
	printf("Volume Serial #  : %08x\n", serial);
	printf("Bitmap start     : %u\n", EXFAT_FIRST_CLUSTER);
	printf("Bitmap file size : %llu\n", (unsigned long long)bitmap_bytes);
	printf("Upcase start     : %u\n", EXFAT_FIRST_CLUSTER + bitmap_clusters);
	printf("Upcase file size : %zu\n", packed_len * 2);
	printf("Root start       : %u\n",
	    EXFAT_FIRST_CLUSTER + bitmap_clusters + upcase_clusters);
	if (dry)
		return (0);

	/* The FAT: the system structures' chains, the rest free. */
	fat_bytes = (size_t)fat_length * bps;
	if ((fat = calloc(1, fat_bytes)) == NULL)
		err(1, NULL);
	fat[0] = EXFAT_MEDIA;
	fat[1] = EXFAT_EOC;
	c = EXFAT_FIRST_CLUSTER;
	for (i = 0; i < bitmap_clusters; i++, c++)
		fat[c] = i + 1 < bitmap_clusters ? c + 1 : EXFAT_EOC;
	for (i = 0; i < upcase_clusters; i++, c++)
		fat[c] = i + 1 < upcase_clusters ? c + 1 : EXFAT_EOC;
	fat[c] = EXFAT_EOC;			/* the root directory */
	zero_sectors(fat_offset, fat_length);
	write_at(fat_offset, fat, fat_bytes);

	/* The bitmap: the clusters in use. */
	if ((bitmap = calloc(1, (size_t)bitmap_clusters * cluster_bytes)) == NULL)
		err(1, NULL);
	for (i = 0; i < used; i++)
		bitmap[i / 8] |= (uint8_t)(1u << (i % 8));
	write_at(heap, bitmap, (size_t)bitmap_clusters * cluster_bytes);

	/* The up-case table. */
	if ((cluster = calloc(upcase_clusters, cluster_bytes)) == NULL)
		err(1, NULL);
	memcpy(cluster, packed, packed_len * 2);
	write_at(heap + (uint64_t)bitmap_clusters * spc, cluster,
	    (size_t)upcase_clusters * cluster_bytes);
	free(cluster);

	/* The root: the label, the bitmap and the table; then the end. */
	if ((cluster = calloc(1, cluster_bytes)) == NULL)
		err(1, NULL);
	{
		struct exfat_label_entry *l = (void *)cluster;
		struct exfat_bitmap_entry *b = (void *)(cluster + 32);
		struct exfat_upcase_entry *u = (void *)(cluster + 64);

		l->type = EXFAT_TYPE_LABEL;
		l->length = (uint8_t)label_len;
		memcpy(l->label, label, label_len * 2);
		b->type = EXFAT_TYPE_BITMAP;
		b->first_cluster = EXFAT_FIRST_CLUSTER;
		b->length = bitmap_bytes;
		u->type = EXFAT_TYPE_UPCASE;
		u->checksum = exfat_table_checksum((const uint8_t *)packed,
		    packed_len * 2);
		u->first_cluster = EXFAT_FIRST_CLUSTER + bitmap_clusters;
		u->length = packed_len * 2;
	}
	write_at(heap + (uint64_t)(bitmap_clusters + upcase_clusters) * spc,
	    cluster, cluster_bytes);
	free(cluster);

	/* The boot region, then its backup. */
	if ((region = calloc(EXFAT_BOOT_SECTORS, bps)) == NULL)
		err(1, NULL);
	boot = (struct exfat_boot *)region;
	boot->jump[0] = 0xeb;
	boot->jump[1] = 0x76;
	boot->jump[2] = 0x90;
	memcpy(boot->fs_name, EXFAT_NAME, 8);
	boot->volume_length = total;
	boot->fat_offset = fat_offset;
	boot->fat_length = fat_length;
	boot->heap_offset = heap;
	boot->cluster_count = count;
	boot->root_cluster = EXFAT_FIRST_CLUSTER + bitmap_clusters +
	    upcase_clusters;
	boot->serial = serial;
	boot->revision = 0x0100;
	boot->sector_shift = (uint8_t)shift_of(bps);
	boot->cluster_shift = (uint8_t)shift_of(spc);
	boot->nfats = 1;
	boot->drive = 0x80;
	boot->percent_used = (uint8_t)((uint64_t)used * 100 / count);
	memset(boot->boot_code, 0xf4, sizeof(boot->boot_code));	/* hlt */
	boot->signature = 0xaa55;
	for (i = 1; i <= 8; i++) {	/* the extended boot sectors' signature */
		uint8_t *s = region + (size_t)i * bps;

		s[bps - 2] = 0x55;
		s[bps - 1] = 0xaa;
	}
	{
		uint32_t sum = exfat_boot_checksum(region, bps);
		uint32_t *cs = (uint32_t *)(region +
		    (size_t)EXFAT_CHECKSUM_SECTOR * bps);

		for (i = 0; i < bps / 4; i++)
			cs[i] = sum;
	}
	write_at(0, region, (size_t)EXFAT_BOOT_SECTORS * bps);
	write_at(EXFAT_BACKUP_BOOT, region, (size_t)EXFAT_BOOT_SECTORS * bps);
	if (fsync(fd) != 0)
		err(1, "%s", special);
	close(fd);
	return (0);
}
