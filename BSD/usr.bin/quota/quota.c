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
 * quota: show a user's or group's disk usage and limits on each volume
 * with quotas, in 1K blocks and files, as BSD's quota does.
 */

#include <sys/param.h>
#include <sys/quota.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "dqfile.h"

static bool qflag, vflag;

struct who {
	int		type;
	uint32_t	id;
	const char	*name;
	bool		printed;
	int		over;
};

static void
usage(void)
{

	fprintf(stderr, "usage: quota [-guqv]\n"
	    "       quota [-qv] -u user ...\n"
	    "       quota [-qv] -g group ...\n");
	exit(1);
}

/* "none", a grace time left, or "expired", as quota shows them. */
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

static void
header(struct who *w)
{

	if (w->printed)
		return;
	w->printed = true;
	printf("Disk quotas for %s %s (%cid %u):\n", dq_typename(w->type),
	    w->name, w->type == USRQUOTA ? 'u' : 'g', w->id);
	if (!qflag)
		printf("%15s%8s %7s%8s%8s%8s %7s%8s%8s\n", "Filesystem",
		    "usage", "quota", "limit", "grace", "files", "quota",
		    "limit", "grace");
}

static int
onfs(const char *mnt, const char *dev, void *arg)
{
	struct who *w = arg;
	struct dqblk b;
	time_t now = time(NULL);
	bool bover, iover, limited;
	const char *msg;

	(void)dev;
	if (!dq_enabled(mnt, w->type) && !dq_wanted(mnt, w->type))
		return (0);
	if (dq_getquota(mnt, w->type, w->id, &b) != 0) {
		if (errno == EPERM || errno == EACCES)
			warnx("%s: not allowed to read %s's quota", mnt,
			    w->name);
		return (0);
	}
	limited = b.dqb_bsoftlimit || b.dqb_bhardlimit ||
	    b.dqb_isoftlimit || b.dqb_ihardlimit;
	if (!limited && !vflag)
		return (0);
	bover = (b.dqb_bhardlimit && b.dqb_curbytes >= b.dqb_bhardlimit) ||
	    (b.dqb_bsoftlimit && b.dqb_curbytes > b.dqb_bsoftlimit);
	iover = (b.dqb_ihardlimit && b.dqb_curinodes >= b.dqb_ihardlimit) ||
	    (b.dqb_isoftlimit && b.dqb_curinodes > b.dqb_isoftlimit);
	if (qflag) {
		if (b.dqb_bhardlimit && b.dqb_curbytes >= b.dqb_bhardlimit)
			msg = "Block limit reached on";
		else if (b.dqb_bsoftlimit && b.dqb_curbytes > b.dqb_bsoftlimit)
			msg = b.dqb_btime && now > (time_t)b.dqb_btime ?
			    "Over block quota on" : "In block grace period on";
		else
			msg = NULL;
		if (msg) {
			header(w);
			printf("\t%s %s\n", msg, mnt);
		}
		if (b.dqb_ihardlimit && b.dqb_curinodes >= b.dqb_ihardlimit)
			msg = "File limit reached on";
		else if (b.dqb_isoftlimit && b.dqb_curinodes > b.dqb_isoftlimit)
			msg = b.dqb_itime && now > (time_t)b.dqb_itime ?
			    "Over file quota on" : "In file grace period on";
		else
			msg = NULL;
		if (msg) {
			header(w);
			printf("\t%s %s\n", msg, mnt);
		}
		if (bover || iover)
			w->over++;
		return (0);
	}
	header(w);
	printf("%15s%8llu%c%7llu%8llu%8s%8u%c%7u%8u%8s\n", mnt,
	    (unsigned long long)DQ_KB(b.dqb_curbytes), bover ? '*' : ' ',
	    (unsigned long long)DQ_KB(b.dqb_bsoftlimit),
	    (unsigned long long)DQ_KB(b.dqb_bhardlimit),
	    b.dqb_bsoftlimit && b.dqb_curbytes > b.dqb_bsoftlimit ?
	    grace(b.dqb_btime) : "",
	    b.dqb_curinodes, iover ? '*' : ' ', b.dqb_isoftlimit,
	    b.dqb_ihardlimit,
	    b.dqb_isoftlimit && b.dqb_curinodes > b.dqb_isoftlimit ?
	    grace(b.dqb_itime) : "");
	if (bover || iover)
		w->over++;
	return (0);
}

static int
show(int type, uint32_t id, const char *name)
{
	struct who w = { type, id, name, false, 0 };

	dq_mounts(onfs, &w);
	if (!w.printed && !qflag)
		printf("Disk quotas for %s %s (%cid %u): none\n",
		    dq_typename(type), name, type == USRQUOTA ? 'u' : 'g', id);
	return (w.over);
}

static int
user(const char *arg)
{
	struct passwd *pw;
	char *end;
	unsigned long id;

	if ((pw = getpwnam(arg)) != NULL)
		return (show(USRQUOTA, pw->pw_uid, pw->pw_name));
	id = strtoul(arg, &end, 10);
	if (*arg == '\0' || *end != '\0') {
		warnx("%s: unknown user", arg);
		return (1);
	}
	return (show(USRQUOTA, (uint32_t)id, arg));
}

static int
group(const char *arg)
{
	struct group *gr;
	char *end;
	unsigned long id;

	if ((gr = getgrnam(arg)) != NULL)
		return (show(GRPQUOTA, gr->gr_gid, gr->gr_name));
	id = strtoul(arg, &end, 10);
	if (*arg == '\0' || *end != '\0') {
		warnx("%s: unknown group", arg);
		return (1);
	}
	return (show(GRPQUOTA, (uint32_t)id, arg));
}

int
main(int argc, char *argv[])
{
	bool gflag = false, uflag = false;
	struct passwd *pw;
	struct group *gr;
	gid_t groups[NGROUPS_MAX];
	int ch, i, n, over = 0;

	while ((ch = getopt(argc, argv, "guqv")) != -1)
		switch (ch) {
		case 'g':
			gflag = true;
			break;
		case 'u':
			uflag = true;
			break;
		case 'q':
			qflag = true;
			break;
		case 'v':
			vflag = true;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		if (!gflag)
			uflag = true;
		if (uflag) {
			if ((pw = getpwuid(getuid())) == NULL)
				errx(1, "who are you?");
			over += show(USRQUOTA, pw->pw_uid, pw->pw_name);
		}
		if (gflag) {
			n = getgroups(NGROUPS_MAX, groups);
			for (i = 0; i < n; i++) {
				gr = getgrgid(groups[i]);
				if (gr != NULL)
					over += show(GRPQUOTA, gr->gr_gid,
					    gr->gr_name);
			}
		}
		return (over ? 1 : 0);
	}
	if (gflag && uflag)
		usage();
	for (i = 0; i < argc; i++)
		over += gflag ? group(argv[i]) : user(argv[i]);
	return (over ? 1 : 0);
}
