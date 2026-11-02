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
#include "MinkarAMDGPU.h"

#include "atom_glue.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDGPU: " fmt "\n", ##__VA_ARGS__)

/* Register dword indices (asic_reg/bif). */
#define mmMM_INDEX		0x0000
#define mmMM_DATA		0x0001
#define mmCONFIG_MEMSIZE	0x150A

/* An expansion ROM is at most this big (PCI: a 16-bit count of 512-byte
 * blocks; AMD's are 64 KB or 128 KB). */
#define kMaxROM			(256 * 1024)

#define super IOService
OSDefineMetaClassAndStructors(MinkarAMDGPU, IOService);

/* The interpreter's register access, through the driver. */
static uint32_t atomRead(void *gpu, uint32_t reg)
{
	return ((MinkarAMDGPU *)gpu)->readReg(reg);
}

static void atomWrite(void *gpu, uint32_t reg, uint32_t value)
{
	((MinkarAMDGPU *)gpu)->writeReg(reg, value);
}

/* The family of a device ID (amdgpu's PCI ID table). */
const char *
MinkarAMDGPU::family(UInt16 d, bool *gcn1)
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
	return "not GCN 1.0";
}

uint32_t
MinkarAMDGPU::readReg(uint32_t reg)
{
	if ((IOByteCount)reg * 4 + 4 <= fRegsSize)
		return OSReadLittleInt32(fRegs, reg * 4);
	IOLockLock(fIndexLock);
	OSWriteLittleInt32(fRegs, mmMM_INDEX * 4, reg * 4);
	uint32_t v = OSReadLittleInt32(fRegs, mmMM_DATA * 4);
	IOLockUnlock(fIndexLock);
	return v;
}

void
MinkarAMDGPU::writeReg(uint32_t reg, uint32_t value)
{
	if ((IOByteCount)reg * 4 + 4 <= fRegsSize) {
		OSWriteLittleInt32(fRegs, reg * 4, value);
		return;
	}
	IOLockLock(fIndexLock);
	OSWriteLittleInt32(fRegs, mmMM_INDEX * 4, reg * 4);
	OSWriteLittleInt32(fRegs, mmMM_DATA * 4, value);
	IOLockUnlock(fIndexLock);
}

/* Keeps a copy of a ROM image if it starts with the PCI ROM signature. */
bool
MinkarAMDGPU::keepROM(const uint8_t *p, IOByteCount length)
{
	if (length < 512 || p[0] != 0x55 || p[1] != 0xAA)
		return false;
	IOByteCount size = (IOByteCount)p[2] * 512;
	if (size == 0 || size > length)
		size = length;
	if (size > kMaxROM)
		size = kMaxROM;
	fBIOS = (uint8_t *)IOMallocZero(kMaxROM);
	if (fBIOS == NULL)
		return false;
	for (IOByteCount i = 0; i < size; i++)	/* bytewise: it may be ROM space */
		fBIOS[i] = p[i];
	fBIOSSize = size;
	return true;
}

/*
 * The option ROM the firmware read at boot, which the loader left in
 * memory under /chosen/pci-roms (Image/patches/efiloader-0009): found by
 * the device's place, its vendor and device ID checked.
 */
bool
MinkarAMDGPU::readPlatformROM()
{
	IORegistryEntry *roms = IORegistryEntry::fromPath("/chosen/pci-roms", gIODTPlane);
	bool good = false;

	if (roms == NULL)
		return false;
	UInt32 id = fPCI->configRead32(kIOPCIConfigVendorID);
	OSIterator *it = roms->getChildIterator(gIODTPlane);
	IORegistryEntry *rom;
	while (!good && it != NULL && (rom = (IORegistryEntry *)it->getNextObject()) != NULL) {
		OSData *loc = OSDynamicCast(OSData, rom->getProperty("location"));
		OSData *reg = OSDynamicCast(OSData, rom->getProperty("reg"));
		OSData *vd = OSDynamicCast(OSData, rom->getProperty("vendor-device"));
		if (loc == NULL || loc->getLength() != 16 || reg == NULL || reg->getLength() != 16 ||
		    vd == NULL || vd->getLength() != 4)
			continue;
		const UInt32 *l = (const UInt32 *)loc->getBytesNoCopy();
		const UInt64 *r = (const UInt64 *)reg->getBytesNoCopy();
		if (*(const UInt32 *)vd->getBytesNoCopy() != id || l[1] != fPCI->getBusNumber() ||
		    l[2] != fPCI->getDeviceNumber() || l[3] != fPCI->getFunctionNumber())
			continue;
		IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(r[0], r[1],
		    kIODirectionIn);
		IOMemoryMap *map = md != NULL ? md->map(kIOMapReadOnly) : NULL;
		if (map != NULL)
			good = keepROM((const uint8_t *)map->getVirtualAddress(), map->getLength());
		OSSafeReleaseNULL(map);
		OSSafeReleaseNULL(md);
	}
	OSSafeReleaseNULL(it);
	roms->release();
	return good;
}

/* The VBIOS from the expansion ROM window, decoded for the copy only. */
bool
MinkarAMDGPU::readExpansionROM()
{
	UInt32 saved = fPCI->configRead32(kIOPCIConfigExpansionROMBase);
	IODeviceMemory *rom = fPCI->getDeviceMemoryWithRegister(kIOPCIConfigExpansionROMBase);

	if (rom == NULL || (saved & ~0x7FFU) == 0) {
		LOG("no expansion ROM window (ROM base 0x%08x)", saved);
		return false;
	}
	fPCI->configWrite32(kIOPCIConfigExpansionROMBase, saved | 1);
	IOMemoryMap *map = rom->map(kIOMapInhibitCache | kIOMapReadOnly);
	bool good = false;
	if (map != NULL) {
		good = keepROM((const uint8_t *)map->getVirtualAddress(), map->getLength());
		if (!good)
			LOG("expansion ROM has no 0x55AA signature");
		map->release();
	}
	fPCI->configWrite32(kIOPCIConfigExpansionROMBase, saved);
	return good;
}

bool
MinkarAMDGPU::start(IOService *provider)
{
	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || !super::start(provider))
		return false;
	fPCI->retain();

	UInt16 device = fPCI->configRead16(kIOPCIConfigDeviceID);
	bool gcn1 = false;
	const char *name = family(device, &gcn1);
	setProperty("MinkarGPUFamily", name);
	LOG("device 0x%04x: %s", device, name);

	/* Registers: BAR 2 on GCN 1.0, BAR 5 after it. */
	fPCI->setMemoryEnable(true);
	UInt8 bar = gcn1 ? kIOPCIConfigBaseAddress2 : kIOPCIConfigBaseAddress5;
	fMMIO = fPCI->mapDeviceMemoryWithRegister(bar, kIOMapInhibitCache);
	fIndexLock = IOLockAlloc();
	if (fMMIO != NULL && fIndexLock != NULL) {
		fRegs = (volatile uint8_t *)fMMIO->getVirtualAddress();
		fRegsSize = fMMIO->getLength();
		setProperty("MinkarRegisterWindowSize", (unsigned long long)fRegsSize, 32);
		if (gcn1) {
			uint32_t mb = readReg(mmCONFIG_MEMSIZE);
			setProperty("MinkarVRAMMegabytes", mb, 32);
			LOG("register window %llu KB, VRAM %u MB",
			    (unsigned long long)fRegsSize / 1024, mb);
		}
	} else
		LOG("cannot map the register window (config 0x%02x)", bar);

	/* The VBIOS, parsed by the interpreter. */
	const char *source = NULL;
	if (readPlatformROM())
		source = "firmware";
	else if (readExpansionROM())
		source = "expansion ROM";
	if (source != NULL) {
		setProperty("MinkarVBIOSSource", source);
		setProperty("MinkarVBIOSSize", (unsigned long long)fBIOSSize, 32);
		struct minkar_vbios_info info;
		fAtom = minkar_atom_open(this, atomRead, atomWrite, fBIOS, &info);
		if (fAtom != NULL) {
			setProperty("MinkarVBIOSPartNumber", info.part_number);
			setProperty("MinkarVBIOSVersion", info.version);
			setProperty("MinkarVBIOSDate", info.date);
			LOG("AtomBIOS %s, version %s", info.part_number, info.version);
		} else
			LOG("the VBIOS is not an AtomBIOS image the interpreter takes");
	} else
		setProperty("MinkarVBIOSSource", "none");

	registerService();
	return true;
}

void
MinkarAMDGPU::stop(IOService *provider)
{
	super::stop(provider);
}

void
MinkarAMDGPU::free()
{
	minkar_atom_close(fAtom);
	if (fBIOS != NULL)
		IOFree(fBIOS, kMaxROM);
	if (fIndexLock != NULL)
		IOLockFree(fIndexLock);
	OSSafeReleaseNULL(fMMIO);
	OSSafeReleaseNULL(fPCI);
	super::free();
}
