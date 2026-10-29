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
 * edquota: edit the quota limits of users or groups, or the grace
 * periods of the volumes, as text in $EDITOR. The limits go to the
 * kernel when quotas are on, and to the quota file.
 */

#include <sys/param.h>
#include <sys/quota.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dqfile.h"

#define	MAXFS	32

static char *fsys[MAXFS];
static int nfs;
static int qtype = USRQUOTA;

static void
usage(void)
{

	fprintf(stderr, "usage: edquota [-u] [-p proto-user] user ...\n"
	    "       edquota -g [-p proto-group] group ...\n"
	    "       edquota [-u | -g] -t\n");
	exit(1);
}

static int
addfs(const char *mnt, const char *dev, void *arg)
{

	(void)dev;
	(void)arg;
	if (nfs < MAXFS && (dq_wanted(mnt, qtype) || dq_enabled(mnt, qtype)))
		fsys[nfs++] = strdup(mnt);
	return (0);
}

static bool
getid(const char *s, uint32_t *id)
{
	struct passwd *pw;
	struct group *gr;
	char *end;
	unsigned long n;

	if (qtype == USRQUOTA && (pw = getpwnam(s)) != NULL) {
		*id = pw->pw_uid;
		return (true);
	}
	if (qtype == GRPQUOTA && (gr = getgrnam(s)) != NULL) {
		*id = gr->gr_gid;
		return (true);
	}
	n = strtoul(s, &end, 10);
	if (*s == '\0' || *end != '\0')
		return (false);
	*id = (uint32_t)n;
	return (true);
}

/* Run the editor on path; true when it worked. */
static bool
edit(const char *path)
{
	const char *ed;
	char *cmd;
	pid_t pid;
	int st;

	if ((ed = getenv("EDITOR")) == NULL || *ed == '\0')
		ed = "vi";
	if (asprintf(&cmd, "%s %s", ed, path) == -1)
		err(1, "asprintf");
	if ((pid = fork()) == -1)
		err(1, "fork");
	if (pid == 0) {
		/* Back to the real user to edit. */
		if (setgid(getgid()) != 0 || setuid(getuid()) != 0)
			_exit(127);
		execl(_PATH_BSHELL, "sh", "-c", cmd, (char *)NULL);
		_exit(127);
	}
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	while (waitpid(pid, &st, 0) == -1)
		if (errno != EINTR)
			err(1, "waitpid");
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	free(cmd);
	return (WIFEXITED(st) && WEXITSTATUS(st) == 0);
}

static FILE *
tempfile(char *path, size_t len)
{
	int fd;
	FILE *f;

	strlcpy(path, "/tmp/EdP.XXXXXX", len);
	if ((fd = mkstemp(path)) == -1)
		err(1, "mkstemp");
	if ((f = fdopen(fd, "w+")) == NULL)
		err(1, "fdopen");
	return (f);
}

/* Edit the limits of one id; proto, when given, supplies them instead. */
static int
editone(const char *who, uint32_t id, const struct dqblk *proto)
{
	struct dqblk b[MAXFS];
	char path[MAXPATHLEN], line[1024], mnt[MAXPATHLEN];
	unsigned long long used, bs, bh;
	unsigned iu, is, ih;
	FILE *f;
	int i, errs = 0;

	for (i = 0; i < nfs; i++) {
		if (dq_getquota(fsys[i], qtype, id, &b[i]) != 0)
			memset(&b[i], 0, sizeof(b[i]));
		if (proto != NULL) {
			b[i].dqb_bsoftlimit = proto[i].dqb_bsoftlimit;
			b[i].dqb_bhardlimit = proto[i].dqb_bhardlimit;
			b[i].dqb_isoftlimit = proto[i].dqb_isoftlimit;
			b[i].dqb_ihardlimit = proto[i].dqb_ihardlimit;
		}
	}
	if (proto == NULL) {
		f = tempfile(path, sizeof(path));
		fprintf(f, "Quotas for %s %s:\n", dq_typename(qtype), who);
		for (i = 0; i < nfs; i++)
			fprintf(f, "%s: 1K blocks in use: %llu, "
			    "limits (soft = %llu, hard = %llu)\n"
			    "\tinodes in use: %u, limits (soft = %u, hard = %u)\n",
			    fsys[i],
			    (unsigned long long)DQ_KB(b[i].dqb_curbytes),
			    (unsigned long long)DQ_KB(b[i].dqb_bsoftlimit),
			    (unsigned long long)DQ_KB(b[i].dqb_bhardlimit),
			    b[i].dqb_curinodes, b[i].dqb_isoftlimit,
			    b[i].dqb_ihardlimit);
		fclose(f);
		if (!edit(path)) {
			unlink(path);
			warnx("%s: the editor failed; nothing changed", who);
			return (1);
		}
		if ((f = fopen(path, "r")) == NULL)
			err(1, "%s", path);
		if (fgets(line, sizeof(line), f) == NULL) {
			warnx("%s: no header line", who);
			errs++;
		}
		while (errs == 0 && fgets(line, sizeof(line), f) != NULL) {
			char *colon = strstr(line, ": 1K blocks in use:");

			if (colon == NULL) {
				warnx("%s: bad line: %s", who, line);
				errs++;
				break;
			}
			*colon = '\0';
			strlcpy(mnt, line, sizeof(mnt));
			if (sscanf(colon + 1, " 1K blocks in use: %llu, limits "
			    "(soft = %llu, hard = %llu)", &used, &bs, &bh) != 3 ||
			    fgets(line, sizeof(line), f) == NULL ||
			    sscanf(line, " inodes in use: %u, limits (soft = %u, "
			    "hard = %u)", &iu, &is, &ih) != 3) {
				warnx("%s: %s: bad limits", who, mnt);
				errs++;
				break;
			}
			for (i = 0; i < nfs; i++)
				if (strcmp(fsys[i], mnt) == 0)
					break;
			if (i == nfs) {
				warnx("%s: %s has no quotas", who, mnt);
				continue;
			}
			b[i].dqb_bsoftlimit = bs * 1024;
			b[i].dqb_bhardlimit = bh * 1024;
			b[i].dqb_isoftlimit = is;
			b[i].dqb_ihardlimit = ih;
		}
		fclose(f);
		unlink(path);
		if (errs)
			return (1);
	}
	for (i = 0; i < nfs; i++) {
		/* A fresh grace period once usage is back under the limit. */
		if (!b[i].dqb_bsoftlimit ||
		    b[i].dqb_curbytes < b[i].dqb_bsoftlimit)
			b[i].dqb_btime = 0;
		if (!b[i].dqb_isoftlimit ||
		    b[i].dqb_curinodes < b[i].dqb_isoftlimit)
			b[i].dqb_itime = 0;
		if (dq_setquota(fsys[i], qtype, id, &b[i]) != 0) {
			warn("%s: %s", fsys[i], who);
			errs++;
		}
	}
	return (errs ? 1 : 0);
}

/* Seconds from "N days|hours|minutes|seconds"; -1 when bad. */
static long
units(unsigned long n, const char *u)
{

	if (strncmp(u, "day", 3) == 0)
		return ((long)n * 86400);
	if (strncmp(u, "hour", 4) == 0)
		return ((long)n * 3600);
	if (strncmp(u, "min", 3) == 0)
		return ((long)n * 60);
	if (strncmp(u, "sec", 3) == 0)
		return ((long)n);
	return (-1);
}

static const char *
fmt(uint32_t secs)
{
	static char buf[2][32];
	static int k;

	k ^= 1;
	if (secs % 86400 == 0)
		snprintf(buf[k], sizeof(buf[k]), "%u days", secs / 86400);
	else if (secs % 3600 == 0)
		snprintf(buf[k], sizeof(buf[k]), "%u hours", secs / 3600);
	else if (secs % 60 == 0)
		snprintf(buf[k], sizeof(buf[k]), "%u minutes", secs / 60);
	else
		snprintf(buf[k], sizeof(buf[k]), "%u seconds", secs);
	return (buf[k]);
}

static int
editgrace(void)
{
	struct dqfile q;
	char path[MAXPATHLEN], line[1024], bu[32], iu[32];
	unsigned long bn, in;
	long bt, it;
	FILE *f;
	int i, errs = 0;

	f = tempfile(path, sizeof(path));
	fprintf(f, "Time units may be: days, hours, minutes, or seconds\n"
	    "Grace period before enforcing soft limits for %ss:\n",
	    dq_typename(qtype));
	for (i = 0; i < nfs; i++) {
		if (dq_open(&q, fsys[i], qtype, true, 0) != 0)
			err(1, "%s: %s quota file", fsys[i], dq_typename(qtype));
		fprintf(f, "%s: block grace period: %s, file grace period: %s\n",
		    fsys[i], fmt(q.hdr.dqh_btime ? q.hdr.dqh_btime : MAX_DQ_TIME),
		    fmt(q.hdr.dqh_itime ? q.hdr.dqh_itime : MAX_IQ_TIME));
		dq_close(&q);
	}
	fclose(f);
	if (!edit(path)) {
		unlink(path);
		errx(1, "the editor failed; nothing changed");
	}
	if ((f = fopen(path, "r")) == NULL)
		err(1, "%s", path);
	while (fgets(line, sizeof(line), f) != NULL) {
		char *colon = strstr(line, ": block grace period:");

		if (colon == NULL)
			continue;
		*colon = '\0';
		if (sscanf(colon + 1, " block grace period: %lu %31[a-z], file "
		    "grace period: %lu %31[a-z]", &bn, bu, &in, iu) != 4 ||
		    (bt = units(bn, bu)) < 0 || (it = units(in, iu)) < 0) {
			warnx("%s: bad grace periods", line);
			errs++;
			continue;
		}
		for (i = 0; i < nfs; i++)
			if (strcmp(fsys[i], line) == 0)
				break;
		if (i == nfs) {
			warnx("%s has no quotas", line);
			continue;
		}
		if (dq_open(&q, fsys[i], qtype, true, 0) != 0) {
			warn("%s", fsys[i]);
			errs++;
			continue;
		}
		q.hdr.dqh_btime = (uint32_t)bt;
		q.hdr.dqh_itime = (uint32_t)it;
		if (dq_puthdr(&q) != 0) {
			warn("%s", q.path);
			errs++;
		}
		dq_close(&q);
	}
	fclose(f);
	unlink(path);
	return (errs ? 1 : 0);
}

int
main(int argc, char *argv[])
{
	struct dqblk proto[MAXFS];
	const char *protoname = NULL;
	bool tflag = false;
	uint32_t id;
	int ch, i, errs = 0;

	if (getuid() != 0)
		errx(1, "permission denied");
	while ((ch = getopt(argc, argv, "gp:tu")) != -1)
		switch (ch) {
		case 'g':
			qtype = GRPQUOTA;
			break;
		case 'u':
			qtype = USRQUOTA;
			break;
		case 'p':
			protoname = optarg;
			break;
		case 't':
			tflag = true;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	dq_mounts(addfs, NULL);
	if (nfs == 0)
		errx(1, "no volume has %s quotas", dq_typename(qtype));
	if (tflag) {
		if (argc != 0 || protoname != NULL)
			usage();
		return (editgrace());
	}
	if (argc == 0)
		usage();
	if (protoname != NULL) {
		if (!getid(protoname, &id))
			errx(1, "%s: unknown %s", protoname, dq_typename(qtype));
		for (i = 0; i < nfs; i++)
			if (dq_getquota(fsys[i], qtype, id, &proto[i]) != 0)
				memset(&proto[i], 0, sizeof(proto[i]));
	}
	for (i = 0; i < argc; i++) {
		if (!getid(argv[i], &id)) {
			warnx("%s: unknown %s", argv[i], dq_typename(qtype));
			errs++;
			continue;
		}
		if (id == 0) {
			warnx("%s: the super-user has no quotas", argv[i]);
			errs++;
			continue;
		}
		errs += editone(argv[i], id, protoname ? proto : NULL);
	}
	return (errs ? 1 : 0);
}
