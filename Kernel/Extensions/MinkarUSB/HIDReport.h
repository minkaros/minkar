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
 * A small parser of HID report descriptors (Device Class Definition for
 * HID 1.11, section 6.2.2), enough for pointing devices: it finds the X,
 * Y and wheel fields and the buttons of the first input report that has
 * them, and reads their values out of a report. Plain C, no allocation.
 */

#ifndef _HIDREPORT_H
#define _HIDREPORT_H

#include <stdint.h>
#include <stdbool.h>

#define HID_MAX_BUTTONS		8

struct HIDField {
	bool		present;
	uint32_t	bitOffset;	/* after the report ID byte, if any */
	uint32_t	bitSize;
	int32_t		logicalMin;
	int32_t		logicalMax;
	bool		relative;
};

struct HIDPointerLayout {
	bool		valid;
	uint8_t		reportID;	/* 0: the device uses no report IDs */
	struct HIDField	x, y, wheel;
	struct HIDField	buttons[HID_MAX_BUTTONS];
	unsigned	buttonCount;
};

/* False when the descriptor has no input report with X and Y. */
bool HIDParsePointer(const uint8_t *desc, uint32_t len, struct HIDPointerLayout *out);

/* The field's value in a report (the report ID byte, if any, included). */
int32_t HIDFieldValue(const struct HIDPointerLayout *layout, const struct HIDField *f,
    const uint8_t *report, uint32_t len);

#endif /* !_HIDREPORT_H */
