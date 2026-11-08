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
#include <IOKit/IODeviceTreeSupport.h>
#include <libkern/OSByteOrder.h>
#include "MinkarAMDDevice.h"

#define LOG(fmt, ...)	IOLog("MinkarAMD: " fmt "\n", ##__VA_ARGS__)

/* Register dword indices (asic_reg/bif). */
#define mmMM_INDEX		0x0000
#define mmMM_DATA		0x0001

/* An expansion ROM is at most this big (PCI: a 16-bit count of 512-byte
 * blocks; AMD's are 64 KB or 128 KB). */
#define kMaxROM			(256 * 1024)

const char *
MinkarAMDFamily(UInt16 d, bool *gcn1)
{
	static const struct { UInt16 lo, hi; const char *name; } si[] = {
		{ 0x6780, 0x679F, "Tahiti" }, { 0x6800, 0x6819, "Pitcairn" },
		{ 0x6820, 0x683F, "Cape Verde" }, { 0x6600, 0x6631, "Oland" },
		{ 0x6660, 0x666F, "Hainan" },
	};
	for (unsigned i = 0; i < sizeof(si) / sizeof(si[0]); i++)
		if (d >= si[i].lo && d <= si[i].hi) {
			*gcn1 = true;
			return si[i].name;
		}
	*gcn1 = false;
	/* RDNA2, Navi 23 (dimgrey_cavefish): the RX 6600s, PC 1's card. */
	if (d >= 0x73E0 && d <= 0x73FF)
		return "Navi 23";
	return "not GCN 1.0";
}

IODeviceMemory *
MinkarAMDBARRange(IOPCIDevice *pci, UInt8 bar)
{
	IODeviceMemory *range = pci->getDeviceMemoryWithRegister(bar);

	if (range != NULL) {
		range->retain();
		return range;
	}
	UInt32 lo = pci->configRead32(bar);
	if (lo & 1)
		return NULL;	/* I/O space */
	uint64_t base = lo & ~(uint64_t)0xF;
	if ((lo & 0x6) == 0x4 && bar < kIOPCIConfigBaseAddress5)
		base |= (uint64_t)pci->configRead32(bar + 4) << 32;
	OSData *reg = OSDynamicCast(OSData, pci->getProperty("reg"));
	if (base == 0 || reg == NULL)
		return NULL;
	const IOPCIPhysicalAddress *e = (const IOPCIPhysicalAddress *)reg->getBytesNoCopy();
	for (unsigned i = 0; i < reg->getLength() / sizeof(IOPCIPhysicalAddress); i++) {
		if (e[i].physHi.s.registerNum != bar)
			continue;
		uint64_t size = ((uint64_t)e[i].lengthHi << 32) | e[i].lengthLo;
		if (size == 0)
			return NULL;
		LOG("BAR 0x%02x at 0x%llx, %llu KB, without IODeviceMemory: from the firmware",
		    bar, (unsigned long long)base, (unsigned long long)size / 1024);
		return IODeviceMemory::withRange(base, size);
	}
	return NULL;
}

bool
MinkarAMDMapRegisters(IOPCIDevice *pci, bool gcn1, struct MinkarAMDRegs *regs)
{
	bzero(regs, sizeof(*regs));
	pci->setMemoryEnable(true);
	IODeviceMemory *range = MinkarAMDBARRange(pci, gcn1 ? kIOPCIConfigBaseAddress2 :
	    kIOPCIConfigBaseAddress5);
	if (range != NULL) {
		regs->map = range->map(kIOMapInhibitCache);
		range->release();
	}
	regs->indexLock = IOLockAlloc();
	if (regs->map == NULL || regs->indexLock == NULL) {
		MinkarAMDUnmapRegisters(regs);
		return false;
	}
	regs->base = (volatile uint8_t *)regs->map->getVirtualAddress();
	regs->size = regs->map->getLength();
	return true;
}

void
MinkarAMDUnmapRegisters(struct MinkarAMDRegs *regs)
{
	OSSafeReleaseNULL(regs->map);
	if (regs->indexLock != NULL)
		IOLockFree(regs->indexLock);
	bzero(regs, sizeof(*regs));
}

uint32_t
MinkarAMDRead(struct MinkarAMDRegs *regs, uint32_t reg)
{
	if ((IOByteCount)reg * 4 + 4 <= regs->size)
		return OSReadLittleInt32(regs->base, reg * 4);
	IOLockLock(regs->indexLock);
	OSWriteLittleInt32(regs->base, mmMM_INDEX * 4, reg * 4);
	uint32_t v = OSReadLittleInt32(regs->base, mmMM_DATA * 4);
	IOLockUnlock(regs->indexLock);
	return v;
}

void
MinkarAMDWrite(struct MinkarAMDRegs *regs, uint32_t reg, uint32_t value)
{
	if ((IOByteCount)reg * 4 + 4 <= regs->size) {
		OSWriteLittleInt32(regs->base, reg * 4, value);
		return;
	}
	IOLockLock(regs->indexLock);
	OSWriteLittleInt32(regs->base, mmMM_INDEX * 4, reg * 4);
	OSWriteLittleInt32(regs->base, mmMM_DATA * 4, value);
	IOLockUnlock(regs->indexLock);
}

/* A copy of a ROM image if it starts with the PCI ROM signature. */
static uint8_t *
keepROM(const uint8_t *p, IOByteCount length, IOByteCount *kept)
{
	if (length < 512 || p[0] != 0x55 || p[1] != 0xAA)
		return NULL;
	IOByteCount size = (IOByteCount)p[2] * 512;
	if (size == 0 || size > length)
		size = length;
	if (size > kMaxROM)
		size = kMaxROM;
	uint8_t *copy = (uint8_t *)IOMallocZero(kMaxROM);
	if (copy == NULL)
		return NULL;
	for (IOByteCount i = 0; i < size; i++)	/* bytewise: it may be ROM space */
		copy[i] = p[i];
	*kept = size;
	return copy;
}

/*
 * The option ROM the firmware read at boot, which the loader left in
 * memory under /chosen/pci-roms: found by the device's place, its vendor
 * and device ID checked.
 */
static uint8_t *
readPlatformROM(IOPCIDevice *pci, IOByteCount *kept)
{
	IORegistryEntry *roms = IORegistryEntry::fromPath("/chosen/pci-roms", gIODTPlane);
	uint8_t *copy = NULL;

	if (roms == NULL)
		return NULL;
	UInt32 id = pci->configRead32(kIOPCIConfigVendorID);
	OSIterator *it = roms->getChildIterator(gIODTPlane);
	IORegistryEntry *rom;
	while (copy == NULL && it != NULL && (rom = (IORegistryEntry *)it->getNextObject()) != NULL) {
		OSData *loc = OSDynamicCast(OSData, rom->getProperty("location"));
		OSData *reg = OSDynamicCast(OSData, rom->getProperty("reg"));
		OSData *vd = OSDynamicCast(OSData, rom->getProperty("vendor-device"));
		if (loc == NULL || loc->getLength() != 16 || reg == NULL || reg->getLength() != 16 ||
		    vd == NULL || vd->getLength() != 4)
			continue;
		const UInt32 *l = (const UInt32 *)loc->getBytesNoCopy();
		const UInt64 *r = (const UInt64 *)reg->getBytesNoCopy();
		if (*(const UInt32 *)vd->getBytesNoCopy() != id || l[1] != pci->getBusNumber() ||
		    l[2] != pci->getDeviceNumber() || l[3] != pci->getFunctionNumber())
			continue;
		IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(r[0], r[1],
		    kIODirectionIn);
		IOMemoryMap *map = md != NULL ? md->map(kIOMapReadOnly) : NULL;
		if (map != NULL)
			copy = keepROM((const uint8_t *)map->getVirtualAddress(), map->getLength(), kept);
		OSSafeReleaseNULL(map);
		OSSafeReleaseNULL(md);
	}
	OSSafeReleaseNULL(it);
	roms->release();
	return copy;
}

/* The VBIOS from the expansion ROM window, decoded for the copy only. */
static uint8_t *
readExpansionROM(IOPCIDevice *pci, IOByteCount *kept)
{
	UInt32 saved = pci->configRead32(kIOPCIConfigExpansionROMBase);
	IODeviceMemory *rom = pci->getDeviceMemoryWithRegister(kIOPCIConfigExpansionROMBase);
	uint8_t *copy = NULL;

	if (rom == NULL || (saved & ~0x7FFU) == 0) {
		LOG("no expansion ROM window (ROM base 0x%08x)", saved);
		return NULL;
	}
	pci->configWrite32(kIOPCIConfigExpansionROMBase, saved | 1);
	IOMemoryMap *map = rom->map(kIOMapInhibitCache | kIOMapReadOnly);
	if (map != NULL) {
		copy = keepROM((const uint8_t *)map->getVirtualAddress(), map->getLength(), kept);
		if (copy == NULL)
			LOG("expansion ROM has no 0x55AA signature");
		map->release();
	}
	pci->configWrite32(kIOPCIConfigExpansionROMBase, saved);
	return copy;
}

bool
MinkarAMDReadVBIOS(IOPCIDevice *pci, uint8_t **bios, IOByteCount *size, const char **source)
{
	*bios = readPlatformROM(pci, size);
	if (*bios != NULL) {
		*source = "firmware";
		return true;
	}
	*bios = readExpansionROM(pci, size);
	if (*bios != NULL) {
		*source = "expansion ROM";
		return true;
	}
	*source = "none";
	return false;
}

void
MinkarAMDFreeVBIOS(uint8_t *bios)
{
	if (bios != NULL)
		IOFree(bios, kMaxROM);
}
