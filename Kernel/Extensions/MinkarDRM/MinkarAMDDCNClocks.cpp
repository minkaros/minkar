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
 * Navi 23's display clocks (DCN 3.0.2; Docs/GPU-RDNA2.md, R2): DISPCLK and
 * DPPCLK, which a faster mode needs raised before its pixel clock is set.
 * As amdgpu's dcn30_clk_mgr.c (MIT) raises them: a hard minimum asked of
 * the SMU through its display (DAL) mailbox, then the DENTIST's dividers
 * of the display's VCO (3650 MHz, amdgpu's default) brought to it. The
 * SMU runs from boot: no firmware of ours is needed.
 */

#include <IOKit/IOLib.h>
#include "MinkarAMDFramebuffer.h"
#include "amd_dcn302_regs.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDFramebuffer: " fmt "\n", ##__VA_ARGS__)

#define M(reg, f)	DCN302_##reg##__##f##_MASK
#define S(reg, f)	DCN302_##reg##__##f##__SHIFT

/* The SMU's DAL mailbox (dcn30_clk_mgr_smu_msg.c) and its messages
 * (dalsmc.h); the clocks (dcn30_smu11_driver_if.h). */
enum {
	kDALResultOK = 0x1,
	kDALSetHardMinByFreq = 0x9,
	kPPCLKDispclk = 9, kPPCLKPixclk = 10,	/* PIXCLK: the DPP clock */
	kDentistVCOKHz = 3650000,
};

/* A message on a mailbox of the SMU: message, argument and response
 * registers; 0 if it does not answer within a second. */
static uint32_t
smuMessage(MinkarAMDFramebuffer *fb, uint32_t msgReg, uint32_t argReg, uint32_t respReg,
    uint32_t message, uint32_t argument, uint32_t *reply)
{
	for (int i = 0; i < 10000 && fb->readReg(respReg) == 0; i++)	/* the last one answered */
		IODelay(10);
	fb->writeReg(respReg, 0);
	fb->writeReg(argReg, argument);
	fb->writeReg(msgReg, message);
	uint32_t result = 0;
	for (int i = 0; i < 100000 && (result = fb->readReg(respReg)) == 0; i++)
		IODelay(10);
	if (reply != NULL)
		*reply = fb->readReg(argReg);
	return result;
}

/* Which mailbox answers, for bring-up: their registers as they are, and
 * the SMU's version asked on each (MP1's C2PMSG_66/82/90, and the DAL
 * ones, C2PMSG_74/51/52, at Navi 21's base and Navi 23's). */
void
MinkarAMDFramebuffer::probeSMU()
{
	static const struct { const char *name; uint32_t msg, arg, resp; } boxes[] = {
		{ "main@0x16000", 0x16282, 0x16292, 0x1629a }, { "main@0x16200", 0x16482, 0x16492, 0x1649a },
		{ "dal@0x16000", 0x1628a, 0x16273, 0x16274 }, { "dal@0x16200", 0x1648a, 0x16473, 0x16474 },
	};
	for (unsigned i = 0; i < sizeof(boxes) / sizeof(boxes[0]); i++) {
		uint32_t m = readReg(boxes[i].msg), a = readReg(boxes[i].arg), r = readReg(boxes[i].resp);
		LOG("SMU %s: message 0x%x, argument 0x%x, response 0x%x", boxes[i].name, m, a, r);
	}
	/* Asked only where the response register says a mailbox is there
	 * (the last message answered: 1): writing into another block's
	 * registers is not safe. */
	for (unsigned i = 0; i < sizeof(boxes) / sizeof(boxes[0]); i++) {
		if (readReg(boxes[i].resp) != 0x1)
			continue;
		uint32_t version = 0;
		uint32_t result = smuMessage(this, boxes[i].msg, boxes[i].arg, boxes[i].resp, 0x2, 0, &version);
		LOG("SMU %s: GetSmuVersion: result 0x%x, 0x%08x", boxes[i].name, result, version);
		if (result == 0x1 && i >= 2 && fSMUDALBase == 0)
			fSMUDALBase = boxes[i].msg;
	}
}

bool
MinkarAMDFramebuffer::smuDAL(uint32_t message, uint32_t argument, uint32_t *reply)
{
	if (fSMUDALBase == 0)
		return false;
	uint32_t msg = fSMUDALBase, arg = fSMUDALBase - 0x28a + 0x273, resp = arg + 1;
	uint32_t result = smuMessage(this, msg, arg, resp, message, argument, reply);
	if (result != kDALResultOK) {
		LOG("SMU: message 0x%x (0x%08x): result 0x%x", message, argument, result);
		return false;
	}
	return true;
}

/* amdgpu's dentist_get_did_from_divider: the divider in quarters. */
static uint32_t
dentistDID(uint32_t divider)
{
	if (divider < 64)
		return divider < 8 ? 0x08 : 0x08 + (divider - 8);
	if (divider < 128)
		return 0x40 + (divider - 64) / 2;
	if (divider < 248)
		return 0x60 + (divider - 128) / 4;
	uint32_t did = 0x7e + (divider - 248) / 264;
	return did > 0x7f ? 0x7f : did;
}

static uint32_t
dentistKHz(uint32_t did)
{
	uint32_t quarters = did < 0x40 ? 8 + (did - 0x08) : did < 0x60 ? 64 + (did - 0x40) * 2 :
	    did < 0x7e ? 128 + (did - 0x60) * 4 : 248 + (did - 0x7e) * 264;
	return (uint32_t)((uint64_t)kDentistVCOKHz * 4 / quarters);
}

/*
 * DISPCLK and DPPCLK raised (never lowered) to what a pixel clock of
 * pixelKHz needs: the pixel clock and a tenth more. True when they are
 * there, or were already.
 */
bool
MinkarAMDFramebuffer::raiseDisplayClocksDCN(uint32_t pixelKHz)
{
	uint32_t cntl = readReg(dcn302_DENTIST_DISPCLK_CNTL);
	uint32_t dispDID = (cntl & M(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER)) >>
	    S(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER);
	uint32_t dppDID = (cntl & M(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER)) >>
	    S(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER);
	uint32_t want = pixelKHz + pixelKHz / 10;
	uint32_t did = dentistDID((uint32_t)((uint64_t)kDentistVCOKHz * 4 / want));
	uint32_t khz = dentistKHz(did);

	LOG("clocks: DISPCLK %u kHz, DPPCLK %u kHz; %u kHz needs %u", dentistKHz(dispDID),
	    dentistKHz(dppDID), pixelKHz, khz);
	if (dentistKHz(dispDID) >= want && dentistKHz(dppDID) >= want)
		return true;
	uint32_t mhz = (khz + 999) / 1000, got = 0;
	if (!smuDAL(kDALSetHardMinByFreq, (kPPCLKPixclk << 16) | mhz, &got))
		return false;
	LOG("clocks: SMU DPPCLK minimum %u MHz: %u MHz", mhz, got);
	if (!smuDAL(kDALSetHardMinByFreq, (kPPCLKDispclk << 16) | mhz, &got))
		return false;
	LOG("clocks: SMU DISPCLK minimum %u MHz: %u MHz", mhz, got);
	/* The refclk raised before the DTO (DPP's DTO is off here). */
	if (did < dispDID) {
		cntl = readReg(dcn302_DENTIST_DISPCLK_CNTL);
		writeReg(dcn302_DENTIST_DISPCLK_CNTL, (cntl & ~M(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER)) |
		    (did << S(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER)));
		for (int i = 0; i < 2000 && !(readReg(dcn302_DENTIST_DISPCLK_CNTL) &
		    M(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_CHG_DONE)); i++)
			IODelay(50);
	}
	if (did < dppDID) {
		cntl = readReg(dcn302_DENTIST_DISPCLK_CNTL);
		writeReg(dcn302_DENTIST_DISPCLK_CNTL, (cntl & ~M(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER)) |
		    (did << S(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER)));
		for (int i = 0; i < 100 && !(readReg(dcn302_DENTIST_DISPCLK_CNTL) &
		    M(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_CHG_DONE)); i++)
			IODelay(5);
	}
	cntl = readReg(dcn302_DENTIST_DISPCLK_CNTL);
	LOG("clocks: DENTIST 0x%08x: DISPCLK %u kHz, DPPCLK %u kHz", cntl,
	    dentistKHz((cntl & M(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER)) >>
	    S(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER)),
	    dentistKHz((cntl & M(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER)) >>
	    S(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER)));
	return true;
}

/* Whether DISPCLK and DPPCLK, as they are, carry a pixel clock of pixelKHz
 * (and a tenth more). */
bool
MinkarAMDFramebuffer::displayClocksCarryDCN(uint32_t pixelKHz)
{
	uint32_t cntl = readReg(dcn302_DENTIST_DISPCLK_CNTL);
	uint32_t want = pixelKHz + pixelKHz / 10;

	return dentistKHz((cntl & M(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER)) >>
	    S(DENTIST_DISPCLK_CNTL, DENTIST_DISPCLK_WDIVIDER)) >= want &&
	    dentistKHz((cntl & M(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER)) >>
	    S(DENTIST_DISPCLK_CNTL, DENTIST_DPPCLK_WDIVIDER)) >= want;
}
