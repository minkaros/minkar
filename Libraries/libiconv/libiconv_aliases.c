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
 * GNU libiconv's names for the same functions. A program built with GNU's
 * <iconv.h> calls libiconv_open, libiconv and so on (its header renames
 * them), and the libiconv Minkar had before this one was GNU's.
 */

#include <sys/types.h>
#include <iconv.h>

#include "iconv-internal.h"

iconv_t	libiconv_open(const char *, const char *);
size_t	libiconv(iconv_t, char **, size_t *, char **, size_t *);
int	libiconv_close(iconv_t);
int	libiconv_open_into(const char *, const char *, iconv_allocation_t *);
int	libiconvctl(iconv_t, int, void *);
void	libiconvlist(int (*)(unsigned int, const char * const *, void *),
	    void *);
void	libiconv_set_relocation_prefix(const char *, const char *);

iconv_t
libiconv_open(const char *tocode, const char *fromcode)
{

	return (__bsd_iconv_open(tocode, fromcode));
}

size_t
libiconv(iconv_t cd, char **in, size_t *inleft, char **out, size_t *outleft)
{

	return (__bsd_iconv(cd, in, inleft, out, outleft));
}

int
libiconv_close(iconv_t cd)
{

	return (__bsd_iconv_close(cd));
}

int
libiconv_open_into(const char *tocode, const char *fromcode,
    iconv_allocation_t *space)
{

	return (__bsd_iconv_open_into(tocode, fromcode, space));
}

int
libiconvctl(iconv_t cd, int request, void *argument)
{

	return (__bsd_iconvctl(cd, request, argument));
}

void
libiconvlist(int (*do_one)(unsigned int, const char * const *, void *),
    void *data)
{

	__bsd_iconvlist(do_one, data);
}

void
libiconv_set_relocation_prefix(const char *orig, const char *curr)
{

	__bsd_iconv_set_relocation_prefix(orig, curr);
}
