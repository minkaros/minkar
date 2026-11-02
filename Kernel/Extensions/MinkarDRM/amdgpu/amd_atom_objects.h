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
 * A VBIOS's display objects: its connectors, the AUX (DDC) channel and the
 * hot-plug detect pin of each, and the encoder that drives it, read from
 * the Object_Header, GPIO_Pin_LUT and display path tables as amdgpu's
 * amdgpu_atombios_get_connector_info_from_object_table does (MIT layouts
 * from atombios.h). Plain C over the image's bytes, for the driver and
 * host tests; no table is run.
 */

#ifndef _MINKAR_AMD_ATOM_OBJECTS_H
#define _MINKAR_AMD_ATOM_OBJECTS_H

#include <stdint.h>
#include <stddef.h>

#define kAtomMaxConnectors	8

/* Data tables, by their index in the master list (atombios.h). */
#define kAtomDataGPIOPinLUT	12
#define kAtomDataObjectHeader	22

/* Object IDs: type, enum and id (ObjectID.h). */
#define ATOM_OBJECT_TYPE(id)	(((id) >> 12) & 0xf)
#define ATOM_OBJECT_ENUM(id)	(((id) >> 8) & 0x7)
#define ATOM_OBJECT_ID(id)	((id) & 0xff)
#define kAtomObjectTypeConnector	3
#define kAtomConnectorDisplayPort	19
#define kAtomConnectorEDP		20

struct atom_connector {
	uint16_t object;	/* the connector's object ID */
	uint16_t encoder;	/* the first encoder on its display path */
	uint16_t deviceTag;	/* the path's ATOM_DEVICE_* bit */
	uint8_t  aux;		/* its I2C record's id: the AUX channel */
	int8_t   hpd;		/* 0 to 5 (HPD1 to HPD6), -1 without one */
};

static inline uint16_t
atom_u16(const uint8_t *b, size_t at)
{
	return (uint16_t)(b[at] | (b[at + 1] << 8));
}

/* A data table's offset in the image, 0 if absent or out of it. */
static inline size_t
atom_data_table(const uint8_t *bios, size_t size, unsigned index)
{
	if (size < 0x4a)
		return 0;
	size_t rom = atom_u16(bios, 0x48);
	if (rom + 0x22 > size)
		return 0;
	size_t master = atom_u16(bios, rom + 0x20);
	if (master + 4 + 2 * index + 2 > size)
		return 0;
	size_t t = atom_u16(bios, master + 4 + 2 * index);
	return t != 0 && t + 4 <= size ? t : 0;
}

/*
 * The HPD pin a GPIO pin ID stands for: on DCE 6 the HPD pins are bits 0,
 * 8, 16, 24, 26 and 28 of DC_GPIO_HPD_A (amdgpu_atombios_get_hpd_info_from_
 * gpio). -1 if it is none of them.
 */
#define kSIDCGPIOHPDA	0x196d
static inline int
atom_hpd_from_gpio(const uint8_t *bios, size_t size, uint8_t gpioID, uint16_t hpdRegister)
{
	static const uint8_t shifts[6] = { 0, 8, 16, 24, 26, 28 };
	size_t lut = atom_data_table(bios, size, kAtomDataGPIOPinLUT);

	if (lut == 0)
		return -1;
	size_t end = lut + atom_u16(bios, lut);
	for (size_t p = lut + 4; p + 4 <= end && p + 4 <= size; p += 4) {
		if (bios[p + 3] != gpioID)
			continue;
		if (atom_u16(bios, p) != hpdRegister)
			return -1;
		for (int i = 0; i < 6; i++)
			if (bios[p + 2] == shifts[i])
				return i;
		return -1;
	}
	return -1;
}

/* The connectors, in the Object_Header's order; how many. */
static inline unsigned
atom_connectors(const uint8_t *bios, size_t size, uint16_t hpdRegister,
    struct atom_connector *out, unsigned max)
{
	size_t oh = atom_data_table(bios, size, kAtomDataObjectHeader);
	unsigned n = 0;

	if (oh == 0 || oh + 16 > size)
		return 0;
	size_t conn = oh + atom_u16(bios, oh + 6), path = oh + atom_u16(bios, oh + 14);
	if (conn + 4 > size || path + 4 > size)
		return 0;
	for (unsigned i = 0; i < bios[conn] && n < max; i++) {
		size_t o = conn + 4 + 8 * (size_t)i;
		if (o + 8 > size)
			break;
		uint16_t id = atom_u16(bios, o);
		if (ATOM_OBJECT_TYPE(id) != kAtomObjectTypeConnector)
			continue;
		struct atom_connector c = { id, 0, 0, 0, -1 };
		/* Its records: I2C (the AUX channel) and HPD. */
		for (size_t r = oh + atom_u16(bios, o + 4); r + 2 <= size && bios[r] != 0xff &&
		    bios[r + 1] != 0; r += bios[r + 1]) {
			if (bios[r] == 1 && r + 3 <= size)
				c.aux = bios[r + 2];
			else if (bios[r] == 2 && r + 3 <= size)
				c.hpd = (int8_t)atom_hpd_from_gpio(bios, size, bios[r + 2], hpdRegister);
		}
		/* Its display path: the device tag and the first encoder. */
		size_t p = path + 4;
		for (unsigned k = 0; k < bios[path] && p + 8 <= size; k++) {
			uint16_t tag = atom_u16(bios, p), psize = atom_u16(bios, p + 2);
			if (atom_u16(bios, p + 4) == id && psize >= 10 && p + 10 <= size) {
				c.deviceTag = tag;
				c.encoder = atom_u16(bios, p + 8);
				break;
			}
			if (psize < 8)
				break;
			p += psize;
		}
		if (c.deviceTag != 0)	/* not on any display path: not wired */
			out[n++] = c;
	}
	return n;
}

#endif
