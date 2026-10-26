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

#include "HIDReport.h"

/* Item types and tags (short items). */
#define ITEM_MAIN		0
#define ITEM_GLOBAL		1
#define ITEM_LOCAL		2

#define MAIN_INPUT		0x8
#define MAIN_COLLECTION		0xA
#define MAIN_END_COLLECTION	0xC
#define MAIN_OUTPUT		0x9
#define MAIN_FEATURE		0xB

#define GLOBAL_USAGE_PAGE	0x0
#define GLOBAL_LOGICAL_MIN	0x1
#define GLOBAL_LOGICAL_MAX	0x2
#define GLOBAL_REPORT_SIZE	0x7
#define GLOBAL_REPORT_ID	0x8
#define GLOBAL_REPORT_COUNT	0x9
#define GLOBAL_PUSH		0xA
#define GLOBAL_POP		0xB

#define LOCAL_USAGE		0x0
#define LOCAL_USAGE_MIN		0x1
#define LOCAL_USAGE_MAX		0x2

#define INPUT_CONSTANT		0x01
#define INPUT_VARIABLE		0x02
#define INPUT_RELATIVE		0x04

#define PAGE_GENERIC_DESKTOP	0x01
#define PAGE_BUTTON		0x09
#define GD_X			0x30
#define GD_Y			0x31
#define GD_WHEEL		0x38

#define MAX_USAGES		16
#define MAX_REPORT_IDS		16
#define STACK_DEPTH		4

struct globals {
	uint32_t	usagePage;
	int32_t		logicalMin;
	int32_t		logicalMax;
	uint32_t	reportSize;
	uint32_t	reportCount;
	uint8_t		reportID;
};

static uint32_t
itemUnsigned(const uint8_t *p, unsigned size)
{
	uint32_t v = 0;

	for (unsigned i = 0; i < size; i++)
		v |= (uint32_t)p[i] << (8 * i);
	return v;
}

static int32_t
itemSigned(const uint8_t *p, unsigned size)
{
	uint32_t v = itemUnsigned(p, size);

	if (size > 0 && size < 4 && (v & (1U << (8 * size - 1))))
		v |= ~0U << (8 * size);
	return (int32_t)v;
}

static void
setField(struct HIDField *f, const struct globals *g, uint32_t offset, bool relative)
{
	f->present = true;
	f->bitOffset = offset;
	f->bitSize = g->reportSize;
	f->logicalMin = g->logicalMin;
	f->logicalMax = g->logicalMax;
	f->relative = relative;
}

bool
HIDParsePointer(const uint8_t *desc, uint32_t len, struct HIDPointerLayout *out)
{
	struct globals g = { 0 }, stack[STACK_DEPTH];
	unsigned depth = 0;
	uint32_t usages[MAX_USAGES];
	unsigned nUsages = 0;
	uint32_t usageMin = 0, usageMax = 0;
	bool haveRange = false;
	/* Bit offset reached in each report, by report ID. */
	uint8_t ids[MAX_REPORT_IDS];
	uint32_t offsets[MAX_REPORT_IDS];
	unsigned nIds = 0;
	struct HIDPointerLayout cur;
	unsigned i = 0;

	for (unsigned k = 0; k < sizeof(*out); k++)
		((uint8_t *)out)[k] = 0;
	for (unsigned k = 0; k < sizeof(cur); k++)
		((uint8_t *)&cur)[k] = 0;

	while (i < len) {
		uint8_t prefix = desc[i];

		if (prefix == 0xFE) {		/* long item: skipped */
			if (i + 2 >= len)
				break;
			i += 3 + desc[i + 1];
			continue;
		}
		unsigned size = prefix & 3;
		if (size == 3)
			size = 4;
		unsigned type = (prefix >> 2) & 3, tag = prefix >> 4;
		if (i + 1 + size > len)
			break;
		const uint8_t *data = desc + i + 1;
		i += 1 + size;

		if (type == ITEM_GLOBAL) {
			switch (tag) {
			case GLOBAL_USAGE_PAGE: g.usagePage = itemUnsigned(data, size); break;
			case GLOBAL_LOGICAL_MIN: g.logicalMin = itemSigned(data, size); break;
			case GLOBAL_LOGICAL_MAX:
				/* Signed only when the minimum is negative. */
				g.logicalMax = g.logicalMin < 0 ? itemSigned(data, size) :
				    (int32_t)itemUnsigned(data, size);
				break;
			case GLOBAL_REPORT_SIZE: g.reportSize = itemUnsigned(data, size); break;
			case GLOBAL_REPORT_ID: g.reportID = (uint8_t)itemUnsigned(data, size); break;
			case GLOBAL_REPORT_COUNT: g.reportCount = itemUnsigned(data, size); break;
			case GLOBAL_PUSH:
				if (depth < STACK_DEPTH)
					stack[depth++] = g;
				break;
			case GLOBAL_POP:
				if (depth > 0)
					g = stack[--depth];
				break;
			}
			continue;
		}
		if (type == ITEM_LOCAL) {
			uint32_t u = itemUnsigned(data, size);

			/* A 1- or 2-byte usage takes the current usage page. */
			if (size < 4 && tag != LOCAL_USAGE_MIN && tag != LOCAL_USAGE_MAX)
				u |= g.usagePage << 16;
			switch (tag) {
			case LOCAL_USAGE:
				if (nUsages < MAX_USAGES)
					usages[nUsages++] = u;
				break;
			case LOCAL_USAGE_MIN: usageMin = u; haveRange = true; break;
			case LOCAL_USAGE_MAX: usageMax = u; haveRange = true; break;
			}
			continue;
		}
		if (type != ITEM_MAIN)
			continue;

		if (tag == MAIN_INPUT || tag == MAIN_OUTPUT || tag == MAIN_FEATURE) {
			/* Where this report's fields have reached. */
			unsigned r = 0;
			while (r < nIds && ids[r] != g.reportID)
				r++;
			if (r == nIds && nIds < MAX_REPORT_IDS) {
				ids[nIds] = g.reportID;
				offsets[nIds++] = 0;
			}
			uint32_t flags = itemUnsigned(data, size);
			uint32_t offset = r < nIds ? offsets[r] : 0;

			if (tag == MAIN_INPUT && !(flags & INPUT_CONSTANT) && (flags & INPUT_VARIABLE) &&
			    (!cur.valid || cur.reportID == g.reportID)) {
				for (uint32_t n = 0; n < g.reportCount; n++) {
					uint32_t usage;

					if (n < nUsages)
						usage = usages[n];
					else if (haveRange && usageMin + n <= usageMax)
						usage = (g.usagePage << 16) | (usageMin + n);
					else if (nUsages > 0)
						usage = usages[nUsages - 1];
					else
						continue;
					uint32_t at = offset + n * g.reportSize;
					bool rel = (flags & INPUT_RELATIVE) != 0;
					uint32_t page = usage >> 16, id = usage & 0xFFFF;

					if (page == PAGE_GENERIC_DESKTOP && id == GD_X)
						setField(&cur.x, &g, at, rel);
					else if (page == PAGE_GENERIC_DESKTOP && id == GD_Y)
						setField(&cur.y, &g, at, rel);
					else if (page == PAGE_GENERIC_DESKTOP && id == GD_WHEEL)
						setField(&cur.wheel, &g, at, rel);
					else if (page == PAGE_BUTTON && id >= 1 && id <= HID_MAX_BUTTONS) {
						setField(&cur.buttons[id - 1], &g, at, rel);
						if (id > cur.buttonCount)
							cur.buttonCount = id;
					}
				}
				if (cur.x.present && cur.y.present) {
					cur.valid = true;
					cur.reportID = g.reportID;
				}
			}
			if (r < nIds)
				offsets[r] += g.reportSize * g.reportCount;
		}
		/* Local items last until the next main item. */
		nUsages = 0;
		haveRange = false;
		usageMin = usageMax = 0;
	}
	if (cur.valid)
		*out = cur;
	return cur.valid;
}

int32_t
HIDFieldValue(const struct HIDPointerLayout *layout, const struct HIDField *f,
    const uint8_t *report, uint32_t len)
{
	uint32_t base = layout->reportID != 0 ? 8 : 0;
	uint32_t start = base + f->bitOffset;
	uint32_t v = 0;

	if (!f->present || f->bitSize == 0 || f->bitSize > 32 || start + f->bitSize > len * 8)
		return 0;
	for (uint32_t b = 0; b < f->bitSize; b++) {
		uint32_t bit = start + b;

		if (report[bit / 8] & (1U << (bit % 8)))
			v |= 1U << b;
	}
	/* Sign-extend when the field can hold negative values. */
	if (f->logicalMin < 0 && f->bitSize < 32 && (v & (1U << (f->bitSize - 1))))
		v |= ~0U << f->bitSize;
	return (int32_t)v;
}
