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
 * quotacheck: count what each user and group has on a volume (space as
 * allocated blocks, and files, a hard-linked file once) and write it to
 * the volume's quota files, keeping the limits they hold; made when
 * missing. While quotas are on, the kernel gets the new usage too.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/quota.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dqfile.h"

static bool all, vflag, want[MAXQUOTAS];
static int errors;

struct use {
	uint32_t	id;
	uint64_t	bytes;
	uint32_t	files;
	bool		used;	/* the slot holds an id */
	bool		done;	/* already in the quota file */
};

struct table {
	struct use	*v;
	size_t		n, cap;
};

static struct table usage[MAXQUOTAS];

/* Inodes with more than one link already counted. */
static ino_t *seen;
static size_t nseen, capseen;

static void
usage_exit(void)
{

	fprintf(stderr, "usage: quotacheck [-g] [-u] [-v] -a\n"
	    "       quotacheck [-g] [-u] [-v] filesystem ...\n");
	exit(1);
}

static struct use *
lookup(struct table *t, uint32_t id)
{
	size_t i;

	if (t->cap == 0) {
		t->cap = 1024;
		if ((t->v = calloc(t->cap, sizeof(*t->v))) == NULL)
			err(1, "calloc");
	}
	if (t->n * 2 >= t->cap) {
		struct table nt = { NULL, 0, t->cap * 2 };

		if ((nt.v = calloc(nt.cap, sizeof(*nt.v))) == NULL)
			err(1, "calloc");
		for (i = 0; i < t->cap; i++)
			if (t->v[i].used)
				*lookup(&nt, t->v[i].id) = t->v[i];
		free(t->v);
		*t = nt;
	}
	for (i = (id * 2654435761U) & (t->cap - 1); t->v[i].used;
	    i = (i + 1) & (t->cap - 1))
		if (t->v[i].id == id)
			return (&t->v[i]);
	t->v[i].used = true;
	t->v[i].id = id;
	t->n++;
	return (&t->v[i]);
}

static bool
seen_before(ino_t ino)
{
	size_t i;

	for (i = 0; i < nseen; i++)
		if (seen[i] == ino)
			return (true);
	if (nseen == capseen) {
		capseen = capseen ? capseen * 2 : 256;
		if ((seen = realloc(seen, capseen * sizeof(*seen))) == NULL)
			err(1, "realloc");
	}
	seen[nseen++] = ino;
	return (false);
}

static void
count(const struct stat *st, const bool types[MAXQUOTAS])
{
	struct use *u;
	uint64_t bytes = (uint64_t)st->st_blocks * 512;

	if (!S_ISDIR(st->st_mode) && st->st_nlink > 1 &&
	    seen_before(st->st_ino))
		return;
	if (types[USRQUOTA] && st->st_uid != 0) {
		u = lookup(&usage[USRQUOTA], st->st_uid);
		u->bytes += bytes;
		u->files++;
	}
	if (types[GRPQUOTA] && st->st_gid != 0) {
		u = lookup(&usage[GRPQUOTA], st->st_gid);
		u->bytes += bytes;
		u->files++;
	}
}

/* Walk the volume at mnt without leaving it. */
static int
walk(const char *mnt, const bool types[MAXQUOTAS])
{
	char *paths[] = { (char *)mnt, NULL };
	FTS *fts;
	FTSENT *e;

	if ((fts = fts_open(paths, FTS_PHYSICAL | FTS_XDEV | FTS_NOCHDIR,
	    NULL)) == NULL) {
		warn("%s", mnt);
		return (-1);
	}
	while ((e = fts_read(fts)) != NULL) {
		switch (e->fts_info) {
		case FTS_DP:
			continue;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			warnx("%s: %s", e->fts_path, strerror(e->fts_errno));
			continue;
		default:
			count(e->fts_statp, types);
		}
	}
	fts_close(fts);
	return (0);
}

struct fix {
	struct dqfile	*q;
	struct table	*t;
	const char	*mnt;
	bool		on;
};

/* Existing entries: their usage as counted (0 when nothing was found). */
static int
fixone(const struct dqblk *fb, void *arg)
{
	struct fix *x = arg;
	struct dqblk b = *fb;
	struct use *u = lookup(x->t, fb->dqb_id);

	if (vflag && (b.dqb_curbytes != u->bytes ||
	    b.dqb_curinodes != u->files))
		printf("%s: %s %u fixed: %llu -> %llu bytes, %u -> %u files\n",
		    x->mnt, dq_typename(x->q->type), fb->dqb_id,
		    (unsigned long long)b.dqb_curbytes,
		    (unsigned long long)u->bytes, b.dqb_curinodes, u->files);
	b.dqb_curbytes = u->bytes;
	b.dqb_curinodes = u->files;
	if (dq_put(x->q, fb->dqb_id, &b) != 0)
		return (-1);
	u->done = true;
	return (0);
}

static int
setuse(const struct dqblk *b, void *arg)
{
	struct fix *x = arg;
	struct dqblk u = *b;

	if (quotactl(x->mnt, QCMD(Q_SETUSE, x->q->type), (int)b->dqb_id,
	    (caddr_t)&u) != 0)
		warn("%s: %s %u: usage to the kernel", x->mnt,
		    dq_typename(x->q->type), b->dqb_id);
	return (0);
}

static void
update(const char *mnt, int type)
{
	struct dqfile q;
	struct table *t = &usage[type];
	struct fix x = { &q, t, mnt, dq_enabled(mnt, type) };
	struct dqblk b;
	size_t i;

	if (dq_open(&q, mnt, type, true, 0) != 0) {
		warn("%s: %s quota file", mnt, dq_typename(type));
		errors++;
		return;
	}
	if (dq_foreach(&q, fixone, &x) != 0) {
		warn("%s", q.path);
		errors++;
	}
	for (i = 0; i < t->cap; i++) {
		if (!t->v[i].used || t->v[i].done)
			continue;
		memset(&b, 0, sizeof(b));
		b.dqb_curbytes = t->v[i].bytes;
		b.dqb_curinodes = t->v[i].files;
		if (vflag)
			printf("%s: %s %u added: %llu bytes, %u files\n", mnt,
			    dq_typename(type), t->v[i].id,
			    (unsigned long long)b.dqb_curbytes, b.dqb_curinodes);
		if (dq_put(&q, t->v[i].id, &b) != 0) {
			warn("%s: %u", q.path, t->v[i].id);
			errors++;
		}
	}
	q.hdr.dqh_chktime = (uint32_t)time(NULL);
	dq_puthdr(&q);
	dq_close(&q);

	/* The kernel's copy, when quotas are on. */
	if (x.on && dq_open(&q, mnt, type, false, 0) == 0) {
		x.q = &q;
		dq_foreach(&q, setuse, &x);
		dq_close(&q);
	}
}

static void
check(const char *mnt, bool explicit)
{
	bool any = want[USRQUOTA] || want[GRPQUOTA], types[MAXQUOTAS];
	int t, n = 0;

	for (t = 0; t < MAXQUOTAS; t++) {
		types[t] = (!any || want[t]) &&
		    (dq_wanted(mnt, t) || (explicit && any));
		n += types[t];
		free(usage[t].v);
		memset(&usage[t], 0, sizeof(usage[t]));
	}
	nseen = 0;
	if (n == 0)
		return;
	if (vflag)
		printf("*** Checking %s%s quotas for %s\n",
		    types[USRQUOTA] ? "user" : "",
		    types[USRQUOTA] && types[GRPQUOTA] ? " and group" :
		    types[GRPQUOTA] ? "group" : "", mnt);
	if (walk(mnt, types) != 0) {
		errors++;
		return;
	}
	for (t = 0; t < MAXQUOTAS; t++)
		if (types[t])
			update(mnt, t);
}

static int
fromall(const char *mnt, const char *dev, void *arg)
{

	(void)dev;
	(void)arg;
	check(mnt, false);
	return (0);
}

int
main(int argc, char *argv[])
{
	struct statfs sfs;
	int ch, i;

	while ((ch = getopt(argc, argv, "aguv")) != -1)
		switch (ch) {
		case 'a':
			all = true;
			break;
		case 'g':
			want[GRPQUOTA] = true;
			break;
		case 'u':
			want[USRQUOTA] = true;
			break;
		case 'v':
			vflag = true;
			break;
		default:
			usage_exit();
		}
	argc -= optind;
	argv += optind;
	if (all == (argc != 0))
		usage_exit();
	if (getuid() != 0)
		errx(1, "permission denied");
	if (all)
		dq_mounts(fromall, NULL);
	for (i = 0; i < argc; i++) {
		if (statfs(argv[i], &sfs) != 0) {
			warn("%s", argv[i]);
			errors++;
			continue;
		}
		check(sfs.f_mntonname, true);
	}
	return (errors ? 1 : 0);
}
