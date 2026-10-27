/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Modifications Copyright (c) 2026 The Minkar contributors
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include "chmod_acl.h"
#endif

static volatile sig_atomic_t siginfo;

static void usage(void) __dead2;
static int may_have_nfs4acl(const FTSENT *ent, int hflag);

static void
siginfo_handler(int sig __unused)
{

	siginfo = 1;
}

int
main(int argc, char *argv[])
{
	FTS *ftsp;
	FTSENT *p;
	mode_t *set;
	int Hflag, Lflag, Rflag, ch, fflag, fts_options, hflag, rval;
	int vflag;
	char *mode;
	mode_t newmode;
#ifdef __APPLE__
	/* macOS's ACL operations (chmod_acl.c). */
	struct chmod_acl_req aclreq;
	enum chmod_acl_op aclop = CHMOD_ACL_NONE;
	int aclword = 0;
#define	CHMOD_OPTS	"CEHILNPRXafghiorstuvwx"
#else
#define	CHMOD_OPTS	"HLPRXfghorstuvwx"
#endif

	set = NULL;
	Hflag = Lflag = Rflag = fflag = hflag = vflag = 0;
	while ((ch = getopt(argc, argv, CHMOD_OPTS)) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = 0;
			break;
		case 'P':
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			/*
			 * In System V the -h option causes chmod to change
			 * the mode of the symbolic link. 4.4BSD's symbolic
			 * links didn't have modes, so it was an undocumented
			 * noop.  In FreeBSD 3.0, lchmod(2) is introduced and
			 * this option does real work.
			 */
			hflag = 1;
			break;
		/*
		 * XXX
		 * "-[rwx]" are valid mode commands.  If they are the entire
		 * argument, getopt has moved past them, so decrement optind.
		 * Regardless, we're done argument processing.
		 */
		case 'g': case 'o': case 'r': case 's':
		case 't': case 'u': case 'w': case 'X': case 'x':
			if (argv[optind - 1][0] == '-' &&
			    argv[optind - 1][1] == ch &&
			    argv[optind - 1][2] == '\0')
				--optind;
			goto done;
		case 'v':
			vflag++;
			break;
#ifdef __APPLE__
		case 'C':
			aclop = CHMOD_ACL_CHECK;
			break;
		case 'E':
			aclop = CHMOD_ACL_SET;
			break;
		case 'I':
			aclop = CHMOD_ACL_DROP_INHERITED;
			break;
		case 'N':
			aclop = CHMOD_ACL_CLEAR;
			break;
		case 'i':
			aclop = CHMOD_ACL_UNINHERIT;
			break;
		case 'a':
			/*
			 * "-a" and "-a#" name an ACL operation, not options.
			 * getopt has moved past a whole "-a" but not "-a#".
			 */
			if (strcmp(argv[optind - 1], "-a") == 0)
				--optind;
			aclword = 1;
			goto done;
#endif
		case '?':
		default:
			usage();
		}
done:	argv += optind;
	argc -= optind;

#ifdef __APPLE__
	if (argc > 0 && (aclword || aclop == CHMOD_ACL_NONE))
		aclop = chmod_acl_op_of(*argv) != CHMOD_ACL_NONE ?
		    chmod_acl_op_of(*argv) : aclop;
	if (aclword && aclop == CHMOD_ACL_NONE)
		usage();
	if (aclop != CHMOD_ACL_NONE) {
		chmod_acl_prepare(&aclreq, aclop, &argc, &argv,
		    chmod_acl_op_of(argc > 0 ? *argv : "") != CHMOD_ACL_NONE);
		if (argc < 1)
			usage();
	} else
#endif
	if (argc < 2)
		usage();

	(void)signal(SIGINFO, siginfo_handler);

	if (Rflag) {
		if (hflag)
			errx(1, "the -R and -h options may not be "
			    "specified together.");
		if (Lflag) {
			fts_options = FTS_LOGICAL;
		} else {
			fts_options = FTS_PHYSICAL;

			if (Hflag) {
				fts_options |= FTS_COMFOLLOW;
			}
		}
	} else if (hflag) {
		fts_options = FTS_PHYSICAL;
	} else {
		fts_options = FTS_LOGICAL;
	}

#ifdef __APPLE__
	if (aclop == CHMOD_ACL_NONE)
#endif
	{
		mode = *argv++;
		if ((set = setmode(mode)) == NULL)
			errx(1, "invalid file mode: %s", mode);
	}

	if ((ftsp = fts_open(argv, fts_options, 0)) == NULL)
		err(1, "fts_open");
	for (rval = 0; errno = 0, (p = fts_read(ftsp)) != NULL;) {
		int atflag;

		if ((fts_options & FTS_LOGICAL) ||
		    ((fts_options & FTS_COMFOLLOW) &&
		    p->fts_level == FTS_ROOTLEVEL))
			atflag = 0;
		else
			atflag = AT_SYMLINK_NOFOLLOW;

		switch (p->fts_info) {
		case FTS_D:
			if (!Rflag)
				fts_set(ftsp, p, FTS_SKIP);
			break;
		case FTS_DNR:			/* Warn, chmod. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_DP:			/* Already changed at FTS_D. */
			continue;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
		default:
			break;
		}
#ifdef __APPLE__
		if (aclop != CHMOD_ACL_NONE) {
			if (chmod_acl_apply(&aclreq, p->fts_accpath,
			    p->fts_path, atflag == 0) != 0) {
				if (!fflag || aclop == CHMOD_ACL_CHECK)
					rval = 1;
			} else if (vflag)
				(void)printf("%s\n", p->fts_path);
			continue;
		}
#endif
		newmode = getmode(set, p->fts_statp->st_mode);
		/*
		 * With NFSv4 ACLs, it is possible that applying a mode
		 * identical to the one computed from an ACL will change
		 * that ACL.
		 */
		if (may_have_nfs4acl(p, hflag) == 0 &&
		    (newmode & ALLPERMS) == (p->fts_statp->st_mode & ALLPERMS))
				continue;
		if (fchmodat(AT_FDCWD, p->fts_accpath, newmode, atflag) == -1
		    && !fflag) {
			warn("%s", p->fts_path);
			rval = 1;
		} else if (vflag || siginfo) {
			(void)printf("%s", p->fts_path);

			if (vflag > 1 || siginfo) {
				char m1[12], m2[12];

				strmode(p->fts_statp->st_mode, m1);
				strmode((p->fts_statp->st_mode &
				    S_IFMT) | newmode, m2);
				(void)printf(": 0%o [%s] -> 0%o [%s]",
				    p->fts_statp->st_mode, m1,
				    (p->fts_statp->st_mode & S_IFMT) |
				    newmode, m2);
			}
			(void)printf("\n");
			siginfo = 0;
		}
	}
	if (errno)
		err(1, "fts_read");
	exit(rval);
}

static void
usage(void)
{
#ifdef __APPLE__
	(void)fprintf(stderr, "%s\n%s\n%s\n%s\n",
	    "usage: chmod [-fhv] [-R [-H | -L | -P]] mode file ...",
	    "       chmod [-fhv] [-R [-H | -L | -P]] [-a | +a | =a] ACE file ...",
	    "       chmod [-fhv] [-R [-H | -L | -P]] [-a# | +a# | =a#] n [ACE] file ...",
	    "       chmod [-fhv] [-R [-H | -L | -P]] [-C | -E | -I | -N | -i] file ...");
#else
	(void)fprintf(stderr,
	    "usage: chmod [-fhv] [-R [-H | -L | -P]] mode file ...\n");
#endif
	exit(1);
}

static int
may_have_nfs4acl(const FTSENT *ent, int hflag)
{
#ifdef __APPLE__
	/* Minkar: Darwin has no NFSv4 ACLs (pathconf _PC_ACL_NFS4). */
	(void)ent;
	(void)hflag;
	return (0);
#else
	int ret;
	static dev_t previous_dev = NODEV;
	static int supports_acls = -1;

	if (previous_dev != ent->fts_statp->st_dev) {
		previous_dev = ent->fts_statp->st_dev;
		supports_acls = 0;

		if (hflag)
			ret = lpathconf(ent->fts_accpath, _PC_ACL_NFS4);
		else
			ret = pathconf(ent->fts_accpath, _PC_ACL_NFS4);
		if (ret > 0)
			supports_acls = 1;
		else if (ret < 0 && errno != EINVAL)
			warn("%s", ent->fts_path);
	}

	return (supports_acls);
#endif
}
