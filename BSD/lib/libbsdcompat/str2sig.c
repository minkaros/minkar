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
 * str2sig(3), as FreeBSD's libc has it: a signal from its name, with or
 * without "SIG" (any case), from "RTMIN+n"/"RTMAX-n" where they exist, or
 * from its number. 0 and the signal in *signum, or -1.
 */

#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "bsdcompat.h"

int
str2sig(const char *restrict str, int *restrict signum)
{
	const char *name = str;
	char *end;
	long n;
	int i;

	if (str == NULL || *str == '\0')
		return (-1);
	if (isdigit((unsigned char)*str)) {
		n = strtol(str, &end, 10);
		if (*end != '\0' || n < 0 || n >= NSIG)
			return (-1);
		*signum = (int)n;
		return (0);
	}
	if (strncasecmp(name, "SIG", 3) == 0)
		name += 3;
	for (i = 1; i < NSIG; i++) {
		if (sys_signame[i] != NULL && strcasecmp(name, sys_signame[i]) == 0) {
			*signum = i;
			return (0);
		}
	}
	return (-1);
}
