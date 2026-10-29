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
 * dot_clean: fold AppleDouble files (._name, which hold a file's extended
 * attributes, resource fork and Finder information on volumes that cannot
 * keep them) back into the files they belong to, then remove them. The
 * unpacking is copyfile(3)'s; which side wins when both have an attribute
 * is the --keep rule.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include <copyfile.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum keep { KEEP_MOSTRECENT, KEEP_DOTBAR, KEEP_NATIVE };

static enum keep keep = KEEP_MOSTRECENT;
static bool fflag, mflag, nflag, sflag, vflag;
static int errors;

static void
usage(void)
{

	fprintf(stderr, "usage: dot_clean [-fmnsv] "
	    "[--keep=mostrecent|dotbar|native] dir ...\n");
	exit(1);
}

/* The native file's attributes, to put back those it had (--keep=native). */
struct saved {
	char	*names;
	ssize_t	 len;
};

static void
save(const char *path, struct saved *sv)
{

	sv->names = NULL;
	sv->len = listxattr(path, NULL, 0, XATTR_NOFOLLOW);
	if (sv->len <= 0)
		return;
	if ((sv->names = malloc((size_t)sv->len)) == NULL ||
	    (sv->len = listxattr(path, sv->names, (size_t)sv->len,
	    XATTR_NOFOLLOW)) <= 0) {
		free(sv->names);
		sv->names = NULL;
	}
}

static void
restore(const char *path, const char *from, struct saved *sv)
{
	char *n;
	ssize_t vlen;
	void *val;

	/* from still has the native values: take them again. */
	for (n = sv->names; n != NULL && n < sv->names + sv->len;
	    n += strlen(n) + 1) {
		vlen = getxattr(from, n, NULL, 0, 0, XATTR_NOFOLLOW);
		if (vlen < 0 || (val = malloc((size_t)vlen + 1)) == NULL)
			continue;
		if (getxattr(from, n, val, (size_t)vlen, 0, XATTR_NOFOLLOW) ==
		    vlen)
			(void)setxattr(path, n, val, (size_t)vlen, 0,
			    XATTR_NOFOLLOW);
		free(val);
	}
	free(sv->names);
}

/* Merge one ._ file into its native file; true when it may go. */
static bool
merge(const char *dot, const char *native)
{
	struct stat ds, ns;
	enum keep k = keep;
	struct saved sv = { NULL, 0 };
	char tmp[PATH_MAX];
	bool ok;

	if (lstat(native, &ns) == -1) {
		if (vflag)
			printf("%s: no native file\n", dot);
		return (nflag);
	}
	if (lstat(dot, &ds) == -1)
		return (false);
	if (k == KEEP_MOSTRECENT)
		k = ds.st_mtime > ns.st_mtime ? KEEP_DOTBAR : KEEP_NATIVE;
	tmp[0] = '\0';
	if (k == KEEP_NATIVE) {
		save(native, &sv);
		/* A copy of the native attributes to restore from. */
		snprintf(tmp, sizeof(tmp), "%s.dot_clean.XXXXXX", native);
		{
			int fd = mkstemp(tmp);

			if (fd == -1 || copyfile(native, tmp, NULL,
			    COPYFILE_XATTR | COPYFILE_NOFOLLOW) != 0) {
				if (fd != -1)
					close(fd);
				(void)unlink(tmp);
				free(sv.names);
				warn("%s", native);
				return (false);
			}
			close(fd);
		}
	}
	ok = copyfile(dot, native, NULL, COPYFILE_UNPACK | COPYFILE_METADATA |
	    COPYFILE_NOFOLLOW) == 0;
	if (!ok)
		warn("%s", dot);
	if (k == KEEP_NATIVE) {
		restore(native, tmp, &sv);
		(void)unlink(tmp);
	}
	if (ok && vflag)
		printf("%s merged into %s\n", dot, native);
	return (ok);
}

static void
one(const char *path, const char *name)
{
	char native[PATH_MAX];
	size_t dirlen = (size_t)(name - path);

	if (dirlen + strlen(name + 2) + 1 > sizeof(native))
		return;
	memcpy(native, path, dirlen);
	strcpy(native + dirlen, name + 2);
	if (merge(path, native) || mflag) {
		if (unlink(path) == -1) {
			warn("%s", path);
			errors++;
		}
	}
}

static void
walk(char *dir)
{
	char *paths[] = { dir, NULL };
	FTS *fts;
	FTSENT *e;

	fts = fts_open(paths, (sflag ? FTS_LOGICAL : FTS_PHYSICAL) | FTS_NOCHDIR,
	    NULL);
	if (fts == NULL) {
		warn("%s", dir);
		errors++;
		return;
	}
	while ((e = fts_read(fts)) != NULL) {
		if (e->fts_info == FTS_D && fflag && e->fts_level > 0) {
			fts_set(fts, e, FTS_SKIP);
			continue;
		}
		if (e->fts_info != FTS_F || strncmp(e->fts_name, "._", 2) != 0 ||
		    e->fts_name[2] == '\0')
			continue;
		one(e->fts_path, e->fts_path + strlen(e->fts_path) -
		    e->fts_namelen);
	}
	fts_close(fts);
}

int
main(int argc, char *argv[])
{
	static const struct option longopts[] = {
		{ "keep", required_argument, NULL, 'k' },
		{ NULL, 0, NULL, 0 },
	};
	int ch, i;

	while ((ch = getopt_long(argc, argv, "fmnsv", longopts, NULL)) != -1)
		switch (ch) {
		case 'f':
			fflag = true;
			break;
		case 'm':
			mflag = true;
			break;
		case 'n':
			nflag = true;
			break;
		case 's':
			sflag = true;
			break;
		case 'v':
			vflag = true;
			break;
		case 'k':
			if (strcmp(optarg, "mostrecent") == 0)
				keep = KEEP_MOSTRECENT;
			else if (strcmp(optarg, "dotbar") == 0)
				keep = KEEP_DOTBAR;
			else if (strcmp(optarg, "native") == 0)
				keep = KEEP_NATIVE;
			else
				usage();
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc == 0)
		usage();
	for (i = 0; i < argc; i++)
		walk(argv[i]);
	return (errors ? 1 : 0);
}
