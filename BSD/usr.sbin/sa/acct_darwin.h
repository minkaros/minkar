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
 * The accounting record as FreeBSD's sa(8) handles it in memory (its
 * struct acctv3: times in microseconds, sizes as floats), filled from
 * Darwin's records by readrec.c. Only the fields sa uses; the flags
 * (AFORK, ASU, ACORE, AXSIG...) are Darwin's <sys/acct.h>'s.
 */

#ifndef _MINKAR_ACCT_DARWIN_H_
#define	_MINKAR_ACCT_DARWIN_H_

#include <sys/types.h>
#include <sys/acct.h>
#include <stdint.h>
#include <stdio.h>

#define	AC_COMM_LEN	16	/* room for Darwin's 10, NUL-padded */
#define	AHZV1		AHZ	/* ticks per second of old summary files */

struct acctv3 {
	char	ac_comm[AC_COMM_LEN];	/* command name */
	float	ac_utime;		/* user time, microseconds */
	float	ac_stime;		/* system time, microseconds */
	float	ac_etime;		/* elapsed time, microseconds */
	time_t	ac_btime;		/* starting time */
	uid_t	ac_uid;			/* user id */
	gid_t	ac_gid;			/* group id */
	float	ac_mem;			/* average memory usage */
	float	ac_io;			/* count of IO blocks */
	dev_t	ac_tty;			/* controlling tty */
	uint8_t	ac_flagx;		/* AFORK, ASU, ACORE, AXSIG... */
};

int	readrec_forward(FILE *, struct acctv3 *);
int	readrec_backward(FILE *, struct acctv3 *);

#endif /* !_MINKAR_ACCT_DARWIN_H_ */
