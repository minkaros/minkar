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
 * strverscmp(3), lpathconf(2) and eaccess(2), which FreeBSD has and
 * Darwin lacks.
 */

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "bsdcompat.h"

/*
 * Compares strings as versions: digit runs compare by value, so "a9" <
 * "a10". A run that starts with 0 (and is longer than one digit) is a
 * fraction and sorts before any whole number: 000 < 00 < 01 < 010 < 09 <
 * 0 < 1 < 9 < 10.
 */
int
strverscmp(const char *s1, const char *s2)
{
	const unsigned char *a = (const unsigned char *)s1;
	const unsigned char *b = (const unsigned char *)s2;
	size_t i = 0, start, la, lb, za, zb;
	int fa, fb;

	while (a[i] != '\0' && a[i] == b[i])
		i++;
	if (a[i] == b[i])
		return (0);
	/* Back up to the start of the digit run the difference is in. */
	start = i;
	while (start > 0 && isdigit(a[start - 1]))
		start--;
	if (!isdigit(a[start]) || !isdigit(b[start]))
		return (a[i] - b[i]);
	for (la = start; isdigit(a[la]); la++)
		;
	for (lb = start; isdigit(b[lb]); lb++)
		;
	la -= start;
	lb -= start;
	fa = a[start] == '0' && la > 1;
	fb = b[start] == '0' && lb > 1;
	if (fa != fb)
		return (fa ? -1 : 1);
	if (fa) {
		/* Fractions: more leading zeros is smaller, then as text. */
		for (za = 0; za < la && a[start + za] == '0'; za++)
			;
		for (zb = 0; zb < lb && b[start + zb] == '0'; zb++)
			;
		if (za != zb)
			return (za > zb ? -1 : 1);
		return (a[i] - b[i]);
	}
	/* Whole numbers: the longer one is larger, else the first digit. */
	if (la != lb)
		return (la < lb ? -1 : 1);
	return (a[i] - b[i]);
}

/* pathconf of a symbolic link itself, not of what it points to. */
long
lpathconf(const char *path, int name)
{
	long r;
	int fd, saved;

	if ((fd = open(path, O_RDONLY | O_SYMLINK | O_NONBLOCK)) == -1)
		return (-1);
	r = fpathconf(fd, name);
	saved = errno;
	close(fd);
	errno = saved;
	return (r);
}

/* access(2) with the effective user and group ids. */
int
eaccess(const char *path, int mode)
{
	return (faccessat(AT_FDCWD, path, mode, AT_EACCESS));
}
