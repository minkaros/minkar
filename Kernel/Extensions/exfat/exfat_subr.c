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
 * exfat: reading the volume. Sectors through the device's buffer cache,
 * the FAT chains, the directories' entry sets, the up-case table, names
 * and times; after Microsoft's published exFAT specification.
 */

#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/utfconv.h>
#include <libkern/OSByteOrder.h>

#include "exfat_fs.h"

#define le16(x)	OSSwapLittleToHostInt16(x)
#define le32(x)	OSSwapLittleToHostInt32(x)
#define le64(x)	OSSwapLittleToHostInt64(x)

/* Most clusters one map gives as contiguous, following a FAT chain. */
#define EXFAT_RUN_MAX	1024

int
exfat_bread(struct exfat_mount *xmp, uint64_t sector, buf_t *bpp,
    uint8_t **data)
{
	buf_t bp = NULL;
	int error;

	error = buf_meta_bread(xmp->devvp, (daddr64_t)sector,
	    (int)xmp->sector_size, NOCRED, &bp);
	if (error) {
		if (bp != NULL)
			buf_brelse(bp);
		*bpp = NULL;
		return (error);
	}
	*bpp = bp;
	*data = (uint8_t *)buf_dataptr(bp);
	return (0);
}

uint64_t
exfat_cluster_sector(struct exfat_mount *xmp, uint32_t cluster)
{
	return (xmp->heap_offset +
	    ((uint64_t)(cluster - EXFAT_FIRST_CLUSTER) << xmp->csector_shift));
}

/* The FAT's entry for a cluster: the next one in its chain. */
static int
exfat_fat_next(struct exfat_mount *xmp, uint32_t cluster, uint32_t *next)
{
	uint64_t byte = (uint64_t)cluster * 4;
	uint8_t *data;
	uint32_t value;
	buf_t bp;
	int error;

	error = exfat_bread(xmp, xmp->fat_offset + (byte >> xmp->sector_shift),
	    &bp, &data);
	if (error)
		return (error);
	memcpy(&value, data + (byte & (xmp->sector_size - 1)), sizeof(value));
	buf_brelse(bp);
	*next = le32(value);
	return (0);
}

static int
exfat_cluster_ok(struct exfat_mount *xmp, uint32_t cluster)
{
	return (cluster >= EXFAT_FIRST_CLUSTER &&
	    cluster < (uint64_t)xmp->cluster_count + EXFAT_FIRST_CLUSTER);
}

/* The clusters of a FAT chain, up to its end; EIO if it is broken. */
int
exfat_chain_length(struct exfat_mount *xmp, uint32_t first, uint32_t *count)
{
	uint32_t cluster = first, next, n = 0;
	int error;

	while (n <= xmp->cluster_count) {
		if (!exfat_cluster_ok(xmp, cluster))
			return (EIO);
		n++;
		if ((error = exfat_fat_next(xmp, cluster, &next)))
			return (error);
		if (next >= EXFAT_MEDIA) {
			*count = n;
			return (0);
		}
		cluster = next;
	}
	return (EIO);		/* a loop */
}

void
exfat_stream_init(struct exfat_stream *st, const struct exfat_info *info)
{
	st->first = info->first;
	st->contig = info->contig;
	st->length = info->length;
	st->hint_index = 0;
	st->hint_cluster = 0;
}

/*
 * The cluster at an index of a stream, and how many clusters from it on
 * are contiguous on the disk (at least one). The stream keeps the last
 * place mapped, so a file read in order follows its chain once.
 */
int
exfat_map(struct exfat_mount *xmp, struct exfat_stream *st, uint32_t index,
    uint32_t *clusterp, uint32_t *runp)
{
	uint64_t nclusters;
	uint32_t at, cluster, next, run;
	int error;

	nclusters = (st->length + xmp->cluster_size - 1) >> xmp->cluster_shift;
	if (!exfat_cluster_ok(xmp, st->first) || index >= nclusters)
		return (EIO);

	if (st->contig) {
		if (st->first + nclusters >
		    (uint64_t)xmp->cluster_count + EXFAT_FIRST_CLUSTER)
			return (EIO);
		*clusterp = st->first + index;
		*runp = (uint32_t)MIN(nclusters - index, UINT32_MAX);
		return (0);
	}

	if (st->hint_cluster != 0 && st->hint_index <= index) {
		at = st->hint_index;
		cluster = st->hint_cluster;
	} else {
		at = 0;
		cluster = st->first;
	}
	while (at < index) {
		if ((error = exfat_fat_next(xmp, cluster, &next)))
			return (error);
		if (!exfat_cluster_ok(xmp, next))
			return (EIO);
		cluster = next;
		at++;
	}
	*clusterp = cluster;

	for (run = 1; index + run < nclusters && run < EXFAT_RUN_MAX; run++) {
		if ((error = exfat_fat_next(xmp, cluster, &next)))
			return (error);
		if (next != cluster + 1)
			break;
		cluster = next;
	}
	*runp = run;
	st->hint_index = index + run - 1;
	st->hint_cluster = cluster;
	return (0);
}

/* Bytes of a stream, from an offset, into the kernel's memory. */
int
exfat_read_stream(struct exfat_mount *xmp, struct exfat_stream *st,
    uint64_t offset, void *dst, size_t len)
{
	uint8_t *out = dst, *data;
	uint32_t cluster, run;
	uint64_t sector;
	size_t in, n;
	buf_t bp;
	int error;

	while (len > 0) {
		error = exfat_map(xmp, st,
		    (uint32_t)(offset >> xmp->cluster_shift), &cluster, &run);
		if (error)
			return (error);
		sector = exfat_cluster_sector(xmp, cluster) +
		    ((offset & (xmp->cluster_size - 1)) >> xmp->sector_shift);
		if ((error = exfat_bread(xmp, sector, &bp, &data)))
			return (error);
		in = offset & (xmp->sector_size - 1);
		n = MIN(len, xmp->sector_size - in);
		memcpy(out, data + in, n);
		buf_brelse(bp);
		out += n;
		offset += n;
		len -= n;
	}
	return (0);
}

void
exfat_dcursor_init(struct exfat_dcursor *c, struct exfat_mount *xmp,
    const struct exfat_info *dir)
{
	c->xmp = xmp;
	exfat_stream_init(&c->stream, dir);
	c->bp = NULL;
	c->sector = 0;
	c->last_byte = 0;
}

void
exfat_dcursor_done(struct exfat_dcursor *c)
{
	if (c->bp != NULL)
		buf_brelse(c->bp);
	c->bp = NULL;
}

/* A directory's entry, by index; ENOENT past its end. */
int
exfat_dentry(struct exfat_dcursor *c, uint64_t index, const uint8_t **entp)
{
	struct exfat_mount *xmp = c->xmp;
	uint64_t byte = index * EXFAT_ENTRY_SIZE, sector;
	uint32_t cluster, run;
	uint8_t *data;
	int error;

	if (byte + EXFAT_ENTRY_SIZE > c->stream.length)
		return (ENOENT);
	error = exfat_map(xmp, &c->stream,
	    (uint32_t)(byte >> xmp->cluster_shift), &cluster, &run);
	if (error)
		return (error);
	sector = exfat_cluster_sector(xmp, cluster) +
	    ((byte & (xmp->cluster_size - 1)) >> xmp->sector_shift);
	if (c->bp == NULL || c->sector != sector) {
		exfat_dcursor_done(c);
		if ((error = exfat_bread(xmp, sector, &c->bp, &data)))
			return (error);
		c->sector = sector;
	} else
		data = (uint8_t *)buf_dataptr(c->bp);
	*entp = data + (byte & (xmp->sector_size - 1));
	c->last_byte = (sector << xmp->sector_shift) +
	    (byte & (xmp->sector_size - 1));
	return (0);
}

/*
 * The next file or directory of a directory, from an entry's index on:
 * a whole entry set whose checksum holds. Deleted entries, the volume's
 * own (bitmap, up-case table, label, GUID) and broken sets are passed
 * over. ENOENT at the end.
 */
int
exfat_dir_next(struct exfat_dcursor *c, uint64_t *indexp, uint64_t parent_id,
    struct exfat_entry *e)
{
	uint8_t set[EXFAT_SET_MAX * EXFAT_ENTRY_SIZE];
	const struct exfat_file_entry *fe;
	const struct exfat_stream_entry *se;
	const struct exfat_name_entry *ne;
	const uint8_t *ent;
	uint64_t index = *indexp, id;
	unsigned n, k, names, chars;
	int error;

	for (;; index++) {
		if ((error = exfat_dentry(c, index, &ent)))
			return (error);
		if (ent[0] == EXFAT_TYPE_END)
			return (ENOENT);
		if (ent[0] != EXFAT_TYPE_FILE)
			continue;
		n = ent[1];
		if (n < 2 || n >= EXFAT_SET_MAX)
			continue;
		id = c->last_byte / EXFAT_ENTRY_SIZE;
		memcpy(set, ent, EXFAT_ENTRY_SIZE);
		for (k = 1; k <= n; k++) {
			error = exfat_dentry(c, index + k, &ent);
			if (error == ENOENT)
				break;
			if (error)
				return (error);
			memcpy(set + k * EXFAT_ENTRY_SIZE, ent, EXFAT_ENTRY_SIZE);
		}
		if (k <= n)
			continue;
		fe = (const struct exfat_file_entry *)set;
		se = (const struct exfat_stream_entry *)(set + EXFAT_ENTRY_SIZE);
		if (exfat_set_checksum(set, n + 1) != le16(fe->checksum) ||
		    se->type != EXFAT_TYPE_STREAM || se->name_length == 0)
			continue;
		names = (se->name_length + EXFAT_NAME_CHARS - 1) /
		    EXFAT_NAME_CHARS;
		if (names > n - 1)
			continue;
		for (k = 0; k < names; k++) {
			ne = (const struct exfat_name_entry *)
			    (set + (2 + k) * EXFAT_ENTRY_SIZE);
			if (ne->type != EXFAT_TYPE_NAME)
				break;
			chars = MIN(EXFAT_NAME_CHARS,
			    se->name_length - k * EXFAT_NAME_CHARS);
			for (unsigned i = 0; i < chars; i++)
				e->name[k * EXFAT_NAME_CHARS + i] =
				    le16(ne->name[i]);
		}
		if (k < names)
			continue;

		e->namelen = se->name_length;
		e->hash = le16(se->name_hash);
		e->index = index;
		e->count = n + 1;
		e->info.id = id;
		e->info.parent_id = parent_id;
		e->info.attr = le16(fe->attributes);
		e->info.ctime = le32(fe->create_time);
		e->info.mtime = le32(fe->modify_time);
		e->info.atime = le32(fe->access_time);
		e->info.c10ms = fe->create_10ms;
		e->info.m10ms = fe->modify_10ms;
		e->info.cutc = fe->create_utc;
		e->info.mutc = fe->modify_utc;
		e->info.autc = fe->access_utc;
		e->info.length = le64(se->length);
		e->info.valid = MIN(le64(se->valid_length), e->info.length);
		if (se->flags & EXFAT_ALLOC_POSSIBLE) {
			e->info.first = le32(se->first_cluster);
			e->info.contig = (se->flags & EXFAT_NO_FAT_CHAIN) != 0;
		} else {
			e->info.first = 0;
			e->info.contig = 0;
		}
		if (e->info.first == 0)
			e->info.length = e->info.valid = 0;
		*indexp = index + n + 1;
		return (0);
	}
}

uint16_t
exfat_upcase(struct exfat_mount *xmp, uint16_t ch)
{
	return (xmp->upcase[ch]);
}

/* A name, already up-cased, in a directory. */
int
exfat_dir_find(struct exfat_mount *xmp, const struct exfat_info *dir,
    const uint16_t *upname, uint16_t len, struct exfat_entry *e)
{
	struct exfat_dcursor c;
	uint64_t index = 0;
	unsigned i;
	int error;

	exfat_dcursor_init(&c, xmp, dir);
	while ((error = exfat_dir_next(&c, &index, dir->id, e)) == 0) {
		if (e->namelen != len)
			continue;
		for (i = 0; i < len; i++)
			if (exfat_upcase(xmp, e->name[i]) != upname[i])
				break;
		if (i == len)
			break;
	}
	exfat_dcursor_done(&c);
	return (error);
}

/*
 * The volume's up-case table, expanded: runs of characters that map to
 * themselves are kept as 0xffff and their count. Without a table, or
 * with a broken one, only ASCII letters fold.
 */
int
exfat_load_upcase(struct exfat_mount *xmp, uint32_t first, uint64_t length,
    uint32_t checksum)
{
	struct exfat_stream st;
	uint16_t *raw = NULL, v;
	uint64_t n, p, ch;
	int error = 0;

	for (ch = 0; ch < EXFAT_UPCASE_CHARS; ch++)
		xmp->upcase[ch] = (uint16_t)ch;
	for (ch = 'a'; ch <= 'z'; ch++)
		xmp->upcase[ch] = (uint16_t)(ch - 'a' + 'A');

	if (first == 0 || length < 2 || length > 2 * 2 * EXFAT_UPCASE_CHARS)
		return (EINVAL);
	raw = OSMalloc((uint32_t)length, exfat_tag);
	if (raw == NULL)
		return (ENOMEM);
	st.first = first;
	st.contig = 0;
	st.length = length;
	st.hint_index = st.hint_cluster = 0;
	if ((error = exfat_read_stream(xmp, &st, 0, raw, (size_t)length)))
		goto out;
	if (exfat_table_checksum((const uint8_t *)raw, length) != checksum) {
		error = EINVAL;
		goto out;
	}
	n = length / 2;
	for (p = 0, ch = 0; p < n && ch < EXFAT_UPCASE_CHARS; p++) {
		v = le16(raw[p]);
		if (v == 0xffff && p + 1 < n) {
			ch += le16(raw[++p]);
			continue;
		}
		xmp->upcase[ch++] = v;
	}
out:
	OSFree(raw, (uint32_t)length, exfat_tag);
	return (error);
}

/* Days since 1970-01-01 of a date of the Gregorian calendar. */
static int64_t
exfat_days(int64_t y, unsigned m, unsigned d)
{
	int64_t era, yoe, doy, doe;

	y -= m <= 2;
	era = (y >= 0 ? y : y - 399) / 400;
	yoe = y - era * 400;
	doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return (era * 146097 + doe - 719468);
}

/*
 * A timestamp: two seconds' resolution, a count of 10 ms on top, and an
 * offset from UTC in quarters of an hour when its top bit is set; else
 * the time is local, west of UTC by what the mount was given.
 */
void
exfat_time(struct exfat_mount *xmp, uint32_t t, uint8_t ms10, uint8_t utc,
    struct timespec *ts)
{
	unsigned sec = (t & 0x1f) * 2, min = (t >> 5) & 0x3f,
	    hour = (t >> 11) & 0x1f, day = (t >> 16) & 0x1f,
	    mon = (t >> 21) & 0x0f, year = 1980 + (t >> 25);
	int64_t s;

	if (t == 0 || mon < 1 || mon > 12 || day < 1) {
		ts->tv_sec = 0;
		ts->tv_nsec = 0;
		return;
	}
	if (ms10 > 199)
		ms10 = 0;
	s = exfat_days(year, mon, day) * 86400 + hour * 3600 + min * 60 +
	    sec + ms10 / 100;
	if (utc & 0x80) {
		int off = (utc & 0x40) ? (int)(utc & 0x7f) - 128 : utc & 0x7f;
		s -= (int64_t)off * 15 * 60;
	} else
		s += xmp->seconds_west;
	ts->tv_sec = (time_t)s;
	ts->tv_nsec = (ms10 % 100) * 10000000L;
}

int
exfat_name_to_utf8(const uint16_t *name, uint16_t len, char *buf,
    size_t size, size_t *outlen)
{
	return (utf8_encodestr(name, (size_t)len * 2, (u_int8_t *)buf, outlen,
	    size, 0, 0));
}
