/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * fsck: check file systems.
 *
 *	fsck [-fnpqy] [-t type] [special | node ...]
 *
 * As on macOS, this is a front end: fsck_<type> checks each file system,
 * given the options -f, -n, -p, -q and -y and the raw device. Without
 * operands it checks the root file system, then every /etc/fstab entry with
 * a nonzero pass number. At boot, launchctl runs "fsck -q" (is the root
 * clean?) and, if not, "fsck -fy" (repair it).
 *
 * The exit status is the first nonzero one of the fsck_<type> runs, so
 * their meanings (for fsck_hfs: 3 means "dirty" after -q, 8 a failure)
 * reach the caller.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define DEFAULT_TYPE	"hfs"
#define FAILURE		8	/* fsck's general failure status */

static char flags[8];		/* "-fnpqy" as given, passed on */

static void __dead2
usage(void)
{
	fprintf(stderr, "usage: fsck [-fnpqy] [-t type] [special | node ...]\n");
	exit(EX_USAGE);
}

/* The raw (character) device for a block device: /dev/disk0s2 -> /dev/rdisk0s2. */
static void
raw_device(const char *dev, char *raw, size_t size)
{
	const char *base = strrchr(dev, '/');
	struct stat sb;

	if (base != NULL && base[1] != 'r') {
		snprintf(raw, size, "%.*sr%s", (int)(base - dev + 1), dev, base + 1);
		if (stat(raw, &sb) == 0 && S_ISCHR(sb.st_mode))
			return;
	}
	strlcpy(raw, dev, size);
}

static int
find_helper(const char *type, char *path, size_t size)
{
	snprintf(path, size, "/sbin/fsck_%s", type);
	if (access(path, X_OK) == 0)
		return 0;
	snprintf(path, size,
	    "/System/Library/Filesystems/%s.fs/Contents/Resources/fsck_%s", type, type);
	if (access(path, X_OK) == 0)
		return 0;
	return -1;
}

/* Run fsck_<type> on the device; returns its exit status. */
static int
checkfs(const char *type, const char *device)
{
	char helper[MAXPATHLEN], raw[MAXPATHLEN], name[MFSNAMELEN + 8];
	const char *argv[4];
	int argc = 0, status;
	pid_t pid;

	if (find_helper(type, helper, sizeof(helper)) != 0) {
		warnx("%s: no fsck_%s to check file system type %s", device, type, type);
		return FAILURE;
	}
	raw_device(device, raw, sizeof(raw));
	snprintf(name, sizeof(name), "fsck_%s", type);
	argv[argc++] = name;
	if (flags[1] != '\0')
		argv[argc++] = flags;
	argv[argc++] = raw;
	argv[argc] = NULL;

	switch (pid = fork()) {
	case -1:
		warn("fork");
		return FAILURE;
	case 0:
		execv(helper, __DECONST(char **, argv));
		warn("exec %s", helper);
		_exit(FAILURE);
	}
	if (waitpid(pid, &status, 0) < 0) {
		warn("waitpid");
		return FAILURE;
	}
	if (WIFSIGNALED(status)) {
		warnx("%s: %s killed by signal %d", raw, name, WTERMSIG(status));
		return FAILURE;
	}
	return WEXITSTATUS(status);
}

/*
 * The device a file system is mounted from. XNU names the root file
 * system's device "root_device" until it is mounted again; the real one
 * is the device the mount point lives on.
 */
static void
mounted_device(const struct statfs *sfs, char *dev, size_t size)
{
	struct stat sb;
	const char *name;

	if (sfs->f_mntfromname[0] != '/' && stat(sfs->f_mntonname, &sb) == 0 &&
	    (name = devname(sb.st_dev, S_IFBLK)) != NULL && name[0] != '?') {
		snprintf(dev, size, "%s%s", _PATH_DEV, name);
		return;
	}
	strlcpy(dev, sfs->f_mntfromname, size);
}

/* Check special or node: mounted, in /etc/fstab, or a device of -t's type. */
static int
check_operand(const char *name, const char *type)
{
	struct statfs sfs;
	struct fstab *fs;
	struct stat sb;

	if ((fs = getfsfile(name)) != NULL || (fs = getfsspec(name)) != NULL)
		return checkfs(fs->fs_vfstype, fs->fs_spec);
	if (stat(name, &sb) == 0 && (S_ISBLK(sb.st_mode) || S_ISCHR(sb.st_mode)))
		return checkfs(type != NULL ? type : DEFAULT_TYPE, name);
	if (statfs(name, &sfs) == 0 && strcmp(sfs.f_mntonname, name) == 0) {
		char dev[MAXPATHLEN];

		mounted_device(&sfs, dev, sizeof(dev));
		return checkfs(sfs.f_fstypename, dev);
	}
	warnx("%s: not a device, mount point or /etc/fstab entry", name);
	return FAILURE;
}

/* No operands: the root file system, then the /etc/fstab entries to check. */
static int
check_all(const char *types)
{
	struct statfs root;
	struct fstab *fs;
	int status, result;

	char rootdev[MAXPATHLEN];

	if (statfs("/", &root) != 0)
		err(FAILURE, "/");
	mounted_device(&root, rootdev, sizeof(rootdev));
	result = checkfs(root.f_fstypename, rootdev);

	if (setfsent() == 0)
		return result;
	while ((fs = getfsent()) != NULL) {
		if (fs->fs_passno <= 0 ||
		    (strcmp(fs->fs_type, FSTAB_RW) != 0 && strcmp(fs->fs_type, FSTAB_RO) != 0 &&
		     strcmp(fs->fs_type, FSTAB_RQ) != 0) ||
		    (types != NULL && strcmp(types, fs->fs_vfstype) != 0) ||
		    strcmp(fs->fs_spec, rootdev) == 0 ||
		    strcmp(fs->fs_file, "/") == 0)
			continue;
		status = checkfs(fs->fs_vfstype, fs->fs_spec);
		if (result == 0)
			result = status;
	}
	endfsent();
	return result;
}

int
main(int argc, char *argv[])
{
	const char *type = NULL;
	size_t n = 1;
	int ch, status, result = 0;

	flags[0] = '-';
	while ((ch = getopt(argc, argv, "fnpqyt:")) != -1) {
		switch (ch) {
		case 'f':
		case 'n':
		case 'p':
		case 'q':
		case 'y':
			if (strchr(flags, ch) == NULL && n < sizeof(flags) - 1)
				flags[n++] = (char)ch;
			break;
		case 't':
			type = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		return check_all(type);
	for (int i = 0; i < argc; i++) {
		status = check_operand(argv[i], type);
		if (result == 0)
			result = status;
	}
	return result;
}
