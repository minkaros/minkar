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


#include "minkar_vram.h"

void
minkar_vram_init(struct minkar_vram *v, uint64_t size, struct minkar_vram_range *ranges,
    unsigned capacity)
{
	v->free = ranges;
	v->capacity = capacity;
	v->size = size;
	v->count = 0;
	v->freeBytes = 0;
	v->taken = 0;
	if (capacity > 0 && size > 0) {
		ranges[0].start = 0;
		ranges[0].size = size;
		v->count = 1;
		v->freeBytes = size;
	}
}

static void
removeAt(struct minkar_vram *v, unsigned i)
{
	for (unsigned j = i; j + 1 < v->count; j++)
		v->free[j] = v->free[j + 1];
	v->count--;
}

static int
insertAt(struct minkar_vram *v, unsigned i, uint64_t start, uint64_t size)
{
	if (v->count == v->capacity)
		return -1;
	for (unsigned j = v->count; j > i; j--)
		v->free[j] = v->free[j - 1];
	v->free[i].start = start;
	v->free[i].size = size;
	v->count++;
	return 0;
}

/* Takes [start, start + size) out of free range i, which holds it. */
static int
carve(struct minkar_vram *v, unsigned i, uint64_t start, uint64_t size)
{
	struct minkar_vram_range r = v->free[i];
	uint64_t before = start - r.start, after = r.start + r.size - (start + size);

	if (before > 0 && after > 0) {
		if (insertAt(v, i + 1, start + size, after) != 0)
			return -1;
		v->free[i].size = before;
	} else if (before > 0)
		v->free[i].size = before;
	else if (after > 0) {
		v->free[i].start = start + size;
		v->free[i].size = after;
	} else
		removeAt(v, i);
	v->freeBytes -= size;
	return 0;
}

/* Room for one more taken range (see the header). */
static int
full(const struct minkar_vram *v)
{
	return v->taken + 1 >= v->capacity;
}

int
minkar_vram_reserve(struct minkar_vram *v, uint64_t start, uint64_t size)
{
	if (size == 0 || start + size < start || full(v))
		return -1;
	for (unsigned i = 0; i < v->count; i++) {
		struct minkar_vram_range r = v->free[i];
		if (start >= r.start && start + size <= r.start + r.size) {
			if (carve(v, i, start, size) != 0)
				return -1;
			v->taken++;
			return 0;
		}
	}
	return -1;
}

int
minkar_vram_alloc(struct minkar_vram *v, uint64_t size, uint64_t align, uint64_t *offset)
{
	if (size == 0 || align == 0 || (align & (align - 1)) != 0 || full(v))
		return -1;
	for (unsigned i = 0; i < v->count; i++) {
		struct minkar_vram_range r = v->free[i];
		uint64_t start = (r.start + align - 1) & ~(align - 1);
		if (start < r.start || start - r.start > r.size || r.size - (start - r.start) < size)
			continue;
		if (carve(v, i, start, size) != 0)
			return -1;
		v->taken++;
		*offset = start;
		return 0;
	}
	return -1;
}

int
minkar_vram_free(struct minkar_vram *v, uint64_t offset, uint64_t size)
{
	unsigned i;

	if (size == 0 || offset + size < offset || offset + size > v->size)
		return -1;
	/* i: the first free range after it. */
	for (i = 0; i < v->count && v->free[i].start < offset; i++)
		;
	if (i > 0 && v->free[i - 1].start + v->free[i - 1].size > offset)
		return -1;
	if (i < v->count && offset + size > v->free[i].start)
		return -1;
	int joinsBefore = i > 0 && v->free[i - 1].start + v->free[i - 1].size == offset;
	int joinsAfter = i < v->count && offset + size == v->free[i].start;
	if (joinsBefore && joinsAfter) {
		v->free[i - 1].size += size + v->free[i].size;
		removeAt(v, i);
	} else if (joinsBefore)
		v->free[i - 1].size += size;
	else if (joinsAfter) {
		v->free[i].start = offset;
		v->free[i].size += size;
	} else if (insertAt(v, i, offset, size) != 0)
		return -1;
	v->freeBytes += size;
	if (v->taken > 0)
		v->taken--;
	return 0;
}

uint64_t
minkar_vram_largest(const struct minkar_vram *v)
{
	uint64_t largest = 0;

	for (unsigned i = 0; i < v->count; i++)
		if (v->free[i].size > largest)
			largest = v->free[i].size;
	return largest;
}
