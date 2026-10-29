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
 * Reading Darwin's process accounting file for sa(8). The kernel's
 * acct(2) writes one fixed-size struct acct (<sys/acct.h>) per process
 * that ends: a 10-character command name, and the user, system and
 * elapsed times and the I/O count as comp_t, a 3-bit base-8 exponent
 * and a 13-bit mantissa in 1/AHZ second ticks. Each record is turned
 * into the in-memory form sa works with (acct_darwin.h).
 */

#include <sys/types.h>
#include <sys/acct.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "acct_darwin.h"

/* A comp_t value in ticks of 1/AHZ second, as a count. */
static double
comp_value(comp_t v)
{
	double m = v & 017777;
	int e;

	for (e = v >> 13; e > 0; e--)
		m *= 8;
	return (m);
}

static int
read_one(FILE *f, struct acctv3 *out)
{
	struct acct in;
	size_t n;

	if ((n = fread(&in, 1, sizeof(in), f)) != sizeof(in)) {
		if (n == 0 && !ferror(f))
			return (0);
		if (!ferror(f))
			errno = EFTYPE;	/* a short record */
		return (EOF);
	}
	memset(out, 0, sizeof(*out));
	memcpy(out->ac_comm, in.ac_comm, sizeof(in.ac_comm));
	out->ac_utime = comp_value(in.ac_utime) / AHZ * 1000000;
	out->ac_stime = comp_value(in.ac_stime) / AHZ * 1000000;
	out->ac_etime = comp_value(in.ac_etime) / AHZ * 1000000;
	out->ac_btime = in.ac_btime;
	out->ac_uid = in.ac_uid;
	out->ac_gid = in.ac_gid;
	out->ac_mem = in.ac_mem;
	out->ac_io = comp_value(in.ac_io) / AHZ;	/* blocks */
	out->ac_tty = in.ac_tty;
	out->ac_flagx = in.ac_flag;
	return (1);
}

/* The record at the current position; 1, 0 at the end, EOF on error. */
int
readrec_forward(FILE *f, struct acctv3 *av)
{

	return (read_one(f, av));
}

/*
 * The record ending at the current position, leaving the position at
 * its start; 1, 0 at the beginning of the file, EOF on error.
 */
int
readrec_backward(FILE *f, struct acctv3 *av)
{
	off_t pos;

	if ((pos = ftello(f)) == -1)
		return (EOF);
	if (pos == 0)
		return (0);
	if (pos < (off_t)sizeof(struct acct)) {
		errno = EFTYPE;
		return (EOF);
	}
	if (fseeko(f, pos - sizeof(struct acct), SEEK_SET) == -1 ||
	    read_one(f, av) != 1 ||
	    fseeko(f, pos - sizeof(struct acct), SEEK_SET) == -1)
		return (EOF);
	return (1);
}
