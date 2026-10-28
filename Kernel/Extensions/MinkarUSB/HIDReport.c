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

/* One Input main item, with the local items that came before it. */
struct inputItem {
	const struct globals *g;
	uint32_t	flags;
	uint32_t	bitOffset;	/* in its report, after the report ID */
	const uint32_t	*usages;	/* page << 16 | id */
	unsigned	nUsages;
	bool		haveRange;
	uint32_t	usageMin, usageMax;	/* ids, on the current page */
};

typedef void (*inputVisitor)(void *ctx, const struct inputItem *item);

/* The usage of the n-th field of an item, 0 when it has none. */
static uint32_t
itemUsage(const struct inputItem *it, uint32_t n)
{
	if (n < it->nUsages)
		return it->usages[n];
	if (it->haveRange && it->usageMin + n <= it->usageMax)
		return (it->g->usagePage << 16) | (it->usageMin + n);
	if (it->nUsages > 0)
		return it->usages[it->nUsages - 1];
	return 0;
}

/*
 * Walks the descriptor's short items, keeping the global state, the
 * pending local items and each report's bit offset (by report ID), and
 * calls visit for every Input item.
 */
static void
walkInputs(const uint8_t *desc, uint32_t len, inputVisitor visit, void *ctx)
{
	struct globals g = { 0 }, stack[STACK_DEPTH];
	unsigned depth = 0;
	uint32_t usages[MAX_USAGES];
	unsigned nUsages = 0;
	uint32_t usageMin = 0, usageMax = 0;
	bool haveRange = false;
	uint8_t ids[MAX_REPORT_IDS];
	uint32_t offsets[MAX_REPORT_IDS];
	unsigned nIds = 0;
	unsigned i = 0;

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
			uint32_t offset = r < nIds ? offsets[r] : 0;

			if (tag == MAIN_INPUT) {
				struct inputItem it = { &g, itemUnsigned(data, size), offset,
				    usages, nUsages, haveRange, usageMin, usageMax };
				visit(ctx, &it);
			}
			if (r < nIds)
				offsets[r] += g.reportSize * g.reportCount;
		}
		/* Local items last until the next main item. */
		nUsages = 0;
		haveRange = false;
		usageMin = usageMax = 0;
	}
}

static void
zero(void *p, unsigned n)
{
	for (unsigned k = 0; k < n; k++)
		((uint8_t *)p)[k] = 0;
}

static void
visitPointer(void *ctx, const struct inputItem *it)
{
	struct HIDPointerLayout *cur = (struct HIDPointerLayout *)ctx;
	const struct globals *g = it->g;

	if ((it->flags & INPUT_CONSTANT) || !(it->flags & INPUT_VARIABLE) ||
	    (cur->valid && cur->reportID != g->reportID))
		return;
	for (uint32_t n = 0; n < g->reportCount; n++) {
		uint32_t usage = itemUsage(it, n);

		if (usage == 0)
			continue;
		uint32_t at = it->bitOffset + n * g->reportSize;
		bool rel = (it->flags & INPUT_RELATIVE) != 0;
		uint32_t page = usage >> 16, id = usage & 0xFFFF;

		if (page == PAGE_GENERIC_DESKTOP && id == GD_X)
			setField(&cur->x, g, at, rel);
		else if (page == PAGE_GENERIC_DESKTOP && id == GD_Y)
			setField(&cur->y, g, at, rel);
		else if (page == PAGE_GENERIC_DESKTOP && id == GD_WHEEL)
			setField(&cur->wheel, g, at, rel);
		else if (page == PAGE_BUTTON && id >= 1 && id <= HID_MAX_BUTTONS) {
			setField(&cur->buttons[id - 1], g, at, rel);
			if (id > cur->buttonCount)
				cur->buttonCount = id;
		}
	}
	if (cur->x.present && cur->y.present) {
		cur->valid = true;
		cur->reportID = g->reportID;
	}
}

bool
HIDParsePointer(const uint8_t *desc, uint32_t len, struct HIDPointerLayout *out)
{
	struct HIDPointerLayout cur;

	zero(out, sizeof(*out));
	zero(&cur, sizeof(cur));
	walkInputs(desc, len, visitPointer, &cur);
	if (cur.valid)
		*out = cur;
	return cur.valid;
}

#define PAGE_KEYBOARD		0x07
#define KEY_FIRST_MODIFIER	0xE0
#define KEY_LAST_MODIFIER	0xE7
#define KEY_ERROR_ROLLOVER	0x01

static void
visitKeyboard(void *ctx, const struct inputItem *it)
{
	struct HIDKeyboardLayout *kb = (struct HIDKeyboardLayout *)ctx;
	const struct globals *g = it->g;

	if (it->flags & INPUT_CONSTANT)
		return;
	/* The keyboard's fields: those of the first report that has some. */
	if (kb->valid && kb->reportID != g->reportID)
		return;

	if (!(it->flags & INPUT_VARIABLE)) {
		/* An array of key usages on the keyboard page. */
		if (g->usagePage != PAGE_KEYBOARD || kb->hasArray ||
		    g->reportSize < 8 || g->reportSize > 16 || g->reportCount == 0)
			return;
		kb->hasArray = true;
		kb->arrayBitOffset = it->bitOffset;
		kb->arrayBitSize = g->reportSize;
		kb->arrayCount = g->reportCount;
	} else {
		if (g->reportSize != 1)
			return;
		for (uint32_t n = 0; n < g->reportCount; n++) {
			uint32_t usage = itemUsage(it, n);

			if ((usage >> 16) != PAGE_KEYBOARD)
				return;
			uint32_t id = usage & 0xFFFF;
			if (id == KEY_FIRST_MODIFIER && !kb->hasModifiers &&
			    n + 8 <= g->reportCount) {
				kb->hasModifiers = true;
				kb->modBitOffset = it->bitOffset + n;
				n += 7;
			} else if (id < KEY_FIRST_MODIFIER && !kb->hasBitmap) {
				/* A bitmap: one bit per usage, from this one on. */
				uint32_t count = 0;
				while (n + count < g->reportCount &&
				    (itemUsage(it, n + count) & 0xFFFF) == id + count &&
				    id + count < KEY_FIRST_MODIFIER)
					count++;
				kb->hasBitmap = true;
				kb->bitmapBitOffset = it->bitOffset + n;
				kb->bitmapFirstUsage = id;
				kb->bitmapCount = count;
				n += count - 1;
			}
		}
	}
	if (kb->hasArray || kb->hasBitmap) {
		kb->valid = true;
		kb->reportID = g->reportID;
	}
}

bool
HIDParseKeyboard(const uint8_t *desc, uint32_t len, struct HIDKeyboardLayout *out)
{
	struct HIDKeyboardLayout kb;

	zero(out, sizeof(*out));
	zero(&kb, sizeof(kb));
	walkInputs(desc, len, visitKeyboard, &kb);
	if (kb.valid)
		*out = kb;
	return kb.valid;
}

static uint32_t
reportBits(const uint8_t *report, uint32_t len, uint32_t start, uint32_t size)
{
	uint32_t v = 0;

	if (size == 0 || size > 32 || start + size > len * 8)
		return 0;
	for (uint32_t b = 0; b < size; b++) {
		uint32_t bit = start + b;

		if (report[bit / 8] & (1U << (bit % 8)))
			v |= 1U << b;
	}
	return v;
}

bool
HIDKeyboardToBoot(const struct HIDKeyboardLayout *kb, const uint8_t *report,
    uint32_t len, uint8_t boot[8])
{
	uint32_t base = 0;
	unsigned n = 2;

	zero(boot, 8);
	if (!kb->valid)
		return false;
	if (kb->reportID != 0) {
		if (len < 1 || report[0] != kb->reportID)
			return false;
		base = 8;
	}
	if (kb->hasModifiers)
		boot[0] = (uint8_t)reportBits(report, len, base + kb->modBitOffset, 8);
	if (kb->hasArray) {
		for (uint32_t k = 0; k < kb->arrayCount && n < 8; k++) {
			uint32_t u = reportBits(report, len,
			    base + kb->arrayBitOffset + k * kb->arrayBitSize, kb->arrayBitSize);

			if (u >= KEY_FIRST_MODIFIER && u <= KEY_LAST_MODIFIER)
				continue;	/* some keyboards repeat modifiers here */
			if (u != 0 && u < 0x100)
				boot[n++] = (uint8_t)u;
		}
	}
	if (kb->hasBitmap) {
		for (uint32_t k = 0; k < kb->bitmapCount; k++) {
			uint32_t u = kb->bitmapFirstUsage + k;

			if (u < 0x04 || !reportBits(report, len, base + kb->bitmapBitOffset + k, 1))
				continue;
			if (n == 8) {
				/* More keys than the boot report holds: rollover. */
				for (n = 2; n < 8; n++)
					boot[n] = KEY_ERROR_ROLLOVER;
				break;
			}
			boot[n++] = (uint8_t)u;
		}
	}
	return true;
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
