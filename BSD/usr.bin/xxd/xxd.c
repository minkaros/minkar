/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * xxd: a hex dump of a file, and back. Our own, written from the
 * command's documented behaviour (macOS ships Vim's): the same options,
 * output columns and C include format, so scripts that read or write its
 * dumps work alike.
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum mode { M_HEX, M_BITS, M_PLAIN, M_INCLUDE };

static enum mode mode = M_HEX;
static int cols = -1;		/* bytes per line */
static int group = -1;		/* bytes per group, 0 for one group */
static bool little;		/* -e */
static bool upper;		/* -u */
static bool capitalize;		/* -C */
static bool autoskip;		/* -a */
static bool decimal;		/* -d */
static bool reverse;		/* -r */
static long long length = -1;	/* -l */
static long long displace;	/* -o */
static const char *varname;	/* -n */

static void
usage(void)
{
	fprintf(stderr,
	    "usage: xxd [-a] [-b | -e | -i | -p] [-C] [-c cols] [-d] [-g bytes]\n"
	    "           [-l len] [-n name] [-o off] [-s [+][-]seek] [-u]\n"
	    "           [infile [outfile]]\n"
	    "       xxd -r [-p] [-c cols] [-s [-]off] [infile [outfile]]\n");
	exit(1);
}

static long long
number(const char *s, const char *what)
{
	char *end;
	long long n;

	errno = 0;
	n = strtoll(s, &end, 0);
	if (errno != 0 || end == s || *end != '\0')
		errx(1, "invalid %s: %s", what, s);
	return n;
}

static int
getbyte(FILE *in, long long *left)
{
	int c;

	if (*left == 0)
		return EOF;
	c = getc(in);
	if (c != EOF && *left > 0)
		(*left)--;
	return c;
}

static void
put_offset(FILE *out, unsigned long long off)
{
	if (decimal)
		fprintf(out, "%08llu: ", off);
	else
		fprintf(out, "%08llx: ", off);	/* -u leaves it lowercase */
}

/* One line of a hex (or bits) dump: n bytes of line[], room for cols. */
static void
dump_line(FILE *out, unsigned long long off, const unsigned char *line, int n)
{
	int g = group == 0 ? cols : group;
	int width = mode == M_BITS ? 8 : 2;
	put_offset(out, off);
	for (int i = 0; i < cols; i++) {
		int at = i;

		if (little) {
			/* -e: each group's bytes in reverse order. */
			int start = i - i % g;
			at = start + (g - 1 - i % g);
		}
		if (at < n) {
			if (mode == M_BITS) {
				for (int b = 7; b >= 0; b--)
					putc(line[at] & (1 << b) ? '1' : '0', out);
			} else {
				fprintf(out, upper ? "%02X" : "%02x", line[at]);
			}
		} else {
			fprintf(out, "%*s", width, "");
		}
		if ((i + 1) % g == 0 && i + 1 < cols)
			putc(' ', out);
	}
	fputs("  ", out);
	for (int i = 0; i < n; i++)
		putc(line[i] >= 0x20 && line[i] < 0x7f ? line[i] : '.', out);
	putc('\n', out);
}

static int
dump(FILE *in, FILE *out, unsigned long long start)
{
	unsigned char *line, *prev;
	unsigned long long off = start + (unsigned long long)displace;
	long long left = length;
	bool skipping = false, havePrev = false;
	int n;

	if ((line = malloc(cols)) == NULL || (prev = malloc(cols)) == NULL)
		err(1, NULL);
	for (;;) {
		int c = 0;

		for (n = 0; n < cols && (c = getbyte(in, &left)) != EOF; n++)
			line[n] = (unsigned char)c;
		if (n == 0)
			break;
		if (autoskip && n == cols && havePrev) {
			bool zero = true;

			for (int i = 0; i < n && zero; i++)
				zero = line[i] == 0 && prev[i] == 0;
			if (zero) {
				if (!skipping)
					fputs("*\n", out);
				skipping = true;
				memcpy(prev, line, n);
				off += n;
				continue;
			}
		}
		skipping = false;
		dump_line(out, off, line, n);
		memcpy(prev, line, n);
		havePrev = n == cols;
		off += n;
		if (n < cols)
			break;
	}
	/* -a: the last line is always shown, so the length can be read. */
	if (skipping)
		dump_line(out, off - cols, prev, cols);
	free(line);
	free(prev);
	return ferror(in) ? 2 : 0;
}

static int
plain(FILE *in, FILE *out)
{
	long long left = length;
	int c, n = 0;

	while ((c = getbyte(in, &left)) != EOF) {
		fprintf(out, upper ? "%02X" : "%02x", c);
		if (++n == cols) {
			putc('\n', out);
			n = 0;
		}
	}
	if (n != 0)
		putc('\n', out);
	return ferror(in) ? 2 : 0;
}

static int
include(FILE *in, FILE *out, const char *infile)
{
	long long left = length;
	unsigned long long total = 0;
	char *name = NULL;
	int c, n = 0;

	if (varname != NULL || infile != NULL) {
		const char *src = varname != NULL ? varname : infile;

		if ((name = strdup(src)) == NULL)
			err(1, NULL);
		for (char *p = name; *p != '\0'; p++) {
			if (!isalnum((unsigned char)*p))
				*p = '_';
			else if (capitalize)
				*p = (char)toupper((unsigned char)*p);
		}
		/* A C name cannot start with a digit. */
		fprintf(out, "unsigned char %s%s[] = {\n",
		    isdigit((unsigned char)name[0]) ? "__" : "", name);
	}
	while ((c = getbyte(in, &left)) != EOF) {
		if (total > 0)
			fputs(n == 0 ? ",\n" : ", ", out);
		if (n == 0)
			fputs("  ", out);
		fprintf(out, upper ? "0X%02X" : "0x%02x", c);
		total++;
		n = (n + 1) % cols;
	}
	if (total > 0)
		putc('\n', out);
	if (name != NULL) {
		fprintf(out, "};\nunsigned int %s%s_%s = %llu;\n",
		    isdigit((unsigned char)name[0]) ? "__" : "", name,
		    capitalize ? "LEN" : "len", total);
		free(name);
	}
	return ferror(in) ? 2 : 0;
}

static int
hexval(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* Writes byte b at offset off of out, filling a gap with zeros. */
static void
put_at(FILE *out, bool seekable, long long *pos, long long off, int b)
{
	if (off != *pos) {
		if (seekable && fseeko(out, off, SEEK_SET) == 0) {
			*pos = off;
		} else {
			while (*pos < off) {
				putc(0, out);
				(*pos)++;
			}
		}
	}
	putc(b, out);
	(*pos)++;
}

static int
revert(FILE *in, FILE *out, long long seek)
{
	bool seekable = fseeko(out, 0, SEEK_CUR) == 0 && !isatty(fileno(out));
	long long pos = 0;
	char *line = NULL;
	size_t cap = 0;
	ssize_t len;

	if (mode == M_PLAIN) {
		int c, hi = -1;
		long long off = seek;

		while ((c = getc(in)) != EOF) {
			int v = hexval(c);

			if (v < 0)
				continue;
			if (hi < 0) {
				hi = v;
			} else {
				put_at(out, seekable, &pos, off++, hi << 4 | v);
				hi = -1;
			}
		}
		return ferror(in) ? 2 : 0;
	}
	while ((len = getline(&line, &cap, in)) > 0) {
		char *p = line, *end;
		unsigned long long addr;
		int spaces = 0, hi = -1;

		addr = strtoull(p, &end, 16);
		if (end == p || *end != ':')
			continue;	/* not a dump line, such as "*" */
		p = end + 1;
		long long off = (long long)addr + seek;
		for (; *p != '\0' && *p != '\n'; p++) {
			int v = hexval((unsigned char)*p);

			if (*p == ' ' || *p == '\t') {
				/* Two blanks end the hex columns. */
				if (++spaces >= 2 && hi < 0 && p > end + 2)
					break;
				continue;
			}
			spaces = 0;
			if (v < 0)
				break;
			if (hi < 0) {
				hi = v;
			} else {
				put_at(out, seekable, &pos, off++, hi << 4 | v);
				hi = -1;
			}
		}
	}
	free(line);
	return ferror(in) ? 2 : 0;
}

int
main(int argc, char *argv[])
{
	const char *seekarg = NULL;
	const char *infile = NULL;
	FILE *in = stdin, *out = stdout;
	int ch, rc;

	/* -c and -g also come as -cols and -groupsize, as Vim's xxd takes. */
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-cols") == 0)
			argv[i] = "-c";
		else if (strcmp(argv[i], "-groupsize") == 0)
			argv[i] = "-g";
		else if (strcmp(argv[i], "-len") == 0)
			argv[i] = "-l";
		else if (strcmp(argv[i], "-seek") == 0)
			argv[i] = "-s";
		else if (strcmp(argv[i], "-ps") == 0 || strcmp(argv[i], "-postscript") == 0 ||
		    strcmp(argv[i], "-plain") == 0)
			argv[i] = "-p";
		else if (strcmp(argv[i], "-include") == 0)
			argv[i] = "-i";
		else if (strcmp(argv[i], "-bits") == 0)
			argv[i] = "-b";
		else if (strcmp(argv[i], "-revert") == 0)
			argv[i] = "-r";
		else if (strcmp(argv[i], "-version") == 0)
			argv[i] = "-v";
		else if (strcmp(argv[i], "-help") == 0)
			argv[i] = "-h";
	}
	while ((ch = getopt(argc, argv, "abCc:dEeg:hil:n:o:pR:rs:uv")) != -1) {
		switch (ch) {
		case 'a': autoskip = true; break;
		case 'b': mode = M_BITS; break;
		case 'C': capitalize = true; break;
		case 'c': cols = (int)number(optarg, "column count"); break;
		case 'd': decimal = true; break;
		case 'E': errx(1, "EBCDIC is not supported"); break;
		case 'e': little = true; break;
		case 'g': group = (int)number(optarg, "group size"); break;
		case 'i': mode = M_INCLUDE; break;
		case 'l': length = number(optarg, "length"); break;
		case 'n': varname = optarg; break;
		case 'o': displace = number(optarg, "offset"); break;
		case 'p': mode = M_PLAIN; break;
		case 'R':
			/* Colour: never on, whatever is asked. */
			if (strcmp(optarg, "always") != 0 && strcmp(optarg, "auto") != 0 &&
			    strcmp(optarg, "never") != 0)
				usage();
			break;
		case 'r': reverse = true; break;
		case 's': seekarg = optarg; break;
		case 'u': upper = true; break;
		case 'v':
			fprintf(stderr, "xxd (Minkar) 1.0\n");
			return 0;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc > 2)
		usage();
	if (length < -1)
		errx(1, "invalid length");

	if (little && mode != M_HEX)
		errx(1, "only one of -b, -e, -i, -p can be given");
	if (cols < 0)
		cols = mode == M_PLAIN ? 30 : mode == M_INCLUDE ? 12 :
		    mode == M_BITS ? 6 : 16;
	if (cols == 0)
		cols = mode == M_PLAIN ? 30 : 16;
	if (cols > 256 && mode != M_PLAIN)
		errx(1, "invalid number of columns (max. 256)");
	if (group < 0)
		group = mode == M_BITS ? 1 : little ? 4 : 2;
	if (little && group != 0 && (group & (group - 1)) != 0)
		errx(1, "number of octets per group must be a power of 2 with -e");

	if (argc >= 1 && strcmp(argv[0], "-") != 0) {
		infile = argv[0];
		if ((in = fopen(infile, "r")) == NULL)
			err(2, "%s", infile);
	}
	/* -r patches an existing outfile in place, as Vim's xxd does. */
	if (argc == 2 && strcmp(argv[1], "-") != 0) {
		if ((out = fopen(argv[1], reverse ? "r+" : "w")) == NULL &&
		    (!reverse || (out = fopen(argv[1], "w")) == NULL))
			err(3, "%s", argv[1]);
	}

	long long seek = 0;
	unsigned long long start = 0;
	if (seekarg != NULL) {
		bool relative = seekarg[0] == '+';
		long long s = number(seekarg + (relative ? 1 : 0), "seek offset");

		if (reverse) {
			seek = s;
		} else if (s < 0) {
			if (fseeko(in, s, SEEK_END) != 0)
				errx(4, "Sorry, cannot seek.");
			start = (unsigned long long)ftello(in);
		} else if (relative) {
			off_t here = ftello(in);

			if (here < 0)
				here = 0;
			if (fseeko(in, s, SEEK_CUR) != 0)
				for (long long i = 0; i < s && getc(in) != EOF; i++)
					;
			start = (unsigned long long)(here + s);
		} else {
			if (fseeko(in, s, SEEK_SET) != 0)
				for (long long i = 0; i < s && getc(in) != EOF; i++)
					;
			start = (unsigned long long)s;
		}
	}

	if (reverse)
		rc = revert(in, out, seek);
	else if (mode == M_PLAIN)
		rc = plain(in, out);
	else if (mode == M_INCLUDE)
		rc = include(in, out, infile);
	else
		rc = dump(in, out, start);

	if (fflush(out) != 0 || ferror(out))
		err(3, "write error");
	return rc;
}
