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
 * lsbom: list a bill of materials (.bom), the record of the files an
 * installer package lays down, as macOS's lsbom does: one line per path
 * with its mode, owner, size and checksum.
 *
 * A BOM file ("BOMStore", big-endian) is a table of blocks and a list of
 * named variables pointing into it. The "Paths" variable is a B-tree of
 * pages; each leaf entry points to a block with the path's id and its
 * attributes and to one with its parent's id and its name, from which the
 * full path is put together.
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum { T_FILE = 1, T_DIR = 2, T_LINK = 3, T_DEV = 4 };

static const uint8_t *bom;
static size_t bomlen;
static uint32_t nblocks;
static const uint8_t *index_;

static bool sflag, mflag, xflag;
static bool want_b, want_c, want_d, want_f, want_l, some;
static const char *params;

struct node {
	uint32_t	id;
	char		*path;
};
static struct node *nodes;
static size_t nnodes, capnodes;

static uint32_t
be32(const uint8_t *p)
{

	return ((uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
	    (uint32_t)p[2] << 8 | p[3]);
}

static uint16_t
be16(const uint8_t *p)
{

	return ((uint16_t)(p[0] << 8 | p[1]));
}

/* Block i: its bytes and length; NULL when out of the file. */
static const uint8_t *
block(uint32_t i, uint32_t *len)
{
	uint32_t a, l;

	if (i >= nblocks)
		return (NULL);
	a = be32(index_ + 4 + 8 * (size_t)i);
	l = be32(index_ + 8 + 8 * (size_t)i);
	if ((size_t)a + l > bomlen)
		return (NULL);
	if (len != NULL)
		*len = l;
	return (bom + a);
}

static const char *
parentpath(uint32_t id)
{
	size_t i;

	for (i = nnodes; i > 0; i--)
		if (nodes[i - 1].id == id)
			return (nodes[i - 1].path);
	return (NULL);
}

static void
remember(uint32_t id, char *path)
{

	if (nnodes == capnodes) {
		capnodes = capnodes ? capnodes * 2 : 256;
		if ((nodes = realloc(nodes, capnodes * sizeof(*nodes))) == NULL)
			err(1, "realloc");
	}
	nodes[nnodes].id = id;
	nodes[nnodes++].path = path;
}

static const char *
uname_(uint32_t uid)
{
	static char buf[16];
	struct passwd *pw = getpwuid(uid);

	if (pw != NULL)
		return (pw->pw_name);
	snprintf(buf, sizeof(buf), "%u", uid);
	return (buf);
}

static const char *
gname_(uint32_t gid)
{
	static char buf[16];
	struct group *gr = getgrgid(gid);

	if (gr != NULL)
		return (gr->gr_name);
	snprintf(buf, sizeof(buf), "%u", gid);
	return (buf);
}

static void
symmode(uint16_t mode, char *s)
{
	const char *rwx = "rwxrwxrwx";
	int i;

	switch (mode & S_IFMT) {
	case S_IFDIR:	s[0] = 'd'; break;
	case S_IFLNK:	s[0] = 'l'; break;
	case S_IFBLK:	s[0] = 'b'; break;
	case S_IFCHR:	s[0] = 'c'; break;
	default:	s[0] = '-'; break;
	}
	for (i = 0; i < 9; i++)
		s[i + 1] = (mode & (0400 >> i)) ? rwx[i] : '-';
	s[10] = '\0';
}

struct entry {
	const char	*path;
	int		 type;
	uint16_t	 mode;
	uint32_t	 uid, gid, mtime, size, cksum;
	const char	*link;
};

static void
custom(const struct entry *e)
{
	const char *p;
	char buf[64];
	bool first = true;
	struct tm tm;
	time_t t;

	for (p = params; *p != '\0'; p++) {
		if (!first)
			putchar('\t');
		first = false;
		switch (*p) {
		case 'c': printf("%u", e->cksum); break;
		case 'f': printf("%s", e->path); break;
		case 'F': printf("\"%s\"", e->path); break;
		case 'g': printf("%u", e->gid); break;
		case 'G': printf("%s", gname_(e->gid)); break;
		case 'm': printf("%o", e->mode); break;
		case 'M': symmode(e->mode, buf); printf("%s", buf); break;
		case 's': printf("%u", e->size); break;
		case 'S':
			if (e->size >= 1024 * 1024)
				printf("%.1fM", e->size / 1048576.0);
			else if (e->size >= 1024)
				printf("%.1fK", e->size / 1024.0);
			else
				printf("%uB", e->size);
			break;
		case 't': printf("%u", e->mtime); break;
		case 'T':
			t = (time_t)e->mtime;
			localtime_r(&t, &tm);
			strftime(buf, sizeof(buf), "%c", &tm);
			printf("%s", buf);
			break;
		case 'u': printf("%u", e->uid); break;
		case 'U': printf("%s", uname_(e->uid)); break;
		case '/': printf("%u/%u", e->uid, e->gid); break;
		case '?': printf("%s/%s", uname_(e->uid), gname_(e->gid)); break;
		default:
			errx(1, "unknown parameter: %c", *p);
		}
	}
	putchar('\n');
}

static void
show(const struct entry *e)
{

	if (some && !((e->type == T_FILE && want_f) ||
	    (e->type == T_DIR && want_d) || (e->type == T_LINK && want_l) ||
	    (e->type == T_DEV && S_ISBLK(e->mode) && want_b) ||
	    (e->type == T_DEV && S_ISCHR(e->mode) && want_c)))
		return;
	if (params != NULL) {
		custom(e);
		return;
	}
	if (sflag) {
		printf("%s\n", e->path);
		return;
	}
	printf("%s", e->path);
	if (!(xflag && (e->type == T_DIR || e->type == T_LINK)))
		printf("\t%o", e->mode);
	printf("\t%u/%u", e->uid, e->gid);
	switch (e->type) {
	case T_FILE:
		if (mflag)
			printf("\t%u", e->mtime);
		printf("\t%u\t%u", e->size, e->cksum);
		break;
	case T_LINK:
		printf("\t%u\t%u\t%s", e->size, e->cksum, e->link);
		break;
	case T_DEV:
		printf("\t%u", e->cksum);	/* the device number */
		break;
	}
	putchar('\n');
}

/* One leaf entry: info is the path's id and attributes, file its name. */
static void
leafentry(uint32_t info, uint32_t file)
{
	const uint8_t *f, *i1, *i2;
	uint32_t flen, l1, l2, parent, id, llen;
	struct entry e;
	const char *pp;
	char *name, *path, *link = NULL;

	if ((f = block(file, &flen)) == NULL || flen < 5 ||
	    (i1 = block(info, &l1)) == NULL || l1 < 8 ||
	    (i2 = block(be32(i1 + 4), &l2)) == NULL || l2 < 31)
		errx(1, "damaged bill of materials");
	parent = be32(f);
	name = strndup((const char *)f + 4, flen - 4);
	id = be32(i1);
	if (parent == 0 || (pp = parentpath(parent)) == NULL)
		path = name;
	else {
		if (asprintf(&path, "%s/%s", pp, name) == -1)
			err(1, "asprintf");
		free(name);
	}
	remember(id, path);

	memset(&e, 0, sizeof(e));
	e.path = path;
	e.type = i2[0];
	e.mode = be16(i2 + 4);
	e.uid = be32(i2 + 6);
	e.gid = be32(i2 + 10);
	e.mtime = be32(i2 + 14);
	e.size = be32(i2 + 18);
	e.cksum = be32(i2 + 23);
	e.link = "";
	if (e.type == T_LINK) {
		llen = be32(i2 + 27);
		if (31 + (size_t)llen <= l2 && llen > 0) {
			link = strndup((const char *)i2 + 31, llen);
			e.link = link;
		}
	}
	show(&e);
	free(link);
}

/* The leaves from page i on, left to right. */
static void
paths(uint32_t i)
{
	const uint8_t *p;
	uint32_t len, k, n;

	/* Down the left side to the first leaf. */
	for (;;) {
		if ((p = block(i, &len)) == NULL || len < 12)
			errx(1, "damaged bill of materials");
		if (be16(p) != 0)
			break;
		if (len < 20)
			errx(1, "damaged bill of materials");
		i = be32(p + 12);
	}
	/* Along the leaves. */
	for (;;) {
		n = be16(p + 2);
		if (12 + (size_t)n * 8 > len)
			errx(1, "damaged bill of materials");
		for (k = 0; k < n; k++)
			leafentry(be32(p + 12 + 8 * k), be32(p + 16 + 8 * k));
		if ((i = be32(p + 4)) == 0)
			break;
		if ((p = block(i, &len)) == NULL || len < 12)
			errx(1, "damaged bill of materials");
	}
}

static void
usage(void)
{

	fprintf(stderr, "usage: lsbom [-b] [-c] [-d] [-f] [-l] [-m] [-s] [-x]\n"
	    "             [-p parameters] bom ...\n");
	exit(1);
}

static void
lsbom(const char *file)
{
	struct stat st;
	uint32_t ioff, voff, nvars, i, tree, root;
	const uint8_t *v, *t;
	size_t p;
	int fd;

	if ((fd = open(file, O_RDONLY)) == -1 || fstat(fd, &st) == -1)
		err(1, "%s", file);
	bomlen = (size_t)st.st_size;
	if (bomlen < 32 || (bom = mmap(NULL, bomlen, PROT_READ, MAP_PRIVATE,
	    fd, 0)) == MAP_FAILED)
		errx(1, "%s: not a bill of materials", file);
	close(fd);
	if (memcmp(bom, "BOMStore", 8) != 0 || be32(bom + 8) != 1)
		errx(1, "%s: not a bill of materials", file);
	ioff = be32(bom + 16);
	voff = be32(bom + 24);
	if ((size_t)ioff + 4 > bomlen || (size_t)voff + 4 > bomlen)
		errx(1, "%s: damaged", file);
	index_ = bom + ioff;
	nblocks = be32(index_);
	if ((size_t)ioff + 4 + (size_t)nblocks * 8 > bomlen)
		errx(1, "%s: damaged", file);

	/* The "Paths" variable. */
	v = bom + voff;
	nvars = be32(v);
	tree = UINT32_MAX;
	for (i = 0, p = 4; i < nvars; i++) {
		uint8_t n;

		if (voff + p + 5 > bomlen)
			errx(1, "%s: damaged", file);
		n = v[p + 4];
		if (n == 5 && memcmp(v + p + 5, "Paths", 5) == 0)
			tree = be32(v + p);
		p += 5 + n;
	}
	if (tree == UINT32_MAX || (t = block(tree, NULL)) == NULL ||
	    memcmp(t, "tree", 4) != 0)
		errx(1, "%s: no paths", file);
	root = be32(t + 8);
	paths(root);
	munmap((void *)(uintptr_t)bom, bomlen);
	while (nnodes > 0)
		free(nodes[--nnodes].path);
}

int
main(int argc, char *argv[])
{
	int ch, i;

	while ((ch = getopt(argc, argv, "bcdflmp:sx")) != -1)
		switch (ch) {
		case 'b': want_b = some = true; break;
		case 'c': want_c = some = true; break;
		case 'd': want_d = some = true; break;
		case 'f': want_f = some = true; break;
		case 'l': want_l = some = true; break;
		case 'm': mflag = true; break;
		case 'p': params = optarg; break;
		case 's': sflag = true; break;
		case 'x': xflag = true; break;
		default: usage();
		}
	argc -= optind;
	argv += optind;
	if (argc == 0)
		usage();
	for (i = 0; i < argc; i++)
		lsbom(argv[i]);
	return (0);
}
