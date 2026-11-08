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
 * Navi 23's display engine (DCN 3.0.2), read only: what the firmware left
 * on each of the four pipes, published as MinkarDisplayPipes for the
 * driver that will take the display over (Docs/GPU-RDNA2.md, R0). Each
 * pipe is a timing generator (OTG) and a plane reader (HUBP); the plane's
 * address is in the engine's own address space, where VRAM starts at
 * DCN_VM_FB_LOCATION_BASE.
 */

#include <IOKit/IOLib.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include "MinkarAMDGPU.h"
#include "amd_dcn302_regs.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDDCN: " fmt "\n", ##__VA_ARGS__)

#define FIELD(v, reg, f)	(((v) & DCN302_##reg##__##f##_MASK) >> DCN302_##reg##__##f##__SHIFT)

static void
setNumber(OSDictionary *d, const char *key, uint64_t value, unsigned bits)
{
	OSNumber *n = OSNumber::withNumber((unsigned long long)value, bits);

	if (n != NULL) {
		d->setObject(key, n);
		n->release();
	}
}

void
MinkarAMDGPU::describeDisplayDCN302()
{
	OSArray *pipes = OSArray::withCapacity(DCN302_PIPES);
	uint64_t fbBase = (uint64_t)FIELD(readReg(dcn302_DCN_VM_FB_LOCATION_BASE),
	    DCN_VM_FB_LOCATION_BASE, FB_BASE) << 24;
	uint64_t fbTop = (uint64_t)FIELD(readReg(dcn302_DCN_VM_FB_LOCATION_TOP),
	    DCN_VM_FB_LOCATION_TOP, FB_TOP) << 24;

	if (pipes == NULL)
		return;
	setProperty("MinkarDisplayVRAMBase", (unsigned long long)fbBase, 64);
	setProperty("MinkarDisplayVRAMTop", (unsigned long long)fbTop, 64);
	for (unsigned p = 0; p < DCN302_PIPES; p++) {
		OSDictionary *d = OSDictionary::withCapacity(12);
		uint32_t otg = readReg(dcn302_OTG_OTG_CONTROL[p]);
		uint32_t hb = readReg(dcn302_OTG_OTG_H_BLANK_START_END[p]);
		uint32_t vb = readReg(dcn302_OTG_OTG_V_BLANK_START_END[p]);
		uint32_t htotal = FIELD(readReg(dcn302_OTG_OTG_H_TOTAL[p]), OTG_OTG_H_TOTAL, OTG_H_TOTAL) + 1;
		uint32_t vtotal = FIELD(readReg(dcn302_OTG_OTG_V_TOTAL[p]), OTG_OTG_V_TOTAL, OTG_V_TOTAL) + 1;
		/* Active: from the end of the blank to its start. */
		uint32_t hactive = FIELD(hb, OTG_OTG_H_BLANK_START_END, OTG_H_BLANK_START) -
		    FIELD(hb, OTG_OTG_H_BLANK_START_END, OTG_H_BLANK_END);
		uint32_t vactive = FIELD(vb, OTG_OTG_V_BLANK_START_END, OTG_V_BLANK_START) -
		    FIELD(vb, OTG_OTG_V_BLANK_START_END, OTG_V_BLANK_END);
		uint32_t hubp = readReg(dcn302_HUBP_DCHUBP_CNTL[p]);
		uint32_t vp = readReg(dcn302_HUBP_DCSURF_PRI_VIEWPORT_DIMENSION[p]);
		uint32_t format = FIELD(readReg(dcn302_HUBP_DCSURF_SURFACE_CONFIG[p]),
		    HUBP_DCSURF_SURFACE_CONFIG, SURFACE_PIXEL_FORMAT);
		/* As programmed. amdgpu's dc writes the pitch less one; on PC 1
		 * the firmware's GOP, which reports 3456 pixels a row, leaves
		 * 3456 here: R1, which draws in its own buffer, settles it. */
		uint32_t pitch = FIELD(readReg(dcn302_HUBPREQ_DCSURF_SURFACE_PITCH[p]),
		    HUBPREQ_DCSURF_SURFACE_PITCH, PITCH);
		uint64_t address = readReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS[p]) |
		    (uint64_t)FIELD(readReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH[p]),
		    HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH, PRIMARY_SURFACE_ADDRESS_HIGH) << 32;
		bool on = FIELD(otg, OTG_OTG_CONTROL, OTG_MASTER_EN);
		bool shown = on && !FIELD(hubp, HUBP_DCHUBP_CNTL, HUBP_BLANK_EN);

		if (d == NULL)
			continue;
		setNumber(d, "Pipe", p, 32);
		d->setObject("On", on ? kOSBooleanTrue : kOSBooleanFalse);
		d->setObject("PlaneShown", shown ? kOSBooleanTrue : kOSBooleanFalse);
		setNumber(d, "HTotal", htotal, 32);
		setNumber(d, "VTotal", vtotal, 32);
		setNumber(d, "HActive", hactive, 32);
		setNumber(d, "VActive", vactive, 32);
		setNumber(d, "ViewportWidth", FIELD(vp, HUBP_DCSURF_PRI_VIEWPORT_DIMENSION, PRI_VIEWPORT_WIDTH), 32);
		setNumber(d, "ViewportHeight", FIELD(vp, HUBP_DCSURF_PRI_VIEWPORT_DIMENSION, PRI_VIEWPORT_HEIGHT), 32);
		setNumber(d, "PitchField", pitch, 32);
		setNumber(d, "PixelFormat", format, 32);
		setNumber(d, "SurfaceAddress", address, 64);
		/* Its offset in VRAM, when it is there. */
		if (address >= fbBase && address < fbTop)
			setNumber(d, "SurfaceVRAMOffset", address - fbBase, 64);
		pipes->setObject(d);
		d->release();
		if (on)
			LOG("pipe %u: %ux%u of %ux%u, plane %s, pitch field %u, format %u, at 0x%llx "
			    "(VRAM 0x%llx..0x%llx)", p, hactive, vactive, htotal, vtotal,
			    shown ? "shown" : "blanked", pitch, format, (unsigned long long)address,
			    (unsigned long long)fbBase, (unsigned long long)fbTop);
	}
	setProperty("MinkarDisplayPipes", pipes);
	pipes->release();
}
