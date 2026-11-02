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

#include "atom_glue.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDGPU: " fmt "\n", ##__VA_ARGS__)

#define mmCONFIG_MEMSIZE	0x150A
#define mmMC_VM_FB_LOCATION	0x0809

/* The DMA engine's test region: past the firmware's framebuffer and ours
 * (MinkarAMDFramebuffer's, at 32 MB), inside the VRAM window. */
#define kDMARegionOffset	(64ULL << 20)
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

	if (!PE_parse_boot_argn("minkaramd_dmatest", &mode, sizeof(mode)) || mode == 0)
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
	IODeviceMemory *region = kDMARegionOffset + kDMARegionSize <= vram->getLength() ?
	    IODeviceMemory::withSubRange(vram, kDMARegionOffset, kDMARegionSize) : NULL;
	IOMemoryMap *map = region != NULL ? region->map(kIOMapWriteCombineCache) : NULL;
	uint64_t vramStart = (uint64_t)(readReg(mmMC_VM_FB_LOCATION) & 0xFFFF) << 24;
	struct MinkarAMDDMA dma;
	if (results != NULL && map != NULL) {
		bool started = MinkarAMDDMAStart(&dma, &fRegs, 0, vramStart + kDMARegionOffset,
		    (volatile uint8_t *)map->getVirtualAddress(), kDMARegionSize);
		results->setObject("Started", started ? kOSBooleanTrue : kOSBooleanFalse);
		if (started) {
			bool ok = MinkarAMDDMASelfTest(&dma, kDMATestBytes, results);
			results->setObject("Passed", ok ? kOSBooleanTrue : kOSBooleanFalse);
			MinkarAMDDMAStop(&dma);
		}
		setProperty("MinkarDMATest", results);
	} else
		LOG("DMA test: cannot map the region");
	OSSafeReleaseNULL(results);
	OSSafeReleaseNULL(map);
	OSSafeReleaseNULL(region);
	vram->release();
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
	MinkarAMDUnmapRegisters(&fRegs);
	OSSafeReleaseNULL(fPCI);
	super::free();
}
