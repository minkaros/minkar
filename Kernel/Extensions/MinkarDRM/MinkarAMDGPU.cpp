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
#include <pexpert/pexpert.h>
#include "MinkarAMDGPU.h"
#include "MinkarAMDDMA.h"
#include "MinkarAMDGART.h"

#include "atom_glue.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDGPU: " fmt "\n", ##__VA_ARGS__)

#define mmCONFIG_MEMSIZE	0x150A
#define mmMC_VM_FB_LOCATION	0x0809

/* VRAM below this is the firmware's framebuffer and ours
 * (MinkarAMDFramebuffer's, at 32 MB): never handed out. */
#define kVRAMReservedLow	(64ULL << 20)
/* The DMA engine's test region, from the allocator, in the VRAM window. */
#define kDMARegionSize		(32ULL << 20)
#define kDMATestBytes		(8ULL << 20)

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

uint32_t
MinkarAMDGPU::readReg(uint32_t reg)
{
	return MinkarAMDRead(&fRegs, reg);
}

void
MinkarAMDGPU::writeReg(uint32_t reg, uint32_t value)
{
	MinkarAMDWrite(&fRegs, reg, value);
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
	const char *name = MinkarAMDFamily(device, &gcn1);
	setProperty("MinkarGPUFamily", name);
	LOG("device 0x%04x: %s", device, name);

	if (MinkarAMDMapRegisters(fPCI, gcn1, &fRegs)) {
		setProperty("MinkarRegisterWindowSize", (unsigned long long)fRegs.size, 32);
		if (gcn1) {
			uint32_t mb = readReg(mmCONFIG_MEMSIZE);
			setProperty("MinkarVRAMMegabytes", mb, 32);
			setUpVRAM(mb);
			fVRAMStart = (uint64_t)(readReg(mmMC_VM_FB_LOCATION) & 0xFFFF) << 24;
			fWindow = MinkarAMDBARRange(fPCI, kIOPCIConfigBaseAddress0);
			fDMALock = IOLockAlloc();
			if (fWindow != NULL && fDMALock != NULL && fVRAMLock != NULL)
				setProperty("IOUserClientClass", "MinkarAMDUserClient");
			LOG("register window %llu KB, VRAM %u MB",
			    (unsigned long long)fRegs.size / 1024, mb);
		}
	} else
		LOG("cannot map the register window");

	/* The VBIOS, parsed by the interpreter (which runs no table here, so
	 * it needs no registers). */
	const char *source = "none";
	if (MinkarAMDReadVBIOS(fPCI, &fBIOS, &fBIOSSize, &source)) {
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
	}
	setProperty("MinkarVBIOSSource", source);

	if (gcn1 && fRegs.base != NULL)
		dmaSelfTest();

	registerService();
	return true;
}

/* VRAM's allocator: all of it, less what the firmware and the display
 * use at its start. */
void
MinkarAMDGPU::setUpVRAM(uint32_t megabytes)
{
	fVRAMLock = IOLockAlloc();
	minkar_vram_init(&fVRAM, (uint64_t)megabytes << 20, fVRAMRanges, kVRAMRanges);
	if (fVRAMLock == NULL || megabytes == 0 ||
	    minkar_vram_reserve(&fVRAM, 0, kVRAMReservedLow) != 0) {
		LOG("VRAM allocator not set up (%u MB)", megabytes);
		minkar_vram_init(&fVRAM, 0, fVRAMRanges, kVRAMRanges);
		return;
	}
	setProperty("MinkarVRAMFreeMegabytes", (unsigned long long)(fVRAM.freeBytes >> 20), 32);
}

/* ---- For the user client ---- */

/* The DMA engine for clients' copies, started at the first: its ring and
 * write-back in 1 MB of VRAM from the allocator. Called locked. */
bool
MinkarAMDGPU::startDMA()
{
	if (fDMAStarted)
		return true;
	IOLockLock(fVRAMLock);
	bool have = minkar_vram_alloc(&fVRAM, 1ULL << 20, 1ULL << 20, &fDMAOffset) == 0;
	IOLockUnlock(fVRAMLock);
	if (!have)
		return false;
	IODeviceMemory *region = fDMAOffset + (1ULL << 20) <= fWindow->getLength() ?
	    IODeviceMemory::withSubRange(fWindow, fDMAOffset, 1ULL << 20) : NULL;
	fDMAMap = region != NULL ? region->map(kIOMapWriteCombineCache) : NULL;
	OSSafeReleaseNULL(region);
	if (fDMAMap != NULL)
		fDMAStarted = MinkarAMDDMAStart(&fDMA, &fRegs, 0, fVRAMStart + fDMAOffset,
		    (volatile uint8_t *)fDMAMap->getVirtualAddress(), 1ULL << 20);
	if (!fDMAStarted) {
		LOG("the DMA engine did not start");
		OSSafeReleaseNULL(fDMAMap);
		IOLockLock(fVRAMLock);
		minkar_vram_free(&fVRAM, fDMAOffset, 1ULL << 20);
		IOLockUnlock(fVRAMLock);
	}
	return fDMAStarted;
}

IOReturn
MinkarAMDGPU::vramInfo(uint64_t *total, uint64_t *available, uint64_t *window)
{
	if (fWindow == NULL || fVRAMLock == NULL)
		return kIOReturnNotReady;
	IOLockLock(fVRAMLock);
	*total = fVRAM.size;
	*available = fVRAM.freeBytes;
	IOLockUnlock(fVRAMLock);
	*window = fWindow->getLength();
	return kIOReturnSuccess;
}

/* In 64 KB units, inside the CPU's window. */
IOReturn
MinkarAMDGPU::createBuffer(uint64_t size, uint64_t *offset)
{
	const uint64_t unit = 64 * 1024;

	if (fWindow == NULL || fVRAMLock == NULL)
		return kIOReturnNotReady;
	if (size == 0 || size > fWindow->getLength())
		return kIOReturnBadArgument;
	size = (size + unit - 1) & ~(unit - 1);
	IOLockLock(fVRAMLock);
	int err = minkar_vram_alloc(&fVRAM, size, unit, offset);
	if (err == 0 && *offset + size > fWindow->getLength()) {
		minkar_vram_free(&fVRAM, *offset, size);
		err = -1;
	}
	IOLockUnlock(fVRAMLock);
	return err == 0 ? kIOReturnSuccess : kIOReturnNoMemory;
}

void
MinkarAMDGPU::destroyBuffer(uint64_t offset, uint64_t size)
{
	const uint64_t unit = 64 * 1024;

	IOLockLock(fVRAMLock);
	minkar_vram_free(&fVRAM, offset, (size + unit - 1) & ~(unit - 1));
	IOLockUnlock(fVRAMLock);
}

IODeviceMemory *
MinkarAMDGPU::bufferMemory(uint64_t offset, uint64_t size)
{
	if (fWindow == NULL || offset + size > fWindow->getLength())
		return NULL;
	return IODeviceMemory::withSubRange(fWindow, offset, size);
}

IOReturn
MinkarAMDGPU::copy(uint64_t dst, uint64_t src, uint64_t bytes, uint64_t *ns)
{
	if (fDMALock == NULL)
		return kIOReturnNotReady;
	IOLockLock(fDMALock);
	IOReturn err = kIOReturnNotReady;
	if (startDMA())
		err = MinkarAMDDMACopyAddresses(&fDMA, fVRAMStart + dst, fVRAMStart + src, bytes, ns) ?
		    kIOReturnSuccess : kIOReturnTimeout;
	IOLockUnlock(fDMALock);
	return err;
}

/* Only inside the register window: MM_INDEX/MM_DATA would need writes. */
IOReturn
MinkarAMDGPU::readWindowRegister(uint32_t reg, uint32_t *value)
{
	if (fRegs.base == NULL || (IOByteCount)reg * 4 + 4 > fRegs.size)
		return kIOReturnBadArgument;
	*value = readReg(reg);
	return kIOReturnSuccess;
}

/*
 * G2's first check, asked for with "minkaramd_dmatest": 1 runs it on the
 * GPUs that light no display, 2 on every GPU. DMA0 copies 8 MB between
 * two surfaces in VRAM; the results go to MinkarDMATest.
 */
void
MinkarAMDGPU::dmaSelfTest()
{
	UInt32 mode = 0;
	PE_Video boot;

	if (!PE_parse_boot_argn("minkaramd_dmatest", &mode, sizeof(mode)) || mode == 0 ||
	    fVRAMLock == NULL)
		return;
	IODeviceMemory *vram = MinkarAMDBARRange(fPCI, kIOPCIConfigBaseAddress0);
	if (vram == NULL) {
		LOG("DMA test: no VRAM window");
		return;
	}
	bool display = false;
	if (getPlatform() != NULL && getPlatform()->getConsoleInfo(&boot) == kIOReturnSuccess &&
	    boot.v_baseAddr >= vram->getPhysicalAddress() &&
	    boot.v_baseAddr < vram->getPhysicalAddress() + vram->getLength())
		display = true;
	if (display && mode < 2) {
		LOG("DMA test: skipped on the GPU that lights the display");
		vram->release();
		return;
	}
	OSDictionary *results = OSDictionary::withCapacity(8);
	/* The region from the allocator: inside the window, for the CPU's
	 * checks, 1 MB aligned. */
	uint64_t offset = 0;
	IOLockLock(fVRAMLock);
	bool have = minkar_vram_alloc(&fVRAM, kDMARegionSize, 1ULL << 20, &offset) == 0;
	IOLockUnlock(fVRAMLock);
	IODeviceMemory *region = have && offset + kDMARegionSize <= vram->getLength() ?
	    IODeviceMemory::withSubRange(vram, offset, kDMARegionSize) : NULL;
	IOMemoryMap *map = region != NULL ? region->map(kIOMapWriteCombineCache) : NULL;
	uint64_t vramStart = (uint64_t)(readReg(mmMC_VM_FB_LOCATION) & 0xFFFF) << 24;
	struct MinkarAMDDMA dma;
	if (results != NULL && map != NULL) {
		bool started = MinkarAMDDMAStart(&dma, &fRegs, 0, vramStart + offset,
		    (volatile uint8_t *)map->getVirtualAddress(), kDMARegionSize);
		results->setObject("Started", started ? kOSBooleanTrue : kOSBooleanFalse);
		if (started) {
			bool ok = MinkarAMDDMASelfTest(&dma, kDMATestBytes, results);
			results->setObject("Passed", ok ? kOSBooleanTrue : kOSBooleanFalse);
			UInt32 gart = 0;
			if (ok && PE_parse_boot_argn("minkaramd_garttest", &gart, sizeof(gart)) &&
			    gart != 0 && (!display || gart >= 2))
				gartSelfTest(&dma, vram, offset, vramStart);
			MinkarAMDDMAStop(&dma);
		}
		setProperty("MinkarDMATest", results);
	} else
		LOG("DMA test: cannot map the region");
	OSSafeReleaseNULL(results);
	OSSafeReleaseNULL(map);
	OSSafeReleaseNULL(region);
	if (have) {
		IOLockLock(fVRAMLock);
		minkar_vram_free(&fVRAM, offset, kDMARegionSize);
		IOLockUnlock(fVRAMLock);
	}
	vram->release();
}

/*
 * G2's GART check ("minkaramd_garttest", with the DMA check; 1 on the GPUs
 * that light no display, 2 on every GPU): the GART on, 1 MB of system
 * memory in it, copied by the DMA engine to VRAM and back, each side
 * checked by the CPU, then the GART off as the firmware had it. Results
 * in MinkarGARTTest.
 */
void
MinkarAMDGPU::gartSelfTest(struct MinkarAMDDMA *dma, IODeviceMemory *window,
    uint64_t region, uint64_t vramStart)
{
	const uint64_t gartSize = 256ULL << 20, bytes = 1ULL << 20;
	const uint64_t tableBytes = MinkarAMDGARTTableBytes(gartSize);
	OSDictionary *results = OSDictionary::withCapacity(6);
	uint64_t table = 0, scratch = 0;
	struct MinkarAMDGART gart;

	if (results == NULL)
		return;
	IOLockLock(fVRAMLock);
	bool have = minkar_vram_alloc(&fVRAM, tableBytes, 64 * 1024, &table) == 0;
	if (have && minkar_vram_alloc(&fVRAM, 4096, 4096, &scratch) != 0) {
		minkar_vram_free(&fVRAM, table, tableBytes);
		have = false;
	}
	IOLockUnlock(fVRAMLock);
	IOBufferMemoryDescriptor *sys = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
	    kIODirectionInOut, bytes, 4096);
	bool on = have && sys != NULL && sys->prepare() == kIOReturnSuccess &&
	    MinkarAMDGARTEnable(&gart, &fRegs, window, gartSize, table, scratch);
	results->setObject("Enabled", on ? kOSBooleanTrue : kOSBooleanFalse);

	uint64_t gpu = 0, ns = 0, badIn = 0, badOut = 0;
	bool mapped = on && MinkarAMDGARTMap(&gart, 0, sys, &gpu);
	bool in = false, out = false;
	if (mapped) {
		volatile uint32_t *s = (volatile uint32_t *)sys->getBytesNoCopy();
		/* The VRAM side: 16 MB into the DMA check's region. */
		uint64_t vramSide = region + (16ULL << 20);
		IODeviceMemory *v = IODeviceMemory::withSubRange(window, vramSide, bytes);
		IOMemoryMap *vm = v != NULL ? v->map(kIOMapWriteCombineCache) : NULL;
		OSSafeReleaseNULL(v);
		volatile uint32_t *t = vm != NULL ? (volatile uint32_t *)(uintptr_t)vm->getVirtualAddress() : NULL;
		uint64_t words = bytes / 4;
		if (t != NULL) {
			/* System memory to VRAM, through the GART. */
			for (uint64_t i = 0; i < words; i++) {
				s[i] = (uint32_t)(i * 2246822519u) ^ 0x3c3c3c3c;
				t[i] = 0;
			}
			OSSynchronizeIO();
			in = MinkarAMDDMACopyAddresses(dma, vramStart + vramSide, gpu, bytes, &ns);
			if (in) {
				MinkarAMDWrite(&fRegs, 0x0bcc, 1);	/* HDP_DEBUG0: invalidate */
				(void)MinkarAMDRead(&fRegs, 0x0bcc);
				for (uint64_t i = 0; i < words; i += 16)
					if (t[i] != ((uint32_t)(i * 2246822519u) ^ 0x3c3c3c3c))
						badIn++;
				results->setObject("ToVRAM", badIn == 0 ? kOSBooleanTrue : kOSBooleanFalse);
				OSNumber *n = OSNumber::withNumber((unsigned long long)ns, 64);
				if (n != NULL) { results->setObject("ToVRAMNanoseconds", n); n->release(); }
			}
			/* And back: VRAM to system memory, snooped by the CPU. */
			for (uint64_t i = 0; i < words; i++)
				s[i] = 0;
			OSSynchronizeIO();
			out = MinkarAMDDMACopyAddresses(dma, gpu, vramStart + vramSide, bytes, &ns);
			if (out) {
				for (uint64_t i = 0; i < words; i++)
					if (s[i] != ((uint32_t)(i * 2246822519u) ^ 0x3c3c3c3c))
						badOut++;
				results->setObject("ToSystem", badOut == 0 ? kOSBooleanTrue : kOSBooleanFalse);
			}
		}
		OSSafeReleaseNULL(vm);
		MinkarAMDGARTUnmap(&gart, 0, (uint32_t)(bytes / 4096));
	}
	if (on)
		MinkarAMDGARTDisable(&gart);
	bool passed = mapped && in && out && badIn == 0 && badOut == 0;
	results->setObject("Passed", passed ? kOSBooleanTrue : kOSBooleanFalse);
	LOG("GART test: %s (to VRAM %s, %llu bad; to system %s, %llu bad)",
	    passed ? "passed" : "FAILED", in ? "copied" : "not copied", (unsigned long long)badIn,
	    out ? "copied" : "not copied", (unsigned long long)badOut);
	setProperty("MinkarGARTTest", results);
	results->release();
	if (sys != NULL) {
		sys->complete();
		sys->release();
	}
	if (have) {
		IOLockLock(fVRAMLock);
		minkar_vram_free(&fVRAM, table, tableBytes);
		minkar_vram_free(&fVRAM, scratch, 4096);
		IOLockUnlock(fVRAMLock);
	}
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
	MinkarAMDFreeVBIOS(fBIOS);
	if (fDMAStarted)
		MinkarAMDDMAStop(&fDMA);
	OSSafeReleaseNULL(fDMAMap);
	OSSafeReleaseNULL(fWindow);
	MinkarAMDUnmapRegisters(&fRegs);
	if (fVRAMLock != NULL)
		IOLockFree(fVRAMLock);
	if (fDMALock != NULL)
		IOLockFree(fDMALock);
	OSSafeReleaseNULL(fPCI);
	super::free();
}
