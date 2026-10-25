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

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include "bsdcompat.h"

ssize_t
copy_file_range(int infd, off_t *inoffp, int outfd, off_t *outoffp, size_t len,
    unsigned int flags)
{
	char buf[64 * 1024];
	size_t done = 0;

	if (flags != 0) {
		errno = EINVAL;
		return (-1);
	}
	while (done < len) {
		size_t want = len - done < sizeof(buf) ? len - done : sizeof(buf);
		ssize_t n, w, off;

		n = inoffp != NULL ? pread(infd, buf, want, *inoffp) : read(infd, buf, want);
		if (n < 0)
			return (done > 0 ? (ssize_t)done : -1);
		if (n == 0)
			break;
		for (off = 0; off < n; off += w) {
			w = outoffp != NULL ? pwrite(outfd, buf + off, (size_t)(n - off), *outoffp + off) :
			    write(outfd, buf + off, (size_t)(n - off));
			if (w < 0)
				return (done > 0 ? (ssize_t)done : -1);
		}
		if (inoffp != NULL)
			*inoffp += n;
		if (outoffp != NULL)
			*outoffp += n;
		done += (size_t)n;
	}
	return ((ssize_t)done);
}
