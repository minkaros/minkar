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
 * MinkarAMDFramebuffer on Navi 23 (DCN 3.0.2; Docs/GPU.md, R1): the
 * pipe the firmware's GOP lit keeps its timing, and its plane (HUBP) is
 * pointed at a framebuffer of the driver's own in VRAM. Only the plane's
 * address and pitch change, under the plane's update lock, taken at the
 * next vertical blank (flip type 0). No firmware is needed for this; the
 * other modes, the EDID and hot plug come with R2.
 */

#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <pexpert/pexpert.h>
#include "MinkarAMDFramebuffer.h"
#include "amd_dcn302_regs.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDFramebuffer: " fmt "\n", ##__VA_ARGS__)

#define FIELD(v, reg, f)	(((v) & DCN302_##reg##__##f##_MASK) >> DCN302_##reg##__##f##__SHIFT)

enum {
	kDCNFormatARGB8888 = 8,		/* SURFACE_PIXEL_FORMAT */
	kDCNLinear = 0,			/* SW_MODE */
	kFlipTestFlips = 30,
};

static uint64_t
uptimeNs(void)
{
	uint64_t ns;

	absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
	return ns;
}

/*
 * The pipe that scans out the firmware's framebuffer: its timing on, its
 * plane shown, linear ARGB8888, at the boot framebuffer's place in VRAM.
 */
bool
MinkarAMDFramebuffer::findDisplayDCN(uint64_t bootAddress, UInt32 bootRowBytes)
{
	fVRAMStart = (uint64_t)FIELD(readReg(dcn302_DCN_VM_FB_LOCATION_BASE),
	    DCN_VM_FB_LOCATION_BASE, FB_BASE) << 24;
	for (unsigned p = 0; p < DCN302_PIPES; p++) {
		if (!FIELD(readReg(dcn302_OTG_OTG_CONTROL[p]), OTG_OTG_CONTROL, OTG_MASTER_EN) ||
		    FIELD(readReg(dcn302_HUBP_DCHUBP_CNTL[p]), HUBP_DCHUBP_CNTL, HUBP_BLANK_EN))
			continue;
		uint64_t surface = readReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS[p]) |
		    (uint64_t)FIELD(readReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH[p]),
		    HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH, PRIMARY_SURFACE_ADDRESS_HIGH) << 32;
		if (surface < fVRAMStart || surface - fVRAMStart >= fVRAM->getLength())
			continue;
		uint64_t cpu = fVRAM->getPhysicalAddress() + (surface - fVRAMStart);
		if ((cpu & ~(uint64_t)0xFFF) != (bootAddress & ~(uint64_t)0xFFF))
			continue;
		uint32_t format = FIELD(readReg(dcn302_HUBP_DCSURF_SURFACE_CONFIG[p]),
		    HUBP_DCSURF_SURFACE_CONFIG, SURFACE_PIXEL_FORMAT);
		uint32_t tiling = FIELD(readReg(dcn302_HUBP_DCSURF_TILING_CONFIG[p]),
		    HUBP_DCSURF_TILING_CONFIG, SW_MODE);
		if (format != kDCNFormatARGB8888 || tiling != kDCNLinear) {
			LOG("pipe %u: format %u, tiling mode %u: not a linear ARGB8888 plane", p,
			    format, tiling);
			return false;
		}
		fCRTC = p;
		fFirmwareOffset = surface - fVRAMStart;
		fFirmwarePitch = bootRowBytes / 4;
		fPitchBias = (int)FIELD(readReg(dcn302_HUBPREQ_DCSURF_SURFACE_PITCH[p]),
		    HUBPREQ_DCSURF_SURFACE_PITCH, PITCH) - (int)fFirmwarePitch;
		LOG("pipe %u: the firmware's plane at VRAM 0x%llx, %u pixels a row, pitch field %+d",
		    p, (unsigned long long)fFirmwareOffset, fFirmwarePitch, fPitchBias);
		return true;
	}
	return false;
}

/*
 * The plane at a surface in VRAM of the given pitch, the rest of the plane
 * as the firmware set it. Taken at the next vertical blank.
 */
bool
MinkarAMDFramebuffer::scanOutDCN(uint64_t offset, UInt32 pitchPixels, bool wait)
{
	unsigned p = fCRTC;
	uint64_t address = fVRAMStart + offset;
	uint32_t flip = readReg(dcn302_HUBPREQ_DCSURF_FLIP_CONTROL[p]);
	uint32_t pitch = readReg(dcn302_HUBPREQ_DCSURF_SURFACE_PITCH[p]);

	pitch &= ~(uint32_t)DCN302_HUBPREQ_DCSURF_SURFACE_PITCH__PITCH_MASK;
	pitch |= ((uint32_t)((int)pitchPixels + fPitchBias) << DCN302_HUBPREQ_DCSURF_SURFACE_PITCH__PITCH__SHIFT) &
	    DCN302_HUBPREQ_DCSURF_SURFACE_PITCH__PITCH_MASK;
	flip &= ~(uint32_t)DCN302_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_FLIP_TYPE_MASK;	/* at vblank */

	writeReg(dcn302_HUBPREQ_DCSURF_FLIP_CONTROL[p],
	    flip | DCN302_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_UPDATE_LOCK_MASK);
	writeReg(dcn302_HUBPREQ_DCSURF_SURFACE_PITCH[p], pitch);
	/* The high half first, then the low (amdgpu's hubp2 order). */
	writeReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH[p], (uint32_t)(address >> 32) & 0xFFFF);
	writeReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS[p], (uint32_t)address);
	writeReg(dcn302_HUBPREQ_DCSURF_FLIP_CONTROL[p],
	    flip & ~(uint32_t)DCN302_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_UPDATE_LOCK_MASK);
	return wait ? surfaceTakenDCN(offset, false) : true;
}

/*
 * Taken once no flip is pending and the plane reads from the new address.
 * timed: polled closely, so that where the scan was then (in the vertical
 * blank or not) says where the flip happened.
 */
bool
MinkarAMDFramebuffer::surfaceTakenDCN(uint64_t offset, bool timed)
{
	unsigned p = fCRTC;
	uint64_t address = fVRAMStart + offset;
	uint64_t start = uptimeNs(), now = start;

	while (now - start < 200000000ULL) {
		uint32_t flip = readReg(dcn302_HUBPREQ_DCSURF_FLIP_CONTROL[p]);
		if (!(flip & DCN302_HUBPREQ_DCSURF_FLIP_CONTROL__SURFACE_FLIP_PENDING_MASK)) {
			uint32_t status = readReg(dcn302_OTG_OTG_STATUS[p]);
			uint32_t position = readReg(dcn302_OTG_OTG_STATUS_POSITION[p]);
			uint64_t inuse = readReg(dcn302_HUBPREQ_DCSURF_SURFACE_EARLIEST_INUSE[p]) |
			    (uint64_t)FIELD(readReg(dcn302_HUBPREQ_DCSURF_SURFACE_EARLIEST_INUSE_HIGH[p]),
			    HUBPREQ_DCSURF_SURFACE_EARLIEST_INUSE_HIGH, SURFACE_EARLIEST_INUSE_ADDRESS_HIGH) << 32;
			if (inuse == address) {
				fTakeNs = uptimeNs() - start;
				fTakeLine = FIELD(position, OTG_OTG_STATUS_POSITION, OTG_VERT_COUNT);
				fTakeInBlank = FIELD(status, OTG_OTG_STATUS, OTG_V_BLANK) != 0;
				return true;
			}
		}
		if (timed)
			IODelay(2);
		else
			IOSleep(1);
		now = uptimeNs();
	}
	LOG("pipe %u: the plane did not take VRAM 0x%llx", p, (unsigned long long)offset);
	return false;
}

/* The time a frame starts: the counter seen moving, spun for up to 50 ms
 * (longer than a frame at 24 Hz). 0 if it does not move. */
static uint64_t
frameEdge(MinkarAMDFramebuffer *fb, uint32_t reg, uint32_t mask, uint32_t *count)
{
	uint32_t f = fb->readReg(reg) & mask, now;

	for (int i = 0; i < 50000; i++) {
		now = fb->readReg(reg) & mask;
		if (now != f) {
			*count = now;
			return uptimeNs();
		}
		IODelay(1);
	}
	return 0;
}

/* The refresh rate: the timing generator's frames over a second, from the
 * start of one frame to the start of another, so that the thread being
 * preempted while it waits costs nothing. */
void
MinkarAMDFramebuffer::measureRefreshDCN()
{
	uint32_t reg = dcn302_OTG_OTG_STATUS_FRAME_COUNT[fCRTC];
	uint32_t mask = DCN302_OTG_OTG_STATUS_FRAME_COUNT__OTG_FRAME_COUNT_MASK;
	uint32_t f0, f1;
	uint64_t t0, t1;

	fRefreshMilliHz = 0;
	t0 = frameEdge(this, reg, mask, &f0);
	IOSleep(1000);
	t1 = frameEdge(this, reg, mask, &f1);
	if (t0 == 0 || t1 <= t0 || ((f1 - f0) & mask) == 0)
		return;
	fRefreshMilliHz = (UInt32)((uint64_t)((f1 - f0) & mask) * 1000000000000ULL / (t1 - t0));
}

/* The pipe's state, for minkar-hwcheck and the bring-up reports. */
void
MinkarAMDFramebuffer::publishTimingsDCN()
{
	static const struct { const char *name; const uint32_t *reg; } regs[] = {
		{ "OTG_H_TOTAL", dcn302_OTG_OTG_H_TOTAL },
		{ "OTG_V_TOTAL", dcn302_OTG_OTG_V_TOTAL },
		{ "OTG_H_BLANK_START_END", dcn302_OTG_OTG_H_BLANK_START_END },
		{ "OTG_V_BLANK_START_END", dcn302_OTG_OTG_V_BLANK_START_END },
		{ "DCSURF_SURFACE_CONFIG", dcn302_HUBP_DCSURF_SURFACE_CONFIG },
		{ "DCSURF_TILING_CONFIG", dcn302_HUBP_DCSURF_TILING_CONFIG },
		{ "DCSURF_PRI_VIEWPORT_DIMENSION", dcn302_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION },
		{ "DCSURF_SURFACE_PITCH", dcn302_HUBPREQ_DCSURF_SURFACE_PITCH },
		{ "DCSURF_PRIMARY_SURFACE_ADDRESS", dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS },
		{ "DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH", dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH },
		{ "DCSURF_FLIP_CONTROL", dcn302_HUBPREQ_DCSURF_FLIP_CONTROL },
	};
	OSDictionary *d = OSDictionary::withCapacity(sizeof(regs) / sizeof(regs[0]));

	if (d == NULL)
		return;
	for (unsigned i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
		OSNumber *n = OSNumber::withNumber(readReg(regs[i].reg[fCRTC]), 32);
		if (n != NULL) {
			d->setObject(regs[i].name, n);
			n->release();
		}
	}
	/* The clocks (DCCG), the engine's: pipe 0's where they are a pipe's. */
	static const struct { const char *name; uint32_t reg; } clocks[] = {
		{ "OTG0_PIXEL_RATE_CNTL", dcn302_OTG0_PIXEL_RATE_CNTL },
		{ "OTG0_PHYPLL_PIXEL_RATE_CNTL", dcn302_OTG0_PHYPLL_PIXEL_RATE_CNTL },
		{ "DP_DTO0_PHASE", dcn302_DP_DTO0_PHASE }, { "DP_DTO0_MODULO", dcn302_DP_DTO0_MODULO },
		{ "PHYASYMCLK_CLOCK_CNTL", dcn302_PHYASYMCLK_CLOCK_CNTL },
		{ "DENTIST_DISPCLK_CNTL", dcn302_DENTIST_DISPCLK_CNTL },
		{ "DPPCLK0_DTO_PARAM", dcn302_DPPCLK0_DTO_PARAM }, { "DPPCLK_DTO_CTRL", dcn302_DPPCLK_DTO_CTRL },
		{ "DCCG_GATE_DISABLE_CNTL", dcn302_DCCG_GATE_DISABLE_CNTL },
		{ "DCCG_GATE_DISABLE_CNTL2", dcn302_DCCG_GATE_DISABLE_CNTL2 },
		{ "DISPCLK_FREQ_CHANGE_CNTL", dcn302_DISPCLK_FREQ_CHANGE_CNTL },
		{ "SYMCLK_CGTT_BLK_CTRL_REG", dcn302_SYMCLK_CGTT_BLK_CTRL_REG },
		{ "DCHUBP_CNTL", dcn302_HUBP_DCHUBP_CNTL[0] },
	};
	for (unsigned i = 0; i < sizeof(clocks) / sizeof(clocks[0]); i++) {
		OSNumber *n = OSNumber::withNumber(readReg(clocks[i].reg), 32);
		if (n != NULL) {
			d->setObject(clocks[i].name, n);
			n->release();
		}
	}
	setProperty("MinkarDisplayRegisters", d);
	d->release();
}

/*
 * "minkaramd_pattern=1": before the display is taken, a test pattern at a
 * pitch other than the firmware's, shown twice for eight seconds each:
 * first (green frame) with the pitch field as the firmware's convention
 * gives it, then (blue frame) as amdgpu's dc writes it, the pitch less one.
 * The one whose lines stand straight is the engine's convention; the
 * driver keeps the first.
 */
void
MinkarAMDFramebuffer::showPitchPatternDCN()
{
	UInt32 w = fModes[0].width, h = fModes[0].height;
	UInt32 pitch = pitchFor(w) + 64;
	IOByteCount bytes = (IOByteCount)pitch * 4 * h;
	int bias = fPitchBias;

	if (fOffset + bytes > fVRAM->getLength())
		return;
	IODeviceMemory *range = IODeviceMemory::withSubRange(fVRAM, fOffset, bytes);
	IOMemoryMap *map = range != NULL ? range->map(kIOMapWriteCombineCache) : NULL;
	OSSafeReleaseNULL(range);
	if (map == NULL)
		return;
	uint32_t *px = (uint32_t *)map->getVirtualAddress();

	for (int phase = 0; phase < 2; phase++) {
		uint32_t frame = phase == 0 ? 0xFF00C000 : 0xFF2060FF;
		for (UInt32 y = 0; y < h; y++) {
			uint32_t *row = px + (size_t)y * pitch;
			for (UInt32 x = 0; x < pitch; x++) {
				uint32_t c = 0xFF202020;
				if (x >= w)
					c = 0xFFFF0000;		/* past the row: never shown */
				else if (x < 8 || y < 8 || x >= w - 8 || y >= h - 8)
					c = frame;
				else if (x % 64 == 0 || y % 64 == 0)
					c = 0xFFFFFFFF;
				else if (x == y || x == w - 1 - y)
					c = 0xFFFFFF00;
				row[x] = c;
			}
		}
		fPitchBias = phase == 0 ? bias : -1;
		if (phase == 1 && bias == -1)
			break;		/* the same convention: nothing more to see */
		LOG("test pattern %s: pitch %u pixels, field %d", phase == 0 ? "green" : "blue",
		    pitch, (int)pitch + fPitchBias);
		if (!scanOutDCN(fOffset, pitch, true))
			break;
		IOSleep(8000);
	}
	fPitchBias = bias;
	scanOutDCN(fFirmwareOffset, fFirmwarePitch, true);
	map->release();
}

/*
 * Page flips between the firmware's framebuffer and ours, which hold the
 * same picture: each one taken at a vertical blank, a frame apart.
 * Published as MinkarFlipTest.
 */
void
MinkarAMDFramebuffer::flipTestDCN()
{
	UInt32 pitch = pitchFor(fModes[0].width);
	unsigned taken = 0, inBlank = 0;
	uint64_t total = 0, worst = 0, best = ~0ULL;
	uint32_t lineMin = ~0U, lineMax = 0;

	for (unsigned i = 0; i < kFlipTestFlips; i++) {
		bool theirs = (i % 2) == 0;
		if (!scanOutDCN(theirs ? fFirmwareOffset : fOffset,
		    theirs ? fFirmwarePitch : pitch, false) ||
		    !surfaceTakenDCN(theirs ? fFirmwareOffset : fOffset, true))
			break;
		taken++;
		inBlank += fTakeInBlank;
		total += fTakeNs;
		worst = fTakeNs > worst ? fTakeNs : worst;
		best = fTakeNs < best ? fTakeNs : best;
		lineMin = fTakeLine < lineMin ? fTakeLine : lineMin;
		lineMax = fTakeLine > lineMax ? fTakeLine : lineMax;
	}
	/* Ours, whatever happened. */
	if (taken < kFlipTestFlips)
		scanOutDCN(fOffset, pitch, true);

	OSDictionary *d = OSDictionary::withCapacity(8);
	if (d == NULL)
		return;
	const struct { const char *key; uint64_t value; } numbers[] = {
		{ "Flips", kFlipTestFlips }, { "Taken", taken }, { "InVerticalBlank", inBlank },
		{ "MeanNs", taken ? total / taken : 0 }, { "MinNs", taken ? best : 0 },
		{ "MaxNs", worst }, { "LineMin", taken ? lineMin : 0 }, { "LineMax", lineMax },
	};
	for (unsigned i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
		OSNumber *n = OSNumber::withNumber((unsigned long long)numbers[i].value, 64);
		if (n != NULL) {
			d->setObject(numbers[i].key, n);
			n->release();
		}
	}
	setProperty("MinkarFlipTest", d);
	d->release();
	LOG("flip test: %u of %u taken, %u in the vertical blank, lines %u..%u, "
	    "%llu us a flip (%llu..%llu)", taken, kFlipTestFlips, inBlank,
	    taken ? lineMin : 0, lineMax, (unsigned long long)(taken ? total / taken / 1000 : 0),
	    (unsigned long long)(taken ? best / 1000 : 0), (unsigned long long)(worst / 1000));
}
