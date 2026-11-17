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
 * A lit display's state on Navi 23 (Docs/GPU-RDNA2.md, R2b): when the
 * firmware lights a display, the registers of its pipe, its DIG and the
 * engine's clocks and hub (amd_dcn302_snapshot.h) are saved, under the
 * display's EDID, in /var/db; when the driver must light that display
 * itself (the machine started without it: a KVM switched elsewhere), it
 * sets them again. The values the firmware computed (its request and
 * prefetch timing among them) are kept as they were.
 */

#include <IOKit/IOLib.h>
#include "MinkarAMDFramebuffer.h"
#include "MinkarAMDFirmware.h"
#include "amd_dcn302_snapshot.h"
#include "amd_dcn302_regs.h"
#include <IOKit/IOPlatformExpert.h>

#define kLastStatePath	"/var/db/minkar-display-last.state"

#define LOG(fmt, ...)	IOLog("MinkarAMDFramebuffer: " fmt "\n", ##__VA_ARGS__)

/* The display's key: FNV-1a of its EDID's base block. */
static uint32_t
edidKey(const uint8_t *edid)
{
	uint32_t h = 2166136261u;

	for (unsigned i = 0; i < 128; i++)
		h = (h ^ edid[i]) * 16777619u;
	return h;
}

static void
statePath(char *path, size_t size, const uint8_t *edid)
{
	snprintf(path, size, "/var/db/minkar-display-%08x.state", edidKey(edid));
}

/* The firmware's state, read now (before the driver changes anything). */
void
MinkarAMDFramebuffer::captureStateDCN()
{
	unsigned count = DCN302_SNAPSHOT_PIPE_REGS + 2 * DCN302_SNAPSHOT_DIG_REGS +
	    DCN302_SNAPSHOT_GLOBAL_REGS;
	size_t bytes = sizeof(struct DCNState) + count * 2 * sizeof(uint32_t);

	if (fDIGFE < 0 || fDIGBE < 0 || fEDIDBytes < 128 || fState != NULL)
		return;
	fState = (struct DCNState *)IOMallocZero(bytes);
	if (fState == NULL)
		return;
	fStateBytes = bytes;
	struct DCNState *s = fState;
	s->magic = kDCNStateMagic;
	s->version = 1;
	s->key = edidKey(fEDID);
	s->width = fModes[0].width;
	s->height = fModes[0].height;
	s->pixelKHz = fPixelKHz;
	s->pipe = fCRTC;
	s->digFE = (uint32_t)fDIGFE;
	s->digBE = (uint32_t)fDIGBE;
	s->digMode = fDIGMode;
	s->hpd = fHPDSelect;
	s->phy = (uint32_t)fPHY;
	s->ddc = (uint32_t)fDDC;
	s->connector = fConnectorID;
	s->encoder = fEncoderID;
	uint32_t *p = s->regs;
	for (unsigned i = 0; i < DCN302_SNAPSHOT_GLOBAL_REGS; i++, p += 2) {
		p[0] = dcn302_snapshot_global[i];
		p[1] = readReg(p[0]);
	}
	for (unsigned i = 0; i < DCN302_SNAPSHOT_PIPE_REGS; i++, p += 2) {
		p[0] = dcn302_snapshot_pipe[fCRTC][i];
		p[1] = readReg(p[0]);
	}
	for (unsigned d = 0; d < 2; d++)
		for (unsigned i = 0; i < DCN302_SNAPSHOT_DIG_REGS; i++, p += 2) {
			p[0] = dcn302_snapshot_dig[d == 0 ? fDIGFE : fDIGBE][i];
			p[1] = readReg(p[0]);
		}
	s->count = count;
	LOG("the firmware's display state: %u registers, display %08x", count, s->key);
}

/* Saved once the root file system is up (the SMU's thread calls this),
 * if it is not already there as it is. */
void
MinkarAMDFramebuffer::saveStateDCN()
{
	char path[64];
	uint8_t *old = NULL;
	size_t oldBytes = 0;

	if (fState == NULL)
		return;
	statePath(path, sizeof(path), fEDID);
	if (MinkarAMDReadFile(path, &old, &oldBytes)) {
		bool same = oldBytes == fStateBytes && memcmp(old, fState, oldBytes) == 0;
		MinkarAMDFreeFirmware(old, oldBytes);
		if (same) {
			MinkarAMDWriteFile(kLastStatePath, (const uint8_t *)fState, fStateBytes);
			return;
		}
	}
	LOG("the display state %s %s", path,
	    MinkarAMDWriteFile(path, (const uint8_t *)fState, fStateBytes) ? "saved" : "not saved");
	/* And as the last display's, which a dark start looks for. */
	MinkarAMDWriteFile(kLastStatePath, (const uint8_t *)fState, fStateBytes);
}

/* ---- A dark start -------------------------------------------------- */

bool
MinkarAMDFramebuffer::startDark(IOService *provider)
{
	thread_t thread;

	fDark = true;
	fDarkProvider = provider;
	LOG("no display at start: waiting for the last one saved");
	retain();
	if (kernel_thread_start(&MinkarAMDFramebuffer::darkThread, this, &thread) != KERN_SUCCESS) {
		release();
		return false;
	}
	thread_deallocate(thread);
	return true;
}

/* The saved state set again: the display clocks raised (the SMU), the
 * pipe's pixel clock (the microcontroller), the engine's, the DIG's and
 * the pipe's registers with the timing generator's enable last, then the
 * transmitter on. boot: the framebuffer the state scans out, as the
 * firmware would have described it. */
/* The colour pipeline's look-up tables (degamma, blend gamma, shaper, 3D
 * LUT, output gamma) keep their curves in RAM no register snapshot holds:
 * restored in RAM mode they would shape colours with what that RAM holds
 * after power on. They are bypassed instead (mode 0, the low two bits;
 * dcn_3_0_2_sh_mask.h), which shows the framebuffer's sRGB as it is. */
static const char *const kLUTModeRegs[] = {
	"CM0_CM_GAMCOR_CONTROL", "CM0_CM_BLNDGAM_CONTROL", "CM0_CM_SHAPER_CONTROL",
	"CM0_CM_3DLUT_MODE", "MPCC_OGAM0_MPCC_OGAM_CONTROL",
};

static uint32_t
withLUTsBypassed(unsigned pipe, uint32_t reg, uint32_t value)
{
	for (unsigned j = 0; j < DCN302_SNAPSHOT_PIPE_REGS; j++) {
		if (dcn302_snapshot_pipe[pipe][j] != reg)
			continue;
		for (unsigned k = 0; k < sizeof(kLUTModeRegs) / sizeof(kLUTModeRegs[0]); k++)
			if (strcmp(dcn302_snapshot_pipe_names[j], kLUTModeRegs[k]) == 0) {
				if (value & 0x3)
					LOG("%s: mode %u bypassed", kLUTModeRegs[k], value & 0x3);
				return value & ~0x3u;
			}
		break;
	}
	return value;
}

bool
MinkarAMDFramebuffer::lightFromState(const struct DCNState *s, PE_Video *boot)
{
	const uint32_t *p = s->regs;
	uint32_t otgControl = dcn302_OTG_OTG_CONTROL[s->pipe], otgValue = 0;
	uint32_t hubpControl = dcn302_HUBP_DCHUBP_CNTL[s->pipe], hubpValue = 0;

	if (!raiseDisplayClocksDCN(s->pixelKHz) || !dmcubPixelClock(s->pixelKHz))
		return false;
	/* The pipe's plane reader (domain 2p) and pixel processor (2p + 1)
	 * powered up, as amdgpu's dcn20_hubp_pg_control and dpp_pg_control
	 * do: with no display at start the firmware left them gated. */
	static const uint32_t config[8] = { dcn302_DOMAIN0_PG_CONFIG, dcn302_DOMAIN1_PG_CONFIG,
	    dcn302_DOMAIN2_PG_CONFIG, dcn302_DOMAIN3_PG_CONFIG, dcn302_DOMAIN4_PG_CONFIG,
	    dcn302_DOMAIN5_PG_CONFIG, dcn302_DOMAIN6_PG_CONFIG, dcn302_DOMAIN7_PG_CONFIG };
	static const uint32_t status[8] = { dcn302_DOMAIN0_PG_STATUS, dcn302_DOMAIN1_PG_STATUS,
	    dcn302_DOMAIN2_PG_STATUS, dcn302_DOMAIN3_PG_STATUS, dcn302_DOMAIN4_PG_STATUS,
	    dcn302_DOMAIN5_PG_STATUS, dcn302_DOMAIN6_PG_STATUS, dcn302_DOMAIN7_PG_STATUS };
	writeReg(dcn302_DC_IP_REQUEST_CNTL, DCN302_DC_IP_REQUEST_CNTL__IP_REQUEST_EN_MASK);
	for (unsigned k = 2 * s->pipe; k <= 2 * s->pipe + 1; k++) {
		uint32_t before = readReg(status[k]);
		writeReg(config[k], readReg(config[k]) & ~DCN302_DOMAIN0_PG_CONFIG__DOMAIN0_POWER_GATE_MASK);
		for (int i = 0; i < 1000 && (readReg(status[k]) & DCN302_DOMAIN0_PG_STATUS__DOMAIN0_PGFSM_PWR_STATUS_MASK); i++)
			IODelay(10);
		LOG("power domain %u: status 0x%08x -> 0x%08x", k, before, readReg(status[k]));
	}
	writeReg(dcn302_DC_IP_REQUEST_CNTL, 0);
	for (uint32_t i = 0; i < s->count; i++, p += 2) {
		if (p[0] == otgControl) {
			otgValue = p[1];
			continue;
		}
		if (p[0] == hubpControl)
			hubpValue = p[1];
		writeReg(p[0], withLUTsBypassed(s->pipe, p[0], p[1]));
	}
	writeReg(otgControl, otgValue);
	IOSleep(50);
	/* The plane reader set again once its timing generator runs, and its
	 * underflow cleared: until then it may have stopped fetching. */
	writeReg(hubpControl, (hubpValue & ~DCN302_HUBP_DCHUBP_CNTL__HUBP_UNDERFLOW_STATUS_MASK) |
	    DCN302_HUBP_DCHUBP_CNTL__HUBP_UNDERFLOW_CLEAR_MASK);
	writeReg(hubpControl, hubpValue & ~DCN302_HUBP_DCHUBP_CNTL__HUBP_UNDERFLOW_STATUS_MASK);
	LOG("plane reader: 0x%08x set, 0x%08x read", hubpValue, readReg(hubpControl));

	/* The surface cleared to black before the display shows it: VRAM
	 * nobody drew in since power on is noise. */
	uint64_t shown = readReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS[s->pipe]) |
	    (uint64_t)(readReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH[s->pipe]) & 0xFFFF) << 32;
	uint32_t rowPixels = readReg(dcn302_HUBPREQ_DCSURF_SURFACE_PITCH[s->pipe]) &
	    DCN302_HUBPREQ_DCSURF_SURFACE_PITCH__PITCH_MASK;
	IOByteCount clearBytes = (IOByteCount)rowPixels * 4 * s->height;
	if (shown >= fVRAMStart && shown - fVRAMStart + clearBytes <= fVRAM->getLength()) {
		IODeviceMemory *range = IODeviceMemory::withSubRange(fVRAM, shown - fVRAMStart, clearBytes);
		IOMemoryMap *map = range != NULL ? range->map(kIOMapWriteCombineCache) : NULL;
		if (map != NULL) {
			bzero((void *)map->getVirtualAddress(), clearBytes);
			map->release();
		}
		OSSafeReleaseNULL(range);
	}

	if (transmitterDCN(true, s->pixelKHz) != kIOReturnSuccess)
		return false;
	IOSleep(100);

	uint64_t surface = readReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS[s->pipe]) |
	    (uint64_t)(readReg(dcn302_HUBPREQ_DCSURF_PRIMARY_SURFACE_ADDRESS_HIGH[s->pipe]) & 0xFFFF) << 32;
	uint32_t pitch = readReg(dcn302_HUBPREQ_DCSURF_SURFACE_PITCH[s->pipe]) &
	    DCN302_HUBPREQ_DCSURF_SURFACE_PITCH__PITCH_MASK;
	bzero(boot, sizeof(*boot));
	boot->v_baseAddr = (unsigned long)(fVRAM->getPhysicalAddress() + (surface - fVRAMStart));
	boot->v_width = s->width;
	boot->v_height = s->height;
	boot->v_rowBytes = pitch * 4;
	boot->v_depth = 32;
	boot->v_display = GRAPHICS_MODE;
	LOG("the saved state is set: pipe %u %s, surface at VRAM 0x%llx, %u pixels a row",
	    s->pipe, (readReg(dcn302_OTG_OTG_CONTROL[s->pipe]) & 1) ? "on" : "off",
	    (unsigned long long)(surface - fVRAMStart), pitch);
	return (readReg(dcn302_OTG_OTG_CONTROL[s->pipe]) & 1) != 0;
}

void
MinkarAMDFramebuffer::darkThread(void *fb, wait_result_t)
{
	MinkarAMDFramebuffer *self = (MinkarAMDFramebuffer *)fb;
	uint8_t *file = NULL;
	size_t bytes = 0;
	int tries = 0;

	/* The last display's state, once the root file system is up. */
	while (!MinkarAMDReadFile(kLastStatePath, &file, &bytes) && ++tries < 120)
		IOSleep(1000);
	const struct DCNState *s = (const struct DCNState *)(const void *)file;
	if (file == NULL || bytes < sizeof(*s) || s->magic != kDCNStateMagic || s->version != 1 ||
	    bytes < sizeof(*s) + (size_t)s->count * 8 || s->pipe >= DCN302_PIPES ||
	    s->digFE >= DCN302_DIGS || s->digBE >= DCN302_DIGS || s->ddc >= DCN302_DDCS || s->hpd >= 5) {
		LOG("no display state saved: the display stays dark");
		MinkarAMDFreeFirmware(file, bytes);
		self->release();
		return;
	}
	self->fCRTC = s->pipe;
	self->fDIGFE = (int)s->digFE;
	self->fDIGBE = (int)s->digBE;
	self->fDIGMode = s->digMode;
	self->fHPDSelect = s->hpd;
	self->fPHY = (int)s->phy;
	self->fDDC = (int)s->ddc;
	self->fConnectorID = (uint8_t)s->connector;
	self->fEncoderID = (uint8_t)s->encoder;
	self->fVRAMStart = (uint64_t)(self->readReg(dcn302_DCN_VM_FB_LOCATION_BASE) &
	    DCN302_DCN_VM_FB_LOCATION_BASE__FB_BASE_MASK) << 24;
	self->describeDMCUB();

	/* The SMU set up (MinkarAMDGPU's thread): its display mailbox answers. */
	for (tries = 0; tries < 120 && self->fSMUDALBase == 0; tries++) {
		self->probeSMU();
		if (self->fSMUDALBase == 0)
			IOSleep(1000);
	}

	static const uint32_t hpdReg[5] = { dcn302_HPD0_DC_HPD_INT_STATUS, dcn302_HPD1_DC_HPD_INT_STATUS,
	    dcn302_HPD2_DC_HPD_INT_STATUS, dcn302_HPD3_DC_HPD_INT_STATUS, dcn302_HPD4_DC_HPD_INT_STATUS };
	bool lit = false;
	LOG("waiting for display %08x on HPD %u", s->key, s->hpd + 1);
	while (!lit && !self->isInactive()) {
		if (!(self->readReg(hpdReg[s->hpd]) & 0x2)) {
			IOSleep(500);
			continue;
		}
		IOSleep(500);	/* the plug settled */
		uint8_t edid[256];
		unsigned edidBytes;
		if (!self->readEDIDDCN(s->ddc, edid, &edidBytes)) {
			IOSleep(500);
			continue;
		}
		if (edidKey(edid) != s->key) {
			LOG("display %08x is not the saved one: it stays dark", edidKey(edid));
			while (self->readReg(hpdReg[s->hpd]) & 0x2)
				IOSleep(1000);
			continue;
		}
		PE_Video boot;
		if (!self->lightFromState(s, &boot)) {
			LOG("the saved state did not light the display");
			break;
		}
		lit = true;
		self->fDark = false;
		/* As if the firmware had lit it. */
		self->getPlatform()->setConsoleInfo(&boot, kPEBaseAddressChange);
		if (!self->takeOver(self->fDarkProvider, &boot)) {
			LOG("the display was lit but not taken over");
			break;
		}
		/* The display is the desktop's from the start: the kernel's
		 * console (a verbose boot's text) does not draw on it. At a
		 * lit start the window server's open turns it off; here the
		 * server came first. */
		self->getPlatform()->setConsoleInfo(NULL, kPEDisableScreen);
		/* The framebuffer starts black: takeOver copied the surface
		 * cleared above into it before registering it, so a window
		 * server that draws at once is not covered. */
	}
	MinkarAMDFreeFirmware(file, bytes);
	self->release();
}
