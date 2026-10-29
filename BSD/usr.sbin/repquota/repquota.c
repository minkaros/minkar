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
 * repquota: a table of every user's or group's usage and limits on a
 * volume, from its quota file (with the kernel's current figures when
 * quotas are on).
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/quota.h>

#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dqfile.h"

static bool all, vflag, want[MAXQUOTAS];
static int errors;

struct rep {
	const char	*mnt;
	int		type;
	bool		on;
};

static void
usage(void)
{

	fprintf(stderr, "usage: repquota [-g] [-u] [-v] -a\n"
	    "       repquota [-g] [-u] [-v] filesystem ...\n");
	exit(1);
}

static const char *
grace(time_t limit)
{
	static char buf[32];
	time_t now = time(NULL);
	long left;

	if (limit == 0)
		return ("");
	if (now >= limit)
		return ("none");
	left = (long)(limit - now);
	if (left >= 2 * 24 * 60 * 60)
		snprintf(buf, sizeof(buf), "%lddays", (left + 12 * 3600) / 86400);
	else if (left >= 2 * 60 * 60)
		snprintf(buf, sizeof(buf), "%ldhours", (left + 1800) / 3600);
	else
		snprintf(buf, sizeof(buf), "%ld:%02ld", left / 3600,
		    (left % 3600) / 60);
	return (buf);
}

static const char *
name(int type, uint32_t id)
{
	static char buf[16];
	struct passwd *pw;
	struct group *gr;

	if (type == USRQUOTA && (pw = getpwuid(id)) != NULL)
		return (pw->pw_name);
	if (type == GRPQUOTA && (gr = getgrgid(id)) != NULL)
		return (gr->gr_name);
	snprintf(buf, sizeof(buf), "#%u", id);
	return (buf);
}

static int
line(const struct dqblk *fb, void *arg)
{
	struct rep *r = arg;
	struct dqblk b = *fb;

	if (r->on && quotactl(r->mnt, QCMD(Q_GETQUOTA, r->type),
	    (int)fb->dqb_id, (caddr_t)&b) != 0)
		b = *fb;
	if (!vflag && b.dqb_curbytes == 0 && b.dqb_curinodes == 0 &&
	    !b.dqb_bsoftlimit && !b.dqb_bhardlimit &&
	    !b.dqb_isoftlimit && !b.dqb_ihardlimit)
		return (0);
	printf("%-10s%c%c%9llu%9llu%9llu%7s%8u%7u%7u%7s\n",
	    name(r->type, fb->dqb_id),
	    b.dqb_bsoftlimit && b.dqb_curbytes >= b.dqb_bsoftlimit ? '+' : '-',
	    b.dqb_isoftlimit && b.dqb_curinodes >= b.dqb_isoftlimit ? '+' : '-',
	    (unsigned long long)DQ_KB(b.dqb_curbytes),
	    (unsigned long long)DQ_KB(b.dqb_bsoftlimit),
	    (unsigned long long)DQ_KB(b.dqb_bhardlimit),
	    b.dqb_bsoftlimit && b.dqb_curbytes >= b.dqb_bsoftlimit ?
	    grace(b.dqb_btime) : "",
	    b.dqb_curinodes, b.dqb_isoftlimit, b.dqb_ihardlimit,
	    b.dqb_isoftlimit && b.dqb_curinodes >= b.dqb_isoftlimit ?
	    grace(b.dqb_itime) : "");
	return (0);
}

static void
report(const char *mnt, const char *dev, int type)
{
	struct dqfile q;
	struct rep r = { mnt, type, dq_enabled(mnt, type) };

	if (dq_open(&q, mnt, type, false, 0) != 0) {
		warn("%s: %s quota file", mnt, dq_typename(type));
		errors++;
		return;
	}
	printf("*** Report for %s quotas on %s (%s)\n", dq_typename(type),
	    mnt, dev);
	printf("%-12s%27s%29s\n", "", "1K Block limits", "File limits");
	printf("%-12s%9s%9s%9s%7s%8s%7s%7s%7s\n",
	    type == USRQUOTA ? "User" : "Group", "used", "soft", "hard",
	    "grace", "used", "soft", "hard", "grace");
	dq_foreach(&q, line, &r);
	printf("\n");
	dq_close(&q);
}

static void
dofs(const char *mnt, const char *dev, bool explicit)
{
	bool any = want[USRQUOTA] || want[GRPQUOTA];
	int t;

	for (t = 0; t < MAXQUOTAS; t++) {
		if (any && !want[t])
			continue;
		if (!dq_wanted(mnt, t) && !dq_enabled(mnt, t) &&
		    !(explicit && any))
			continue;
		report(mnt, dev, t);
	}
}

static int
fromall(const char *mnt, const char *dev, void *arg)
{

	(void)arg;
	dofs(mnt, dev, false);
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
			usage();
		}
	argc -= optind;
	argv += optind;
	if (all == (argc != 0))
		usage();
	if (all)
		dq_mounts(fromall, NULL);
	for (i = 0; i < argc; i++) {
		if (statfs(argv[i], &sfs) != 0) {
			warn("%s", argv[i]);
			errors++;
			continue;
		}
		dofs(sfs.f_mntonname, sfs.f_mntfromname, true);
	}
	return (errors ? 1 : 0);
}
