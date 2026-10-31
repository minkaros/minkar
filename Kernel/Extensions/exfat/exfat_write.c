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
 * exfat: changing the volume. Clusters are taken from and given back to
 * the allocation bitmap; files we make follow FAT chains (a contiguous
 * file from elsewhere stays so while it can grow in place, and becomes a
 * chain when it cannot); entry sets are built, updated and deleted in
 * place. All through the device's buffer cache, written later (sync,
 * fsync and unmount push them). The caller holds the mount's meta lock
 * exclusive.
 */

#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kauth.h>
#include <sys/time.h>
#include <sys/utfconv.h>
#include <libkern/OSByteOrder.h>

#include "exfat_fs.h"

#define le16(x)		OSSwapLittleToHostInt16(x)
#define le32(x)		OSSwapLittleToHostInt32(x)
#define le64(x)		OSSwapLittleToHostInt64(x)
#define tole16(x)	OSSwapHostToLittleInt16(x)
#define tole32(x)	OSSwapHostToLittleInt32(x)
#define tole64(x)	OSSwapHostToLittleInt64(x)

static int
exfat_cluster_ok(struct exfat_mount *xmp, uint32_t cluster)
{
	return (cluster >= EXFAT_FIRST_CLUSTER &&
	    cluster < (uint64_t)xmp->cluster_count + EXFAT_FIRST_CLUSTER);
}

/* A date of the Gregorian calendar from days since 1970-01-01. */
static void
exfat_civil(int64_t days, int64_t *y, unsigned *m, unsigned *d)
{
	int64_t z = days + 719468, era, doe, yoe, doy, mp;

	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	*d = (unsigned)(doy - (153 * mp + 2) / 5 + 1);
	*m = (unsigned)(mp < 10 ? mp + 3 : mp - 9);
	*y = yoe + era * 400 + (*m <= 2);
}

/*
 * A time as exFAT keeps it: local, in the mount's time zone, with its
 * offset from UTC (in quarters of an hour) marked valid.
 */
void
exfat_encode_time(struct exfat_mount *xmp, const struct timespec *ts,
    uint32_t *t, uint8_t *ms10, uint8_t *utc)
{
	int64_t local = (int64_t)ts->tv_sec - xmp->seconds_west, days, secs, y;
	unsigned m, d;
	int off;

	days = local >= 0 ? local / 86400 : (local - 86399) / 86400;
	secs = local - days * 86400;
	exfat_civil(days, &y, &m, &d);
	if (y < 1980) {
		y = 1980;
		m = d = 1;
		secs = 0;
	} else if (y > 2107) {
		y = 2107;
		m = 12;
		d = 31;
		secs = 86399;
	}
	*t = (uint32_t)((y - 1980) << 25) | (m << 21) | (d << 16) |
	    (uint32_t)((secs / 3600) << 11) | (uint32_t)(((secs / 60) % 60) << 5) |
	    (uint32_t)((secs % 60) / 2);
	*ms10 = (uint8_t)((secs % 2) * 100 + ts->tv_nsec / 10000000);
	off = -xmp->seconds_west / 900;
	*utc = (uint8_t)(0x80 | (off & 0x7f));
}

void
exfat_now(struct exfat_mount *xmp, uint32_t *t, uint8_t *ms10, uint8_t *utc)
{
	struct timespec now;

	nanotime(&now);
	exfat_encode_time(xmp, &now, t, ms10, utc);
}

/* The FAT's entry for a cluster becomes value. */
static int
exfat_fat_set(struct exfat_mount *xmp, uint32_t cluster, uint32_t value)
{
	uint64_t byte = (uint64_t)cluster * 4;
	uint8_t *data;
	buf_t bp;
	int error;

	error = exfat_bread(xmp, xmp->fat_offset + (byte >> xmp->sector_shift),
	    &bp, &data);
	if (error)
		return (error);
	value = tole32(value);
	memcpy(data + (byte & (xmp->sector_size - 1)), &value, sizeof(value));
	buf_bdwrite(bp);
	return (0);
}

/* The bitmap's sector that holds a cluster's bit. */
static int
exfat_bitmap_sector(struct exfat_mount *xmp, uint64_t bitmap_sector,
    buf_t *bpp, uint8_t **data)
{
	uint64_t offset = bitmap_sector << xmp->sector_shift;
	uint32_t cluster, run;
	int error;

	error = exfat_map(xmp, &xmp->bitmap,
	    (uint32_t)(offset >> xmp->cluster_shift), &cluster, &run);
	if (error)
		return (error);
	return (exfat_bread(xmp, exfat_cluster_sector(xmp, cluster) +
	    ((offset & (xmp->cluster_size - 1)) >> xmp->sector_shift), bpp,
	    data));
}

/* A free cluster, marked in use: from hint on, round the volume. */
static int
exfat_alloc(struct exfat_mount *xmp, uint32_t hint, uint32_t *out)
{
	uint64_t count = xmp->cluster_count, bps = (uint64_t)xmp->sector_size * 8;
	uint64_t idx, scanned = 0, b;
	uint8_t *data;
	buf_t bp;
	int error;

	if (xmp->free_clusters == 0)
		return (ENOSPC);
	if (!exfat_cluster_ok(xmp, hint))
		hint = exfat_cluster_ok(xmp, xmp->alloc_hint) ?
		    xmp->alloc_hint : EXFAT_FIRST_CLUSTER;
	idx = hint - EXFAT_FIRST_CLUSTER;
	while (scanned < count) {
		if ((error = exfat_bitmap_sector(xmp, idx / bps, &bp, &data)))
			return (error);
		for (b = idx % bps; b < bps && idx < count && scanned < count;
		    b++, idx++, scanned++) {
			if (data[b / 8] & (1u << (b % 8)))
				continue;
			data[b / 8] |= (uint8_t)(1u << (b % 8));
			buf_bdwrite(bp);
			xmp->free_clusters--;
			*out = (uint32_t)idx + EXFAT_FIRST_CLUSTER;
			xmp->alloc_hint = *out + 1;
			return (0);
		}
		buf_brelse(bp);
		if (idx >= count)
			idx = 0;
	}
	return (ENOSPC);
}

static int
exfat_free(struct exfat_mount *xmp, uint32_t cluster)
{
	uint64_t idx = cluster - EXFAT_FIRST_CLUSTER;
	uint64_t bps = (uint64_t)xmp->sector_size * 8;
	uint8_t *data, bit;
	buf_t bp;
	int error;

	if (!exfat_cluster_ok(xmp, cluster))
		return (EIO);
	if ((error = exfat_bitmap_sector(xmp, idx / bps, &bp, &data)))
		return (error);
	bit = (uint8_t)(1u << (idx % 8));
	if (data[(idx % bps) / 8] & bit) {
		data[(idx % bps) / 8] &= (uint8_t)~bit;
		xmp->free_clusters++;
		buf_bdwrite(bp);
	} else
		buf_brelse(bp);
	return (0);
}

/*
 * A directory's cluster freed: its sectors may still sit in the buffer
 * cache, dirty; they must not be written over what uses it next.
 */
static void
exfat_forget_cluster(struct exfat_mount *xmp, uint32_t cluster)
{
	uint64_t sector = exfat_cluster_sector(xmp, cluster);
	uint64_t n = 1ull << xmp->csector_shift, i;
	buf_t bp;

	for (i = 0; i < n; i++) {
		bp = buf_getblk(xmp->devvp, (daddr64_t)(sector + i),
		    (int)xmp->sector_size, 0, 0, BLK_META | BLK_ONLYVALID);
		if (bp != NULL) {
			buf_markinvalid(bp);
			buf_brelse(bp);
		}
	}
}

/* A new directory's cluster: all zeros (no entries, the end marker). */
int
exfat_zero_cluster(struct exfat_mount *xmp, uint32_t cluster)
{
	uint64_t sector = exfat_cluster_sector(xmp, cluster);
	uint64_t n = 1ull << xmp->csector_shift, i;
	buf_t bp;

	for (i = 0; i < n; i++) {
		bp = buf_getblk(xmp->devvp, (daddr64_t)(sector + i),
		    (int)xmp->sector_size, 0, 0, BLK_META);
		if (bp == NULL)
			return (EIO);
		buf_clear(bp);
		buf_bdwrite(bp);
	}
	return (0);
}

static uint64_t
exfat_clusters_of(struct exfat_mount *xmp, uint64_t bytes)
{
	return ((bytes + xmp->cluster_size - 1) >> xmp->cluster_shift);
}

/*
 * A stream grows to `clusters` clusters. A new stream is a FAT chain; a
 * contiguous one stays so while the next cluster is free, and becomes a
 * chain when it is not. On ENOSPC it is left as it was.
 */
int
exfat_grow(struct exfat_mount *xmp, struct exfat_stream *st,
    uint64_t clusters)
{
	uint64_t have = st->first ? exfat_clusters_of(xmp, st->length) : 0, n, i;
	uint32_t last = 0, c, run;
	int error;

	if (clusters <= have)
		return (0);
	if (clusters > xmp->free_clusters + have)
		return (ENOSPC);
	if (have > 0 &&
	    (error = exfat_map(xmp, st, (uint32_t)(have - 1), &last, &run)))
		return (error);
	for (n = have; n < clusters; n++) {
		error = exfat_alloc(xmp, last ? last + 1 : 0, &c);
		if (error) {
			(void)exfat_shrink(xmp, st, have, 0);
			return (error);
		}
		if (n == 0) {
			st->first = c;
			st->contig = 0;
			error = exfat_fat_set(xmp, c, EXFAT_EOC);
		} else if (st->contig && c == last + 1) {
			error = 0;
		} else if (st->contig) {
			/* It cannot stay contiguous: write its chain. */
			for (i = 0; i + 1 < n && error == 0; i++)
				error = exfat_fat_set(xmp, st->first + (uint32_t)i,
				    st->first + (uint32_t)i + 1);
			if (error == 0)
				error = exfat_fat_set(xmp, last, c);
			if (error == 0)
				error = exfat_fat_set(xmp, c, EXFAT_EOC);
			st->contig = 0;
		} else {
			error = exfat_fat_set(xmp, c, EXFAT_EOC);
			if (error == 0)
				error = exfat_fat_set(xmp, last, c);
		}
		st->length = (n + 1) << xmp->cluster_shift;
		st->hint_index = (uint32_t)n;
		st->hint_cluster = c;
		last = c;
		if (error) {
			(void)exfat_shrink(xmp, st, have, 0);
			return (error);
		}
	}
	return (0);
}

/*
 * A stream shrinks to `clusters` clusters; the rest go back to the
 * bitmap (a directory's are first dropped from the buffer cache).
 */
int
exfat_shrink(struct exfat_mount *xmp, struct exfat_stream *st,
    uint64_t clusters, int directory)
{
	uint64_t have = st->first ? exfat_clusters_of(xmp, st->length) : 0, n;
	uint32_t c, next, keep, run;
	int error = 0;

	if (clusters >= have)
		return (0);
	if (st->contig) {
		for (n = clusters; n < have; n++) {
			c = st->first + (uint32_t)n;
			if (directory)
				exfat_forget_cluster(xmp, c);
			(void)exfat_free(xmp, c);
		}
	} else {
		if (clusters == 0)
			c = st->first;
		else {
			error = exfat_map(xmp, st, (uint32_t)(clusters - 1), &keep,
			    &run);
			if (error == 0)
				error = exfat_fat_next(xmp, keep, &c);
			if (error == 0)
				error = exfat_fat_set(xmp, keep, EXFAT_EOC);
			if (error)
				return (error);
		}
		for (n = clusters; n < have && exfat_cluster_ok(xmp, c); n++) {
			if ((error = exfat_fat_next(xmp, c, &next)))
				break;
			(void)exfat_fat_set(xmp, c, 0);
			if (directory)
				exfat_forget_cluster(xmp, c);
			(void)exfat_free(xmp, c);
			c = next;
		}
	}
	if (clusters == 0) {
		st->first = 0;
		st->contig = 0;
	}
	st->length = clusters << xmp->cluster_shift;
	st->hint_index = 0;
	st->hint_cluster = 0;
	return (error);
}

/*
 * A name for a new entry, from UTF-8: precomposed UTF-16, at most 255
 * characters, none of the ones exFAT forbids.
 */
int
exfat_name_from_utf8(const char *utf8, size_t len, uint16_t *name,
    uint16_t *namelen)
{
	size_t bytes;
	unsigned i;
	int error;

	error = utf8_decodestr((const u_int8_t *)utf8, len, name, &bytes,
	    EXFAT_NAME_MAX * 2, 0, UTF_PRECOMPOSED);
	if (error)
		return (error == ENAMETOOLONG ? ENAMETOOLONG : EINVAL);
	if (bytes == 0)
		return (EINVAL);
	for (i = 0; i < bytes / 2; i++) {
		uint16_t c = name[i];

		if (c < 0x20 || c == '"' || c == '*' || c == '/' || c == ':' ||
		    c == '<' || c == '>' || c == '?' || c == '\\' || c == '|')
			return (EINVAL);
	}
	*namelen = (uint16_t)(bytes / 2);
	return (0);
}

/* A file's or directory's entry set, with its name; how many entries. */
unsigned
exfat_build_set(struct exfat_mount *xmp, const struct exfat_info *info,
    const uint16_t *name, uint16_t namelen, uint8_t *set)
{
	struct exfat_file_entry *fe = (struct exfat_file_entry *)set;
	struct exfat_stream_entry *se =
	    (struct exfat_stream_entry *)(set + EXFAT_ENTRY_SIZE);
	uint16_t up[EXFAT_NAME_MAX];
	unsigned names = (namelen + EXFAT_NAME_CHARS - 1) / EXFAT_NAME_CHARS;
	unsigned count = 2 + names, i, k;

	bzero(set, count * EXFAT_ENTRY_SIZE);
	fe->type = EXFAT_TYPE_FILE;
	fe->secondary_count = (uint8_t)(count - 1);
	fe->attributes = tole16(info->attr);
	fe->create_time = tole32(info->ctime);
	fe->modify_time = tole32(info->mtime);
	fe->access_time = tole32(info->atime);
	fe->create_10ms = info->c10ms;
	fe->modify_10ms = info->m10ms;
	fe->create_utc = info->cutc;
	fe->modify_utc = info->mutc;
	fe->access_utc = info->autc;

	for (i = 0; i < namelen; i++)
		up[i] = exfat_upcase(xmp, name[i]);
	se->type = EXFAT_TYPE_STREAM;
	se->flags = EXFAT_ALLOC_POSSIBLE |
	    (info->contig && info->first ? EXFAT_NO_FAT_CHAIN : 0);
	se->name_length = (uint8_t)namelen;
	se->name_hash = tole16(exfat_name_hash(up, namelen));
	se->valid_length = tole64(info->valid);
	se->first_cluster = tole32(info->first);
	se->length = tole64(info->length);

	for (k = 0; k < names; k++) {
		struct exfat_name_entry *ne = (struct exfat_name_entry *)
		    (set + (2 + k) * EXFAT_ENTRY_SIZE);

		ne->type = EXFAT_TYPE_NAME;
		for (i = 0; i < EXFAT_NAME_CHARS &&
		    k * EXFAT_NAME_CHARS + i < namelen; i++)
			ne->name[i] = tole16(name[k * EXFAT_NAME_CHARS + i]);
	}
	fe->checksum = tole16(exfat_set_checksum(set, count));
	return (count);
}

/* Entries by their device bytes, read into or written from set. */
static int
exfat_set_io(struct exfat_mount *xmp, const uint64_t *pos, unsigned count,
    uint8_t *set, int write)
{
	uint64_t sector, cur = UINT64_MAX;
	uint8_t *data = NULL;
	buf_t bp = NULL;
	unsigned i;
	int error;

	for (i = 0; i < count; i++) {
		sector = pos[i] >> xmp->sector_shift;
		if (sector != cur) {
			if (bp != NULL) {
				if (write)
					buf_bdwrite(bp);
				else
					buf_brelse(bp);
			}
			if ((error = exfat_bread(xmp, sector, &bp, &data)))
				return (error);
			cur = sector;
		}
		if (write)
			memcpy(data + (pos[i] & (xmp->sector_size - 1)),
			    set + i * EXFAT_ENTRY_SIZE, EXFAT_ENTRY_SIZE);
		else
			memcpy(set + i * EXFAT_ENTRY_SIZE,
			    data + (pos[i] & (xmp->sector_size - 1)),
			    EXFAT_ENTRY_SIZE);
	}
	if (bp != NULL) {
		if (write)
			buf_bdwrite(bp);
		else
			buf_brelse(bp);
	}
	return (0);
}

int
exfat_write_set(struct exfat_mount *xmp, const uint64_t *pos, unsigned count,
    const uint8_t *set)
{
	return (exfat_set_io(xmp, pos, count, __DECONST(uint8_t *, set), 1));
}

int
exfat_read_set(struct exfat_mount *xmp, const uint64_t *pos, unsigned count,
    uint8_t *set)
{
	return (exfat_set_io(xmp, pos, count, set, 0));
}

/* A file's set on the disk takes its attributes, times and data. */
int
exfat_update_set(struct exfat_mount *xmp, const struct exfat_info *info)
{
	uint8_t set[EXFAT_SET_MAX * EXFAT_ENTRY_SIZE];
	struct exfat_file_entry *fe = (struct exfat_file_entry *)set;
	struct exfat_stream_entry *se =
	    (struct exfat_stream_entry *)(set + EXFAT_ENTRY_SIZE);
	int error;

	if (info->count < 2)
		return (0);		/* the root has none */
	if ((error = exfat_read_set(xmp, info->pos, info->count, set)))
		return (error);
	if (fe->type != EXFAT_TYPE_FILE || se->type != EXFAT_TYPE_STREAM)
		return (EIO);
	fe->attributes = tole16(info->attr);
	fe->create_time = tole32(info->ctime);
	fe->modify_time = tole32(info->mtime);
	fe->access_time = tole32(info->atime);
	fe->create_10ms = info->c10ms;
	fe->modify_10ms = info->m10ms;
	fe->create_utc = info->cutc;
	fe->modify_utc = info->mutc;
	fe->access_utc = info->autc;
	se->flags = (uint8_t)((se->flags & ~EXFAT_NO_FAT_CHAIN) |
	    EXFAT_ALLOC_POSSIBLE |
	    (info->contig && info->first ? EXFAT_NO_FAT_CHAIN : 0));
	se->valid_length = tole64(info->valid);
	se->first_cluster = tole32(info->first);
	se->length = tole64(info->length);
	fe->checksum = tole16(exfat_set_checksum(set, info->count));
	return (exfat_write_set(xmp, info->pos, info->count, set));
}

/* A set's entries marked unused: their space is free in the directory. */
int
exfat_delete_set(struct exfat_mount *xmp, const uint64_t *pos, unsigned count)
{
	uint8_t set[EXFAT_SET_MAX * EXFAT_ENTRY_SIZE];
	unsigned i;
	int error;

	if ((error = exfat_read_set(xmp, pos, count, set)))
		return (error);
	for (i = 0; i < count; i++)
		set[i * EXFAT_ENTRY_SIZE] &= (uint8_t)~EXFAT_INUSE;
	return (exfat_write_set(xmp, pos, count, set));
}

/*
 * Room for `count` entries in a row in a directory: unused entries (the
 * end marker and all after it are unused), or a new cluster at its end.
 */
int
exfat_dir_space(struct exfat_mount *xmp, struct exfat_node *dnp,
    unsigned count, uint64_t *pos)
{
	struct exfat_dcursor c;
	const uint8_t *ent;
	uint64_t index, start = 0;
	unsigned run = 0;
	int error;

	for (;;) {
		exfat_dcursor_init(&c, xmp, &dnp->info);
		for (index = start;; index++) {
			error = exfat_dentry(&c, index, &ent);
			if (error)
				break;
			if (ent[0] & EXFAT_INUSE) {
				run = 0;
				continue;
			}
			pos[run++] = c.last_byte;
			if (run == count) {
				exfat_dcursor_done(&c);
				return (0);
			}
		}
		exfat_dcursor_done(&c);
		if (error != ENOENT)
			return (error);

		/* Full: one more cluster, zeroed; its size in its set. */
		struct exfat_stream st;
		uint64_t clusters = exfat_clusters_of(xmp, dnp->info.length);

		lck_mtx_lock(dnp->lock);
		st = dnp->stream;
		error = exfat_grow(xmp, &st, clusters + 1);
		if (error == 0)
			error = exfat_zero_cluster(xmp, st.hint_cluster);
		if (error == 0) {
			dnp->stream = st;
			dnp->info.first = st.first;
			dnp->info.contig = st.contig;
			dnp->info.length = dnp->info.valid = st.length;
		} else
			(void)exfat_shrink(xmp, &st, clusters, 1);
		lck_mtx_unlock(dnp->lock);
		if (error)
			return (error);
		exfat_node_changed(xmp, dnp);
		start = index;		/* the run goes on into it */
	}
}

/* A directory holds nothing (0), or something (ENOTEMPTY). */
int
exfat_dir_empty(struct exfat_mount *xmp, const struct exfat_info *dir)
{
	struct exfat_dcursor c;
	const uint8_t *ent;
	uint64_t index;
	int error;

	exfat_dcursor_init(&c, xmp, dir);
	for (index = 0;; index++) {
		if ((error = exfat_dentry(&c, index, &ent)))
			break;
		if (ent[0] == EXFAT_TYPE_END)
			break;
		if (ent[0] == EXFAT_TYPE_FILE) {
			error = ENOTEMPTY;
			break;
		}
	}
	exfat_dcursor_done(&c);
	return (error == ENOENT ? 0 : error);
}

/* The volume's dirty flag, set while mounted read-write. */
int
exfat_set_dirty(struct exfat_mount *xmp, int dirty)
{
	struct exfat_boot *b;
	uint8_t *data;
	uint16_t flags;
	buf_t bp;
	int error;

	if ((error = exfat_bread(xmp, 0, &bp, &data)))
		return (error);
	b = (struct exfat_boot *)data;
	flags = le16(b->flags);
	if (dirty)
		flags |= EXFAT_DIRTY;
	else
		flags &= (uint16_t)~EXFAT_DIRTY;
	b->flags = tole16(flags);
	return (buf_bwrite(bp));
}
