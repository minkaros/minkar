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
 * MinkarAMDGPU (ADR 0002, G1): AMD GPUs, first by watching. It does not
 * drive the display (IOGOPFramebuffer still does, in another match
 * category): it maps the register window, reads the VBIOS from the card's
 * expansion ROM, parses it with amdgpu's AtomBIOS interpreter, and
 * publishes what it found, for minkar-hwcheck and for the mode setting
 * that comes next.
 */

#ifndef _MINKARAMDGPU_H
#define _MINKARAMDGPU_H

#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>

struct minkar_atom;

class MinkarAMDGPU : public IOService
{
	OSDeclareDefaultStructors(MinkarAMDGPU);

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;

	/* Registers by dword index, through the window or MM_INDEX/MM_DATA. */
	uint32_t readReg(uint32_t reg);
	void writeReg(uint32_t reg, uint32_t value);

private:
	const char *family(UInt16 device, bool *gcn1);
	bool readPlatformROM();
	bool readExpansionROM();
	bool keepROM(const uint8_t *p, IOByteCount length);

	IOPCIDevice *		fPCI;
	IOMemoryMap *		fMMIO;
	volatile uint8_t *	fRegs;
	IOByteCount		fRegsSize;
	IOLock *		fIndexLock;	/* MM_INDEX/MM_DATA pairs */
	uint8_t *		fBIOS;
	IOByteCount		fBIOSSize;
	struct minkar_atom *	fAtom;
};

#endif
