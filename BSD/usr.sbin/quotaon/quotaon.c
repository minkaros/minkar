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
 * quotaon, quotaoff: turn file system quotas on or off. A volume wants
 * user or group quotas when .quota.ops.user or .quota.ops.group is at
 * its root; quotaon then hands the kernel its .quota.user or .quota.group
 * (made by quotacheck).
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/quota.h>

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dqfile.h"

static bool off, verbose, all;
static bool want[MAXQUOTAS];
static int errors;

static void
usage(void)
{

	fprintf(stderr, "usage: %s [-g] [-u] [-v] -a\n"
	    "       %s [-g] [-u] [-v] filesystem ...\n",
	    off ? "quotaoff" : "quotaon", off ? "quotaoff" : "quotaon");
	exit(1);
}

static void
doone(const char *mnt, int type)
{
	char path[MAXPATHLEN];
	int rv;

	if (off) {
		rv = quotactl(mnt, QCMD(Q_QUOTAOFF, type), 0, NULL);
	} else {
		dq_filepath(mnt, type, path, sizeof(path));
		rv = quotactl(mnt, QCMD(Q_QUOTAON, type), 0, path);
	}
	if (rv != 0) {
		warn("%s: %s quotas", mnt, dq_typename(type));
		errors++;
	} else if (verbose)
		printf("%s: %s quotas turned %s\n", mnt, dq_typename(type),
		    off ? "off" : "on");
}

/* The types to act on for mnt: those asked for, else those it wants. */
static void
dofs(const char *mnt, bool explicit)
{
	bool any = want[USRQUOTA] || want[GRPQUOTA];
	int t;

	for (t = 0; t < MAXQUOTAS; t++) {
		if (any && !want[t])
			continue;
		if (!dq_wanted(mnt, t) && !(explicit && any))
			continue;
		doone(mnt, t);
	}
}

static int
fromall(const char *mnt, const char *dev, void *arg)
{

	(void)dev;
	(void)arg;
	dofs(mnt, false);
	return (0);
}

int
main(int argc, char *argv[])
{
	struct statfs sfs;
	int ch, i;

	off = strcmp(basename(argv[0]), "quotaoff") == 0;
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
			verbose = true;
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
		dofs(sfs.f_mntonname, true);
	}
	return (errors ? 1 : 0);
}
