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
 * closefrom(lowfd): close every open descriptor from lowfd up, as on
 * FreeBSD. Darwin has no such call: the open ones are listed in /dev/fd
 * (each process sees its own there), and when that cannot be read, every
 * number up to the descriptor limit is closed.
 */

#include <sys/types.h>
#include <sys/resource.h>

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include "bsdcompat.h"

void
closefrom(int lowfd)
{
	struct rlimit rl;
	struct dirent *e;
	DIR *d;
	long fd, max;
	char *end;
	int dfd, *fds = NULL;
	size_t n = 0, cap = 0, i;

	if (lowfd < 0)
		lowfd = 0;
	if ((d = opendir("/dev/fd")) != NULL) {
		dfd = dirfd(d);
		/* Collect first: closing while reading would upset readdir. */
		while ((e = readdir(d)) != NULL) {
			fd = strtol(e->d_name, &end, 10);
			if (*end != '\0' || fd < lowfd || fd == dfd || fd > INT_MAX)
				continue;
			if (n == cap) {
				int *nf;

				cap = cap ? cap * 2 : 64;
				if ((nf = realloc(fds, cap * sizeof(*fds))) == NULL)
					break;
				fds = nf;
			}
			fds[n++] = (int)fd;
		}
		closedir(d);
		if (e == NULL) {
			for (i = 0; i < n; i++)
				(void)close(fds[i]);
			free(fds);
			return;
		}
		free(fds);
	}
	max = sysconf(_SC_OPEN_MAX);
	if (getrlimit(RLIMIT_NOFILE, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY &&
	    (long)rl.rlim_cur > max)
		max = (long)rl.rlim_cur;
	if (max < 0 || max > 65536)
		max = 65536;
	for (fd = lowfd; fd < max; fd++)
		(void)close((int)fd);
}
