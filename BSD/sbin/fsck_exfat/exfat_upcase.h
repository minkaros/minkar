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
 * exFAT's up-case table: what each UTF-16 character compares as, kept on
 * the volume. newfs_exfat makes one (Latin, Greek, Cyrillic, Armenian,
 * Georgian and full-width letters) and writes it compressed: a run of
 * characters that map to themselves is 0xffff and the run's length.
 */

#ifndef _EXFAT_UPCASE_H_
#define _EXFAT_UPCASE_H_

#include <stdint.h>
#include <stddef.h>

#define EXFAT_UPCASE_CHARS	65536
#define EXFAT_UPCASE_RUN	0xffff

/* Lower case ranges and the distance to their upper case. */
static const struct {
	uint16_t	lo, hi;
	int32_t		delta;
} exfat_upcase_ranges[] = {
	{ 0x0061, 0x007a, -32 },	/* a-z */
	{ 0x00e0, 0x00f6, -32 },	/* Latin-1 */
	{ 0x00f8, 0x00fe, -32 },
	{ 0x03b1, 0x03c1, -32 },	/* Greek */
	{ 0x03c3, 0x03cb, -32 },
	{ 0x0430, 0x044f, -32 },	/* Cyrillic */
	{ 0x0450, 0x045f, -80 },
	{ 0x0561, 0x0586, -48 },	/* Armenian */
	{ 0x24d0, 0x24e9, -26 },	/* circled letters */
	{ 0xff41, 0xff5a, -32 },	/* full-width a-z */
};

/* Ranges where upper and lower case alternate (upper even). */
static const struct {
	uint16_t	lo, hi;
} exfat_upcase_pairs[] = {
	{ 0x0100, 0x012f }, { 0x0132, 0x0137 }, { 0x014a, 0x0177 },
	{ 0x0182, 0x0185 }, { 0x01a0, 0x01a5 }, { 0x01de, 0x01ef },
	{ 0x01f8, 0x021f }, { 0x0222, 0x0233 }, { 0x03d8, 0x03ef },
	{ 0x0460, 0x0481 }, { 0x048a, 0x04bf }, { 0x04d0, 0x052f },
	{ 0x1e00, 0x1e95 }, { 0x1ea0, 0x1eff },
};

/* Odd-start ranges where upper and lower case alternate (upper odd). */
static const struct {
	uint16_t	lo, hi;
} exfat_upcase_odd_pairs[] = {
	{ 0x0139, 0x0148 }, { 0x0179, 0x017e }, { 0x01cd, 0x01dc },
	{ 0x04c1, 0x04ce },
};

/* The full table: table[c] is what c compares as. */
static inline void
exfat_upcase_default(uint16_t *table)
{
	size_t i;
	uint32_t c;

	for (c = 0; c < EXFAT_UPCASE_CHARS; c++)
		table[c] = (uint16_t)c;
	for (i = 0; i < sizeof(exfat_upcase_ranges) / sizeof(exfat_upcase_ranges[0]); i++)
		for (c = exfat_upcase_ranges[i].lo; c <= exfat_upcase_ranges[i].hi; c++)
			table[c] = (uint16_t)((int32_t)c + exfat_upcase_ranges[i].delta);
	for (i = 0; i < sizeof(exfat_upcase_pairs) / sizeof(exfat_upcase_pairs[0]); i++)
		for (c = exfat_upcase_pairs[i].lo; c <= exfat_upcase_pairs[i].hi; c++)
			if (c & 1)
				table[c] = (uint16_t)(c - 1);
	for (i = 0; i < sizeof(exfat_upcase_odd_pairs) / sizeof(exfat_upcase_odd_pairs[0]); i++)
		for (c = exfat_upcase_odd_pairs[i].lo; c <= exfat_upcase_odd_pairs[i].hi; c++)
			if (!(c & 1))
				table[c] = (uint16_t)(c - 1);
	table[0x00ff] = 0x0178;		/* ÿ */
	table[0x03c2] = 0x03a3;		/* final sigma */
	table[0x03ac] = 0x0386;		/* Greek with tonos */
	for (c = 0x03ad; c <= 0x03af; c++)
		table[c] = (uint16_t)(c - 37);
	table[0x03cc] = 0x038c;
	table[0x03cd] = 0x038e;
	table[0x03ce] = 0x038f;
	for (c = 0x10d0; c <= 0x10f5; c++)	/* Georgian */
		table[c] = (uint16_t)(c - 0x10d0 + 0x10a0);
}

/*
 * The table compressed into out (room for EXFAT_UPCASE_CHARS entries):
 * returns its length in entries.
 */
static inline size_t
exfat_upcase_compress(const uint16_t *table, uint16_t *out)
{
	size_t n = 0;
	uint32_t c = 0;

	while (c < EXFAT_UPCASE_CHARS) {
		uint32_t run = c;

		while (run < EXFAT_UPCASE_CHARS && table[run] == run)
			run++;
		/* A literal 0xffff would read as a run: the last run is one. */
		if (run - c > 2 || run == EXFAT_UPCASE_CHARS) {
			out[n++] = EXFAT_UPCASE_RUN;
			out[n++] = (uint16_t)(run - c);
			c = run;
		} else {
			out[n++] = table[c];
			c++;
		}
	}
	return (n);
}

/*
 * A table as stored (count entries, compressed or not) into the full
 * one: returns 0, or -1 if it does not describe 65536 characters at most.
 */
static inline int
exfat_upcase_expand(const uint16_t *in, size_t count, uint16_t *table)
{
	uint32_t c = 0;
	size_t i;

	for (i = 0; i < EXFAT_UPCASE_CHARS; i++)
		table[i] = (uint16_t)i;
	for (i = 0; i < count && c < EXFAT_UPCASE_CHARS; i++) {
		if (in[i] == EXFAT_UPCASE_RUN && i + 1 < count) {
			c += in[++i];
			continue;
		}
		table[c++] = in[i];
	}
	return (c <= EXFAT_UPCASE_CHARS ? 0 : -1);
}

#endif /* !_EXFAT_UPCASE_H_ */
