/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * Casper's fileargs for Minkar: no capability mode, so the files named
 * on the command line are opened directly, with the flags and mode given
 * to fileargs_init.
 */

#ifndef _RAVYNOS_COMPAT_CAP_FILEARGS_H
#define _RAVYNOS_COMPAT_CAP_FILEARGS_H

#include <sys/capsicum.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#define FA_OPEN		1
#define FA_LSTAT	2
#define FA_REALPATH	4

typedef struct fileargs {
	int	fa_flags;
	mode_t	fa_mode;
} fileargs_t;

static inline fileargs_t *
fileargs_init(int argc, char *argv[], int flags, mode_t mode,
    cap_rights_t *rightsp, int operations)
{
	fileargs_t *fa = (fileargs_t *)malloc(sizeof(*fa));

	(void)argc; (void)argv; (void)rightsp; (void)operations;
	if (fa != NULL) {
		fa->fa_flags = flags;
		fa->fa_mode = mode;
	}
	return (fa);
}

static inline fileargs_t *
fileargs_cinit(void *cas, int argc, char *argv[], int flags, mode_t mode,
    cap_rights_t *rightsp, int operations)
{
	(void)cas;
	return (fileargs_init(argc, argv, flags, mode, rightsp, operations));
}

static inline int
fileargs_open(fileargs_t *fa, const char *name)
{
	return (open(name, fa->fa_flags, fa->fa_mode));
}

static inline FILE *
fileargs_fopen(fileargs_t *fa, const char *name, const char *mode)
{
	(void)fa;
	return (fopen(name, mode));
}

static inline int
fileargs_lstat(fileargs_t *fa, const char *name, struct stat *sb)
{
	(void)fa;
	return (lstat(name, sb));
}

static inline char *
fileargs_realpath(fileargs_t *fa, const char *pathname, char *reserved_path)
{
	(void)fa;
	return (realpath(pathname, reserved_path));
}

static inline void
fileargs_free(fileargs_t *fa)
{
	free(fa);
}

#endif /* !_RAVYNOS_COMPAT_CAP_FILEARGS_H */
