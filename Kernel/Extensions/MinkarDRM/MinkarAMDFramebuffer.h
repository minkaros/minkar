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
 * MinkarAMDFramebuffer (ADR 0002, G1): the display of an AMD GCN 1.0 GPU
 * (DCE 6), as an IOFramebuffer, in place of IOGOPFramebuffer on the GPU
 * whose display the firmware lit. The firmware trained the link and set
 * the display's native timing; this driver takes the display engine from
 * there: it scans out a framebuffer of its own in VRAM, programmed through
 * the engine's registers, and gives the other modes through the scaler,
 * with the VBIOS's command tables run by the AtomBIOS interpreter.
 */

#ifndef _MINKARAMDFRAMEBUFFER_H
#define _MINKARAMDFRAMEBUFFER_H

#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOPCIDevice.h>
#include "MinkarAMDDevice.h"
#include "edid.h"

struct minkar_atom;

class MinkarAMDFramebuffer : public IOFramebuffer
{
	OSDeclareDefaultStructors(MinkarAMDFramebuffer);

public:
	virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;

	virtual IOReturn enableController() APPLE_KEXT_OVERRIDE;
	virtual const char *getPixelFormats() APPLE_KEXT_OVERRIDE;
	virtual IOItemCount getDisplayModeCount(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getInformationForDisplayMode(IODisplayModeID displayMode,
	    IODisplayModeInformation *info) APPLE_KEXT_OVERRIDE;
	virtual UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode,
	    IOIndex depth) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
	    IOPixelAperture aperture, IOPixelInformation *info) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getCurrentDisplayMode(IODisplayModeID *displayMode,
	    IOIndex *depth) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) APPLE_KEXT_OVERRIDE;
	virtual IODeviceMemory *getApertureRange(IOPixelAperture aperture) APPLE_KEXT_OVERRIDE;
	virtual bool isConsoleDevice(void) APPLE_KEXT_OVERRIDE;
	virtual bool hasDDCConnect(IOIndex connectIndex) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getAttributeForConnection(IOIndex connectIndex, IOSelect attribute,
	    uintptr_t *value) APPLE_KEXT_OVERRIDE;
	virtual IOReturn getDDCBlock(IOIndex connectIndex, UInt32 blockNumber, IOSelect blockType,
	    IOOptionBits options, UInt8 *data, IOByteCount *length) APPLE_KEXT_OVERRIDE;

	/* The framebuffer being shown: its VRAM offset and size (the current
	 * mode's rows). */
	bool scanout(uint64_t *offset, uint64_t *bytes);
	/* Bring-up (G2): sets one of the EDID's modes of the current size
	 * through the VBIOS's tables, by stages (0 dividers only, 1 the PLL,
	 * 2 the timing, 3 all of it); out: result, dividers, the PLL's
	 * registers, the refresh rate measured after. */
	IOReturn bringUpModeSet(uint32_t stage, uint32_t edidIndex, uint64_t *out);

	uint32_t readReg(uint32_t reg) { return MinkarAMDRead(&fRegs, reg); }
	void writeReg(uint32_t reg, uint32_t value) { MinkarAMDWrite(&fRegs, reg, value); }

private:
	/* A mode: the native size at one of the display's refresh rates
	 * (timing, an index in fTimings), or a smaller size scaled up to the
	 * timing in use (timing -1, refresh 0). Mode 0 is the firmware's. */
	struct Mode {
		UInt32 width, height;
		UInt32 refreshMilliHz;
		int timing;
	};
	enum { kMaxModes = 16, kMaxTimings = 4 };

	bool findDisplay(uint64_t bootAddress);
	void readSink();
	IOReturn setTiming(const struct edid_mode *m, uint32_t stage, uint64_t *out);
	void setBestNativeMode();
	void addTimedModes();
	void addScaledModes();
	void tookTiming(const struct edid_mode *m);
	int aux(uint8_t request, uint32_t address, uint8_t *buffer, uint8_t size);
	void measureRefresh();
	void publishTimings();
	bool scanOut(uint64_t offset, UInt32 width, UInt32 height, UInt32 pitchPixels,
	    bool wait);
	bool surfaceTaken(uint64_t offset);
	bool showMode(unsigned index);
	void setConsole();
	const Mode *modeFor(IODisplayModeID id);
	UInt32 pitchFor(UInt32 width) { return (width + 63) & ~63U; }

	IOPCIDevice *		fPCI;
	struct MinkarAMDRegs	fRegs;
	uint8_t *		fBIOS;
	IOByteCount		fBIOSSize;
	struct minkar_atom *	fAtom;

	unsigned		fCRTC;		/* the lit display's crtc */
	uint32_t		fCRTCOffset;
	uint64_t		fVRAMStart;	/* VRAM in the GPU's address space */
	IODeviceMemory *	fVRAM;		/* its CPU window (BAR 0) */
	uint64_t		fFirmwareOffset;	/* the firmware's framebuffer */
	UInt32			fFirmwarePitch;	/* in pixels */
	uint64_t		fOffset;	/* ours, in VRAM */
	IOByteCount		fSize;
	IOMemoryMap *		fMap;		/* ours, for the driver */
	UInt32			fRefreshMilliHz;

	/* The display's connector (from the VBIOS's objects) and what it says
	 * of itself over AUX: its DPCD and its EDID. */
	int			fConnector;	/* -1: not found */
	uint8_t			fAUX, fHPD;
	uint16_t		fEncoder;
	uint8_t			fDPCD[16];
	uint8_t			fEDID[256];
	unsigned		fEDIDBytes;

	Mode			fModes[kMaxModes];
	unsigned		fModeCount;
	unsigned		fCurrent;	/* index in fModes; 0 is native */
	/* The native size's timings from the EDID, and the one the display
	 * runs at (-1: the firmware's, not one of them). */
	struct edid_mode	fTimings[kMaxTimings];
	unsigned		fTimingCount;
	int			fTiming;
};

#endif
