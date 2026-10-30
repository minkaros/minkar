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
 * mount: mount file systems, or list the mounted ones.
 *
 *	mount [-dfruvw] [-o options] [-t type] special | node
 *	mount [-dfruvw] [-o options] [-t type] special node
 *	mount -a [-dfruvw] [-o options] [-t types]
 *	mount
 *
 * As on macOS, this is a front end: the file-system-specific mount_<type>
 * program does the mounting. It gets every option in one -o list, where
 * -u, -w, -r and -f become "update", "rw", "ro" and "force"; an option
 * that starts with "-" ("-m=755") is passed to it as its own ("-m 755").
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

static bool dryrun, verbose;

static const struct {
	uint32_t flag;
	const char *name;
} flagnames[] = {
	{ MNT_ASYNC,		"asynchronous" },
	{ MNT_EXPORTED,		"NFS exported" },
	{ MNT_LOCAL,		"local" },
	{ MNT_NODEV,		"nodev" },
	{ MNT_NOEXEC,		"noexec" },
	{ MNT_NOSUID,		"nosuid" },
	{ MNT_QUOTA,		"with quotas" },
	{ MNT_RDONLY,		"read-only" },
	{ MNT_SYNCHRONOUS,	"synchronous" },
	{ MNT_UNION,		"union" },
	{ MNT_AUTOMOUNTED,	"automounted" },
	{ MNT_JOURNALED,	"journaled" },
	{ MNT_DEFWRITE,		"defwrite" },
	{ MNT_IGNORE_OWNERSHIP,	"noowners" },
	{ MNT_QUARANTINE,	"quarantine" },
	{ MNT_DONTBROWSE,	"nobrowse" },
	{ MNT_NOATIME,		"noatime" },
	{ MNT_MULTILABEL,	"multilabel" },
};

static void __dead2
usage(void)
{
	fprintf(stderr,
	    "usage: mount [-dfruvw] [-o options] [-t type] special | node\n"
	    "       mount [-dfruvw] [-o options] [-t type] special node\n"
	    "       mount -a [-dfruvw] [-o options] [-t types]\n"
	    "       mount\n");
	exit(EX_USAGE);
}

/* Append opt to the comma-separated list *list. */
static void
addopt(char **list, const char *opt)
{
	char *joined;

	if (opt == NULL || *opt == '\0')
		return;
	if (*list == NULL) {
		if ((joined = strdup(opt)) == NULL)
			err(EX_OSERR, NULL);
	} else if (asprintf(&joined, "%s,%s", *list, opt) < 0) {
		err(EX_OSERR, NULL);
	}
	free(*list);
	*list = joined;
}

/* Whether the comma-separated list has the option opt. */
static bool
hasopt(const char *list, const char *opt)
{
	size_t len = strlen(opt);
	const char *p = list;

	while (p != NULL && *p != '\0') {
		if (strncmp(p, opt, len) == 0 && (p[len] == ',' || p[len] == '\0'))
			return true;
		p = strchr(p, ',');
		if (p != NULL)
			p++;
	}
	return false;
}

/*
 * Whether type passes the -t filter: a list of types to mount, or, with a
 * "no" prefix ("nonfs,smbfs" skips both), of types to leave alone.
 */
static bool
type_selected(const char *filter, const char *type)
{
	bool negate;
	char *copy, *word, *rest;
	bool found = false;

	if (filter == NULL)
		return true;
	negate = strncmp(filter, "no", 2) == 0;
	if ((copy = strdup(negate ? filter + 2 : filter)) == NULL)
		err(EX_OSERR, NULL);
	for (rest = copy; (word = strsep(&rest, ",")) != NULL;) {
		if (strncmp(word, "no", 2) == 0 && negate)
			word += 2;
		if (strcmp(word, type) == 0)
			found = true;
	}
	free(copy);
	return negate ? !found : found;
}

static void
print_mount(const struct statfs *sfs)
{
	uint32_t flags = sfs->f_flags & MNT_VISFLAGMASK;

	printf("%s on %s (%s", sfs->f_mntfromname, sfs->f_mntonname,
	    sfs->f_fstypename);
	for (size_t i = 0; i < sizeof(flagnames) / sizeof(flagnames[0]); i++) {
		if (flags & flagnames[i].flag)
			printf(", %s", flagnames[i].name);
	}
	if (sfs->f_owner != 0)
		printf(", mounted by uid %u", sfs->f_owner);
	printf(")\n");
}

static int
list_mounts(void)
{
	struct statfs *mounts;
	int n;

	if ((n = getmntinfo(&mounts, MNT_NOWAIT)) == 0)
		err(EX_OSERR, "getmntinfo");
	for (int i = 0; i < n; i++)
		print_mount(&mounts[i]);
	return 0;
}

/* Print the mount on node, after a verbose mount. */
static void
print_node(const char *node)
{
	struct statfs sfs;

	if (statfs(node, &sfs) == 0)
		print_mount(&sfs);
}

/* Whether a file system is mounted on node. */
static bool
is_mounted(const char *node)
{
	struct statfs *mounts;
	int n = getmntinfo(&mounts, MNT_NOWAIT);

	for (int i = 0; i < n; i++) {
		if (strcmp(mounts[i].f_mntonname, node) == 0)
			return true;
	}
	return false;
}

/* The mount_<type> program: in /sbin, or in the file system's bundle. */
static int
find_helper(const char *type, char *path, size_t size)
{
	snprintf(path, size, "/sbin/mount_%s", type);
	if (access(path, X_OK) == 0)
		return 0;
	snprintf(path, size,
	    "/System/Library/Filesystems/%s.fs/Contents/Resources/mount_%s", type, type);
	if (access(path, X_OK) == 0)
		return 0;
	return -1;
}

/* Run mount_<type> -o options special node; returns its exit status. */
static int
mountfs(const char *type, const char *special, const char *node, const char *options)
{
	char helper[MAXPATHLEN], name[MFSNAMELEN + 8];
	char *opts = NULL, *rest = NULL, *o, *p;
	const char *argv[64];
	size_t restlen;
	int argc = 0, status;
	pid_t pid;

	if (find_helper(type, helper, sizeof(helper)) != 0) {
		warnx("%s: no mount_%s for file system type %s", node, type, type);
		return 1;
	}
	snprintf(name, sizeof(name), "mount_%s", type);
	argv[argc++] = name;
	if (options != NULL) {
		/*
		 * As macOS's mount: an option that starts with "-" is the
		 * helper's own ("-m=755" gives "-m 755", as diskarbitrationd
		 * asks of mount_hfs); the others stay in the -o list.
		 */
		if ((opts = strdup(options)) == NULL ||
		    (rest = calloc(1, restlen = strlen(options) + 1)) == NULL)
			err(EX_OSERR, NULL);
		for (o = opts; (p = strsep(&o, ",")) != NULL;) {
			if (*p == '\0')
				continue;
			if (*p == '-' && argc < 56) {
				argv[argc++] = p;
				if ((p = strchr(p, '=')) != NULL) {
					*p++ = '\0';
					argv[argc++] = p;
				}
				continue;
			}
			if (rest[0] != '\0')
				strlcat(rest, ",", restlen);
			strlcat(rest, p, restlen);
		}
		if (rest[0] != '\0') {
			argv[argc++] = "-o";
			argv[argc++] = rest;
		}
	}
	argv[argc++] = special;
	argv[argc++] = node;
	argv[argc] = NULL;

	if (verbose || dryrun) {
		printf("exec:");
		for (int i = 0; i < argc; i++)
			printf(" %s", argv[i]);
		printf("\n");
		fflush(stdout);
	}
	if (dryrun) {
		free(opts);
		free(rest);
		return 0;
	}

	pid = fork();
	if (pid == 0) {
		execv(helper, __DECONST(char **, argv));
		warn("exec %s", helper);
		_exit(EX_OSERR);
	}
	free(opts);
	free(rest);
	if (pid == -1) {
		warn("fork");
		return 1;
	}
	if (waitpid(pid, &status, 0) < 0) {
		warn("waitpid");
		return 1;
	}
	if (WIFSIGNALED(status)) {
		warnx("%s: %s killed by signal %d", node, name, WTERMSIG(status));
		return 1;
	}
	if (WEXITSTATUS(status) != 0)
		return WEXITSTATUS(status);
	if (verbose)
		print_node(node);
	return 0;
}

/* The options of an fstab entry, the command line's after them. */
static char *
entry_options(const struct fstab *fs, const char *cmdopts)
{
	char *opts = NULL;

	addopt(&opts, fs->fs_mntops);
	if (strcmp(fs->fs_type, FSTAB_RO) == 0)
		addopt(&opts, "ro");
	addopt(&opts, cmdopts);
	return opts;
}

/* mount -a: everything in /etc/fstab that is not mounted yet. */
static int
mount_all(const char *types, const char *cmdopts)
{
	struct fstab *fs;
	int status = 0;

	if (setfsent() == 0) {
		if (errno != ENOENT)
			warn("%s", _PATH_FSTAB);
		return 0;
	}
	while ((fs = getfsent()) != NULL) {
		if (strcmp(fs->fs_type, FSTAB_SW) == 0 ||
		    strcmp(fs->fs_type, FSTAB_XX) == 0 ||
		    hasopt(fs->fs_mntops, "noauto") ||
		    !type_selected(types, fs->fs_vfstype) ||
		    is_mounted(fs->fs_file))
			continue;
		char *opts = entry_options(fs, cmdopts);
		if (mountfs(fs->fs_vfstype, fs->fs_spec, fs->fs_file, opts) != 0)
			status = 1;
		free(opts);
	}
	endfsent();
	return status;
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

/* mount -u node: the mounted file system's own type and device. */
static int
update_mount(const char *path, const char *cmdopts)
{
	struct statfs sfs;
	char dev[MAXPATHLEN];

	if (statfs(path, &sfs) != 0)
		err(EX_NOINPUT, "%s", path);
	if (strcmp(sfs.f_mntonname, path) != 0 && verbose)
		printf("%s: updating the file system mounted on %s\n", path, sfs.f_mntonname);
	mounted_device(&sfs, dev, sizeof(dev));
	return mountfs(sfs.f_fstypename, dev, sfs.f_mntonname, cmdopts);
}

/* mount special-or-node: the rest comes from /etc/fstab. */
static int
mount_fstab(const char *name, const char *type, const char *cmdopts)
{
	struct fstab *fs;
	int status;

	if ((fs = getfsfile(name)) == NULL && (fs = getfsspec(name)) == NULL)
		errx(EX_NOINPUT, "%s: unknown special file or file system", name);
	if (type != NULL && strcmp(type, fs->fs_vfstype) != 0)
		errx(EX_DATAERR, "%s: fstab says type %s, not %s", name, fs->fs_vfstype, type);
	char *opts = entry_options(fs, cmdopts);
	status = mountfs(fs->fs_vfstype, fs->fs_spec, fs->fs_file, opts);
	free(opts);
	return status;
}

int
main(int argc, char *argv[])
{
	bool all = false, update = false, readonly = false, readwrite = false, force = false;
	char *options = NULL, *types = NULL;
	int ch;

	while ((ch = getopt(argc, argv, "adfo:rt:uvw")) != -1) {
		switch (ch) {
		case 'a':
			all = true;
			break;
		case 'd':
			dryrun = true;
			break;
		case 'f':
			force = true;
			break;
		case 'o':
			addopt(&options, optarg);
			break;
		case 'r':
			readonly = true;
			readwrite = false;
			break;
		case 't':
			types = optarg;
			break;
		case 'u':
			update = true;
			break;
		case 'v':
			verbose = true;
			break;
		case 'w':
			readwrite = true;
			readonly = false;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (update)
		addopt(&options, "update");
	if (readonly)
		addopt(&options, "ro");
	if (readwrite)
		addopt(&options, "rw");
	if (force)
		addopt(&options, "force");

	if (all) {
		if (argc != 0)
			usage();
		return mount_all(types, options);
	}
	switch (argc) {
	case 0:
		if (options != NULL || types != NULL)
			usage();
		return list_mounts();
	case 1:
		if (update)
			return update_mount(argv[0], options);
		return mount_fstab(argv[0], types, options);
	case 2:
		return mountfs(types != NULL ? types : DEFAULT_TYPE, argv[0], argv[1], options);
	default:
		usage();
	}
}
