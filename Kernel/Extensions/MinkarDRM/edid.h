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
 * A display's modes from its EDID: the detailed timing descriptors of the
 * base block and of a CEA-861 extension, the CEA video codes (VICs) of
 * the extension's video data block, and the base block's established and
 * standard timings, for the modes whose timings are listed here (the
 * common progressive CEA-861 and VESA DMT ones; others are left out). Plain C over the EDID's bytes, for the
 * drivers and host tests. Written from the EDID 1.3/1.4 and CEA-861
 * layouts.
 */

#ifndef _MINKAR_EDID_H
#define _MINKAR_EDID_H

#include <stdint.h>
#include <stddef.h>

struct edid_mode {
	uint32_t clock;			/* pixel clock, kHz */
	uint16_t hactive, hfront, hsync, hback;
	uint16_t vactive, vfront, vsync, vback;
	uint8_t  hpositive, vpositive;	/* sync polarities */
	uint8_t  preferred;		/* the first detailed timing */
	uint8_t  vic;			/* 0, or its CEA video code */
};

static inline uint32_t
edid_htotal(const struct edid_mode *m)
{
	return (uint32_t)m->hactive + m->hfront + m->hsync + m->hback;
}

static inline uint32_t
edid_vtotal(const struct edid_mode *m)
{
	return (uint32_t)m->vactive + m->vfront + m->vsync + m->vback;
}

/* The refresh rate in mHz. */
static inline uint32_t
edid_refresh_mhz(const struct edid_mode *m)
{
	uint64_t total = (uint64_t)edid_htotal(m) * edid_vtotal(m);
	return total ? (uint32_t)(((uint64_t)m->clock * 1000000ULL + total / 2) / total) : 0;
}

/* An 18-byte detailed timing descriptor; 0 if it is not a (progressive)
 * timing. */
static inline int
edid_dtd(const uint8_t *d, struct edid_mode *m)
{
	uint32_t clock = (uint32_t)(d[0] | (d[1] << 8)) * 10;
	if (clock == 0 || (d[17] & 0x80))	/* a display descriptor, or interlaced */
		return 0;
	uint16_t hblank = (uint16_t)(d[3] | ((d[4] & 0x0f) << 8));
	uint16_t vblank = (uint16_t)(d[6] | ((d[7] & 0x0f) << 8));
	m->clock = clock;
	m->hactive = (uint16_t)(d[2] | ((d[4] & 0xf0) << 4));
	m->vactive = (uint16_t)(d[5] | ((d[7] & 0xf0) << 4));
	m->hfront = (uint16_t)(d[8] | ((d[11] & 0xc0) << 2));
	m->hsync = (uint16_t)(d[9] | ((d[11] & 0x30) << 4));
	m->vfront = (uint16_t)((d[10] >> 4) | ((d[11] & 0x0c) << 2));
	m->vsync = (uint16_t)((d[10] & 0x0f) | ((d[11] & 0x03) << 4));
	if (m->hfront + m->hsync > hblank || m->vfront + m->vsync > vblank)
		return 0;
	m->hback = (uint16_t)(hblank - m->hfront - m->hsync);
	m->vback = (uint16_t)(vblank - m->vfront - m->vsync);
	/* Digital separate sync: bit 2 vertical, bit 1 horizontal polarity. */
	m->hpositive = (d[17] & 0x18) == 0x18 ? (d[17] >> 1) & 1 : 0;
	m->vpositive = (d[17] & 0x18) == 0x18 ? (d[17] >> 2) & 1 : 0;
	m->preferred = 0;
	m->vic = 0;
	return 1;
}

/* CEA-861 timings of the common progressive video codes. */
static const struct { uint8_t vic; struct edid_mode m; } edid_cea_modes[] = {
	{ 1,  { 25175,  640, 16,  96,  48, 480,  10, 2, 33, 0, 0, 0, 1 } },
	{ 2,  { 27000,  720, 16,  62,  60, 480,   9, 6, 30, 0, 0, 0, 2 } },
	{ 4,  { 74250, 1280, 110, 40, 220, 720,   5, 5, 20, 1, 1, 0, 4 } },
	{ 16, { 148500, 1920, 88, 44, 148, 1080,  4, 5, 36, 1, 1, 0, 16 } },
	{ 17, { 27000,  720, 12,  64,  68, 576,   5, 5, 39, 0, 0, 0, 17 } },
	{ 19, { 74250, 1280, 440, 40, 220, 720,   5, 5, 20, 1, 1, 0, 19 } },
	{ 31, { 148500, 1920, 528, 44, 148, 1080, 4, 5, 36, 1, 1, 0, 31 } },
	{ 32, { 74250, 1920, 638, 44, 148, 1080,  4, 5, 36, 1, 1, 0, 32 } },
	{ 33, { 74250, 1920, 528, 44, 148, 1080,  4, 5, 36, 1, 1, 0, 33 } },
	{ 34, { 74250, 1920,  88, 44, 148, 1080,  4, 5, 36, 1, 1, 0, 34 } },
};

/* VESA DMT timings of the common modes, with their refresh in Hz. */
static const struct { uint8_t hz; struct edid_mode m; } edid_dmt_modes[] = {
	{ 60, { 25175,   640,  16,  96,  48,  480, 10, 2, 33, 0, 0, 0, 0 } },
	{ 60, { 40000,   800,  40, 128,  88,  600,  1, 4, 23, 1, 1, 0, 0 } },
	{ 60, { 65000,  1024,  24, 136, 160,  768,  3, 6, 29, 0, 0, 0, 0 } },
	{ 75, { 108000, 1152,  64, 128, 256,  864,  1, 3, 32, 1, 1, 0, 0 } },
	{ 60, { 83500,  1280,  72, 128, 200,  800,  3, 6, 22, 0, 1, 0, 0 } },
	{ 60, { 108000, 1280,  96, 112, 312,  960,  1, 3, 36, 1, 1, 0, 0 } },
	{ 60, { 108000, 1280,  48, 112, 248, 1024,  1, 3, 38, 1, 1, 0, 0 } },
	{ 60, { 106500, 1440,  80, 152, 232,  900,  3, 6, 25, 0, 1, 0, 0 } },
	{ 60, { 108000, 1600,  24,  80,  96,  900,  1, 3, 96, 1, 1, 0, 0 } },
	{ 60, { 146250, 1680, 104, 176, 280, 1050,  3, 6, 30, 0, 1, 0, 0 } },
	{ 60, { 148500, 1920,  88,  44, 148, 1080,  4, 5, 36, 1, 1, 0, 0 } },
};

static inline const struct edid_mode *
edid_dmt(unsigned h, unsigned v, unsigned hz)
{
	for (size_t i = 0; i < sizeof(edid_dmt_modes) / sizeof(edid_dmt_modes[0]); i++)
		if (edid_dmt_modes[i].m.hactive == h && edid_dmt_modes[i].m.vactive == v &&
		    edid_dmt_modes[i].hz == hz)
			return &edid_dmt_modes[i].m;
	return NULL;
}

static inline int
edid_has(const struct edid_mode *modes, unsigned n, const struct edid_mode *m)
{
	for (unsigned i = 0; i < n; i++)
		if (modes[i].clock == m->clock && modes[i].hactive == m->hactive &&
		    modes[i].vactive == m->vactive && edid_htotal(&modes[i]) == edid_htotal(m) &&
		    edid_vtotal(&modes[i]) == edid_vtotal(m))
			return 1;
	return 0;
}

/*
 * The modes of an EDID of bytes (128 per block, the base block's header
 * and checksum checked; a bad extension is skipped), at most max; how
 * many. The first detailed timing, the display's preferred mode, comes
 * first.
 */
static inline unsigned
edid_modes(const uint8_t *e, size_t bytes, struct edid_mode *out, unsigned max)
{
	static const uint8_t header[8] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 };
	unsigned n = 0;
	uint8_t sum = 0;
	struct edid_mode m;

	if (bytes < 128)
		return 0;
	for (int i = 0; i < 8; i++)
		if (e[i] != header[i])
			return 0;
	for (int i = 0; i < 128; i++)
		sum = (uint8_t)(sum + e[i]);
	if (sum != 0)
		return 0;
	for (int i = 0; i < 4 && n < max; i++)
		if (edid_dtd(e + 54 + 18 * i, &m) && !edid_has(out, n, &m)) {
			m.preferred = n == 0;
			out[n++] = m;
		}
	/* Established timings (bytes 35-36) and standard timings (38-53). */
	static const struct { uint8_t byte, bit; uint16_t h, v; uint8_t hz; } established[] = {
		{ 35, 5, 640, 480, 60 }, { 35, 0, 800, 600, 60 }, { 36, 3, 1024, 768, 60 },
	};
	for (size_t i = 0; i < sizeof(established) / sizeof(established[0]) && n < max; i++) {
		const struct edid_mode *d;
		if ((e[established[i].byte] >> established[i].bit) & 1 &&
		    (d = edid_dmt(established[i].h, established[i].v, established[i].hz)) != NULL &&
		    !edid_has(out, n, d))
			out[n++] = *d;
	}
	for (int i = 0; i < 8 && n < max; i++) {
		uint8_t b1 = e[38 + 2 * i], b2 = e[39 + 2 * i];
		if ((b1 == 0x01 && b2 == 0x01) || b1 == 0)
			continue;
		unsigned h = (b1 + 31u) * 8, v;
		switch (b2 >> 6) {
		case 0: v = h * 10 / 16; break;
		case 1: v = h * 3 / 4; break;
		case 2: v = h * 4 / 5; break;
		default: v = h * 9 / 16; break;
		}
		const struct edid_mode *d = edid_dmt(h, v, (b2 & 0x3f) + 60u);
		if (d != NULL && !edid_has(out, n, d))
			out[n++] = *d;
	}
	/* CEA-861 extensions: their detailed timings and video codes. */
	for (size_t b = 128; b + 128 <= bytes && b / 128 <= e[126]; b += 128) {
		const uint8_t *x = e + b;
		sum = 0;
		for (int i = 0; i < 128; i++)
			sum = (uint8_t)(sum + x[i]);
		if (x[0] != 0x02 || sum != 0)
			continue;
		unsigned dtds = x[2];
		if (dtds < 4 || dtds > 127)
			continue;
		/* The data blocks, before the detailed timings. */
		for (unsigned p = 4; p < dtds; ) {
			unsigned tag = x[p] >> 5, len = x[p] & 0x1f;
			if (p + 1 + len > dtds)
				break;
			if (tag == 2)	/* video data block: one code a byte */
				for (unsigned k = 0; k < len && n < max; k++) {
					uint8_t vic = x[p + 1 + k] & 0x7f;
					for (size_t c = 0; c < sizeof(edid_cea_modes) / sizeof(edid_cea_modes[0]); c++)
						if (edid_cea_modes[c].vic == vic && !edid_has(out, n, &edid_cea_modes[c].m))
							out[n++] = edid_cea_modes[c].m;
				}
			p += 1 + len;
		}
		for (unsigned p = dtds; p + 18 <= 127 && n < max; p += 18)
			if (edid_dtd(x + p, &m) && !edid_has(out, n, &m))
				out[n++] = m;
	}
	return n;
}

/*
 * The refresh rates of one size, as indices into modes, fastest first: the
 * modes of width x height whose clock a link of limitKHz carries, one for
 * each rate (rates within 0.5 Hz are the same; the first listed wins, so
 * the preferred mode stands for its rate), at most max; how many.
 */
static inline unsigned
edid_rates(const struct edid_mode *modes, unsigned n, unsigned width, unsigned height,
    uint32_t limitKHz, unsigned *out, unsigned max)
{
	unsigned count = 0;

	for (unsigned i = 0; i < n && count < max; i++) {
		if (modes[i].hactive != width || modes[i].vactive != height ||
		    modes[i].clock > limitKHz)
			continue;
		uint32_t hz = edid_refresh_mhz(&modes[i]);
		int same = 0;
		for (unsigned k = 0; k < count && !same; k++) {
			uint32_t other = edid_refresh_mhz(&modes[out[k]]);
			same = (hz > other ? hz - other : other - hz) <= 500;
		}
		if (!same)
			out[count++] = i;
	}
	for (unsigned i = 1; i < count; i++)	/* fastest first */
		for (unsigned k = i; k > 0 &&
		    edid_refresh_mhz(&modes[out[k]]) > edid_refresh_mhz(&modes[out[k - 1]]); k--) {
			unsigned t = out[k];
			out[k] = out[k - 1];
			out[k - 1] = t;
		}
	return count;
}

#endif
