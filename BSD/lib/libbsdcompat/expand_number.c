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

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include "bsdcompat.h"

int
expand_number(const char *buf, uint64_t *num)
{
	char *end;
	uintmax_t n;
	unsigned shift;

	errno = 0;
	while (isspace((unsigned char)*buf))
		buf++;
	if (*buf == '-') {
		errno = EINVAL;
		return (-1);
	}
	n = strtoumax(buf, &end, 0);
	if (errno != 0)
		return (-1);
	if (end == buf) {
		errno = EINVAL;
		return (-1);
	}
	switch (tolower((unsigned char)*end)) {
	case '\0':			shift = 0; break;
	case 'b':			shift = 0; break;
	case 'k':			shift = 10; break;
	case 'm':			shift = 20; break;
	case 'g':			shift = 30; break;
	case 't':			shift = 40; break;
	case 'p':			shift = 50; break;
	case 'e':			shift = 60; break;
	default:
		errno = EINVAL;
		return (-1);
	}
	if (*end != '\0') {
		end++;
		/* "k", "kb" and "kib" all mean 1024. */
		if (shift != 0 && (*end == 'i' || *end == 'I'))
			end++;
		if (shift != 0 && (*end == 'b' || *end == 'B'))
			end++;
		if (*end != '\0') {
			errno = EINVAL;
			return (-1);
		}
	}
	if (shift != 0 && (n >> (64 - shift)) != 0) {
		errno = ERANGE;
		return (-1);
	}
	*num = (uint64_t)n << shift;
	return (0);
}
