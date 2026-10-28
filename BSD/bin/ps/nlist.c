/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Modifications Copyright (c) 2026 The Minkar contributors
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#ifdef __APPLE__
#include <sys/param.h>
#endif
#include <sys/sysctl.h>

#include <stddef.h>
#ifdef __APPLE__
#include <stdint.h>
#include <unistd.h>
#endif

#include "ps.h"

fixpt_t	ccpu;				/* kernel _ccpu variable */
int	nlistread;			/* if nlist already read. */
unsigned long	mempages;		/* number of pages of phys. memory */
int	fscale;				/* kernel _fscale variable */

int
donlist(void)
{
	size_t oldlen;

#ifdef __APPLE__
	/*
	 * Darwin has no kern.ccpu or kern.fscale: %CPU comes from the
	 * threads' usage, in FSCALE units. Memory is hw.memsize.
	 */
	uint64_t memsize;

	ccpu = 0;
	fscale = FSCALE;
	oldlen = sizeof(memsize);
	if (sysctlbyname("hw.memsize", &memsize, &oldlen, NULL, 0) == -1)
		return (1);
	mempages = (unsigned long)(memsize / (uint64_t)getpagesize());
	nlistread = 1;
	return (0);
#endif
	oldlen = sizeof(ccpu);
	if (sysctlbyname("kern.ccpu", &ccpu, &oldlen, NULL, 0) == -1)
		return (1);
	oldlen = sizeof(fscale);
	if (sysctlbyname("kern.fscale", &fscale, &oldlen, NULL, 0) == -1)
		return (1);
	oldlen = sizeof(mempages);
	if (sysctlbyname("hw.availpages", &mempages, &oldlen, NULL, 0) == -1)
		return (1);
	nlistread = 1;
	return (0);
}
