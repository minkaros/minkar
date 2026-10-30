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
 * fsck_exfat: checks an exFAT file system, after Microsoft's published
 * specification, and repairs what can be repaired safely.
 *
 *	fsck_exfat -q special		was it unmounted cleanly? (0, or 3)
 *	fsck_exfat [-fnpy] special	check (and repair)
 *
 * The boot region (signatures, geometry, checksum; a damaged main region
 * is restored from its backup), the system files (the allocation bitmap,
 * the up-case table and its checksum), every directory's entry sets
 * (checksums, name hashes, cluster chains: valid, without loops, not
 * shared), and the bitmap against the clusters in use. Repaired: the
 * main boot region, the bitmap, entry set checksums and name hashes, and
 * the volume's dirty flag.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "exfat_format.h"
#include "exfat_upcase.h"

#define EXIT_OK		0
#define EXIT_DIRTY	3	/* -q: not unmounted cleanly */
#define EXIT_ERRORS	8	/* errors left */

static int fd;
static bool opt_n, opt_y, opt_p, opt_f, writable;
static unsigned bps, spc, cluster_bytes;
static struct exfat_boot boot;
static uint32_t *fat;
static uint8_t *used;		/* the clusters found in use, a bit each */
static uint8_t *bitmap;		/* the bitmap as on the volume */
static uint64_t bitmap_length;
static uint32_t bitmap_cluster;
static uint16_t *upcase;
static unsigned errors, fixed, unrepairable;
static char label[64];

static void
usage(void)
{
	fprintf(stderr, "usage: fsck_exfat -q special\n"
	    "       fsck_exfat [-fnpy] special\n");
	exit(EXIT_ERRORS);
}

static void
msg(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	printf("** ");
	vprintf(fmt, ap);
	printf("\n");
	fflush(stdout);
	va_end(ap);
}

/* A problem: true if it is to be repaired (-y, -p, or a yes). */
static bool
problem(bool repairable, const char *fmt, ...)
{
	va_list ap;
	char line[16];

	errors++;
	if (!repairable)
		unrepairable++;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	if (!repairable || opt_n || !writable) {
		printf(".\n");
		return (false);
	}
	if (opt_y || opt_p) {
		printf(". Repaired.\n");
		fixed++;
		return (true);
	}
	printf(". Repair? [yn] ");
	fflush(stdout);
	if (fgets(line, sizeof(line), stdin) != NULL &&
	    (line[0] == 'y' || line[0] == 'Y')) {
		fixed++;
		return (true);
	}
	return (false);
}

static bool
read_sectors(uint64_t sector, void *buf, size_t count)
{
	size_t len = count * bps;

	return (pread(fd, buf, len, (off_t)(sector * bps)) == (ssize_t)len);
}

static void
write_sectors(uint64_t sector, const void *buf, size_t count)
{
	size_t len = count * bps;

	if (pwrite(fd, buf, len, (off_t)(sector * bps)) != (ssize_t)len)
		err(EXIT_ERRORS, "write at sector %llu", (unsigned long long)sector);
}

static uint64_t
cluster_sector(uint32_t c)
{
	return (boot.heap_offset + (uint64_t)(c - EXFAT_FIRST_CLUSTER) * spc);
}

static bool
valid_cluster(uint32_t c)
{
	return (c >= EXFAT_FIRST_CLUSTER &&
	    c < boot.cluster_count + EXFAT_FIRST_CLUSTER);
}

static bool
bit(const uint8_t *map, uint32_t c)
{
	uint32_t i = c - EXFAT_FIRST_CLUSTER;

	return ((map[i / 8] >> (i % 8)) & 1);
}

static void
set_bit(uint8_t *map, uint32_t c, bool on)
{
	uint32_t i = c - EXFAT_FIRST_CLUSTER;

	if (on)
		map[i / 8] |= (uint8_t)(1u << (i % 8));
	else
		map[i / 8] &= (uint8_t)~(1u << (i % 8));
}

/* ---- the boot region ------------------------------------------------- */

/* Checks a boot region (main or backup): NULL if fine, else why not. */
static const char *
check_region(const uint8_t *region)
{
	const struct exfat_boot *b = (const void *)region;
	uint32_t sum, *cs;
	uint64_t clusters;
	unsigned i, sector = 1u << b->sector_shift;

	if (memcmp(b->fs_name, EXFAT_NAME, 8) != 0)
		return ("not an exFAT boot sector");
	if (b->signature != 0xaa55)
		return ("no boot signature");
	if (b->sector_shift < 9 || b->sector_shift > 12)
		return ("bytes per sector out of range");
	if (sector != bps)
		return ("bytes per sector differ from the region's");
	if (b->sector_shift + b->cluster_shift > 25)
		return ("cluster size out of range");
	if (b->nfats != 1 && b->nfats != 2)
		return ("number of FATs out of range");
	if (b->fat_offset < 24 || b->fat_length == 0 ||
	    (uint64_t)b->fat_length * sector / 4 <
	    (uint64_t)b->cluster_count + 2)
		return ("the FAT's place or size is wrong");
	if (b->heap_offset < b->fat_offset + (uint64_t)b->fat_length * b->nfats)
		return ("the cluster heap overlaps the FAT");
	clusters = (b->volume_length - b->heap_offset) >> b->cluster_shift;
	if (b->heap_offset >= b->volume_length || b->cluster_count > clusters ||
	    b->cluster_count > 0xfffffff5u)
		return ("the cluster count does not fit the volume");
	if (b->root_cluster < EXFAT_FIRST_CLUSTER ||
	    b->root_cluster >= b->cluster_count + EXFAT_FIRST_CLUSTER)
		return ("the root directory's cluster is out of range");
	for (i = 1; i <= 8; i++) {
		const uint8_t *s = region + (size_t)i * sector;

		if (s[sector - 2] != 0x55 || s[sector - 1] != 0xaa)
			return ("an extended boot sector's signature is wrong");
	}
	sum = exfat_boot_checksum(region, sector);
	cs = (uint32_t *)(void *)(region + (size_t)EXFAT_CHECKSUM_SECTOR * sector);
	for (i = 0; i < sector / 4; i++)
		if (cs[i] != sum)
			return ("the boot region's checksum is wrong");
	return (NULL);
}

static bool
check_boot(void)
{
	uint8_t first[4096], *main_region, *backup;
	const char *why, *why_backup;
	const struct exfat_boot *b;

	memset(first, 0, sizeof(first));
	if (pread(fd, first, sizeof(first), 0) < 512) {
		/* Not even a sector: not a volume. */
		printf("** The volume is not exFAT.\n");
		return (false);
	}
	b = (const void *)first;
	if (memcmp(b->fs_name, EXFAT_NAME, 8) != 0) {
		/* The main sector may be gone: the backup tells the size. */
		bps = 512;
		if (pread(fd, first, 512, 12 * 512) == 512 &&
		    memcmp(b->fs_name, EXFAT_NAME, 8) == 0)
			bps = 1u << b->sector_shift;
		else if (pread(fd, first, 4096, 12 * 4096) == 4096 &&
		    memcmp(b->fs_name, EXFAT_NAME, 8) == 0)
			bps = 1u << b->sector_shift;
		else {
			printf("** The volume is not exFAT.\n");
			return (false);
		}
	} else
		bps = 1u << (b->sector_shift >= 9 && b->sector_shift <= 12 ?
		    b->sector_shift : 9);

	main_region = calloc(EXFAT_BOOT_SECTORS, bps);
	backup = calloc(EXFAT_BOOT_SECTORS, bps);
	if (main_region == NULL || backup == NULL)
		err(EXIT_ERRORS, NULL);
	msg("Checking main boot region.");
	why = read_sectors(0, main_region, EXFAT_BOOT_SECTORS) ?
	    check_region(main_region) : "unreadable";
	why_backup = read_sectors(EXFAT_BACKUP_BOOT, backup, EXFAT_BOOT_SECTORS) ?
	    check_region(backup) : "unreadable";
	if (why != NULL) {
		if (why_backup != NULL) {
			printf("The main boot region is damaged (%s), and so is "
			    "its backup (%s).\n", why, why_backup);
			errors++;
			return (false);
		}
		if (problem(true, "The main boot region is damaged (%s); its "
		    "backup is fine", why))
			write_sectors(0, backup, EXFAT_BOOT_SECTORS);
		memcpy(main_region, backup, (size_t)EXFAT_BOOT_SECTORS * bps);
	} else if (why_backup != NULL)
		printf("** The backup boot region is damaged (%s); the main "
		    "one is fine.\n", why_backup);
	memcpy(&boot, main_region, sizeof(boot));
	spc = 1u << boot.cluster_shift;
	cluster_bytes = spc * bps;
	free(main_region);
	free(backup);
	return (true);
}

/* ---- clusters and chains ---------------------------------------------- */

/*
 * The clusters of a chain: from the FAT, or contiguous; checked, and
 * marked in use. Returns how many there are (0 if the chain is broken).
 */
static uint32_t
walk_chain(uint32_t first, uint64_t length, bool contiguous, const char *what,
    uint32_t **out)
{
	uint64_t want = (length + cluster_bytes - 1) / cluster_bytes;
	uint32_t n = 0, c = first, *list = NULL;
	size_t alloc = 0;

	if (length == 0 && first == 0)
		return (0);
	if (!valid_cluster(first)) {
		problem(false, "%s: its first cluster (%u) is out of range",
		    what, first);
		return (0);
	}
	for (;;) {
		if (n == alloc) {
			alloc = alloc ? alloc * 2 : 16;
			if ((list = realloc(list, alloc * sizeof(*list))) == NULL)
				err(EXIT_ERRORS, NULL);
		}
		if (bit(used, c)) {
			problem(false, "%s: cluster %u is used twice (a loop, "
			    "or another file's)", what, c);
			free(list);
			return (0);
		}
		set_bit(used, c, true);
		list[n++] = c;
		if (contiguous) {
			if (n == want)
				break;
			c++;
			if (!valid_cluster(c)) {
				problem(false, "%s: runs past the volume's end", what);
				free(list);
				return (0);
			}
			continue;
		}
		if (fat[c] == EXFAT_EOC)
			break;
		if (!valid_cluster(fat[c])) {
			problem(false, "%s: the FAT entry of cluster %u (0x%x) is "
			    "not valid", what, c, fat[c]);
			free(list);
			return (0);
		}
		c = fat[c];
	}
	if (want != 0 && n < want) {
		problem(false, "%s: %u clusters for %llu bytes", what, n,
		    (unsigned long long)length);
		free(list);
		return (0);
	}
	if (out != NULL)
		*out = list;
	else
		free(list);
	return (n);
}

static uint8_t *
read_chain(const uint32_t *list, uint32_t n)
{
	uint8_t *buf;
	uint32_t i;

	if ((buf = malloc((size_t)n * cluster_bytes)) == NULL)
		err(EXIT_ERRORS, NULL);
	for (i = 0; i < n; i++)
		if (!read_sectors(cluster_sector(list[i]),
		    buf + (size_t)i * cluster_bytes, spc))
			err(EXIT_ERRORS, "cannot read cluster %u", list[i]);
	return (buf);
}

static void
write_back(const uint32_t *list, uint32_t index, const uint8_t *buf)
{
	write_sectors(cluster_sector(list[index]),
	    buf + (size_t)index * cluster_bytes, spc);
}

/* ---- directories ----------------------------------------------------- */

static unsigned nfiles, ndirs;

static void check_directory(uint32_t first, uint64_t length, bool contiguous,
    const char *path, bool root);

static void
utf16_to_utf8(const uint16_t *s, unsigned n, char *out, size_t len)
{
	size_t o = 0;
	unsigned i;

	for (i = 0; i < n && o + 4 < len; i++) {
		uint16_t c = s[i];

		if (c < 0x80)
			out[o++] = (char)c;
		else if (c < 0x800) {
			out[o++] = (char)(0xc0 | c >> 6);
			out[o++] = (char)(0x80 | (c & 0x3f));
		} else {
			out[o++] = (char)(0xe0 | c >> 12);
			out[o++] = (char)(0x80 | ((c >> 6) & 0x3f));
			out[o++] = (char)(0x80 | (c & 0x3f));
		}
	}
	out[o] = '\0';
}

/* The entry sets of one directory. */
static void
check_entries(uint8_t *dir, uint32_t nclusters, const uint32_t *list,
    const char *path, bool root)
{
	size_t nentries = (size_t)nclusters * cluster_bytes / EXFAT_ENTRY_SIZE;
	size_t i = 0;

	while (i < nentries) {
		uint8_t *e = dir + i * EXFAT_ENTRY_SIZE;
		uint8_t type = e[0];

		if (type == EXFAT_TYPE_END)
			break;
		if (!(type & EXFAT_INUSE)) {
			i++;		/* deleted */
			continue;
		}
		if (type == EXFAT_TYPE_BITMAP || type == EXFAT_TYPE_UPCASE ||
		    type == EXFAT_TYPE_LABEL) {
			/* The root's system entries, checked before. */
			if (!root)
				problem(false, "%s: a system entry (0x%02x) out of "
				    "the root directory", path, type);
			i++;
			continue;
		}
		if (type == EXFAT_TYPE_FILE) {
			struct exfat_file_entry *f = (void *)e;
			struct exfat_stream_entry *st;
			unsigned count = f->secondary_count, k, need;
			uint16_t name[EXFAT_NAME_MAX], up[EXFAT_NAME_MAX];
			char uname[EXFAT_NAME_MAX * 3 + 1], child[4096];
			uint16_t sum;
			bool dirty = false;

			if (count < 2 || count > 18 || i + count >= nentries) {
				problem(false, "%s: an entry set at %zu has %u "
				    "secondary entries", path, i, count);
				i++;
				continue;
			}
			st = (void *)(e + EXFAT_ENTRY_SIZE);
			if (st->type != EXFAT_TYPE_STREAM) {
				problem(false, "%s: an entry set at %zu has no "
				    "stream entry", path, i);
				i += 1 + count;
				continue;
			}
			need = (st->name_length + EXFAT_NAME_CHARS - 1) /
			    EXFAT_NAME_CHARS;
			if (st->name_length == 0 || need != count - 1) {
				problem(false, "%s: an entry set at %zu: its name "
				    "length (%u) does not fit its entries", path,
				    i, st->name_length);
				i += 1 + count;
				continue;
			}
			for (k = 0; k < need; k++) {
				struct exfat_name_entry *ne = (void *)(e +
				    (size_t)(2 + k) * EXFAT_ENTRY_SIZE);

				if (ne->type != EXFAT_TYPE_NAME)
					break;
				memcpy(name + k * EXFAT_NAME_CHARS, ne->name,
				    sizeof(ne->name));
			}
			if (k < need) {
				problem(false, "%s: an entry set at %zu lacks a "
				    "name entry", path, i);
				i += 1 + count;
				continue;
			}
			utf16_to_utf8(name, st->name_length, uname, sizeof(uname));
			snprintf(child, sizeof(child), "%s/%s",
			    strcmp(path, "/") == 0 ? "" : path, uname);
			for (k = 0; k < st->name_length; k++)
				up[k] = upcase[name[k]];
			if (exfat_name_hash(up, st->name_length) != st->name_hash &&
			    problem(true, "%s: its name hash is wrong", child)) {
				st->name_hash = exfat_name_hash(up, st->name_length);
				dirty = true;
			}
			sum = exfat_set_checksum(e, count + 1);
			if (sum != f->checksum &&
			    problem(true, "%s: its entry set's checksum is wrong",
			    child)) {
				f->checksum = exfat_set_checksum(e, count + 1);
				dirty = true;
			} else if (dirty)
				f->checksum = exfat_set_checksum(e, count + 1);
			if (dirty) {
				/* The clusters the set lies in. */
				size_t lo = i * EXFAT_ENTRY_SIZE / cluster_bytes;
				size_t hi = (i + count) * EXFAT_ENTRY_SIZE /
				    cluster_bytes;

				for (; lo <= hi; lo++)
					write_back(list, (uint32_t)lo, dir);
			}
			if (st->valid_length > st->length)
				problem(false, "%s: its valid length is past its "
				    "length", child);
			if (f->attributes & EXFAT_ATTR_DIR) {
				ndirs++;
				check_directory(st->first_cluster, st->length,
				    (st->flags & EXFAT_NO_FAT_CHAIN) != 0, child,
				    false);
			} else {
				nfiles++;
				if (st->length > 0 || st->first_cluster != 0)
					walk_chain(st->first_cluster, st->length,
					    (st->flags & EXFAT_NO_FAT_CHAIN) != 0,
					    child, NULL);
			}
			i += 1 + count;
			continue;
		}
		if (type == EXFAT_TYPE_GUID || (type & EXFAT_BENIGN)) {
			i++;		/* benign: nothing to check */
			continue;
		}
		problem(false, "%s: an entry of unknown critical type 0x%02x at "
		    "%zu", path, type, i);
		i++;
	}
}

static void
check_directory(uint32_t first, uint64_t length, bool contiguous,
    const char *path, bool root)
{
	uint32_t *list = NULL, n;
	uint8_t *dir;

	if (first == 0) {
		if (!root)
			problem(false, "%s: a directory without clusters", path);
		return;
	}
	n = walk_chain(first, length, contiguous, path, &list);
	if (n == 0)
		return;
	dir = read_chain(list, n);
	check_entries(dir, n, list, path, root);
	free(dir);
	free(list);
}

/* ---- the system files -------------------------------------------------- */

static bool
check_system(void)
{
	uint32_t *list = NULL, n, i, upcase_cluster = 0, upcase_sum = 0;
	uint64_t upcase_length = 0;
	uint8_t *root;
	size_t e, nentries;
	bool have_bitmap = false, have_upcase = false;

	msg("Checking system files.");
	/* The root's chain is walked again with the rest: count it out. */
	n = walk_chain(boot.root_cluster, 0, false, "the root directory", &list);
	if (n == 0)
		return (false);
	root = read_chain(list, n);
	for (i = 0; i < n; i++)
		set_bit(used, list[i], false);
	nentries = (size_t)n * cluster_bytes / EXFAT_ENTRY_SIZE;
	for (e = 0; e < nentries; e++) {
		uint8_t *p = root + e * EXFAT_ENTRY_SIZE;

		if (p[0] == EXFAT_TYPE_END)
			break;
		if (p[0] == EXFAT_TYPE_BITMAP && !have_bitmap) {
			struct exfat_bitmap_entry *b = (void *)p;

			bitmap_cluster = b->first_cluster;
			bitmap_length = b->length;
			have_bitmap = true;
		} else if (p[0] == EXFAT_TYPE_UPCASE) {
			struct exfat_upcase_entry *u = (void *)p;

			upcase_cluster = u->first_cluster;
			upcase_length = u->length;
			upcase_sum = u->checksum;
			have_upcase = true;
		} else if (p[0] == EXFAT_TYPE_LABEL) {
			struct exfat_label_entry *l = (void *)p;

			utf16_to_utf8(l->label, l->length > EXFAT_LABEL_MAX ?
			    EXFAT_LABEL_MAX : l->length, label, sizeof(label));
		}
	}
	free(root);
	free(list);
	if (!have_bitmap || bitmap_length < ((uint64_t)boot.cluster_count + 7) / 8) {
		problem(false, "The allocation bitmap is %s",
		    have_bitmap ? "too small" : "missing");
		return (false);
	}
	if (!have_upcase) {
		problem(false, "The up-case table is missing");
		return (false);
	}
	n = walk_chain(bitmap_cluster, bitmap_length, false,
	    "the allocation bitmap", &list);
	if (n == 0)
		return (false);
	bitmap = read_chain(list, n);
	free(list);

	msg("Checking upper case translation table.");
	n = walk_chain(upcase_cluster, upcase_length, false, "the up-case table",
	    &list);
	if (n == 0)
		return (false);
	{
		uint8_t *t = read_chain(list, n);

		free(list);
		if (exfat_table_checksum(t, upcase_length) != upcase_sum)
			problem(false, "The up-case table's checksum is wrong");
		if (exfat_upcase_expand((const uint16_t *)(void *)t,
		    (size_t)(upcase_length / 2), upcase) != 0)
			problem(false, "The up-case table is not valid");
		free(t);
	}
	if (label[0] != '\0')
		msg("Volume name is %s.", label);
	return (true);
}

/* ---- the bitmap ------------------------------------------------------ */

static void
check_bitmap(void)
{
	uint32_t c, missing = 0, extra = 0, k;

	msg("Checking active bitmap.");
	for (c = EXFAT_FIRST_CLUSTER; c < boot.cluster_count + EXFAT_FIRST_CLUSTER;
	    c++) {
		bool u = bit(used, c), b = bit(bitmap, c);

		if (u && !b)
			missing++;
		else if (!u && b)
			extra++;
	}
	if (missing == 0 && extra == 0)
		return;
	/*
	 * With a broken chain or directory, what it held is not found and
	 * looks free: the bitmap is left as it is, or those files' clusters
	 * would be given away.
	 */
	if (!problem(unrepairable == 0, "The bitmap has %u clusters in use "
	    "marked free, and %u free marked in use%s", missing, extra,
	    unrepairable > 0 ? " (left as it is: other problems remain)" : ""))
		return;
	for (c = EXFAT_FIRST_CLUSTER; c < boot.cluster_count + EXFAT_FIRST_CLUSTER;
	    c++)
		set_bit(bitmap, c, bit(used, c));
	/* Its own chain was checked with the system files. */
	for (k = 0, c = bitmap_cluster; valid_cluster(c) &&
	    (uint64_t)k * cluster_bytes < bitmap_length; k++, c = fat[c])
		write_sectors(cluster_sector(c), bitmap + (size_t)k * cluster_bytes,
		    spc);
}

/* ---- main ------------------------------------------------------------ */

static int
quick_check(const char *special)
{
	uint8_t s[4096];
	const struct exfat_boot *b = (const void *)s;

	if (pread(fd, s, sizeof(s), 0) < 512 ||
	    memcmp(b->fs_name, EXFAT_NAME, 8) != 0) {
		printf("** %s: not exFAT\n", special);
		return (EXIT_ERRORS);
	}
	if (b->flags & EXFAT_DIRTY) {
		printf("** %s: dirty\n", special);
		return (EXIT_DIRTY);
	}
	printf("** %s: clean\n", special);
	return (EXIT_OK);
}

static void
set_dirty(bool dirty)
{
	uint8_t *s = malloc(bps);
	struct exfat_boot *b;

	if (s == NULL || !read_sectors(0, s, 1))
		err(EXIT_ERRORS, "cannot read the boot sector");
	b = (struct exfat_boot *)s;
	if (dirty)
		b->flags |= EXFAT_DIRTY;
	else
		b->flags &= (uint16_t)~EXFAT_DIRTY;
	/* The flags are not in the checksum: one sector to write. */
	write_sectors(0, s, 1);
	free(s);
}

int
main(int argc, char **argv)
{
	const char *special;
	bool quick = false;
	size_t fat_bytes;
	int ch;

	while ((ch = getopt(argc, argv, "fnpqyM")) != -1) {
		switch (ch) {
		case 'f':
			opt_f = true;
			break;
		case 'n':
			opt_n = true;
			opt_y = false;
			break;
		case 'p':
			opt_p = true;
			break;
		case 'q':
			quick = true;
			break;
		case 'y':
			opt_y = true;
			opt_n = false;
			break;
		case 'M':	/* fsck(8)'s: no memory limit here */
			break;
		default:
			usage();
		}
	}
	if (argc - optind != 1)
		usage();
	special = argv[optind];
	writable = !opt_n && !quick;
	if ((fd = open(special, writable ? O_RDWR : O_RDONLY)) < 0) {
		if (!writable || (fd = open(special, O_RDONLY)) < 0)
			err(EXIT_ERRORS, "%s", special);
		writable = false;
		printf("** %s (NO WRITE)\n", special);
	}
	if (quick)
		return (quick_check(special));

	printf("** Checking volume.\n");
	if (!check_boot())
		return (EXIT_ERRORS);
	if (opt_p && !opt_f && !(boot.flags & EXFAT_DIRTY)) {
		printf("** %s: clean; skipping the checks\n", special);
		return (EXIT_OK);
	}
	fat_bytes = (size_t)boot.fat_length * bps;
	used = calloc(1, ((size_t)boot.cluster_count + 7) / 8 + 1);
	upcase = calloc(EXFAT_UPCASE_CHARS, sizeof(*upcase));
	if ((fat = malloc(fat_bytes)) == NULL || used == NULL || upcase == NULL)
		err(EXIT_ERRORS, NULL);
	if (!read_sectors(boot.fat_offset, fat, boot.fat_length))
		err(EXIT_ERRORS, "cannot read the FAT");
	if (!check_system())
		goto out;
	msg("Checking file system hierarchy.");
	check_directory(boot.root_cluster, 0, false, "/", true);
	check_bitmap();
	printf("** %u files, %u folders.\n", nfiles, ndirs);
out:
	if (errors == fixed && writable && (boot.flags & EXFAT_DIRTY)) {
		set_dirty(false);
		printf("** The volume is marked clean.\n");
	}
	if (errors == 0) {
		printf("** The volume %s appears to be OK.\n",
		    label[0] ? label : special);
		return (EXIT_OK);
	}
	if (errors == fixed) {
		printf("** The volume %s was repaired successfully.\n",
		    label[0] ? label : special);
		return (EXIT_OK);
	}
	printf("** The volume %s could not be verified completely (%u "
	    "problem%s left).\n", label[0] ? label : special, errors - fixed,
	    errors - fixed == 1 ? "" : "s");
	return (EXIT_ERRORS);
}
