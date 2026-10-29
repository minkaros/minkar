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
 * The pw_fields bits of FreeBSD's struct passwd, kept beside Darwin's,
 * which has no room for them: a small table from struct address to bits.
 * The password tools handle a few entries at a time (the one being read,
 * its copy, the one being edited), so the oldest slot is reused when the
 * table is full.
 */

#include "pw_compat.h"

#define	PW_SLOTS	64

static struct {
	const struct passwd *pw;
	int fields;
} slots[PW_SLOTS];
static unsigned next_slot;

int *
__pw_fields(const struct passwd *pw)
{
	unsigned i;

	for (i = 0; i < PW_SLOTS; i++)
		if (slots[i].pw == pw)
			return (&slots[i].fields);
	i = next_slot++ % PW_SLOTS;
	slots[i].pw = pw;
	/* Not read by pw_scan: from the files, every field there. */
	slots[i].fields = _PWF_FILES | _PWF_NAME | _PWF_PASSWD | _PWF_UID |
	    _PWF_GID | _PWF_CHANGE | _PWF_CLASS | _PWF_GECOS | _PWF_DIR |
	    _PWF_SHELL | _PWF_EXPIRE;
	return (&slots[i].fields);
}
