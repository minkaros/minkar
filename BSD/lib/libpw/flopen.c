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
 * flopen(3), as FreeBSD's libutil documents it: open a file and take an
 * exclusive flock(2) on it, without blocking when O_NONBLOCK is given;
 * if the file was replaced or removed while waiting, open it again. With
 * O_TRUNC, the file is only truncated once the lock is held.
 */

#include <sys/file.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#include "pw_compat.h"

int
flopen(const char *path, int flags, ...)
{
	struct stat fst, pst;
	mode_t mode = 0;
	int fd, lock, saved;
	va_list ap;

	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = (mode_t)va_arg(ap, int);
		va_end(ap);
	}
	lock = LOCK_EX;
	if (flags & O_NONBLOCK)
		lock |= LOCK_NB;
	for (;;) {
		if ((fd = open(path, flags & ~O_TRUNC, mode)) == -1)
			return (-1);
		if (flock(fd, lock) == -1)
			goto fail;
		if (stat(path, &pst) == -1) {
			if (errno == ENOENT) {
				/* Removed while we waited: try again. */
				close(fd);
				continue;
			}
			goto fail;
		}
		if (fstat(fd, &fst) == -1)
			goto fail;
		if (fst.st_dev != pst.st_dev || fst.st_ino != pst.st_ino) {
			/* Replaced while we waited: try again. */
			close(fd);
			continue;
		}
		if ((flags & O_TRUNC) && ftruncate(fd, 0) == -1)
			goto fail;
		return (fd);
	}
fail:
	saved = errno;
	close(fd);
	errno = saved;
	return (-1);
}
