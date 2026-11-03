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


#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>
#include "MinkarAMDFramebuffer.h"

#include "atom_glue.h"
#include "amd_atom_objects.h"
#include "edid.h"
#include "amd_pll.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDFramebuffer: " fmt "\n", ##__VA_ARGS__)

/* Register dword indices, DCE 6 and GMC 6 (asic_reg/dce/dce_6_0_d.h,
 * asic_reg/gmc/gmc_6_0_d.h). The display engine's are per crtc, at the
 * crtc's offset. */
#define mmMC_VM_FB_LOCATION			0x0809
#define mmGRPH_ENABLE				0x1A00
#define mmGRPH_CONTROL				0x1A01
#define mmGRPH_SWAP_CNTL			0x1A03
#define mmGRPH_PRIMARY_SURFACE_ADDRESS		0x1A04
#define mmGRPH_SECONDARY_SURFACE_ADDRESS	0x1A05
#define mmGRPH_PITCH				0x1A06
#define mmGRPH_PRIMARY_SURFACE_ADDRESS_HIGH	0x1A07
#define mmGRPH_SECONDARY_SURFACE_ADDRESS_HIGH	0x1A08
#define mmGRPH_SURFACE_OFFSET_X			0x1A09
#define mmGRPH_SURFACE_OFFSET_Y			0x1A0A
#define mmGRPH_X_START				0x1A0B
#define mmGRPH_Y_START				0x1A0C
#define mmGRPH_X_END				0x1A0D
#define mmGRPH_Y_END				0x1A0E
#define mmGRPH_UPDATE				0x1A11
#define mmGRPH_FLIP_CONTROL			0x1A12
#define mmDESKTOP_HEIGHT			0x1AC1
#define mmSCL_SCALER_ENABLE			0x1B42
#define mmVIEWPORT_START			0x1B5C
#define mmVIEWPORT_SIZE				0x1B5D
#define mmEXT_OVERSCAN_LEFT_RIGHT		0x1B5E
#define mmEXT_OVERSCAN_TOP_BOTTOM		0x1B5F
#define mmCRTC_H_TOTAL				0x1B80
#define mmCRTC_H_BLANK_START_END		0x1B81
#define mmCRTC_H_SYNC_A				0x1B82
#define mmCRTC_V_TOTAL				0x1B87
#define mmCRTC_V_BLANK_START_END		0x1B8D
#define mmCRTC_V_SYNC_A				0x1B8E
#define mmCRTC_CONTROL				0x1B9C
#define mmCRTC_STATUS_FRAME_COUNT		0x1BA6

#define GRPH_ENABLE_EN				0x00000001
#define GRPH_CONTROL_DEPTH_MASK			0x00000003
#define GRPH_CONTROL_FORMAT_MASK		0x00000700
#define GRPH_CONTROL_DEPTH_32BPP		0x00000002	/* format 0: ARGB8888 */
#define GRPH_UPDATE_SURFACE_UPDATE_PENDING	0x00000004
#define GRPH_UPDATE_LOCK			0x00010000
#define CRTC_CONTROL_MASTER_EN			0x00000001

static const uint32_t kCRTCOffsets[6] = { 0x0000, 0x0300, 0x2600, 0x2900, 0x2C00, 0x2F00 };

/* Smaller modes, scaled to the display's native one with its aspect kept. */
static const struct { UInt32 width, height; } kScaledModes[] = {
	{ 2560, 1440 }, { 2560, 1080 }, { 1920, 1200 }, { 1920, 1080 },
	{ 1680, 1050 }, { 1600, 900 }, { 1440, 900 }, { 1280, 1024 },
	{ 1280, 800 }, { 1280, 720 }, { 1024, 768 },
};

enum { kDepthIndex = 0 };

#define super IOFramebuffer
OSDefineMetaClassAndStructors(MinkarAMDFramebuffer, IOFramebuffer);

static uint32_t atomRead(void *fb, uint32_t reg)
{
	return ((MinkarAMDFramebuffer *)fb)->readReg(reg);
}

static void atomWrite(void *fb, uint32_t reg, uint32_t value)
{
	((MinkarAMDFramebuffer *)fb)->writeReg(reg, value);
}

/*
 * The GPU whose VRAM window (BAR 0) holds the firmware's framebuffer, a
 * GCN 1.0 one; "minkaramd_fb=0" leaves every display to IOGOPFramebuffer.
 */
IOService *
MinkarAMDFramebuffer::probe(IOService *provider, SInt32 *score)
{
	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
	PE_Video boot;
	UInt32 enabled = 1;
	bool gcn1 = false;

	if (PE_parse_boot_argn("minkaramd_fb", &enabled, sizeof(enabled)) && enabled == 0)
		return NULL;
	if (pci == NULL || getPlatform() == NULL ||
	    getPlatform()->getConsoleInfo(&boot) != kIOReturnSuccess || boot.v_baseAddr == 0)
		return NULL;
	MinkarAMDFamily(pci->configRead16(kIOPCIConfigDeviceID), &gcn1);
	if (!gcn1)
		return NULL;
	IODeviceMemory *vram = MinkarAMDBARRange(pci, kIOPCIConfigBaseAddress0);
	uint64_t fb = boot.v_baseAddr & ~(uint64_t)0xFFF;
	bool ours = vram != NULL && fb >= vram->getPhysicalAddress() &&
	    fb < vram->getPhysicalAddress() + vram->getLength();
	OSSafeReleaseNULL(vram);
	return ours ? super::probe(provider, score) : NULL;
}

/*
 * The crtc that scans out the firmware's framebuffer: enabled, its
 * surface address in VRAM the boot framebuffer's address in the window.
 */
bool
MinkarAMDFramebuffer::findDisplay(uint64_t bootAddress)
{
	fVRAMStart = (uint64_t)(readReg(mmMC_VM_FB_LOCATION) & 0xFFFF) << 24;
	for (unsigned c = 0; c < 6; c++) {
		uint32_t off = kCRTCOffsets[c];
		if (!(readReg(mmCRTC_CONTROL + off) & CRTC_CONTROL_MASTER_EN) ||
		    !(readReg(mmGRPH_ENABLE + off) & GRPH_ENABLE_EN))
			continue;
		uint64_t surface = ((uint64_t)readReg(mmGRPH_PRIMARY_SURFACE_ADDRESS_HIGH + off) << 32) |
		    readReg(mmGRPH_PRIMARY_SURFACE_ADDRESS + off);
		LOG("crtc %u: on, surface 0x%llx (VRAM at 0x%llx)", c,
		    (unsigned long long)surface, (unsigned long long)fVRAMStart);
		if (surface < fVRAMStart || surface - fVRAMStart >= fVRAM->getLength())
			continue;
		uint64_t cpu = fVRAM->getPhysicalAddress() + (surface - fVRAMStart);
		if ((cpu & ~(uint64_t)0xFFF) != (bootAddress & ~(uint64_t)0xFFF))
			continue;
		fCRTC = c;
		fCRTCOffset = off;
		fFirmwareOffset = surface - fVRAMStart;
		fFirmwarePitch = readReg(mmGRPH_PITCH + off);
		return true;
	}
	return false;
}

/* The refresh rate, from the crtc's frame counter over ten frames. */
void
MinkarAMDFramebuffer::measureRefresh()
{
	uint32_t reg = mmCRTC_STATUS_FRAME_COUNT + fCRTCOffset;
	uint64_t t0, t1, ns;
	uint32_t f;

	fRefreshMilliHz = 0;
	f = readReg(reg);
	for (int i = 0; i < 100 && readReg(reg) == f; i++)
		IOSleep(1);
	f = readReg(reg);
	clock_get_uptime(&t0);
	for (int i = 0; i < 500 && readReg(reg) - f < 10; i++)
		IOSleep(1);
	clock_get_uptime(&t1);
	if (readReg(reg) - f < 10)
		return;
	absolutetime_to_nanoseconds(t1 - t0, &ns);
	if (ns > 0)
		fRefreshMilliHz = (UInt32)(10ULL * 1000000000000ULL / ns);
}

/* The crtc's state, for minkar-hwcheck and for the bring-up reports. */
void
MinkarAMDFramebuffer::publishTimings()
{
	static const struct { const char *name; uint32_t reg; } regs[] = {
		{ "CRTC_H_TOTAL", mmCRTC_H_TOTAL }, { "CRTC_H_BLANK_START_END", mmCRTC_H_BLANK_START_END },
		{ "CRTC_H_SYNC_A", mmCRTC_H_SYNC_A }, { "CRTC_V_TOTAL", mmCRTC_V_TOTAL },
		{ "CRTC_V_BLANK_START_END", mmCRTC_V_BLANK_START_END }, { "CRTC_V_SYNC_A", mmCRTC_V_SYNC_A },
		{ "CRTC_CONTROL", mmCRTC_CONTROL }, { "GRPH_CONTROL", mmGRPH_CONTROL },
		{ "GRPH_SWAP_CNTL", mmGRPH_SWAP_CNTL }, { "GRPH_PITCH", mmGRPH_PITCH },
		{ "GRPH_X_END", mmGRPH_X_END }, { "GRPH_Y_END", mmGRPH_Y_END },
		{ "VIEWPORT_SIZE", mmVIEWPORT_SIZE }, { "DESKTOP_HEIGHT", mmDESKTOP_HEIGHT },
		{ "SCL_SCALER_ENABLE", mmSCL_SCALER_ENABLE },
		{ "EXT_OVERSCAN_LEFT_RIGHT", mmEXT_OVERSCAN_LEFT_RIGHT },
		{ "EXT_OVERSCAN_TOP_BOTTOM", mmEXT_OVERSCAN_TOP_BOTTOM },
		{ "GRPH_PRIMARY_SURFACE_ADDRESS", mmGRPH_PRIMARY_SURFACE_ADDRESS },
	};
	OSDictionary *d = OSDictionary::withCapacity(sizeof(regs) / sizeof(regs[0]));

	if (d == NULL)
		return;
	for (unsigned i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
		OSNumber *n = OSNumber::withNumber(readReg(regs[i].reg + fCRTCOffset), 32);
		if (n != NULL) {
			d->setObject(regs[i].name, n);
			n->release();
		}
	}
	setProperty("MinkarDisplayRegisters", d);
	d->release();
}

/*
 * Points the crtc at a surface in VRAM: linear, 32 bits per pixel, the
 * firmware's tiling and swap kept; it takes effect at the next vertical
 * blank. The sequence of amdgpu's dce_v6_0.c (MIT).
 */
bool
MinkarAMDFramebuffer::scanOut(uint64_t offset, UInt32 width, UInt32 height, UInt32 pitchPixels,
    bool wait)
{
	uint32_t off = fCRTCOffset;
	uint64_t address = fVRAMStart + offset;
	uint32_t control = readReg(mmGRPH_CONTROL + off);

	control &= ~(GRPH_CONTROL_DEPTH_MASK | GRPH_CONTROL_FORMAT_MASK);
	control |= GRPH_CONTROL_DEPTH_32BPP;

	writeReg(mmGRPH_UPDATE + off, readReg(mmGRPH_UPDATE + off) | GRPH_UPDATE_LOCK);
	writeReg(mmGRPH_FLIP_CONTROL + off, 0);
	writeReg(mmGRPH_PRIMARY_SURFACE_ADDRESS_HIGH + off, (uint32_t)(address >> 32));
	writeReg(mmGRPH_SECONDARY_SURFACE_ADDRESS_HIGH + off, (uint32_t)(address >> 32));
	writeReg(mmGRPH_PRIMARY_SURFACE_ADDRESS + off, (uint32_t)address & ~0xFFU);
	writeReg(mmGRPH_SECONDARY_SURFACE_ADDRESS + off, (uint32_t)address & ~0xFFU);
	writeReg(mmGRPH_CONTROL + off, control);
	writeReg(mmGRPH_SURFACE_OFFSET_X + off, 0);
	writeReg(mmGRPH_SURFACE_OFFSET_Y + off, 0);
	writeReg(mmGRPH_X_START + off, 0);
	writeReg(mmGRPH_Y_START + off, 0);
	writeReg(mmGRPH_X_END + off, width);
	writeReg(mmGRPH_Y_END + off, height);
	writeReg(mmGRPH_PITCH + off, pitchPixels);
	writeReg(mmDESKTOP_HEIGHT + off, height);
	writeReg(mmVIEWPORT_START + off, 0);
	writeReg(mmVIEWPORT_SIZE + off, (width << 16) | ((height + 1) & ~1U));
	writeReg(mmGRPH_UPDATE + off, readReg(mmGRPH_UPDATE + off) & ~GRPH_UPDATE_LOCK);
	return wait ? surfaceTaken(offset) : true;
}

/*
 * The surface is taken at the next vertical blank, and only once nothing
 * holds the crtc's double-buffered registers.
 */
bool
MinkarAMDFramebuffer::surfaceTaken(uint64_t offset)
{
	uint32_t off = fCRTCOffset;
	uint32_t address = (uint32_t)(fVRAMStart + offset) & ~0xFFU;

	for (int i = 0; i < 200; i++) {
		if (!(readReg(mmGRPH_UPDATE + off) & GRPH_UPDATE_SURFACE_UPDATE_PENDING))
			return readReg(mmGRPH_PRIMARY_SURFACE_ADDRESS + off) == address;
		IOSleep(1);
	}
	LOG("crtc %u: the surface update is still pending", fCRTC);
	return false;
}

/*
 * A mode: the native one passes straight through; a smaller one is the
 * viewport, scaled up to the display's timing with its aspect kept
 * (borders left and right, or top and bottom), all in one frame.
 */
bool
MinkarAMDFramebuffer::showMode(unsigned index)
{
	const Mode &m = fModes[index];
	const Mode &n = fModes[0];
	int left = 0, right = 0, top = 0, bottom = 0;
	bool scaled = index != 0;

	if (scaled) {
		uint64_t a1 = (uint64_t)m.height * n.width, a2 = (uint64_t)n.height * m.width;
		if (a1 > a2)
			left = right = (int)(n.width - a2 / m.height) / 2;
		else if (a2 > a1)
			top = bottom = (int)(n.height - a1 / m.width) / 2;
	}
	/* Black first: the window server draws the new mode in full. */
	bzero((void *)fMap->getVirtualAddress(), (size_t)pitchFor(m.width) * 4 * m.height);

	/* Held while it changes, so it all takes effect in one frame; the
	 * surface is checked once released. */
	bool ok = true;
	if (fAtom != NULL)
		minkar_atom_crtc_lock(fAtom, (int)fCRTC, 1);
	scanOut(fOffset, m.width, m.height, pitchFor(m.width), false);
	if (fAtom != NULL) {
		if (minkar_atom_overscan(fAtom, (int)fCRTC, left, right, top, bottom) != 0 ||
		    minkar_atom_scaler(fAtom, (int)fCRTC, scaled) != 0)
			ok = false;
		minkar_atom_crtc_lock(fAtom, (int)fCRTC, 0);
	}
	if (!surfaceTaken(fOffset))
		ok = false;
	if (!ok) {
		LOG("%ux%u: the display engine did not take it", m.width, m.height);
		return false;
	}
	fCurrent = index;
	setConsole();
	publishTimings();
	LOG("%ux%u%s", m.width, m.height, scaled ? ", scaled" : "");
	return true;
}

/* ---- The display, over AUX ---- */

#define mmDC_HPD1_INT_STATUS	0x1807	/* HPDn's at 3 dwords apart */
#define DC_HPD_SENSE		0x2

/* One AUX transaction, retried while the sink defers (DP: up to 7 times). */
int
MinkarAMDFramebuffer::aux(uint8_t request, uint32_t address, uint8_t *buffer, uint8_t size)
{
	for (int tries = 0; tries < 8; tries++) {
		uint8_t reply = 0;
		int n = minkar_atom_aux(fAtom, fAUX, fHPD, request, address, buffer, size, &reply);
		if (n < 0)
			return n;
		uint8_t r = reply >> 4;
		bool i2c = (request & ~0x4) <= 0x1;
		uint8_t status = i2c ? ((r >> 2) & 3) : (r & 3);
		if ((r & 3) == 0 && status == 0)
			return n;		/* ACK */
		if ((r & 3) == 1 || status == 1)
			return -1;		/* NACK */
		IODelay(500);			/* DEFER */
	}
	return -1;
}

/*
 * The connector whose HPD pin senses the display, its DPCD (the receiver's
 * capabilities, 16 bytes at 0) and its EDID through I2C over AUX (slave
 * 0x50, each 128-byte block in 16-byte reads), as drm_dp_i2c_xfer does.
 */
void
MinkarAMDFramebuffer::readSink()
{
	struct atom_connector c[kAtomMaxConnectors];
	unsigned n = atom_connectors(fBIOS, fBIOSSize, kSIDCGPIOHPDA, c, kAtomMaxConnectors);

	fConnector = -1;
	fEDIDBytes = 0;
	for (unsigned i = 0; i < n && fConnector < 0; i++) {
		if (c[i].hpd < 0 || ATOM_OBJECT_ID(c[i].object) != kAtomConnectorDisplayPort)
			continue;
		if (!(readReg(mmDC_HPD1_INT_STATUS + 3 * c[i].hpd) & DC_HPD_SENSE))
			continue;
		fConnector = (int)i;
		fAUX = c[i].aux;
		fHPD = (uint8_t)c[i].hpd;
		fEncoder = c[i].encoder;
	}
	if (fConnector < 0) {
		LOG("no DisplayPort connector senses a display");
		return;
	}
	setProperty("MinkarConnector", (unsigned long long)c[fConnector].object, 32);
	setProperty("MinkarEncoder", (unsigned long long)fEncoder, 32);
	setProperty("MinkarAUXChannel", (unsigned long long)fAUX, 32);
	setProperty("MinkarHPD", (unsigned long long)fHPD + 1, 32);

	bzero(fDPCD, sizeof(fDPCD));
	if (aux(0x9, 0x000, fDPCD, sizeof(fDPCD)) == (int)sizeof(fDPCD)) {
		setProperty("MinkarDPCD", fDPCD, sizeof(fDPCD));
		LOG("DPCD %u.%u, %u.%u Gbps x %u lanes", fDPCD[0] >> 4, fDPCD[0] & 0xf,
		    fDPCD[1] * 27 / 100, fDPCD[1] * 27 % 100, fDPCD[2] & 0x1f);
	} else {
		bzero(fDPCD, sizeof(fDPCD));
		LOG("no DPCD from AUX channel 0x%02x: not a DisplayPort sink", fAUX);
	}

	/* The EDID: over AUX (I2C over AUX) to a DisplayPort sink; on the
	 * DDC line itself (hardware I2C) when there is no DPCD: an HDMI or
	 * DVI sink on a dual-mode port (the Mac Pro 6,1's HDMI port). */
	bool dp = fDPCD[0] != 0;
	if (!dp) {
		for (unsigned block = 0; block < 2; block++) {
			if (minkar_atom_i2c(fAtom, fAUX, 0x50, 0, (uint8_t)(block * 128), NULL, 0,
			    fEDID + block * 128, 128) != 0)
				break;
			fEDIDBytes = (block + 1) * 128;
			if (block == 0 && fEDID[126] == 0)
				break;
		}
		setProperty("MinkarSinkType", "HDMI or DVI (DDC)");
	} else
		setProperty("MinkarSinkType", "DisplayPort");
	uint8_t zero = 0;
	for (unsigned block = 0; dp && block < 2; block++) {
		uint8_t offset = (uint8_t)(block * 128);
		if (aux(0x0 | 0x4, 0x50, &offset, 1) != 1)
			break;
		bool good = true;
		for (unsigned at = 0; at < 128 && good; at += 16)
			good = aux(0x1 | 0x4, 0x50, fEDID + block * 128 + at, 16) == 16;
		(void)aux(0x1, 0x50, &zero, 0);	/* stop */
		if (!good)
			break;
		fEDIDBytes = (block + 1) * 128;
		if (block == 0 && fEDID[126] == 0)
			break;	/* no extension */
	}
	if (fEDIDBytes >= 128 && fEDID[0] == 0x00 && fEDID[1] == 0xff && fEDID[7] == 0x00) {
		setProperty("MinkarEDID", fEDID, fEDIDBytes);
		LOG("EDID: %u bytes, vendor %c%c%c, product 0x%04x", fEDIDBytes,
		    '@' + ((fEDID[8] >> 2) & 0x1f), '@' + (((fEDID[8] & 3) << 3) | (fEDID[9] >> 5)),
		    '@' + (fEDID[9] & 0x1f), fEDID[10] | (fEDID[11] << 8));
	} else {
		LOG("no EDID from the display");
		fEDIDBytes = 0;
	}
}

/*
 * The smaller modes, shown through the scaler: the display's own (from
 * its EDID), largest first; without an EDID, the usual sizes.
 */
void
MinkarAMDFramebuffer::addScaledModes()
{
	struct edid_mode e[32];
	unsigned n = fEDIDBytes >= 128 ? edid_modes(fEDID, fEDIDBytes, e, 32) : 0;

	for (unsigned i = 0; i < n; i++) {	/* largest first */
		for (unsigned j = i + 1; j < n; j++)
			if ((uint32_t)e[j].hactive * e[j].vactive > (uint32_t)e[i].hactive * e[i].vactive) {
				struct edid_mode t = e[i];
				e[i] = e[j];
				e[j] = t;
			}
	}
	for (unsigned i = 0; i < n && fModeCount < kMaxModes; i++) {
		bool known = false;
		if (e[i].hactive > fModes[0].width || e[i].vactive > fModes[0].height ||
		    e[i].hactive < 640 || e[i].vactive < 480)
			continue;
		for (unsigned k = 0; k < fModeCount; k++)
			known |= fModes[k].width == e[i].hactive && fModes[k].height == e[i].vactive;
		if (known)
			continue;
		fModes[fModeCount].width = e[i].hactive;
		fModes[fModeCount].height = e[i].vactive;
		fModeCount++;
	}
	if (n > 0) {
		setProperty("MinkarModesFrom", "EDID");
		return;
	}
	for (unsigned i = 0; i < sizeof(kScaledModes) / sizeof(kScaledModes[0]) &&
	    fModeCount < kMaxModes; i++) {
		if (kScaledModes[i].width >= fModes[0].width ||
		    kScaledModes[i].height > fModes[0].height)
			continue;
		fModes[fModeCount].width = kScaledModes[i].width;
		fModes[fModeCount].height = kScaledModes[i].height;
		fModeCount++;
	}
	setProperty("MinkarModesFrom", "the usual sizes");
}

/* ---- Setting modes through the VBIOS (bring-up) ---- */

#define mmCRTC0_PIXEL_RATE_CNTL	0x0140	/* CRTCn's at 4 dwords apart */
#define mmPLL_REF_DIV		0x1700	/* PPLL0's; PPLL1 0x1710, PPLL2 0x1720 */

IOReturn
MinkarAMDFramebuffer::bringUpModeSet(uint32_t stage, uint32_t edidIndex, uint64_t *out)
{
	struct edid_mode modes[32];
	unsigned n = fEDIDBytes >= 128 ? edid_modes(fEDID, fEDIDBytes, modes, 32) : 0;
	struct amd_pll_limits l;
	struct amd_pll_dividers d;

	if (fAtom == NULL || fConnector < 0 || edidIndex >= n || stage > 3)
		return kIOReturnBadArgument;
	const struct edid_mode *m = &modes[edidIndex];
	if (m->hactive != fModes[fCurrent].width || m->vactive != fModes[fCurrent].height ||
	    fCurrent != 0)
		return kIOReturnUnsupported;	/* the framebuffer stays as it is */
	if (!atom_pll_limits(fBIOS, fBIOSSize, &l.reference, &l.inMin, &l.inMax, &l.vcoMin, &l.vcoMax))
		return kIOReturnUnsupported;
	l.refDivMin = 1;
	l.refDivMax = 0x3ff;
	l.fbDivMin = 4;
	l.fbDivMax = 0xfff;
	l.postDivMin = 2;
	l.postDivMax = 0x7f;
	if (!amd_pll_compute(&l, m->clock, &d))
		return kIOReturnUnsupported;

	/* The path: the crtc's PLL (its pixel rate source: 1 PPLL1, 2 PPLL2,
	 * 0 PPLL0), the connector's encoder and DIG, an HDMI sink when it
	 * has no DPCD and a CEA extension. */
	struct atom_connector c[kAtomMaxConnectors];
	unsigned nc = atom_connectors(fBIOS, fBIOSSize, kSIDCGPIOHPDA, c, kAtomMaxConnectors);
	if ((unsigned)fConnector >= nc)
		return kIOReturnNotFound;
	uint32_t source = readReg(mmCRTC0_PIXEL_RATE_CNTL + 4 * fCRTC) & 3;
	struct minkar_atom_output o;
	o.crtc = (uint8_t)fCRTC;
	o.pll = source == 1 ? 0 : source == 2 ? 1 : 2;
	o.encoderID = (uint8_t)ATOM_OBJECT_ID(fEncoder);
	o.linkB = ATOM_OBJECT_ENUM(fEncoder) == 2;
	o.connectorID = (uint8_t)ATOM_OBJECT_ID(c[fConnector].object);
	o.hpd = fHPD;
	o.hdmi = fDPCD[0] == 0 && fEDIDBytes > 128;
	struct minkar_atom_timing t = {
		m->clock, m->hactive, m->hfront, m->hsync, m->hback,
		m->vactive, m->vfront, m->vsync, m->vback, m->hpositive, m->vpositive,
		(uint16_t)d.refDiv, (uint16_t)d.fbDiv, (uint16_t)d.fbFrac, (uint16_t)d.postDiv,
	};
	/* A single TMDS link: 340 MHz for HDMI 1.4, 165 MHz for DVI. */
	if (m->clock > (o.hdmi ? 340000u : 165000u))
		return kIOReturnUnsupported;
	int rc = 0;
	LOG("bring-up mode set, stage %u: %ux%u, %u kHz: crtc %u, PPLL index %u, dividers %u %u.%u %u, "
	    "encoder 0x%02x%s, DIG %d, %s", stage, m->hactive, m->vactive, m->clock, o.crtc, o.pll,
	    d.refDiv, d.fbDiv, d.fbFrac, d.postDiv, o.encoderID, o.linkB ? " link B" : "",
	    minkar_atom_dig_for(&o), o.hdmi ? "HDMI" : "DVI");

	switch (stage) {
	case 1:
		rc = minkar_atom_set_pixel_clock(fAtom, &o, &t);
		break;
	case 2:
		rc = minkar_atom_set_timing(fAtom, (int)o.crtc, &t);
		break;
	case 3:
		/* As DRM's helpers do: the encoder off, the crtc off, the mode,
		 * the crtc on, the encoder on (dce_v6_0.c, atombios_*.c). */
		rc |= minkar_atom_transmitter(fAtom, &o, 0, t.clock);
		rc |= minkar_atom_crtc_lock(fAtom, (int)o.crtc, 1);
		rc |= minkar_atom_crtc_blank(fAtom, (int)o.crtc, 1);
		rc |= minkar_atom_crtc_enable(fAtom, (int)o.crtc, 0);
		rc |= minkar_atom_set_pixel_clock(fAtom, &o, &t);
		rc |= minkar_atom_set_timing(fAtom, (int)o.crtc, &t);
		scanOut(fOffset, m->hactive, m->vactive, pitchFor(m->hactive), false);
		rc |= minkar_atom_overscan(fAtom, (int)o.crtc, 0, 0, 0, 0);
		rc |= minkar_atom_scaler(fAtom, (int)o.crtc, 0);
		rc |= minkar_atom_select_crtc_source(fAtom, &o);
		rc |= minkar_atom_crtc_enable(fAtom, (int)o.crtc, 1);
		rc |= minkar_atom_crtc_blank(fAtom, (int)o.crtc, 0);
		rc |= minkar_atom_crtc_lock(fAtom, (int)o.crtc, 0);
		rc |= minkar_atom_dig_setup(fAtom, &o, t.clock);
		rc |= minkar_atom_transmitter(fAtom, &o, 1, t.clock);
		break;
	default:
		break;
	}
	if (stage != 0) {
		IOSleep(200);
		measureRefresh();
		setProperty("MinkarRefreshMilliHz", fRefreshMilliHz, 32);
		publishTimings();
	}
	uint32_t pll = mmPLL_REF_DIV + (o.pll == 2 ? 0 : 0x10 * (o.pll + 1));
	out[0] = (uint64_t)(int64_t)rc;
	out[1] = (uint64_t)d.refDiv << 24 | (uint64_t)d.fbDiv << 12 | (uint64_t)d.fbFrac << 8 | d.postDiv;
	out[2] = readReg(pll);
	out[3] = readReg(pll + 1);
	out[4] = readReg(pll + 2);
	out[5] = stage != 0 ? fRefreshMilliHz : (uint64_t)edid_refresh_mhz(m);
	LOG("bring-up mode set, stage %u: result %d, PLL 0x%08x 0x%08x 0x%08x, %u mHz", stage, rc,
	    (uint32_t)out[2], (uint32_t)out[3], (uint32_t)out[4], (uint32_t)out[5]);
	return kIOReturnSuccess;
}

bool
MinkarAMDFramebuffer::hasDDCConnect(IOIndex connectIndex)
{
	return connectIndex == 0 && fEDIDBytes >= 128;
}

/* With an EDID, IODisplay reads it through hasDDCConnect and getDDCBlock. */
IOReturn
MinkarAMDFramebuffer::getAttributeForConnection(IOIndex connectIndex, IOSelect attribute,
    uintptr_t *value)
{
	if (connectIndex == 0 && attribute == kConnectionSupportsHLDDCSense)
		return fEDIDBytes >= 128 ? kIOReturnSuccess : kIOReturnUnsupported;
	return super::getAttributeForConnection(connectIndex, attribute, value);
}

/* The EDID, 128 bytes a block, as IODisplay asks for it. */
IOReturn
MinkarAMDFramebuffer::getDDCBlock(IOIndex connectIndex, UInt32 blockNumber, IOSelect blockType,
    IOOptionBits options, UInt8 *data, IOByteCount *length)
{
	(void)blockType;
	(void)options;
	if (connectIndex != 0 || blockNumber < 1 || blockNumber * 128 > fEDIDBytes ||
	    *length < 128)
		return kIOReturnUnsupported;
	bcopy(fEDID + (blockNumber - 1) * 128, data, 128);
	*length = 128;
	return kIOReturnSuccess;
}

/* The kernel's console draws into our framebuffer, as it did into the
 * firmware's. */
void
MinkarAMDFramebuffer::setConsole()
{
	const Mode &m = fModes[fCurrent];
	PE_Video console;

	bzero(&console, sizeof(console));
	console.v_baseAddr = (fVRAM->getPhysicalAddress() + fOffset) | 1;	/* map it */
	console.v_width = m.width;
	console.v_height = m.height;
	console.v_depth = 32;
	console.v_rowBytes = pitchFor(m.width) * 4;
	console.v_display = GRAPHICS_MODE;
	console.v_scale = kPEScaleFactor1x;
	/* As IOGOPFramebuffer publishes them, for minkar-hwcheck --display:
	 * the framebuffer and the window it lies in. */
	setProperty("MinkarFramebufferAddress",
	    (unsigned long long)(fVRAM->getPhysicalAddress() + fOffset), 64);
	setProperty("MinkarFramebufferWidth", (unsigned long long)m.width, 32);
	setProperty("MinkarFramebufferHeight", (unsigned long long)m.height, 32);
	setProperty("MinkarFramebufferRowBytes", (unsigned long long)console.v_rowBytes, 32);
	setProperty("MinkarFramebufferWindowBase",
	    (unsigned long long)fVRAM->getPhysicalAddress(), 64);
	setProperty("MinkarFramebufferWindowSize", (unsigned long long)fVRAM->getLength(), 64);
	getPlatform()->setConsoleInfo(&console, kPEAcquireScreen);
	getPlatform()->setConsoleInfo(&console, kPEGraphicsMode);
	getPlatform()->setConsoleInfo(&console, kPEEnableScreen);
}

bool
MinkarAMDFramebuffer::start(IOService *provider)
{
	PE_Video boot;
	bool gcn1;

	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || getPlatform() == NULL ||
	    getPlatform()->getConsoleInfo(&boot) != kIOReturnSuccess)
		return false;
	fPCI->retain();
	MinkarAMDFamily(fPCI->configRead16(kIOPCIConfigDeviceID), &gcn1);
	fVRAM = MinkarAMDBARRange(fPCI, kIOPCIConfigBaseAddress0);
	if (fVRAM == NULL || !MinkarAMDMapRegisters(fPCI, gcn1, &fRegs)) {
		LOG("cannot map the registers or VRAM");
		return false;
	}
	if (!findDisplay(boot.v_baseAddr)) {
		LOG("no crtc scans out the firmware's framebuffer at 0x%llx",
		    (unsigned long long)boot.v_baseAddr);
		return false;
	}

	/* The native mode: the firmware's, which is the display's. */
	uint32_t viewport = readReg(mmVIEWPORT_SIZE + fCRTCOffset);
	fModes[0].width = boot.v_width;
	fModes[0].height = boot.v_height;
	if ((viewport >> 16) != boot.v_width || ((viewport & 0x1FFF) & ~1U) != (boot.v_height & ~1U))
		LOG("viewport %ux%u, firmware mode %ux%u", viewport >> 16, viewport & 0x1FFF,
		    (unsigned)boot.v_width, (unsigned)boot.v_height);
	fModeCount = 1;
	measureRefresh();

	/* Ours, after the firmware's, on a 16 MB boundary: as big as the
	 * native mode needs, which every other mode fits in. */
	fSize = (IOByteCount)pitchFor(fModes[0].width) * 4 * fModes[0].height;
	fOffset = (fFirmwareOffset + (uint64_t)fFirmwarePitch * 4 * fModes[0].height +
	    0xFFFFFF) & ~(uint64_t)0xFFFFFF;
	if (fOffset + fSize > fVRAM->getLength()) {
		LOG("no room in the VRAM window for a %llu-byte framebuffer", (unsigned long long)fSize);
		return false;
	}
	IODeviceMemory *ours = IODeviceMemory::withSubRange(fVRAM, fOffset, fSize);
	fMap = ours != NULL ? ours->map(kIOMapWriteCombineCache) : NULL;
	OSSafeReleaseNULL(ours);
	if (fMap == NULL) {
		LOG("cannot map the framebuffer");
		return false;
	}

	/* The VBIOS, for the scaled modes. */
	const char *source;
	if (MinkarAMDReadVBIOS(fPCI, &fBIOS, &fBIOSSize, &source))
		fAtom = minkar_atom_open(this, atomRead, atomWrite, fBIOS, NULL);
	if (fAtom != NULL)
		readSink();
	if (fAtom != NULL && minkar_atom_has_table(fAtom, MINKAR_ATOM_ENABLE_SCALER) &&
	    minkar_atom_has_table(fAtom, MINKAR_ATOM_SET_CRTC_OVERSCAN) &&
	    minkar_atom_has_table(fAtom, MINKAR_ATOM_UPDATE_CRTC_DOUBLE_BUFFER))
		addScaledModes();
	else
		LOG("no VBIOS display tables (VBIOS from %s): the native mode only", source);

	if (!super::start(provider))
		return false;

	/* Take the display: the firmware's picture copied over, then ours
	 * scanned out at the native mode. */
	IODeviceMemory *theirs = IODeviceMemory::withSubRange(fVRAM, fFirmwareOffset,
	    (IOByteCount)fFirmwarePitch * 4 * fModes[0].height);
	IOMemoryMap *from = theirs != NULL ? theirs->map(kIOMapInhibitCache | kIOMapReadOnly) : NULL;
	OSSafeReleaseNULL(theirs);
	uint8_t *to = (uint8_t *)fMap->getVirtualAddress();
	if (from != NULL) {
		const uint8_t *p = (const uint8_t *)from->getVirtualAddress();
		for (UInt32 y = 0; y < fModes[0].height; y++)
			bcopy(p + (size_t)y * fFirmwarePitch * 4, to + (size_t)y * pitchFor(fModes[0].width) * 4,
			    (size_t)fModes[0].width * 4);
		from->release();
	} else
		bzero(to, fSize);
	if (!scanOut(fOffset, fModes[0].width, fModes[0].height, pitchFor(fModes[0].width), true)) {
		/* Give the display back as it was. */
		scanOut(fFirmwareOffset, fModes[0].width, fModes[0].height, fFirmwarePitch, true);
		LOG("the display engine did not take our framebuffer");
		return false;
	}
	fCurrent = 0;
	setConsole();
	publishTimings();

	setProperty("MinkarDisplayCRTC", fCRTC, 32);
	setProperty("MinkarFirmwareScanoutOffset", (unsigned long long)fFirmwareOffset, 64);
	setProperty("MinkarScanoutOffset", (unsigned long long)fOffset, 64);
	setProperty("MinkarRefreshMilliHz", fRefreshMilliHz, 32);
	setProperty("MinkarScaledModes", fModeCount > 1);
	/* The window server presents through this GPU's user client. */
	setProperty("MinkarAcceleratorClass", "MinkarAMDGPU");
	LOG("crtc %u, %ux%u at %u.%03u Hz, framebuffer at VRAM 0x%llx, %u modes", fCRTC,
	    fModes[0].width, fModes[0].height, fRefreshMilliHz / 1000, fRefreshMilliHz % 1000,
	    (unsigned long long)fOffset, fModeCount);

	setupForCurrentConfig();
	registerService();
	return true;
}

void
MinkarAMDFramebuffer::free()
{
	minkar_atom_close(fAtom);
	MinkarAMDFreeVBIOS(fBIOS);
	OSSafeReleaseNULL(fMap);
	OSSafeReleaseNULL(fVRAM);
	MinkarAMDUnmapRegisters(&fRegs);
	OSSafeReleaseNULL(fPCI);
	super::free();
}

bool
MinkarAMDFramebuffer::scanout(uint64_t *offset, uint64_t *bytes)
{
	if (fMap == NULL || fCurrent >= fModeCount)
		return false;
	*offset = fOffset;
	*bytes = (uint64_t)pitchFor(fModes[fCurrent].width) * 4 * fModes[fCurrent].height;
	return true;
}

/* ---- IOFramebuffer ----------------------------------------------------- */

IOReturn
MinkarAMDFramebuffer::enableController()
{
	return kIOReturnSuccess;
}

const char *
MinkarAMDFramebuffer::getPixelFormats()
{
	return IO32BitDirectPixels "\0";
}

IOItemCount
MinkarAMDFramebuffer::getDisplayModeCount(void)
{
	return fModeCount;
}

/* Mode IDs are 1 and up, in fModes order; 1 is native. */
IOReturn
MinkarAMDFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes)
{
	for (unsigned i = 0; i < fModeCount; i++)
		allDisplayModes[i] = (IODisplayModeID)(i + 1);
	return kIOReturnSuccess;
}

const MinkarAMDFramebuffer::Mode *
MinkarAMDFramebuffer::modeFor(IODisplayModeID id)
{
	if (id < 1 || (unsigned)id > fModeCount)
		return NULL;
	return &fModes[id - 1];
}

IOReturn
MinkarAMDFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode,
    IODisplayModeInformation *info)
{
	const Mode *m = modeFor(displayMode);

	if (m == NULL)
		return kIOReturnUnsupportedMode;
	bzero(info, sizeof(*info));
	info->nominalWidth = m->width;
	info->nominalHeight = m->height;
	info->refreshRate = (IOFixed1616)(((uint64_t)fRefreshMilliHz << 16) / 1000);
	info->maxDepthIndex = kDepthIndex;
	return kIOReturnSuccess;
}

UInt64
MinkarAMDFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex)
{
	return 0;
}

IOReturn
MinkarAMDFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
    IOPixelAperture aperture, IOPixelInformation *info)
{
	const Mode *m = modeFor(displayMode);

	if (m == NULL || aperture != kIOFBSystemAperture || depth != kDepthIndex)
		return kIOReturnUnsupportedMode;
	bzero(info, sizeof(*info));
	info->activeWidth = m->width;
	info->activeHeight = m->height;
	info->bytesPerRow = pitchFor(m->width) * 4;
	strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
	info->pixelType = kIORGBDirectPixels;
	info->componentMasks[0] = 0x00ff0000;
	info->componentMasks[1] = 0x0000ff00;
	info->componentMasks[2] = 0x000000ff;
	info->bitsPerPixel = 32;
	info->componentCount = 3;
	info->bitsPerComponent = 8;
	return kIOReturnSuccess;
}

IOReturn
MinkarAMDFramebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth)
{
	if (displayMode)
		*displayMode = (IODisplayModeID)(fCurrent + 1);
	if (depth)
		*depth = kDepthIndex;
	return kIOReturnSuccess;
}

IOReturn
MinkarAMDFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
	if (modeFor(displayMode) == NULL || depth != kDepthIndex)
		return kIOReturnUnsupportedMode;
	if ((unsigned)displayMode - 1 == fCurrent)
		return kIOReturnSuccess;
	if (showMode((unsigned)displayMode - 1))
		return kIOReturnSuccess;
	showMode(0);
	return kIOReturnUnsupportedMode;
}

IODeviceMemory *
MinkarAMDFramebuffer::getApertureRange(IOPixelAperture)
{
	return IODeviceMemory::withRange(fVRAM->getPhysicalAddress() + fOffset, fSize);
}

bool
MinkarAMDFramebuffer::isConsoleDevice(void)
{
	return true;
}
