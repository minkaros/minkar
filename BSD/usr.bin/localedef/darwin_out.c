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
 * localedef's output in the formats Darwin's libc reads, where they
 * differ from FreeBSD's:
 *
 * LC_CTYPE: "RuneMagA", 4.4BSD's _RuneLocale with 32-bit big-endian
 * fields (magic, encoding, sgetrune, sputrune, invalid rune, the
 * 256-entry type, lower and upper tables, the three range lists' counts,
 * the variable part's pointer and length, the class count and pointer),
 * then the ranges, each (min, max, map, types pointer 0). FreeBSD's
 * ctype.c computes the same tables; they are only laid out differently.
 *
 * LC_COLLATE: Minkar's libc collates by code point (the "1.1A" table with
 * every character weighing its value plus one, as all of Minkar's
 * locales do); a definition's order is not kept.
 */

#include <sys/types.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "localedef.h"
#include "darwin_out.h"

#define	INVALID_RUNE	0xFFFD

static int
put32(FILE *f, uint32_t v)
{
	uint8_t b[4] = { v >> 24, v >> 16, v >> 8, v };

	return (fwrite(b, 1, 4, f) == 4 ? 0 : -1);
}

static int
ranges(FILE *f, const _FileRuneEntry *e, uint32_t n)
{
	uint32_t i;

	for (i = 0; i < n; i++)
		if (put32(f, (uint32_t)e[i].min) || put32(f, (uint32_t)e[i].max) ||
		    put32(f, (uint32_t)e[i].map) || put32(f, 0))
			return (-1);
	return (0);
}

int
darwin_write_ctype(FILE *f, const _FileRuneLocale *rl,
    const _FileRuneEntry *ct, uint32_t nct, const _FileRuneEntry *lo,
    uint32_t nlo, const _FileRuneEntry *up, uint32_t nup)
{
	char enc[32];
	int i;

	if (fwrite("RuneMagA", 1, 8, f) != 8)
		return (-1);
	memset(enc, 0, sizeof(enc));
	strlcpy(enc, rl->encoding, sizeof(enc));
	if (fwrite(enc, 1, sizeof(enc), f) != sizeof(enc))
		return (-1);
	/* sgetrune, sputrune (filled in by libc), the invalid rune */
	if (put32(f, 0) || put32(f, 0) || put32(f, INVALID_RUNE))
		return (-1);
	for (i = 0; i < _CACHED_RUNES; i++)
		if (put32(f, rl->runetype[i]))
			return (-1);
	for (i = 0; i < _CACHED_RUNES; i++)
		if (put32(f, (uint32_t)rl->maplower[i]))
			return (-1);
	for (i = 0; i < _CACHED_RUNES; i++)
		if (put32(f, (uint32_t)rl->mapupper[i]))
			return (-1);
	/* The three range lists: count and pointer. */
	if (put32(f, nct) || put32(f, 0) || put32(f, nlo) || put32(f, 0) ||
	    put32(f, nup) || put32(f, 0))
		return (-1);
	/* variable, variable_len, ncharclasses, charclasses */
	if (put32(f, 0) || put32(f, 0) || put32(f, 0) || put32(f, 0))
		return (-1);
	if (ranges(f, ct, nct) || ranges(f, lo, nlo) || ranges(f, up, nup))
		return (-1);
	return (0);
}

int
darwin_write_collate(FILE *f)
{
	static const char version[10] = "1.1A\n";
	/* two forward directives; no chains */
	static const uint8_t info[4] = { 1, 1, 0, 2 };
	int c;

	if (fwrite(version, 1, sizeof(version), f) != sizeof(version) ||
	    fwrite(info, 1, sizeof(info), f) != sizeof(info))
		return (-1);
	/* undefined characters: their value minus -1 */
	if (put32(f, (uint32_t)-1) || put32(f, (uint32_t)-1))
		return (-1);
	/* no substitutions, no chains, no large table */
	if (put32(f, 0) || put32(f, 0) || put32(f, 0) || put32(f, 0))
		return (-1);
	for (c = 0; c < 256; c++)
		if (put32(f, (uint32_t)c + 1) || put32(f, (uint32_t)c + 1))
			return (-1);
	return (0);
}
