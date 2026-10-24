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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "bsdcompat.h"

long long
strtonum(const char *numstr, long long minval, long long maxval, const char **errstr)
{
	long long v = 0;
	char *end;
	int saved = errno;

	if (minval > maxval) {
		*errstr = "invalid";
		errno = EINVAL;
		return (0);
	}
	errno = 0;
	v = strtoll(numstr, &end, 10);
	if (numstr == end || *end != '\0') {
		*errstr = "invalid";
		errno = EINVAL;
		return (0);
	}
	if ((v == LLONG_MIN && errno == ERANGE) || v < minval) {
		*errstr = "too small";
		errno = ERANGE;
		return (0);
	}
	if ((v == LLONG_MAX && errno == ERANGE) || v > maxval) {
		*errstr = "too large";
		errno = ERANGE;
		return (0);
	}
	*errstr = NULL;
	errno = saved;
	return (v);
}

int
feature_present(const char *feature)
{
	static const char *const features[] = { "inet", "inet6", NULL };

	for (int i = 0; features[i] != NULL; i++)
		if (strcmp(feature, features[i]) == 0)
			return (1);
	return (0);
}
