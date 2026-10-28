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
 * Included before every Citrus source: the paths FreeBSD keeps in
 * <paths.h> and the libc functions Darwin does not have.
 */
#ifndef _MINKAR_ICONV_H_
#define _MINKAR_ICONV_H_

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

/* A host test builds with its own table directories. */
#ifndef _PATH_I18NMODULE
#define	_PATH_I18NMODULE	"/usr/lib/i18n"
#endif
#ifndef _PATH_ESDB
#define	_PATH_ESDB		"/usr/share/i18n/esdb"
#endif
#ifndef _PATH_CSMAPPER
#define	_PATH_CSMAPPER		"/usr/share/i18n/csmapper"
#endif

#if !defined(__linux__)	/* glibc has secure_getenv */
static inline char *
minkar_secure_getenv(const char *name)
{

	return (issetugid() ? NULL : getenv(name));
}
#define	secure_getenv(name)	minkar_secure_getenv(name)
#endif

static inline void *
minkar_reallocarray(void *ptr, size_t nmemb, size_t size)
{
	size_t n;

	if (__builtin_mul_overflow(nmemb, size, &n)) {
		errno = ENOMEM;
		return (NULL);
	}
	return (realloc(ptr, n));
}
#define	reallocarray(p, n, s)	minkar_reallocarray(p, n, s)

/* What FreeBSD's <sys/cdefs.h> and <sys/limits.h> give _strtol.h. */
#ifndef __packed
#define	__packed	__attribute__((__packed__))
#endif
#if defined(__linux__)
/* The table tools on a Linux build host: tools/hostcompat.c. */
#include <stdio.h>
char	*fgetln(FILE *, size_t *);
size_t	 strlcpy(char *, const char *, size_t);
const char *getprogname(void);
#endif

/* What BSD's <sys/cdefs.h> gives and glibc's does not (the host test). */
#include <sys/cdefs.h>
#ifndef __unused
#define	__unused	__attribute__((__unused__))
#endif
#ifndef __DECONST
#define	__DECONST(type, var)	((type)(uintptr_t)(const void *)(var))
#endif

/* BSD <sys/queue.h> macros glibc's lacks (the host test). */
#include <sys/queue.h>
#ifndef TAILQ_FOREACH_SAFE
#define	TAILQ_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = TAILQ_FIRST((head));				\
	    (var) && ((tvar) = TAILQ_NEXT((var), field), 1);		\
	    (var) = (tvar))
#endif
#ifndef LIST_FOREACH_SAFE
#define	LIST_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = LIST_FIRST((head));				\
	    (var) && ((tvar) = LIST_NEXT((var), field), 1);		\
	    (var) = (tvar))
#endif

/* EFTYPE is BSD's (Darwin has it); the table tools also build on Linux. */
#ifndef EFTYPE
#define	EFTYPE	EINVAL
#endif

/* libiconv always takes its locks (FreeBSD libc skips them until a
   second thread starts). */
#define	__isthreaded	1

#define	__INT_MIN	LONG_MIN
#define	__INT_MAX	LONG_MAX
#define	__UINT_MAX	ULONG_MAX

#endif
