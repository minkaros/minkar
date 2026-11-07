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
 * What MinkarAMDGPU and MinkarAMDFramebuffer share: the device's family,
 * its register window and the VBIOS.
 */

#ifndef _MINKARAMDDEVICE_H
#define _MINKARAMDDEVICE_H

#include <IOKit/IOLocks.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>

struct MinkarAMDRegs {
	IOMemoryMap *		map;
	volatile uint8_t *	base;
	IOByteCount		size;
	IOLock *		indexLock;	/* MM_INDEX/MM_DATA pairs */
};

/* The family of a device ID (amdgpu's PCI ID table); *gcn1 if GCN 1.0. */
const char *MinkarAMDFamily(UInt16 device, bool *gcn1);

/*
 * A memory BAR's window. The PCI family's IODeviceMemory when it made one;
 * else (a GPU behind PC 2's bridges, where it does not) the
 * firmware's address from the BAR and the size from "reg", without
 * sizing the BAR of a GPU that is lighting a display. Retained.
 */
IODeviceMemory *MinkarAMDBARRange(IOPCIDevice *pci, UInt8 bar);

/* Registers: BAR 2 on GCN 1.0, BAR 5 after it. */
bool MinkarAMDMapRegisters(IOPCIDevice *pci, bool gcn1, struct MinkarAMDRegs *regs);
void MinkarAMDUnmapRegisters(struct MinkarAMDRegs *regs);
/* By dword index, through the window or MM_INDEX/MM_DATA. */
uint32_t MinkarAMDRead(struct MinkarAMDRegs *regs, uint32_t reg);
void MinkarAMDWrite(struct MinkarAMDRegs *regs, uint32_t reg, uint32_t value);

/*
 * A copy of the VBIOS: the one the firmware read at boot (loader patch
 * efiloader-0009, /chosen/pci-roms), else the expansion ROM window. Freed
 * with MinkarAMDFreeVBIOS.
 */
bool MinkarAMDReadVBIOS(IOPCIDevice *pci, uint8_t **bios, IOByteCount *size,
    const char **source);
void MinkarAMDFreeVBIOS(uint8_t *bios);

#endif
