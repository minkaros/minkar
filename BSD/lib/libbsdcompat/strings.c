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

/* strchrnul, reallocarray and setproctitle, as FreeBSD has them. */

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

#include "bsdcompat.h"

char *
strchrnul(const char *s, int c)
{
	const char ch = (char)c;

	while (*s != '\0' && *s != ch)
		s++;
	return ((char *)(uintptr_t)s);
}

void *
reallocarray(void *ptr, size_t nmemb, size_t size)
{

	if (size != 0 && nmemb > SIZE_MAX / size) {
		errno = ENOMEM;
		return (NULL);
	}
	return (realloc(ptr, nmemb * size));
}

/*
 * XNU keeps a process's arguments where ps reads them, and there is no
 * room to rewrite them safely here: the title stays the command line.
 */
void
setproctitle(const char *fmt, ...)
{

	(void)fmt;
}
