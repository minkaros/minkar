/*-
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
 * mkcsmapper and mkesdb run on the build host. These are the BSD libc
 * functions they use that glibc does not have; on a BSD or macOS host
 * this file builds to nothing.
 */

#if defined(__linux__)

#define	_GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char	*fgetln(FILE *, size_t *);
size_t	 strlcpy(char *, const char *, size_t);
const char *getprogname(void);

extern char *program_invocation_short_name;

const char *
getprogname(void)
{

	return (program_invocation_short_name);
}

size_t
strlcpy(char *dst, const char *src, size_t size)
{
	size_t len = strlen(src);

	if (size > 0) {
		size_t n = len < size - 1 ? len : size - 1;

		memcpy(dst, src, n);
		dst[n] = '\0';
	}
	return (len);
}

/*
 * One line, newline included and not terminated, valid until the next
 * call. One buffer for all streams: the tools read one file at a time.
 */
char *
fgetln(FILE *fp, size_t *lenp)
{
	static char *buf;
	static size_t cap;
	ssize_t n;

	n = getline(&buf, &cap, fp);
	if (n <= 0) {
		*lenp = 0;
		return (NULL);
	}
	*lenp = (size_t)n;
	return (buf);
}

#endif /* __linux__ */
