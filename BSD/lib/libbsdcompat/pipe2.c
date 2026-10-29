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
 * pipe2(2): a pipe with O_CLOEXEC and O_NONBLOCK set on both ends. Darwin
 * has no pipe2; this sets the flags after pipe(2), so the close-on-exec
 * flag is not atomic with the pipe's creation.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "bsdcompat.h"

int
pipe2(int fds[2], int flags)
{
	int i, saved;

	if ((flags & ~(O_CLOEXEC | O_NONBLOCK)) != 0) {
		errno = EINVAL;
		return (-1);
	}
	if (pipe(fds) == -1)
		return (-1);
	for (i = 0; i < 2; i++) {
		if (((flags & O_CLOEXEC) != 0 &&
		    fcntl(fds[i], F_SETFD, FD_CLOEXEC) == -1) ||
		    ((flags & O_NONBLOCK) != 0 &&
		    fcntl(fds[i], F_SETFL, fcntl(fds[i], F_GETFL) | O_NONBLOCK) == -1)) {
			saved = errno;
			close(fds[0]);
			close(fds[1]);
			errno = saved;
			return (-1);
		}
	}
	return (0);
}
