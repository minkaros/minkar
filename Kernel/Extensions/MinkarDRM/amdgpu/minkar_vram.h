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
 * A GPU's VRAM, by ranges (ADR 0002, G2): what is free, as a sorted list
 * of ranges in a fixed array, first fit with alignment, and ranges merged
 * again when freed. Plain C without allocation, for the driver and for
 * host tests. Offsets are from the start of VRAM.
 *
 * There are never more free ranges than taken ones plus one, so an
 * allocation or a reserve is refused while capacity - 1 ranges are taken:
 * a free then always has room and never fails on a valid range.
 */

#ifndef _MINKAR_VRAM_H
#define _MINKAR_VRAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct minkar_vram_range {
	uint64_t start, size;
};

struct minkar_vram {
	struct minkar_vram_range *free;	/* sorted by start, none touching */
	unsigned count, capacity;
	uint64_t size;			/* all of VRAM */
	uint64_t freeBytes;
	unsigned taken;			/* allocated and reserved ranges */
};

/* All of [0, size) free; ranges is the caller's, capacity entries. */
void minkar_vram_init(struct minkar_vram *v, uint64_t size, struct minkar_vram_range *ranges,
    unsigned capacity);
/* Takes [start, start + size) out of the free ranges: in use by someone
 * else (the firmware's framebuffer, the VBIOS's). 0, or -1 if any part of
 * it is not free or the ranges are full. */
int minkar_vram_reserve(struct minkar_vram *v, uint64_t start, uint64_t size);
/* The lowest free range of size bytes at a multiple of align (a power of
 * two): its offset in *offset. 0, or -1 if there is none. */
int minkar_vram_alloc(struct minkar_vram *v, uint64_t size, uint64_t align, uint64_t *offset);
/* The highest free range of size bytes at a multiple of align, at or
 * above floor (buffers the CPU does not reach, kept clear of the low part
 * the CPU's window shows): its offset in *offset. 0, or -1 if none. */
int minkar_vram_alloc_top(struct minkar_vram *v, uint64_t size, uint64_t align, uint64_t floor,
    uint64_t *offset);
/* Gives a range back. 0, or -1 if it overlaps a free one, lies outside
 * VRAM, or the ranges are full. */
int minkar_vram_free(struct minkar_vram *v, uint64_t offset, uint64_t size);
/* The largest free range. */
uint64_t minkar_vram_largest(const struct minkar_vram *v);

#ifdef __cplusplus
}
#endif

#endif
