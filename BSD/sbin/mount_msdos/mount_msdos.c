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
 * mount_msdos: mount a FAT12, FAT16 or FAT32 file system with the msdosfs
 * kext (file system type "msdos"), as on macOS. Files have no owner or
 * mode on FAT: they take -u, -g and -m, or by default those of the mount
 * point. The kext converts times between local and GMT with the offset
 * given here.
 */

#include <sys/types.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mntopts.h>

/* The kext's mount arguments (its msdosfsmount.h). */
struct msdosfs_args {
	char		*fspec;		/* block device */
	uid_t		uid;		/* owner of the files */
	gid_t		gid;		/* group of the files */
	mode_t		mask;		/* their permissions */
	uint32_t	flags;
	uint32_t	magic;
	int32_t		secondsWest;	/* local time, west of GMT */
	u_int8_t	label[64];
};
#define MSDOSFSMNT_SECONDSWEST	0x40
#define MSDOSFS_ARGSMAGIC	0xe4eff301

static struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	MOPT_FORCE,
	{ NULL, 0, 0, 0 }
};

static void
usage(void)
{
	fprintf(stderr, "usage: mount_msdos [-o options] [-u user] [-g group] "
	    "[-m mask] special node\n");
	exit(1);
}

static uid_t
user_id(const char *s)
{
	struct passwd *pw;
	char *end;
	long id;

	if ((pw = getpwnam(s)) != NULL)
		return (pw->pw_uid);
	id = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0' || id < 0)
		errx(1, "unknown user: %s", s);
	return ((uid_t)id);
}

static gid_t
group_id(const char *s)
{
	struct group *gr;
	char *end;
	long id;

	if ((gr = getgrnam(s)) != NULL)
		return (gr->gr_gid);
	id = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0' || id < 0)
		errx(1, "unknown group: %s", s);
	return ((gid_t)id);
}

static mode_t
file_mask(const char *s)
{
	char *end;
	long mask;

	mask = strtol(s, &end, 8);
	if (*s == '\0' || *end != '\0' || mask < 0 || mask > 0777)
		errx(1, "invalid mask: %s", s);
	return ((mode_t)mask);
}

int
main(int argc, char *argv[])
{
	struct msdosfs_args args;
	struct stat sb;
	char node[MAXPATHLEN];
	int ch, mntflags = 0, altflags = 0;
	int set_uid = 0, set_gid = 0, set_mask = 0;
	time_t now;
	struct tm local;

	memset(&args, 0, sizeof(args));
	while ((ch = getopt(argc, argv, "o:u:g:m:")) != -1) {
		switch (ch) {
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &altflags);
			break;
		case 'u':
			args.uid = user_id(optarg);
			set_uid = 1;
			break;
		case 'g':
			args.gid = group_id(optarg);
			set_gid = 1;
			break;
		case 'm':
			args.mask = file_mask(optarg);
			set_mask = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 2)
		usage();

	args.fspec = argv[0];
	if (realpath(argv[1], node) == NULL)
		err(1, "%s", argv[1]);
	if (stat(node, &sb) == -1)
		err(1, "%s", node);
	if (!set_uid)
		args.uid = sb.st_uid;
	if (!set_gid)
		args.gid = sb.st_gid;
	if (!set_mask)
		args.mask = sb.st_mode & ACCESSPERMS;

	now = time(NULL);
	localtime_r(&now, &local);
	args.secondsWest = (int32_t)-local.tm_gmtoff;
	args.flags = MSDOSFSMNT_SECONDSWEST;
	args.magic = MSDOSFS_ARGSMAGIC;

	if (mount("msdos", node, mntflags, &args) == -1) {
		if (errno == ENODEV || errno == EOPNOTSUPP)
			errx(1, "%s on %s: the msdosfs kext is not loaded", args.fspec, node);
		err(1, "%s on %s", args.fspec, node);
	}
	return (0);
}
