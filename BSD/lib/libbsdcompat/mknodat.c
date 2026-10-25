/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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

#include <sys/param.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include "bsdcompat.h"

/* path relative to fd, as an absolute path (fcntl F_GETPATH). */
static int
resolve(int fd, const char *path, char out[PATH_MAX])
{
	char dir[PATH_MAX];

	if (fd == AT_FDCWD || path[0] == '/') {
		if (strlcpy(out, path, PATH_MAX) >= PATH_MAX) {
			errno = ENAMETOOLONG;
			return (-1);
		}
		return (0);
	}
	if (fcntl(fd, F_GETPATH, dir) == -1)
		return (-1);
	if ((size_t)snprintf(out, PATH_MAX, "%s/%s", dir, path) >= PATH_MAX) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	return (0);
}

int
mkfifoat(int fd, const char *path, mode_t mode)
{
	char full[PATH_MAX];

	if (resolve(fd, path, full) != 0)
		return (-1);
	return (mkfifo(full, mode));
}

int
mknodat(int fd, const char *path, mode_t mode, dev_t dev)
{
	char full[PATH_MAX];

	if (resolve(fd, path, full) != 0)
		return (-1);
	return (mknod(full, mode, dev));
}

int
chflagsat(int fd, const char *path, unsigned long flags, int atflag)
{
	char full[PATH_MAX];

	if (resolve(fd, path, full) != 0)
		return (-1);
	if (atflag & AT_SYMLINK_NOFOLLOW)
		return (lchflags(full, (unsigned int)flags));
	return (chflags(full, (unsigned int)flags));
}

int
fdscandir(int dirfd, struct dirent ***namelist,
    int (*select)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **))
{
	char dir[PATH_MAX];

	if (fcntl(dirfd, F_GETPATH, dir) == -1)
		return (-1);
	return (scandir(dir, namelist, select, compar));
}
