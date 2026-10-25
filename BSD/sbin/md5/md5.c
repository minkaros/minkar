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
 * md5, sha1, sha224, sha256, sha384, sha512, sha512t224, sha512t256 and
 * shasum: digests of files, strings or standard input.
 *
 *	md5 [-pqr] [-s string] [file ...]	MD5 (file) = digest
 *	shasum [-a alg] [-c] [-b | -t] [file ...]	digest  file
 *
 * The program's name chooses the digest (and shasum's output); shasum's
 * -a takes 1, 224, 256, 384, 512, 512224 or 512256, and -c checks the
 * digests listed in the files. Original implementation (macOS's shasum is
 * a Perl script), on libmd.
 */

#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <md5.h>
#include <sha.h>
#include <sha224.h>
#include <sha256.h>
#include <sha384.h>
#include <sha512.h>
#include <sha512t.h>

struct algorithm {
	const char	*progname;	/* md5, sha256, ... */
	const char	*shasum;	/* shasum -a */
	const char	*label;		/* MD5 (file) = ... */
	size_t		 len;		/* digest length */
	char		*(*file)(const char *, char *);
	char		*(*fd)(int, char *);
	char		*(*data)(const void *, unsigned int, char *);
};

static const struct algorithm algorithms[] = {
	{ "md5", NULL, "MD5", 16, MD5File, MD5Fd, MD5Data },
	{ "sha1", "1", "SHA1", 20, SHA1_File, SHA1_Fd, SHA1_Data },
	{ "sha224", "224", "SHA224", 28, SHA224_File, SHA224_Fd, SHA224_Data },
	{ "sha256", "256", "SHA256", 32, SHA256_File, SHA256_Fd, SHA256_Data },
	{ "sha384", "384", "SHA384", 48, SHA384_File, SHA384_Fd, SHA384_Data },
	{ "sha512", "512", "SHA512", 64, SHA512_File, SHA512_Fd, SHA512_Data },
	{ "sha512t224", "512224", "SHA512t224", 28, SHA512_224_File, SHA512_224_Fd, SHA512_224_Data },
	{ "sha512t256", "512256", "SHA512t256", 32, SHA512_256_File, SHA512_256_Fd, SHA512_256_Data },
};
#define	NALGS	(sizeof(algorithms) / sizeof(algorithms[0]))

static const struct algorithm *
by_shasum(const char *a)
{
	for (size_t i = 0; i < NALGS; i++)
		if (algorithms[i].shasum != NULL && strcmp(algorithms[i].shasum, a) == 0)
			return (&algorithms[i]);
	return (NULL);
}

/* shasum -c: lines "digest  file" (or "digest *file"). */
static int
check(const struct algorithm *alg, const char *list)
{
	FILE *f = strcmp(list, "-") == 0 ? stdin : fopen(list, "r");
	char *line = NULL, buf[2 * 64 + 1];
	size_t cap = 0;
	ssize_t n;
	int bad = 0, total = 0;

	if (f == NULL) {
		warn("%s", list);
		return (1);
	}
	while ((n = getline(&line, &cap, f)) > 0) {
		char *name, *sum = line;
		const struct algorithm *a = alg;

		line[strcspn(line, "\r\n")] = '\0';
		if ((name = strchr(line, ' ')) == NULL)
			continue;
		*name++ = '\0';
		if (*name == ' ' || *name == '*')
			name++;
		if (a == NULL) {
			/* The digest's length says which SHA. */
			for (size_t i = 1; i < 6 && a == NULL; i++)
				if (strlen(sum) == 2 * algorithms[i].len)
					a = &algorithms[i];
			if (a == NULL)
				continue;
		}
		total++;
		if (a->file(name, buf) == NULL) {
			printf("%s: FAILED open or read\n", name);
			bad++;
		} else if (strcasecmp(buf, sum) != 0) {
			printf("%s: FAILED\n", name);
			bad++;
		} else {
			printf("%s: OK\n", name);
		}
	}
	free(line);
	if (f != stdin)
		fclose(f);
	if (bad > 0)
		warnx("WARNING: %d of %d computed checksums did NOT match", bad, total);
	return (bad > 0);
}

static int
shasum_main(int argc, char *argv[])
{
	const struct algorithm *alg = NULL;
	bool docheck = false;
	char buf[2 * 64 + 1];
	int ch, rv = 0;

	while ((ch = getopt(argc, argv, "a:bctU0")) != -1) {
		switch (ch) {
		case 'a':
			if ((alg = by_shasum(optarg)) == NULL)
				errx(EX_USAGE, "unknown algorithm: %s", optarg);
			break;
		case 'c':
			docheck = true;
			break;
		case 'b': case 't': case 'U': case '0':
			break;	/* binary and text are the same here */
		default:
			fprintf(stderr, "usage: shasum [-a 1|224|256|384|512|512224|512256] [-c] [file ...]\n");
			return (EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;
	if (docheck) {
		if (argc == 0)
			return (check(alg, "-"));
		for (int i = 0; i < argc; i++)
			rv |= check(alg, argv[i]);
		return (rv);
	}
	if (alg == NULL)
		alg = by_shasum("1");
	if (argc == 0) {
		if (alg->fd(STDIN_FILENO, buf) == NULL)
			err(1, "stdin");
		printf("%s  -\n", buf);
		return (0);
	}
	for (int i = 0; i < argc; i++) {
		if (alg->file(argv[i], buf) == NULL) {
			warn("%s", argv[i]);
			rv = 1;
			continue;
		}
		printf("%s  %s\n", buf, argv[i]);
	}
	return (rv);
}

int
main(int argc, char *argv[])
{
	const struct algorithm *alg = NULL;
	const char *prog = getprogname();
	bool quiet = false, reverse = false, passthrough = false, any = false;
	char buf[2 * 64 + 1];
	int ch, rv = 0;

	if (strcmp(prog, "shasum") == 0)
		return (shasum_main(argc, argv));
	for (size_t i = 0; i < NALGS; i++)
		if (strcmp(algorithms[i].progname, prog) == 0)
			alg = &algorithms[i];
	if (alg == NULL)
		alg = &algorithms[0];

	while ((ch = getopt(argc, argv, "pqrs:")) != -1) {
		switch (ch) {
		case 'p':
			passthrough = true;
			break;
		case 'q':
			quiet = true;
			break;
		case 'r':
			reverse = true;
			break;
		case 's':
			alg->data(optarg, (unsigned int)strlen(optarg), buf);
			if (quiet)
				printf("%s\n", buf);
			else if (reverse)
				printf("%s \"%s\"\n", buf, optarg);
			else
				printf("%s (\"%s\") = %s\n", alg->label, optarg, buf);
			any = true;
			break;
		default:
			fprintf(stderr, "usage: %s [-pqr] [-s string] [file ...]\n", prog);
			return (EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0 && !any) {
		if (passthrough) {
			/* -p: copy standard input to standard output, then the digest. */
			char *data = NULL;
			size_t len = 0, cap = 0;
			size_t n;
			char chunk[8192];

			while ((n = fread(chunk, 1, sizeof(chunk), stdin)) > 0) {
				if (len + n > cap && (data = realloc(data, cap = (len + n) * 2)) == NULL)
					err(1, "realloc");
				memcpy(data + len, chunk, n);
				len += n;
				fwrite(chunk, 1, n, stdout);
			}
			alg->data(data != NULL ? data : "", (unsigned int)len, buf);
			free(data);
		} else if (alg->fd(STDIN_FILENO, buf) == NULL) {
			err(1, "stdin");
		}
		printf("%s\n", buf);
		return (0);
	}
	for (int i = 0; i < argc; i++) {
		if (alg->file(argv[i], buf) == NULL) {
			warn("%s", argv[i]);
			rv = 1;
			continue;
		}
		if (quiet)
			printf("%s\n", buf);
		else if (reverse)
			printf("%s %s\n", buf, argv[i]);
		else
			printf("%s (%s) = %s\n", alg->label, argv[i], buf);
	}
	return (rv);
}
