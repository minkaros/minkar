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
 * What rtsold's sources take from FreeBSD that Darwin names otherwise:
 * its fast monotonic clock, <unistd.h> where FreeBSD's headers bring it
 * in, and the interface's IPv6 flags.
 */

#ifndef _MINKAR_RTSOL_DARWIN_H_
#define	_MINKAR_RTSOL_DARWIN_H_

#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#ifndef CLOCK_MONOTONIC_FAST
#define	CLOCK_MONOTONIC_FAST	CLOCK_MONOTONIC
#endif

/*
 * The interface's IPv6 flags, as Minkar's kernel numbers them
 * (<netinet6/nd6.h>, not in the SDK), set with SIOCSIFINFO_FLAGS.
 */
#ifndef ND6_IFF_ACCEPT_RTADV
#define	ND6_IFF_ACCEPT_RTADV	0x2
#endif
#ifndef ND6_IFF_IFDISABLED
#define	ND6_IFF_IFDISABLED	0x8
#endif
#ifndef SIOCSIFINFO_IN6
#define	SIOCSIFINFO_IN6		SIOCSIFINFO_FLAGS
#endif

#endif /* !_MINKAR_RTSOL_DARWIN_H_ */
