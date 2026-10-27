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
 * xattr: list and change extended attributes, with the options of macOS's
 * xattr(1). Written for Minkar.
 *
 *   xattr [-lrsvx] file ...			list names (-l: and values)
 *   xattr -p [-lrsvx] name file ...		print a value
 *   xattr -w [-rsx] name value file ...	write a value
 *   xattr -d [-rsv] name file ...		delete an attribute
 *   xattr -c [-rsv] file ...			delete all attributes
 *
 * -r goes into directories, -s acts on a symbolic link itself, -v names
 * the file even when there is only one, -x reads and writes values as hex.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum op { LIST, PRINT, WRITE, DELETE, CLEAR };

static enum op	op = LIST;
static bool	lflag, rflag, sflag, vflag, xflag;
static int	nfiles;
static const char *aname;
static char	*avalue;
static size_t	avaluelen;
static int	rval;

static void
usage(void)
{
	fprintf(stderr,
	    "usage: xattr [-lrsvx] file ...\n"
	    "       xattr -p [-lrsvx] attr_name file ...\n"
	    "       xattr -w [-rsx] attr_name attr_value file ...\n"
	    "       xattr -d [-rsv] attr_name file ...\n"
	    "       xattr -c [-rsv] file ...\n");
	exit(2);
}

static int
options(void)
{
	return (sflag ? XATTR_NOFOLLOW : 0);
}

/* Prints a value: as text, or 16 hex bytes a line with -x. */
static void
print_value(const char *buf, ssize_t len)
{
	ssize_t i;

	if (!xflag) {
		fwrite(buf, 1, len, stdout);
		putchar('\n');
		return;
	}
	for (i = 0; i < len; i++)
		printf("%02X%s", (unsigned char)buf[i],
		    (i % 16 == 15 || i == len - 1) ? "\n" : " ");
}

/* Reads "4F 4B" (spaces optional) into bytes. */
static char *
parse_hex(const char *s, size_t *lenp)
{
	char *out, *o;
	int hi = -1, v;

	if ((out = o = malloc(strlen(s) / 2 + 1)) == NULL)
		err(1, NULL);
	for (; *s != '\0'; s++) {
		if (isspace((unsigned char)*s))
			continue;
		if (!isxdigit((unsigned char)*s))
			errx(1, "invalid hex value: %s", s);
		v = isdigit((unsigned char)*s) ? *s - '0' :
		    tolower((unsigned char)*s) - 'a' + 10;
		if (hi < 0)
			hi = v;
		else {
			*o++ = (char)(hi << 4 | v);
			hi = -1;
		}
	}
	if (hi >= 0)
		errx(1, "invalid hex value: odd number of digits");
	*lenp = o - out;
	return (out);
}

static ssize_t
get_value(const char *path, const char *name, char **bufp)
{
	ssize_t len;
	char *buf;

	*bufp = NULL;
	if ((len = getxattr(path, name, NULL, 0, 0, options())) < 0)
		return (-1);
	if ((buf = malloc(len + 1)) == NULL)
		err(1, NULL);
	if ((len = getxattr(path, name, buf, len, 0, options())) < 0) {
		free(buf);
		return (-1);
	}
	*bufp = buf;
	return (len);
}

static void
prefix(const char *path)
{
	if (nfiles > 1 || vflag || rflag)
		printf("%s: ", path);
}

static void
do_list(const char *path)
{
	ssize_t len, vlen;
	char *names, *n, *val;

	if ((len = listxattr(path, NULL, 0, options())) < 0)
		goto fail;
	if (len == 0)
		return;
	if ((names = malloc(len)) == NULL)
		err(1, NULL);
	if ((len = listxattr(path, names, len, options())) < 0) {
		free(names);
		goto fail;
	}
	for (n = names; n < names + len; n += strlen(n) + 1) {
		prefix(path);
		if (!lflag) {
			printf("%s\n", n);
			continue;
		}
		printf("%s:%s", n, xflag ? "\n" : " ");
		if ((vlen = get_value(path, n, &val)) < 0) {
			printf("\n");
			continue;
		}
		print_value(val, vlen);
		free(val);
	}
	free(names);
	return;
fail:
	warn("%s", path);
	rval = 1;
}

static void
do_one(const char *path)
{
	ssize_t len;
	char *val;

	switch (op) {
	case LIST:
		do_list(path);
		break;
	case PRINT:
		if ((len = get_value(path, aname, &val)) < 0) {
			if (errno == ENOATTR)
				warnx("%s: No such xattr: %s", path, aname);
			else
				warn("%s", path);
			rval = 1;
			break;
		}
		prefix(path);
		if (lflag)
			printf("%s:%s", aname, xflag ? "\n" : " ");
		print_value(val, len);
		free(val);
		break;
	case WRITE:
		if (setxattr(path, aname, avalue, avaluelen, 0, options()) != 0) {
			warn("%s", path);
			rval = 1;
		}
		break;
	case DELETE:
		if (removexattr(path, aname, options()) != 0) {
			if (errno == ENOATTR)
				warnx("%s: No such xattr: %s", path, aname);
			else
				warn("%s", path);
			rval = 1;
		} else if (vflag)
			printf("%s\n", path);
		break;
	case CLEAR: {
		ssize_t nlen;
		char *names, *n;

		if ((nlen = listxattr(path, NULL, 0, options())) <= 0) {
			if (nlen < 0) {
				warn("%s", path);
				rval = 1;
			}
			break;
		}
		if ((names = malloc(nlen)) == NULL)
			err(1, NULL);
		nlen = listxattr(path, names, nlen, options());
		for (n = names; nlen > 0 && n < names + nlen; n += strlen(n) + 1)
			if (removexattr(path, n, options()) != 0) {
				warn("%s: %s", path, n);
				rval = 1;
			}
		free(names);
		if (vflag)
			printf("%s\n", path);
		break;
	}
	}
}

int
main(int argc, char *argv[])
{
	FTS *fts;
	FTSENT *e;
	int ch, i;

	while ((ch = getopt(argc, argv, "pwdclrsvxh")) != -1)
		switch (ch) {
		case 'p':
			op = PRINT;
			break;
		case 'w':
			op = WRITE;
			break;
		case 'd':
			op = DELETE;
			break;
		case 'c':
			op = CLEAR;
			break;
		case 'l':
			lflag = true;
			break;
		case 'r':
			rflag = true;
			break;
		case 's':
			sflag = true;
			break;
		case 'v':
			vflag = true;
			break;
		case 'x':
			xflag = true;
			break;
		case 'h':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (op == PRINT || op == WRITE || op == DELETE) {
		if (argc < 1)
			usage();
		aname = *argv++;
		argc--;
	}
	if (op == WRITE) {
		if (argc < 1)
			usage();
		if (xflag)
			avalue = parse_hex(*argv, &avaluelen);
		else {
			avalue = *argv;
			avaluelen = strlen(avalue);
		}
		argv++;
		argc--;
	}
	if (argc < 1)
		usage();
	nfiles = argc;

	for (i = 0; i < argc; i++) {
		if (!rflag) {
			do_one(argv[i]);
			continue;
		}
		char *one[] = { argv[i], NULL };
		if ((fts = fts_open(one, FTS_NOCHDIR | (sflag ? FTS_PHYSICAL :
		    FTS_PHYSICAL | FTS_COMFOLLOW), NULL)) == NULL)
			err(1, "fts_open");
		while ((e = fts_read(fts)) != NULL) {
			if (e->fts_info == FTS_DP)
				continue;
			if (e->fts_info == FTS_ERR || e->fts_info == FTS_DNR ||
			    e->fts_info == FTS_NS) {
				warnx("%s: %s", e->fts_path, strerror(e->fts_errno));
				rval = 1;
				continue;
			}
			do_one(e->fts_path);
		}
		fts_close(fts);
	}
	return (rval);
}
