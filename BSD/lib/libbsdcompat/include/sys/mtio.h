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
 * Magnetic tape ioctls for the FreeBSD commands built on Minkar (pax).
 * Darwin has no tape driver: every device rejects these requests, so the
 * commands treat their archive as a plain file or character device.
 */

#ifndef _MINKAR_COMPAT_SYS_MTIO_H_
#define	_MINKAR_COMPAT_SYS_MTIO_H_

#include <sys/types.h>
#include <sys/ioctl.h>

struct mtop {
	short		mt_op;		/* MTWEOF, MTFSF, ... */
	int32_t		mt_count;	/* how many times */
};

#define	MTWEOF		0	/* write end-of-file marks */
#define	MTFSF		1	/* forward space file marks */
#define	MTBSF		2	/* backward space file marks */
#define	MTFSR		3	/* forward space records */
#define	MTBSR		4	/* backward space records */
#define	MTREW		5	/* rewind */
#define	MTOFFL		6	/* rewind and go offline */
#define	MTNOP		7	/* status only */

struct mtget {
	short		mt_type;
	short		mt_dsreg;
	short		mt_erreg;
	short		mt_resid;
	int32_t		mt_fileno;
	int32_t		mt_blkno;
};

#define	MTIOCTOP	_IOW('m', 1, struct mtop)
#define	MTIOCGET	_IOR('m', 2, struct mtget)

#endif /* !_MINKAR_COMPAT_SYS_MTIO_H_ */
