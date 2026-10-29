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
 * Reading and writing Darwin's quota hash files (dqfile.h), and the
 * quotactl(2) calls the quota tools share.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/quota.h>

#include <errno.h>
#include <fcntl.h>
#include <libkern/OSByteOrder.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dqfile.h"

static const char *const qfnames[] = INITQFNAMES;
static const uint32_t qfmagics[] = INITQMAGICS;

/*
 * The files are big-endian on disk (checked against what the kernel
 * writes: image/bsd-quota in the tests).
 */
#define	DQ32(x)	OSSwapBigToHostInt32(x)
#define	DQ64(x)	OSSwapBigToHostInt64(x)
#define	HQ32(x)	OSSwapHostToBigInt32(x)
#define	HQ64(x)	OSSwapHostToBigInt64(x)

const char *
dq_typename(int type)
{

	return (type == USRQUOTA || type == GRPQUOTA ? qfnames[type] :
	    "undefined");
}

void
dq_filepath(const char *mnt, int type, char *buf, size_t len)
{

	snprintf(buf, len, "%s%s%s.%s", mnt,
	    mnt[strlen(mnt) - 1] == '/' ? "" : "/", QUOTAFILENAME,
	    dq_typename(type));
}

void
dq_opspath(const char *mnt, int type, char *buf, size_t len)
{

	snprintf(buf, len, "%s%s%s.%s", mnt,
	    mnt[strlen(mnt) - 1] == '/' ? "" : "/", QUOTAOPSNAME,
	    dq_typename(type));
}

bool
dq_wanted(const char *mnt, int type)
{
	char path[MAXPATHLEN];

	dq_opspath(mnt, type, path, sizeof(path));
	return (access(path, F_OK) == 0);
}

bool
dq_enabled(const char *mnt, int type)
{
	int status = 0;

	if (quotactl(mnt, QCMD(Q_QUOTASTAT, type), 0, (caddr_t)&status) != 0)
		return (false);
	return (status != 0);
}

static void
blk_in(struct dqblk *b)
{

	b->dqb_bhardlimit = DQ64(b->dqb_bhardlimit);
	b->dqb_bsoftlimit = DQ64(b->dqb_bsoftlimit);
	b->dqb_curbytes = DQ64(b->dqb_curbytes);
	b->dqb_ihardlimit = DQ32(b->dqb_ihardlimit);
	b->dqb_isoftlimit = DQ32(b->dqb_isoftlimit);
	b->dqb_curinodes = DQ32(b->dqb_curinodes);
	b->dqb_btime = DQ32(b->dqb_btime);
	b->dqb_itime = DQ32(b->dqb_itime);
	b->dqb_id = DQ32(b->dqb_id);
}

static void
blk_out(struct dqblk *b)
{

	b->dqb_bhardlimit = HQ64(b->dqb_bhardlimit);
	b->dqb_bsoftlimit = HQ64(b->dqb_bsoftlimit);
	b->dqb_curbytes = HQ64(b->dqb_curbytes);
	b->dqb_ihardlimit = HQ32(b->dqb_ihardlimit);
	b->dqb_isoftlimit = HQ32(b->dqb_isoftlimit);
	b->dqb_curinodes = HQ32(b->dqb_curinodes);
	b->dqb_btime = HQ32(b->dqb_btime);
	b->dqb_itime = HQ32(b->dqb_itime);
	b->dqb_id = HQ32(b->dqb_id);
}

static void
hdr_swap(struct dqfilehdr *h, uint32_t (*f)(uint32_t))
{

	h->dqh_magic = f(h->dqh_magic);
	h->dqh_version = f(h->dqh_version);
	h->dqh_maxentries = f(h->dqh_maxentries);
	h->dqh_entrycnt = f(h->dqh_entrycnt);
	h->dqh_flags = f(h->dqh_flags);
	h->dqh_chktime = f(h->dqh_chktime);
	h->dqh_btime = f(h->dqh_btime);
	h->dqh_itime = f(h->dqh_itime);
}

static uint32_t
be_in(uint32_t x)
{

	return (DQ32(x));
}

static uint32_t
be_out(uint32_t x)
{

	return (HQ32(x));
}

/* Entries for a volume, from its size and the per-GB rates. */
static uint32_t
default_entries(const char *mnt, int type)
{
	struct statfs sfs;
	uint64_t gb, n;
	uint32_t want, lo, hi;

	lo = type == USRQUOTA ? QF_MIN_USERS : QF_MIN_GROUPS;
	hi = type == USRQUOTA ? QF_MAX_USERS : QF_MAX_GROUPS;
	if (statfs(mnt, &sfs) != 0)
		return (lo);
	gb = ((uint64_t)sfs.f_blocks * sfs.f_bsize) >> 30;
	n = gb * (type == USRQUOTA ? QF_USERS_PER_GB : QF_GROUPS_PER_GB);
	if (n < lo)
		n = lo;
	if (n > hi)
		n = hi;
	for (want = 1; want < n; want <<= 1)
		continue;
	return (want);
}

int
dq_puthdr(struct dqfile *q)
{
	struct dqfilehdr h = q->hdr;

	hdr_swap(&h, be_out);
	if (pwrite(q->fd, &h, sizeof(h), 0) != (ssize_t)sizeof(h))
		return (-1);
	return (0);
}

int
dq_open(struct dqfile *q, const char *mnt, int type, bool create,
    uint32_t maxentries)
{
	struct stat st;
	ssize_t n;

	memset(q, 0, sizeof(*q));
	q->type = type;
	dq_filepath(mnt, type, q->path, sizeof(q->path));
	q->fd = open(q->path, create ? O_RDWR | O_CREAT : O_RDWR, 0640);
	if (q->fd == -1 && !create && errno == EACCES)
		q->fd = open(q->path, O_RDONLY);
	if (q->fd == -1)
		return (-1);
	if (fstat(q->fd, &st) != 0)
		goto fail;
	if (st.st_size == 0) {
		struct dqblk empty;
		uint32_t i;

		if (!create) {
			errno = ENOENT;
			goto fail;
		}
		if (maxentries == 0)
			maxentries = default_entries(mnt, type);
		q->hdr.dqh_magic = qfmagics[type];
		q->hdr.dqh_version = QF_VERSION;
		q->hdr.dqh_maxentries = maxentries;
		q->hdr.dqh_btime = MAX_DQ_TIME;
		q->hdr.dqh_itime = MAX_IQ_TIME;
		strlcpy(q->hdr.dqh_string, QF_STRING_TAG,
		    sizeof(q->hdr.dqh_string));
		if (dq_puthdr(q) != 0)
			goto fail;
		memset(&empty, 0, sizeof(empty));
		for (i = 0; i < maxentries; i++)
			if (pwrite(q->fd, &empty, sizeof(empty),
			    dqoffset(i)) != (ssize_t)sizeof(empty))
				goto fail;
		return (0);
	}
	n = pread(q->fd, &q->hdr, sizeof(q->hdr), 0);
	if (n != (ssize_t)sizeof(q->hdr))
		goto bad;
	hdr_swap(&q->hdr, be_in);
	if (q->hdr.dqh_magic != qfmagics[type] ||
	    q->hdr.dqh_version != QF_VERSION ||
	    q->hdr.dqh_maxentries == 0 ||
	    (q->hdr.dqh_maxentries & (q->hdr.dqh_maxentries - 1)) != 0)
		goto bad;
	return (0);
bad:
	errno = EFTYPE;
fail:
	{
		int e = errno;

		close(q->fd);
		q->fd = -1;
		errno = e;
	}
	return (-1);
}

void
dq_close(struct dqfile *q)
{

	if (q->fd != -1)
		close(q->fd);
	q->fd = -1;
}

/*
 * The slot for id: where it is, or the free slot where it would go.
 * -1 when the table is full and id is not in it.
 */
static int64_t
dq_slot(struct dqfile *q, uint32_t id, struct dqblk *b, bool *found)
{
	uint32_t mask, shift, i, skip, n;

	mask = q->hdr.dqh_maxentries - 1;
	shift = dqhashshift(q->hdr.dqh_maxentries);
	i = dqhash1(id, shift, mask);
	skip = dqhash2(id, mask);
	for (n = 0; n <= mask; n++) {
		if (pread(q->fd, b, sizeof(*b), dqoffset(i)) !=
		    (ssize_t)sizeof(*b))
			return (-1);
		blk_in(b);
		if (b->dqb_id == id) {
			*found = true;
			return (i);
		}
		if (b->dqb_id == 0) {
			*found = false;
			return (i);
		}
		i = (i + skip) & mask;
	}
	errno = ENOSPC;
	return (-1);
}

int
dq_get(struct dqfile *q, uint32_t id, struct dqblk *b)
{
	bool found = false;

	if (dq_slot(q, id, b, &found) == -1) {
		if (errno != ENOSPC)
			return (-1);
		found = false;	/* full, and not there */
	}
	if (!found) {
		memset(b, 0, sizeof(*b));
		b->dqb_id = id;
	}
	return (0);
}

int
dq_put(struct dqfile *q, uint32_t id, const struct dqblk *in)
{
	struct dqblk b;
	int64_t slot;
	bool found;

	if (id == 0) {		/* 0 marks a free slot: root has none */
		errno = EINVAL;
		return (-1);
	}
	if ((slot = dq_slot(q, id, &b, &found)) == -1)
		return (-1);
	b = *in;
	b.dqb_id = id;
	blk_out(&b);
	if (pwrite(q->fd, &b, sizeof(b), dqoffset(slot)) != (ssize_t)sizeof(b))
		return (-1);
	if (!found) {
		q->hdr.dqh_entrycnt++;
		return (dq_puthdr(q));
	}
	return (0);
}

int
dq_foreach(struct dqfile *q, int (*fn)(const struct dqblk *, void *),
    void *arg)
{
	struct dqblk b;
	uint32_t i;
	int rv;

	for (i = 0; i < q->hdr.dqh_maxentries; i++) {
		if (pread(q->fd, &b, sizeof(b), dqoffset(i)) !=
		    (ssize_t)sizeof(b))
			return (-1);
		blk_in(&b);
		if (b.dqb_id == 0)
			continue;
		if ((rv = fn(&b, arg)) != 0)
			return (rv);
	}
	return (0);
}

int
dq_getquota(const char *mnt, int type, uint32_t id, struct dqblk *b)
{
	struct dqfile q;
	int rv;

	memset(b, 0, sizeof(*b));
	if (dq_enabled(mnt, type))
		return (quotactl(mnt, QCMD(Q_GETQUOTA, type), (int)id,
		    (caddr_t)b));
	if (dq_open(&q, mnt, type, false, 0) != 0)
		return (-1);
	rv = dq_get(&q, id, b);
	dq_close(&q);
	return (rv);
}

int
dq_setquota(const char *mnt, int type, uint32_t id, const struct dqblk *b)
{
	struct dqfile q;
	struct dqblk cur;
	int rv;

	if (dq_enabled(mnt, type)) {
		cur = *b;
		return (quotactl(mnt, QCMD(Q_SETQUOTA, type), (int)id,
		    (caddr_t)&cur));
	}
	if (dq_open(&q, mnt, type, true, 0) != 0)
		return (-1);
	/* Keep the usage the file has; set only the limits. */
	if ((rv = dq_get(&q, id, &cur)) == 0) {
		cur.dqb_bhardlimit = b->dqb_bhardlimit;
		cur.dqb_bsoftlimit = b->dqb_bsoftlimit;
		cur.dqb_ihardlimit = b->dqb_ihardlimit;
		cur.dqb_isoftlimit = b->dqb_isoftlimit;
		cur.dqb_btime = b->dqb_btime;
		cur.dqb_itime = b->dqb_itime;
		rv = dq_put(&q, id, &cur);
	}
	dq_close(&q);
	return (rv);
}

int
dq_mounts(int (*fn)(const char *, const char *, void *), void *arg)
{
	struct statfs *mnts;
	int i, n, rv;

	if ((n = getmntinfo(&mnts, MNT_NOWAIT)) == 0)
		return (-1);
	for (i = 0; i < n; i++) {
		if ((mnts[i].f_flags & MNT_LOCAL) == 0 ||
		    strcmp(mnts[i].f_fstypename, "devfs") == 0)
			continue;
		if ((rv = fn(mnts[i].f_mntonname, mnts[i].f_mntfromname,
		    arg)) != 0)
			return (rv);
	}
	return (0);
}
