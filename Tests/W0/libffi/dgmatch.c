/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * dgmatch PATTERN-FILE OUTPUT-FILE: exits 0 if the output contains a
 * match of the pattern, as DejaGnu's dg-output checks it (an extended
 * regular expression, unanchored, where . matches a newline too). An
 * empty pattern matches anything.
 */

#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

static char *slurp(const char *path)
{
	FILE *f = fopen(path, "rb");
	char *buf;
	long n;

	if (f == NULL || fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0)
		return NULL;
	buf = malloc((size_t)n + 1);
	if (buf == NULL || fread(buf, 1, (size_t)n, f) != (size_t)n)
		return NULL;
	buf[n] = 0;
	fclose(f);
	return buf;
}

int main(int argc, char **argv)
{
	char *pattern, *output;
	regex_t re;
	int err;

	if (argc != 3 || (pattern = slurp(argv[1])) == NULL || (output = slurp(argv[2])) == NULL) {
		fprintf(stderr, "usage: dgmatch PATTERN-FILE OUTPUT-FILE\n");
		return 2;
	}
	if (*pattern == 0)
		return 0;
	if ((err = regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB)) != 0) {
		char msg[256];
		regerror(err, &re, msg, sizeof msg);
		fprintf(stderr, "dgmatch: %s: %s\n", argv[1], msg);
		return 2;
	}
	return regexec(&re, output, 0, NULL, 0) == 0 ? 0 : 1;
}
