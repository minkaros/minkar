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
 * ditto: copy directory hierarchies, and create and extract archives.
 *
 *   ditto [options] src ... dst
 *   ditto -c [-k] [-z | -j] [--keepParent] [options] src archive
 *   ditto -x [-k] [options] archive ... dst
 *
 * Copying merges each source into dst: a directory's contents go into
 * dst (made if missing), a file goes to dst (or into it, if dst is a
 * directory). Modes, owners (as root), times, extended attributes and
 * ACLs are kept, through copyfile(3). Archives are CPIO (gzip with -z,
 * bzip2 with -j) or, with -k, PKZip, through libarchive.
 *
 * Options: -v (say what is copied), -V (every file), -X (do not cross
 * into other devices), --norsrc (no resource forks), --noextattr,
 * --noacl, --noqtn (accepted; the same as on macOS without quarantine).
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include <archive.h>
#include <archive_entry.h>
#include <copyfile.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

static bool vflag, Vflag, Xflag;
static bool rsrc = true, extattr = true, acl = true;
static bool keep_parent;
static int status;

static void usage(void) __dead2;

static copyfile_flags_t
meta_flags(void)
{
	copyfile_flags_t f = COPYFILE_STAT;

	if (acl)
		f |= COPYFILE_ACL;
	if (extattr || rsrc)
		f |= COPYFILE_XATTR;
	return (f);
}

/* Without resource forks (--norsrc), drop the one copyfile brought. */
static void
fix_xattrs(const char *dst)
{
	if (!rsrc)
		(void)removexattr(dst, XATTR_RESOURCEFORK_NAME, XATTR_NOFOLLOW);
	if (!extattr) {
		/* Keep only the resource fork, if that was asked for. */
		char names[8192];
		ssize_t n = listxattr(dst, names, sizeof(names),
		    XATTR_NOFOLLOW);

		for (ssize_t i = 0; i < n; i += (ssize_t)strlen(names + i) + 1)
			if (!rsrc || strcmp(names + i,
			    XATTR_RESOURCEFORK_NAME) != 0)
				(void)removexattr(dst, names + i,
				    XATTR_NOFOLLOW);
	}
}

static void
copy_item(const char *src, const char *dst, dev_t root_dev);

static void
copy_dir(const char *src, const char *dst, const struct stat *sb,
    dev_t root_dev)
{
	struct stat db;
	DIR *dir;
	struct dirent *de;

	if (stat(dst, &db) == -1) {
		/* Writable while we fill it; its own mode comes at the end. */
		if (mkdir(dst, (sb->st_mode & 07777) | S_IRWXU) == -1) {
			warn("%s", dst);
			status = 1;
			return;
		}
	} else if (!S_ISDIR(db.st_mode)) {
		warnx("%s: not a directory", dst);
		status = 1;
		return;
	}
	if ((dir = opendir(src)) == NULL) {
		warn("%s", src);
		status = 1;
		return;
	}
	while ((de = readdir(dir)) != NULL) {
		char s[PATH_MAX], d[PATH_MAX];

		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;
		if (snprintf(s, sizeof(s), "%s/%s", src, de->d_name) >=
		    (int)sizeof(s) ||
		    snprintf(d, sizeof(d), "%s/%s", dst, de->d_name) >=
		    (int)sizeof(d)) {
			warnx("%s/%s: name too long", src, de->d_name);
			status = 1;
			continue;
		}
		copy_item(s, d, root_dev);
	}
	closedir(dir);
	/* Times and modes last, so filling it does not change them. */
	if (copyfile(src, dst, NULL, meta_flags()) == -1) {
		warn("%s: metadata", dst);
		status = 1;
	}
	fix_xattrs(dst);
}

static void
copy_item(const char *src, const char *dst, dev_t root_dev)
{
	struct stat sb, db;

	if (lstat(src, &sb) == -1) {
		warn("%s", src);
		status = 1;
		return;
	}
	if (Vflag)
		printf("copying %s\n", src);

	if (S_ISDIR(sb.st_mode)) {
		if (Xflag && sb.st_dev != root_dev)
			return;
		copy_dir(src, dst, &sb, root_dev);
		return;
	}
	/* Anything else replaces what dst holds, unless that is a directory. */
	if (lstat(dst, &db) == 0) {
		if (S_ISDIR(db.st_mode)) {
			warnx("%s: is a directory", dst);
			status = 1;
			return;
		}
		if (!S_ISREG(sb.st_mode) || !S_ISREG(db.st_mode))
			(void)unlink(dst);
	}
	if (S_ISLNK(sb.st_mode)) {
		char target[PATH_MAX];
		ssize_t n = readlink(src, target, sizeof(target) - 1);

		if (n == -1 || (target[n] = '\0', symlink(target, dst) == -1)) {
			warn("%s", dst);
			status = 1;
			return;
		}
		(void)copyfile(src, dst, NULL, meta_flags() | COPYFILE_NOFOLLOW);
		return;
	}
	if (S_ISREG(sb.st_mode)) {
		if (copyfile(src, dst, NULL, meta_flags() | COPYFILE_DATA) ==
		    -1) {
			warn("%s", dst);
			status = 1;
			return;
		}
		fix_xattrs(dst);
		return;
	}
	if (S_ISFIFO(sb.st_mode) ? mkfifo(dst, sb.st_mode & 07777) :
	    mknod(dst, sb.st_mode, sb.st_rdev)) {
		warn("%s", dst);
		status = 1;
		return;
	}
	(void)copyfile(src, dst, NULL, meta_flags());
}

static void
copy_main(int argc, char **argv)
{
	const char *dst = argv[argc - 1];
	struct stat db;
	bool dst_dir = stat(dst, &db) == 0 && S_ISDIR(db.st_mode);

	if (argc > 2 && !dst_dir) {
		/* Several sources: dst is a directory, made if missing. */
		if (mkdir(dst, 0755) == -1 && errno != EEXIST)
			err(1, "%s", dst);
		dst_dir = true;
	}
	for (int i = 0; i < argc - 1; i++) {
		const char *src = argv[i];
		struct stat sb;
		char d[PATH_MAX];

		if (lstat(src, &sb) == -1) {
			warn("%s", src);
			status = 1;
			continue;
		}
		if (vflag)
			printf("Copying %s\n", src);
		if (S_ISDIR(sb.st_mode) || !dst_dir) {
			copy_item(src, dst, sb.st_dev);
		} else {
			char *s = strdup(src);

			snprintf(d, sizeof(d), "%s/%s", dst, basename(s));
			free(s);
			copy_item(src, d, sb.st_dev);
		}
	}
}

/* ---------------------------------------------------------------------
 * Archives
 */

static int
archive_create(const char *src, const char *out, bool zip, int filter)
{
	struct archive *disk, *a;
	struct archive_entry *e;
	char *parent = NULL, *copy;
	size_t srclen;
	int r;

	a = archive_write_new();
	if (zip)
		archive_write_set_format_zip(a);
	else {
		archive_write_set_format_cpio(a);
		if (filter == 'z')
			archive_write_add_filter_gzip(a);
		else if (filter == 'j')
			archive_write_add_filter_bzip2(a);
	}
	if (archive_write_open_filename(a, strcmp(out, "-") == 0 ? NULL :
	    out) != ARCHIVE_OK)
		errx(1, "%s: %s", out, archive_error_string(a));

	disk = archive_read_disk_new();
	archive_read_disk_set_standard_lookup(disk);
	archive_read_disk_set_behavior(disk,
	    (acl ? 0 : ARCHIVE_READDISK_NO_ACL) |
	    (extattr || rsrc ? 0 : ARCHIVE_READDISK_NO_XATTR) |
	    (Xflag ? ARCHIVE_READDISK_NO_TRAVERSE_MOUNTS : 0));
	if (archive_read_disk_open(disk, src) != ARCHIVE_OK)
		errx(1, "%s: %s", src, archive_error_string(disk));
	if ((copy = strdup(src)) == NULL)
		err(EX_OSERR, "strdup");
	if (keep_parent)
		parent = strdup(basename(copy));
	free(copy);
	srclen = strlen(src);
	while (srclen > 1 && src[srclen - 1] == '/')
		srclen--;

	for (;;) {
		const char *path;
		char name[PATH_MAX];
		const char *rel;

		e = archive_entry_new();
		r = archive_read_next_header2(disk, e);
		if (r == ARCHIVE_EOF) {
			archive_entry_free(e);
			break;
		}
		if (r != ARCHIVE_OK) {
			warnx("%s", archive_error_string(disk));
			status = 1;
			if (r == ARCHIVE_FATAL) {
				archive_entry_free(e);
				break;
			}
		}
		archive_read_disk_descend(disk);
		/* Names inside the archive are relative to src. */
		path = archive_entry_pathname(e);
		rel = path + (strncmp(path, src, srclen) == 0 ? srclen : 0);
		while (*rel == '/')
			rel++;
		if (parent != NULL)
			snprintf(name, sizeof(name), "%s%s%s", parent,
			    *rel ? "/" : "", rel);
		else if (*rel == '\0') {
			/* The top directory itself is not stored. */
			archive_entry_free(e);
			continue;
		} else
			snprintf(name, sizeof(name), "%s", rel);
		archive_entry_set_pathname(e, name);
		if (!rsrc)
			archive_entry_xattr_clear(e);
		if (Vflag)
			fprintf(stderr, "adding %s\n", name);
		r = archive_write_header(a, e);
		if (r < ARCHIVE_WARN)
			errx(1, "%s: %s", name, archive_error_string(a));
		if (r == ARCHIVE_WARN)
			warnx("%s: %s", name, archive_error_string(a));
		if (archive_entry_size(e) > 0 &&
		    archive_entry_filetype(e) == AE_IFREG) {
			int fd = open(archive_entry_sourcepath(e), O_RDONLY);
			char buf[65536];
			ssize_t n;

			if (fd == -1) {
				warn("%s", archive_entry_sourcepath(e));
				status = 1;
			} else {
				while ((n = read(fd, buf, sizeof(buf))) > 0)
					if (archive_write_data(a, buf,
					    (size_t)n) < 0)
						errx(1, "%s: %s", out,
						    archive_error_string(a));
				close(fd);
			}
		}
		archive_entry_free(e);
	}
	free(parent);
	archive_read_close(disk);
	archive_read_free(disk);
	if (archive_write_close(a) != ARCHIVE_OK)
		errx(1, "%s: %s", out, archive_error_string(a));
	archive_write_free(a);
	return (status);
}

static void
archive_extract(const char *in, const char *dst)
{
	struct archive *a, *ext;
	struct archive_entry *e;
	int cwd;
	int flags = ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM |
	    ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_SECURE_NODOTDOT |
	    ARCHIVE_EXTRACT_SECURE_SYMLINKS;
	int r;

	if (acl)
		flags |= ARCHIVE_EXTRACT_ACL;
	if (extattr || rsrc)
		flags |= ARCHIVE_EXTRACT_XATTR;
	if (geteuid() == 0)
		flags |= ARCHIVE_EXTRACT_OWNER;

	if (mkdir(dst, 0755) == -1 && errno != EEXIST)
		err(1, "%s", dst);
	/*
	 * Work inside dst with the archive's relative names, as tar -C
	 * does: libarchive's symlink check would otherwise refuse a path
	 * through a link above dst, such as /tmp -> private/tmp.
	 */
	if ((cwd = open(".", O_RDONLY | O_DIRECTORY)) == -1)
		err(1, "current directory");
	a = archive_read_new();
	archive_read_support_format_all(a);
	archive_read_support_filter_all(a);
	if (archive_read_open_filename(a, strcmp(in, "-") == 0 ? NULL : in,
	    65536) != ARCHIVE_OK)
		errx(1, "%s: %s", in, archive_error_string(a));
	if (chdir(dst) == -1)
		err(1, "%s", dst);
	ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, flags);
	archive_write_disk_set_standard_lookup(ext);

	while ((r = archive_read_next_header(a, &e)) != ARCHIVE_EOF) {
		char name[PATH_MAX];

		if (r < ARCHIVE_WARN)
			errx(1, "%s: %s", in, archive_error_string(a));
		snprintf(name, sizeof(name), "%s/%s", dst,
		    archive_entry_pathname(e));
		if (Vflag)
			printf("extracting %s\n", name);
		r = archive_write_header(ext, e);
		if (r < ARCHIVE_WARN) {
			warnx("%s: %s", name, archive_error_string(ext));
			status = 1;
			continue;
		}
		if (archive_entry_size(e) > 0) {
			const void *buf;
			size_t size;
			la_int64_t off;

			while ((r = archive_read_data_block(a, &buf, &size,
			    &off)) == ARCHIVE_OK)
				if (archive_write_data_block(ext, buf, size,
				    off) < ARCHIVE_WARN) {
					warnx("%s: %s", name,
					    archive_error_string(ext));
					status = 1;
					break;
				}
			if (r < ARCHIVE_WARN && r != ARCHIVE_EOF) {
				warnx("%s: %s", in, archive_error_string(a));
				status = 1;
			}
		}
		if (archive_write_finish_entry(ext) < ARCHIVE_WARN) {
			warnx("%s: %s", name, archive_error_string(ext));
			status = 1;
		}
	}
	archive_read_close(a);
	archive_read_free(a);
	archive_write_close(ext);
	archive_write_free(ext);
	if (fchdir(cwd) == -1)
		err(1, "current directory");
	close(cwd);
}

int
main(int argc, char *argv[])
{
	bool create = false, extract = false, zip = false;
	int filter = 0, i, nargs = 0;
	char **args;

	if ((args = calloc((size_t)argc, sizeof(char *))) == NULL)
		err(EX_OSERR, "calloc");
	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (strcmp(a, "--") == 0) {
			for (i++; i < argc; i++)
				args[nargs++] = argv[i];
			break;
		}
		if (a[0] != '-' || a[1] == '\0') {
			args[nargs++] = argv[i];
			continue;
		}
		if (strcmp(a, "--keepParent") == 0)
			keep_parent = true;
		else if (strcmp(a, "--rsrc") == 0 || strcmp(a, "-rsrc") == 0 ||
		    strcmp(a, "-rsrcFork") == 0)
			rsrc = true;
		else if (strcmp(a, "--norsrc") == 0 || strcmp(a, "-norsrc") == 0)
			rsrc = false;
		else if (strcmp(a, "--extattr") == 0)
			extattr = true;
		else if (strcmp(a, "--noextattr") == 0)
			extattr = false;
		else if (strcmp(a, "--acl") == 0)
			acl = true;
		else if (strcmp(a, "--noacl") == 0)
			acl = false;
		else if (strcmp(a, "--qtn") == 0 || strcmp(a, "--noqtn") == 0 ||
		    strcmp(a, "--sequesterRsrc") == 0 ||
		    strcmp(a, "--hfsCompression") == 0 ||
		    strcmp(a, "--nohfsCompression") == 0 ||
		    strcmp(a, "--preserveHFSCompression") == 0 ||
		    strcmp(a, "--nopreserveHFSCompression") == 0 ||
		    strcmp(a, "--persistRootless") == 0 ||
		    strcmp(a, "--nopersistRootless") == 0)
			;	/* nothing to do on Minkar */
		else if (a[1] == '-')
			errx(EX_USAGE, "unknown option: %s", a);
		else {
			for (const char *p = a + 1; *p; p++) {
				switch (*p) {
				case 'c': create = true; break;
				case 'x': extract = true; break;
				case 'k': zip = true; break;
				case 'z': filter = 'z'; break;
				case 'j': filter = 'j'; break;
				case 'v': vflag = true; break;
				case 'V': vflag = Vflag = true; break;
				case 'X': Xflag = true; break;
				case 'h': usage();
				default:
					errx(EX_USAGE, "unknown option: -%c",
					    *p);
				}
			}
		}
	}
	if (create && extract)
		usage();
	if (nargs < 2)
		usage();
	if (create) {
		if (nargs != 2)
			usage();
		status = archive_create(args[0], args[1], zip, filter);
	} else if (extract) {
		for (i = 0; i < nargs - 1; i++)
			archive_extract(args[i], args[nargs - 1]);
	} else
		copy_main(nargs, args);
	free(args);
	return (status);
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: ditto [-v] [-V] [-X] [options] src ... dst\n"
	    "       ditto -c [-k] [-z | -j] [--keepParent] [options] src "
	    "archive\n"
	    "       ditto -x [-k] [options] archive ... dst\n"
	    "options: --norsrc --noextattr --noacl (and their opposites)\n");
	exit(EX_USAGE);
}
