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
#include "MinkarAMDGPU.h"
#include "MinkarAMDFramebuffer.h"
#include "MinkarAMDDMA.h"
#include "MinkarAMDFirmware.h"
#include "MinkarAMDGFX.h"
#include "amd_si_gfx.h"
#include "amd_si_vm.h"
#include "amd_si_dma.h"
#include "MinkarAMDGART.h"
#include "amd_si_ih.h"

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

	if (gcn1 && fRegs.base != NULL) {
		dmaSelfTest();
		bool display = lightsDisplay();
		setProperty("MinkarLightsDisplay", display);
		startGART(display);
		startInterrupts(display);
		startGFXTest(name);
	}

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

/* Whether the firmware's framebuffer, the boot display, is in this GPU's
 * VRAM window. */
bool
MinkarAMDGPU::lightsDisplay()
{
	PE_Video boot;
	IODeviceMemory *vram = MinkarAMDBARRange(fPCI, kIOPCIConfigBaseAddress0);
	bool display = vram != NULL && getPlatform() != NULL &&
	    getPlatform()->getConsoleInfo(&boot) == kIOReturnSuccess &&
	    boot.v_baseAddr >= vram->getPhysicalAddress() &&
	    boot.v_baseAddr < vram->getPhysicalAddress() + vram->getLength();
	OSSafeReleaseNULL(vram);
	return display;
}

/*
 * The GART, on for the driver's life when asked for ("minkaramd_gart": 1
 * on the GPUs that light no display, 2 on every GPU), for clients'
 * system buffers.
 */
void
MinkarAMDGPU::startGART(bool display)
{
	const uint64_t size = 256ULL << 20, tableBytes = MinkarAMDGARTTableBytes(size);
	UInt32 mode = 0;

	if (!PE_parse_boot_argn("minkaramd_gart", &mode, sizeof(mode)) || mode == 0 ||
	    (display && mode < 2) || fWindow == NULL || fVRAMLock == NULL)
		return;
	IOLockLock(fVRAMLock);
	bool have = minkar_vram_alloc(&fVRAM, tableBytes, 64 * 1024, &fGARTTable) == 0;
	if (have && minkar_vram_alloc(&fVRAM, 4096, 4096, &fGARTScratch) != 0) {
		minkar_vram_free(&fVRAM, fGARTTable, tableBytes);
		have = false;
	}
	IOLockUnlock(fVRAMLock);
	if (!have || !MinkarAMDGARTEnable(&fGART, &fRegs, fWindow, size, fGARTTable, fGARTScratch)) {
		LOG("the GART did not start");
		if (have) {
			IOLockLock(fVRAMLock);
			minkar_vram_free(&fVRAM, fGARTTable, tableBytes);
			minkar_vram_free(&fVRAM, fGARTScratch, 4096);
			IOLockUnlock(fVRAMLock);
		}
		return;
	}
	minkar_vram_init(&fGARTPages, size, fGARTRanges, kGARTRanges);
	setProperty("MinkarGARTMegabytes", (unsigned long long)(size >> 20), 32);
}

/* ---- Interrupts ---- */

#define kIHRingBytes	(64 * 1024)

/*
 * The interrupt ring, on when asked for ("minkaramd_irq": 1 on the GPUs
 * that light no display, 2 on every GPU) and the GART is on: the ring and
 * its write-back in system memory through the GART (si_ih_irq_init), the
 * MSI (or the pin) on a work loop of the GPU's own.
 */
void
MinkarAMDGPU::startInterrupts(bool display)
{
	UInt32 mode = 0;

	if (!PE_parse_boot_argn("minkaramd_irq", &mode, sizeof(mode)) || mode == 0 ||
	    (display && mode < 2) || !fGART.on)
		return;
	fTrapLock = IOLockAlloc();
	fWorkLoop = IOWorkLoop::workLoop();
	fIHMemory = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionInOut,
	    kIHRingBytes + 4096, 4096);
	if (fTrapLock == NULL || fWorkLoop == NULL || fIHMemory == NULL ||
	    fIHMemory->prepare() != kIOReturnSuccess) {
		LOG("interrupts: no memory");
		return;
	}
	bzero(fIHMemory->getBytesNoCopy(), kIHRingBytes + 4096);
	if (mapSystemMemory(fIHMemory, &fIHGPU) != kIOReturnSuccess) {
		LOG("interrupts: the ring is not in the GART");
		return;
	}

	/* MSI when there is one, else the pin. */
	int index = 0, type;
	bool msi = false;
	OSArray *controllers = OSDynamicCast(OSArray, fPCI->getProperty(gIOInterruptControllersKey));
	for (unsigned i = 0; controllers != NULL && i < controllers->getCount(); i++)
		if (fPCI->getInterruptType((int)i, &type) == kIOReturnSuccess &&
		    (type & kIOInterruptTypePCIMessaged)) {
			index = (int)i;
			msi = true;
			break;
		}
	fInterrupt = IOFilterInterruptEventSource::filterInterruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventSource::Action, this, &MinkarAMDGPU::interruptHandler),
	    OSMemberFunctionCast(IOFilterInterruptEventSource::Filter, this, &MinkarAMDGPU::interruptFilter),
	    fPCI, index);
	if (fInterrupt == NULL || fWorkLoop->addEventSource(fInterrupt) != kIOReturnSuccess) {
		LOG("interrupts: no interrupt source");
		OSSafeReleaseNULL(fInterrupt);
		return;
	}

	/* si_ih_irq_init. */
	addr64_t dummy = fIHMemory->getPhysicalSegment(kIHRingBytes, NULL, kIOMemoryMapperNone);
	writeReg(mmSI_IH_RB_CNTL, readReg(mmSI_IH_RB_CNTL) & ~SI_IH_RB_ENABLE);
	writeReg(mmSI_IH_CNTL, readReg(mmSI_IH_CNTL) & ~SI_IH_ENABLE_INTR);
	writeReg(mmSI_IH_RB_RPTR, 0);
	writeReg(mmSI_IH_RB_WPTR, 0);
	writeReg(mmSI_INTERRUPT_CNTL2, (uint32_t)(dummy >> 8));
	writeReg(mmSI_INTERRUPT_CNTL, readReg(mmSI_INTERRUPT_CNTL) &
	    ~(SI_IH_DUMMY_RD_OVERRIDE | SI_IH_REQ_NONSNOOP_EN));
	writeReg(mmSI_IH_RB_BASE, (uint32_t)(fIHGPU >> 8));
	uint64_t wptrAddr = fIHGPU + kIHRingBytes;
	writeReg(mmSI_IH_RB_WPTR_ADDR_LO, (uint32_t)wptrAddr);
	writeReg(mmSI_IH_RB_WPTR_ADDR_HI, (uint32_t)(wptrAddr >> 32) & 0xff);
	writeReg(mmSI_IH_RB_CNTL, si_ih_rb_cntl(kIHRingBytes));
	writeReg(mmSI_IH_RB_RPTR, 0);
	writeReg(mmSI_IH_RB_WPTR, 0);
	writeReg(mmSI_IH_CNTL, si_ih_cntl(msi));
	fPCI->setBusMasterEnable(true);
	fIHRptr = 0;
	fInterrupt->enable();
	writeReg(mmSI_IH_CNTL, readReg(mmSI_IH_CNTL) | SI_IH_ENABLE_INTR);
	writeReg(mmSI_IH_RB_CNTL, readReg(mmSI_IH_RB_CNTL) | SI_IH_RB_ENABLE);
	fIHOn = true;
	setProperty("MinkarInterruptType", msi ? "MSI" : "pin");
	LOG("interrupts on (%s), ring at GPU address 0x%llx", msi ? "MSI" : "pin",
	    (unsigned long long)fIHGPU);
}

void
MinkarAMDGPU::stopInterrupts()
{
	if (fIHOn) {
		writeReg(mmSI_IH_RB_CNTL, readReg(mmSI_IH_RB_CNTL) & ~SI_IH_RB_ENABLE);
		writeReg(mmSI_IH_CNTL, readReg(mmSI_IH_CNTL) & ~SI_IH_ENABLE_INTR);
		writeReg(mmSI_IH_RB_RPTR, 0);
		writeReg(mmSI_IH_RB_WPTR, 0);
		fIHOn = false;
	}
	if (fInterrupt != NULL) {
		fInterrupt->disable();
		fWorkLoop->removeEventSource(fInterrupt);
		OSSafeReleaseNULL(fInterrupt);
	}
	if (fIHMemory != NULL) {
		if (fIHGPU != 0)
			unmapSystemMemory(fIHGPU, kIHRingBytes + 4096);
		fIHMemory->complete();
		OSSafeReleaseNULL(fIHMemory);
	}
	OSSafeReleaseNULL(fWorkLoop);
	if (fTrapLock != NULL) {
		IOLockFree(fTrapLock);
		fTrapLock = NULL;
	}
}

/* Primary interrupt time: the work is on the work loop. */
bool
MinkarAMDGPU::interruptFilter(IOFilterInterruptEventSource *)
{
	return fIHOn;
}

static void
onEntry(const struct si_ih_entry *e, void *info)
{
	uint64_t *traps = (uint64_t *)info;

	if (e->source == kSISourceDMA0Trap || e->source == kSISourceDMA1Trap)
		(*traps)++;
}

/* The entries since the last time; traps wake the fence waits. */
void
MinkarAMDGPU::interruptHandler(IOInterruptEventSource *, int)
{
	volatile uint32_t *ring = (volatile uint32_t *)fIHMemory->getBytesNoCopy();
	uint32_t wptr = ring[kIHRingBytes / 4];	/* the write-back, snooped */
	uint64_t traps = 0;
	int overflowed;

	fIHRptr = si_ih_drain(ring, kIHRingBytes, fIHRptr, wptr, &overflowed, onEntry, &traps);
	if (overflowed) {
		/* Clear it at once, so the next overflow is seen (si_ih_get_wptr). */
		uint32_t c = readReg(mmSI_IH_RB_CNTL);
		writeReg(mmSI_IH_RB_CNTL, c | SI_IH_WPTR_OVERFLOW_CLEAR);
		writeReg(mmSI_IH_RB_CNTL, c & ~SI_IH_WPTR_OVERFLOW_CLEAR);
		fIHOverflows++;
	}
	writeReg(mmSI_IH_RB_RPTR, fIHRptr);	/* re-arms the MSI */
	IOLockLock(fTrapLock);
	fInterrupts++;
	fDMATraps += traps;
	if (traps > 0)
		IOLockWakeup(fTrapLock, &fDMATraps, false);
	IOLockUnlock(fTrapLock);
}

/* A fence wait between looks: until a trap, at most 1 ms. */
void
MinkarAMDGPU::sleepForTrap(void *info)
{
	MinkarAMDGPU *me = (MinkarAMDGPU *)info;
	AbsoluteTime deadline;

	clock_interval_to_deadline(1, kMillisecondScale, &deadline);
	IOLockLock(me->fTrapLock);
	IOLockSleepDeadline(me->fTrapLock, &me->fDMATraps, deadline, THREAD_UNINT);
	IOLockUnlock(me->fTrapLock);
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
	if (fDMAStarted && fIHOn)
		MinkarAMDDMAUseInterrupts(&fDMA, sleepForTrap, this);
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
		err = MinkarAMDDMACopyAddresses(&fDMA, dst, src, bytes, ns) ?
		    kIOReturnSuccess : kIOReturnTimeout;
	IOLockUnlock(fDMALock);
	if (fIHOn) {
		IOLockLock(fTrapLock);
		uint64_t interrupts = fInterrupts, traps = fDMATraps;
		IOLockUnlock(fTrapLock);
		setProperty("MinkarInterrupts", (unsigned long long)interrupts, 64);
		setProperty("MinkarDMATraps", (unsigned long long)traps, 64);
	}
	return err;
}

/* Prepared memory's pages into the GART, from the GART's allocator. */
IOReturn
MinkarAMDGPU::mapSystemMemory(IOMemoryDescriptor *m, uint64_t *gpuAddress)
{
	uint64_t offset, size = (m->getLength() + 4095) & ~4095ULL;

	if (!fGART.on)
		return kIOReturnUnsupported;
	if (size == 0 || size > fGARTPages.size)
		return kIOReturnBadArgument;
	IOLockLock(fVRAMLock);
	bool have = minkar_vram_alloc(&fGARTPages, size, 4096, &offset) == 0;
	bool mapped = have && MinkarAMDGARTMap(&fGART, (uint32_t)(offset / 4096), m, gpuAddress);
	if (have && !mapped)
		minkar_vram_free(&fGARTPages, offset, size);
	IOLockUnlock(fVRAMLock);
	return mapped ? kIOReturnSuccess : kIOReturnNoMemory;
}

void
MinkarAMDGPU::unmapSystemMemory(uint64_t gpuAddress, uint64_t size)
{
	uint64_t offset = gpuAddress - fGART.start;

	size = (size + 4095) & ~4095ULL;
	IOLockLock(fVRAMLock);
	MinkarAMDGARTUnmap(&fGART, (uint32_t)(offset / 4096), (uint32_t)(size / 4096));
	minkar_vram_free(&fGARTPages, offset, size);
	IOLockUnlock(fVRAMLock);
}

/* Wired kernel memory the client maps too, its pages in the GART. */
IOReturn
MinkarAMDGPU::createSystemBuffer(uint64_t size, IOBufferMemoryDescriptor **memory,
    uint64_t *gpuAddress)
{
	if (!fGART.on)
		return kIOReturnUnsupported;
	if (size == 0 || size > fGARTPages.size)
		return kIOReturnBadArgument;
	size = (size + 4095) & ~4095ULL;
	IOBufferMemoryDescriptor *m = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task,
	    kIODirectionInOut | kIOMemoryKernelUserShared, size, 4096);
	if (m == NULL)
		return kIOReturnNoMemory;
	if (m->prepare() != kIOReturnSuccess) {
		m->release();
		return kIOReturnNoMemory;
	}
	bzero(m->getBytesNoCopy(), size);
	IOReturn err = mapSystemMemory(m, gpuAddress);
	if (err != kIOReturnSuccess) {
		m->complete();
		m->release();
		return err;
	}
	*memory = m;
	return kIOReturnSuccess;
}

void
MinkarAMDGPU::destroySystemBuffer(IOBufferMemoryDescriptor *m, uint64_t gpuAddress)
{
	unmapSystemMemory(gpuAddress, m->getLength());
	m->complete();
	m->release();
}

/* ---- Shared buffers ---- */

IOReturn
MinkarAMDGPU::createShared(uint64_t size, uint32_t *id, uint64_t *offset, uint64_t *rounded)
{
	const uint64_t unit = 64 * 1024;
	IOReturn err = createBuffer(size, offset);

	if (err != kIOReturnSuccess)
		return err;
	*rounded = (size + unit - 1) & ~(unit - 1);
	IOLockLock(fVRAMLock);
	unsigned slot;
	for (slot = 0; slot < kSharedBuffers && fShared[slot].holders != 0; slot++)
		;
	if (slot < kSharedBuffers) {
		if (++fNextSharedID == 0)
			fNextSharedID = 1;
		fShared[slot].id = fNextSharedID;
		fShared[slot].holders = 1;
		fShared[slot].offset = *offset;
		fShared[slot].size = *rounded;
		*id = fNextSharedID;
	}
	IOLockUnlock(fVRAMLock);
	if (slot == kSharedBuffers) {
		destroyBuffer(*offset, size);
		return kIOReturnNoResources;
	}
	return kIOReturnSuccess;
}

IOReturn
MinkarAMDGPU::retainShared(uint32_t id, uint64_t *offset, uint64_t *size)
{
	IOReturn err = kIOReturnNotFound;

	IOLockLock(fVRAMLock);
	for (unsigned i = 0; i < kSharedBuffers; i++)
		if (fShared[i].holders != 0 && fShared[i].id == id) {
			fShared[i].holders++;
			*offset = fShared[i].offset;
			*size = fShared[i].size;
			err = kIOReturnSuccess;
			break;
		}
	IOLockUnlock(fVRAMLock);
	return err;
}

void
MinkarAMDGPU::releaseShared(uint32_t id)
{
	uint64_t offset = 0, size = 0;

	IOLockLock(fVRAMLock);
	for (unsigned i = 0; i < kSharedBuffers; i++)
		if (fShared[i].holders != 0 && fShared[i].id == id) {
			if (--fShared[i].holders == 0) {
				offset = fShared[i].offset;
				size = fShared[i].size;
			}
			break;
		}
	IOLockUnlock(fVRAMLock);
	if (size != 0)
		destroyBuffer(offset, size);
}

/* The display's framebuffer is MinkarAMDFramebuffer's, on the same PCI
 * device. */
IOReturn
MinkarAMDGPU::copyToScanout(uint64_t src, uint64_t offset, uint64_t bytes, uint64_t *ns)
{
	uint64_t fb = 0, fbBytes = 0;
	bool found = false;

	OSIterator *it = fPCI->getClientIterator();
	OSObject *o;
	while (!found && it != NULL && (o = it->getNextObject()) != NULL) {
		MinkarAMDFramebuffer *f = OSDynamicCast(MinkarAMDFramebuffer, o);
		found = f != NULL && f->scanout(&fb, &fbBytes);
	}
	OSSafeReleaseNULL(it);
	if (!found)
		return kIOReturnNotFound;
	if (bytes == 0 || offset > fbBytes || bytes > fbBytes - offset)
		return kIOReturnBadArgument;
	return copy(fVRAMStart + fb + offset, src, bytes, ns);
}

IOReturn
MinkarAMDGPU::modeSet(const uint64_t *in, uint64_t *out)
{
	IOReturn err = kIOReturnNotFound;
	OSIterator *it = fPCI->getClientIterator();
	OSObject *o;
	while (err == kIOReturnNotFound && it != NULL && (o = it->getNextObject()) != NULL) {
		MinkarAMDFramebuffer *f = OSDynamicCast(MinkarAMDFramebuffer, o);
		if (f != NULL) {
			IOLockLock(fDMALock);
			err = f->bringUpModeSet((uint32_t)in[0], (uint32_t)in[1], out);
			IOLockUnlock(fDMALock);
		}
	}
	OSSafeReleaseNULL(it);
	return err;
}

IOReturn
MinkarAMDGPU::i2cRead(const uint64_t *in, uint64_t *out)
{
	uint8_t data[16];

	if (fAtom == NULL || in[3] == 0 || in[3] > 16 || in[1] > 0x7f || in[2] > 0xff)
		return kIOReturnBadArgument;
	bzero(data, sizeof(data));
	IOLockLock(fDMALock);
	int r = minkar_atom_i2c(fAtom, (uint8_t)in[0], (uint8_t)in[1], 0, (uint8_t)in[2], NULL, 0, data,
	    (uint8_t)in[3]);
	IOLockUnlock(fDMALock);
	out[0] = (uint64_t)(int64_t)r;
	memcpy(&out[1], data, 8);
	memcpy(&out[2], data + 8, 8);
	return kIOReturnSuccess;
}

IOReturn
MinkarAMDGPU::auxTransaction(const uint64_t *in, uint64_t *out, uint32_t *trace,
    uint32_t *traceBytes)
{
	uint8_t data[16], reply = 0, dataOut = 0;
	int execute = 0;

	if (fAtom == NULL || in[4] > 16 || in[1] > 5)
		return kIOReturnBadArgument;
	bzero(data, sizeof(data));
	IOLockLock(fDMALock);	/* one table at a time from here */
	minkar_atom_trace(fAtom, trace != NULL);
	int n = minkar_atom_aux(fAtom, (uint8_t)in[0], (uint8_t)in[1], (uint8_t)in[2],
	    (uint32_t)in[3], data, (uint8_t)in[4], &reply);
	minkar_atom_trace(fAtom, 0);
	minkar_atom_aux_last(fAtom, &execute, &dataOut);
	if (trace != NULL)
		*traceBytes = minkar_atom_trace_get(fAtom, trace, *traceBytes / 8) * 8;
	IOLockUnlock(fDMALock);
	out[0] = (uint64_t)(int64_t)n;
	out[1] = reply;
	out[2] = (uint64_t)(int64_t)execute;
	out[3] = dataOut;
	memcpy(&out[4], data, 8);
	memcpy(&out[5], data + 8, 8);
	return kIOReturnSuccess;
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

	if (!PE_parse_boot_argn("minkaramd_dmatest", &mode, sizeof(mode)) || mode == 0 ||
	    fVRAMLock == NULL)
		return;
	IODeviceMemory *vram = MinkarAMDBARRange(fPCI, kIOPCIConfigBaseAddress0);
	if (vram == NULL) {
		LOG("DMA test: no VRAM window");
		return;
	}
	bool display = lightsDisplay();
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
 * G2's command processor check ("minkaramd_gfxtest": 1 on the GPUs that
 * light no display, 2 on every GPU): AMD's microcode loaded from
 * /usr/share/firmware/amdgpu, ring 0 in VRAM, a scratch register written
 * through it; the results go to MinkarGFXTest. The files are there once the
 * root file system is, so it runs on a thread of its own that waits for
 * them (five minutes at most).
 */
void
MinkarAMDGPU::startGFXTest(const char *family)
{
	UInt32 mode = 0;
	thread_t thread;

	/* The microcode's files are named after the chip, in lower case. */
	size_t n = 0;
	for (; family[n] != '\0' && family[n] != ' ' && n < sizeof(fMicrocodePrefix) - 1; n++)
		fMicrocodePrefix[n] = (char)(family[n] >= 'A' && family[n] <= 'Z' ?
		    family[n] - 'A' + 'a' : family[n]);
	fMicrocodePrefix[n] = '\0';
	if (!PE_parse_boot_argn("minkaramd_gfxtest", &mode, sizeof(mode)) || mode == 0 ||
	    fVRAMLock == NULL || fDMALock == NULL || fWindow == NULL)
		return;
	if (lightsDisplay() && mode < 2) {
		LOG("GFX test: skipped on the GPU that lights the display");
		return;
	}
	retain();
	if (kernel_thread_start(&MinkarAMDGPU::gfxTestThread, this, &thread) != KERN_SUCCESS) {
		release();
		return;
	}
	thread_deallocate(thread);
}

void
MinkarAMDGPU::gfxTestThread(void *gpu, wait_result_t)
{
	MinkarAMDGPU *self = (MinkarAMDGPU *)gpu;
	self->gfxSelfTest();
	self->release();
	thread_terminate(current_thread());
}

/* The command processor's three microcodes (prefetch parser, micro
 * engine, constant engine), read and parsed. */
bool
MinkarAMDGPU::readMicrocode(uint8_t *file[3], size_t bytes[3], struct amd_ucode code[3])
{
	static const char *const parts[3] = { "pfp", "me", "ce" };
	char name[40];

	for (int i = 0; i < 3; i++) {
		file[i] = NULL;
		bytes[i] = 0;
	}
	for (int i = 0; i < 3; i++) {
		snprintf(name, sizeof(name), "%s_%s.bin", fMicrocodePrefix, parts[i]);
		if (!MinkarAMDReadFirmware(name, &file[i], &bytes[i]) ||
		    amd_ucode_parse(file[i], bytes[i], &code[i]) != 0) {
			freeMicrocode(file, bytes);
			return false;
		}
	}
	return true;
}

void
MinkarAMDGPU::freeMicrocode(uint8_t *file[3], size_t bytes[3])
{
	for (int i = 0; i < 3; i++) {
		MinkarAMDFreeFirmware(file[i], bytes[i]);
		file[i] = NULL;
		bytes[i] = 0;
	}
}

void
MinkarAMDGPU::gfxSelfTest()
{
	uint8_t *file[3];
	size_t bytes[3];
	struct amd_ucode code[3];
	char name[40];
	bool have = false;

	for (int tries = 0; tries < 150 && !(have = readMicrocode(file, bytes, code)); tries++)
		IOSleep(2000);
	if (!have) {
		LOG("GFX test: no microcode (%s_*.bin)", fMicrocodePrefix);
		return;
	}
	OSDictionary *results = OSDictionary::withCapacity(24);
	if (results == NULL) {
		freeMicrocode(file, bytes);
		return;
	}
	for (int i = 0; i < 3; i++) {
		snprintf(name, sizeof(name), "Microcode%sVersion", i == 0 ? "PFP" : i == 1 ? "ME" : "CE");
		OSNumber *v = OSNumber::withNumber(code[i].version, 32);
		if (v != NULL) {
			results->setObject(name, v);
			v->release();
		}
	}
	{
		uint64_t offset = 0;
		IOLockLock(fVRAMLock);
		bool ring = minkar_vram_alloc(&fVRAM, kMinkarAMDGFXRegionBytes, 64 * 1024, &offset) == 0;
		IOLockUnlock(fVRAMLock);
		IODeviceMemory *region = ring && offset + kMinkarAMDGFXRegionBytes <= fWindow->getLength() ?
		    IODeviceMemory::withSubRange(fWindow, offset, kMinkarAMDGFXRegionBytes) : NULL;
		IOMemoryMap *map = region != NULL ? region->map(kIOMapWriteCombineCache) : NULL;
		if (map != NULL) {
			struct MinkarAMDGFX gfx;
			uint64_t ns = 0;
			bool started = false, passed = false, ib = false;
			IOLockLock(fDMALock);
			/* Not over the processor clients use. */
			if (!fGFXStarted) {
				MinkarAMDGFXLoadMicrocode(&fRegs, &code[0], &code[1], &code[2]);
				started = MinkarAMDGFXStart(&gfx, &fRegs, fVRAMStart + offset,
				    (volatile uint8_t *)map->getVirtualAddress(), results);
				passed = started && MinkarAMDGFXRingTest(&gfx, &ns, results);
				ib = passed && MinkarAMDGFXIBTest(&gfx, results);
				MinkarAMDGFXStop(&gfx);
			}
			IOLockUnlock(fDMALock);
			results->setObject("Started", started ? kOSBooleanTrue : kOSBooleanFalse);
			results->setObject("Passed", passed ? kOSBooleanTrue : kOSBooleanFalse);
			OSNumber *t = OSNumber::withNumber((unsigned long long)ns, 64);
			if (t != NULL) {
				results->setObject("RingTestNanoseconds", t);
				t->release();
			}
			LOG("GFX test: microcode %u/%u/%u, %s, ring test %s in %llu ns, "
			    "indirect buffer and fence %s", code[0].version, code[1].version,
			    code[2].version, started ? "started" : "not started",
			    passed ? "passed" : "failed", (unsigned long long)ns,
			    ib ? "passed" : "failed");
		} else
			LOG("GFX test: cannot map the ring");
		OSSafeReleaseNULL(map);
		OSSafeReleaseNULL(region);
		if (ring) {
			IOLockLock(fVRAMLock);
			minkar_vram_free(&fVRAM, offset, kMinkarAMDGFXRegionBytes);
			IOLockUnlock(fVRAMLock);
		}
	}
	setProperty("MinkarGFXTest", results);
	results->release();
	freeMicrocode(file, bytes);
}

/*
 * The command processor for clients' indirect buffers, started at the
 * first: its microcode from the files, its ring and write-back in VRAM
 * from the allocator, a ring test before it is used. Called locked.
 */
bool
MinkarAMDGPU::startGFX()
{
	uint8_t *file[3];
	size_t bytes[3];
	struct amd_ucode code[3];

	if (fGFXStarted)
		return true;
	if (!readMicrocode(file, bytes, code)) {
		LOG("no microcode for the command processor (%s_*.bin)", fMicrocodePrefix);
		return false;
	}
	IOLockLock(fVRAMLock);
	bool have = minkar_vram_alloc(&fVRAM, kMinkarAMDGFXRegionBytes, 64 * 1024, &fGFXOffset) == 0;
	IOLockUnlock(fVRAMLock);
	IODeviceMemory *region = have && fGFXOffset + kMinkarAMDGFXRegionBytes <= fWindow->getLength() ?
	    IODeviceMemory::withSubRange(fWindow, fGFXOffset, kMinkarAMDGFXRegionBytes) : NULL;
	fGFXMap = region != NULL ? region->map(kIOMapWriteCombineCache) : NULL;
	OSSafeReleaseNULL(region);
	if (fGFXMap != NULL) {
		uint64_t ns = 0;
		MinkarAMDGFXLoadMicrocode(&fRegs, &code[0], &code[1], &code[2]);
		fGFXStarted = MinkarAMDGFXStart(&fGFX, &fRegs, fVRAMStart + fGFXOffset,
		    (volatile uint8_t *)fGFXMap->getVirtualAddress(), NULL) &&
		    MinkarAMDGFXRingTest(&fGFX, &ns, NULL);
		if (!fGFXStarted)
			MinkarAMDGFXStop(&fGFX);
	}
	freeMicrocode(file, bytes);
	if (!fGFXStarted) {
		LOG("the command processor did not start");
		OSSafeReleaseNULL(fGFXMap);
		if (have) {
			IOLockLock(fVRAMLock);
			minkar_vram_free(&fVRAM, fGFXOffset, kMinkarAMDGFXRegionBytes);
			IOLockUnlock(fVRAMLock);
		}
		return false;
	}
	setProperty("MinkarGFXStarted", true);
	LOG("the command processor runs clients' command buffers");
	return true;
}

IOReturn
MinkarAMDGPU::runGFX(uint64_t address, uint32_t dwords, unsigned vmid, uint64_t *ns,
    uint32_t *fault)
{
	if (fDMALock == NULL || fWindow == NULL || fVRAMLock == NULL || vmid >= kSIVMContexts)
		return kIOReturnNotReady;
	IOLockLock(fDMALock);
	IOReturn err = kIOReturnNotReady;
	if (vmid != 0 && fVMDirectory[vmid] == 0)
		err = kIOReturnBadArgument;
	else if (startGFX()) {
		err = MinkarAMDGFXRunCommands(&fGFX, address, dwords, vmid, ns) ?
		    kIOReturnSuccess : kIOReturnTimeout;
		/* A client's stray access: said, and cleared for the next. */
		uint32_t status = vmid != 0 ? readReg(mmSI_VM_CONTEXT1_PROTECTION_FAULT_STATUS) : 0;
		if (status != 0) {
			LOG("context %u: protection fault 0x%08x at page 0x%08x", si_vm_fault_vmid(status),
			    status, readReg(mmSI_VM_CONTEXT1_PROTECTION_FAULT_ADDR));
			writeReg(mmSI_VM_CONTEXT1_CNTL2, readReg(mmSI_VM_CONTEXT1_CNTL2) | 1);
			writeReg(mmSI_VM_CONTEXT1_CNTL2, readReg(mmSI_VM_CONTEXT1_CNTL2) & ~1u);
		}
		if (fault != NULL)
			*fault = status;
	}
	IOLockUnlock(fDMALock);
	return err;
}

/* ---- Clients' GPU address spaces ---- */

/* VRAM through the window, for the page tables; NULL outside it. */
volatile uint64_t *
MinkarAMDGPU::vramPointer(uint64_t offset, uint64_t bytes)
{
	if (fWindowMap == NULL)
		fWindowMap = fWindow->map(kIOMapWriteCombineCache);
	if (fWindowMap == NULL || offset + bytes > fWindow->getLength())
		return NULL;
	return (volatile uint64_t *)(uintptr_t)(fWindowMap->getVirtualAddress() + offset);
}

/* The tables' writes reach VRAM, then the context's TLBs are dropped.
 * Called with fDMALock held: no command buffer runs meanwhile. */
void
MinkarAMDGPU::vmFlush(unsigned vmid)
{
	OSSynchronizeIO();
	writeReg(mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL, 1);
	(void)readReg(mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL);
	writeReg(mmSI_VM_INVALIDATE_REQUEST, 1u << vmid);
	(void)readReg(mmSI_VM_INVALIDATE_REQUEST);
}

IOReturn
MinkarAMDGPU::createVM(unsigned *vmid)
{
	if (fDMALock == NULL || fVRAMLock == NULL || fWindow == NULL || !fGART.on)
		return kIOReturnNotReady;
	IOLockLock(fDMALock);
	unsigned id = 1;
	while (id < kSIVMContexts && fVMDirectory[id] != 0)
		id++;
	if (id == kSIVMContexts) {
		IOLockUnlock(fDMALock);
		return kIOReturnNoResources;
	}
	uint64_t pd = 0;
	IOLockLock(fVRAMLock);
	bool have = minkar_vram_alloc(&fVRAM, kSIVMPDBytes, 64 * 1024, &pd) == 0;
	IOLockUnlock(fVRAMLock);
	volatile uint64_t *p = have ? vramPointer(pd, kSIVMPDBytes) : NULL;
	if (p == NULL) {
		if (have) {
			IOLockLock(fVRAMLock);
			minkar_vram_free(&fVRAM, pd, kSIVMPDBytes);
			IOLockUnlock(fVRAMLock);
		}
		IOLockUnlock(fDMALock);
		return kIOReturnNoMemory;
	}
	for (uint32_t i = 0; i < kSIVMPDEntries; i++)
		p[i] = 0;		/* nothing mapped: faults, to the scratch page */
	fVMDirectory[id] = pd;
	writeReg(si_vm_base_reg(id), (uint32_t)((fVRAMStart + pd) >> 12));
	vmFlush(id);
	IOLockUnlock(fDMALock);
	*vmid = id;
	return kIOReturnSuccess;
}

void
MinkarAMDGPU::destroyVM(unsigned vmid)
{
	if (vmid == 0 || vmid >= kSIVMContexts || fDMALock == NULL)
		return;
	IOLockLock(fDMALock);
	uint64_t pd = fVMDirectory[vmid];
	volatile uint64_t *p = pd != 0 ? vramPointer(pd, kSIVMPDBytes) : NULL;
	if (p != NULL) {
		/* Its tables, then its directory; the context back at the
		 * GART's table, as at start. */
		IOLockLock(fVRAMLock);
		for (uint32_t i = 0; i < kSIVMPDEntries; i++) {
			uint64_t table = si_vm_pde_table(p[i]);
			if (table != 0)
				minkar_vram_free(&fVRAM, table - fVRAMStart, kSIVMPTBytes);
			p[i] = 0;
		}
		minkar_vram_free(&fVRAM, pd, kSIVMPDBytes);
		IOLockUnlock(fVRAMLock);
		writeReg(si_vm_base_reg(vmid), (uint32_t)((fVRAMStart + fGARTTable) >> 12));
		vmFlush(vmid);
	}
	fVMDirectory[vmid] = 0;
	IOLockUnlock(fDMALock);
}

/* VRAM at offset (page aligned, bytes of it) at va in the context. */
IOReturn
MinkarAMDGPU::vmMapVRAM(unsigned vmid, uint64_t va, uint64_t offset, uint64_t bytes)
{
	if (vmid == 0 || vmid >= kSIVMContexts || !si_vm_range_ok(va, bytes) ||
	    (offset & (kSIGPUPage - 1)) != 0)
		return kIOReturnBadArgument;
	IOLockLock(fDMALock);
	uint64_t pd = fVMDirectory[vmid];
	volatile uint64_t *dir = pd != 0 ? vramPointer(pd, kSIVMPDBytes) : NULL;
	IOReturn err = dir != NULL ? kIOReturnSuccess : kIOReturnNotReady;
	for (uint64_t done = 0; err == kIOReturnSuccess && done < bytes; done += kSIGPUPage) {
		uint32_t d, t;
		si_vm_index(va + done, &d, &t);
		uint64_t table = si_vm_pde_table(dir[d]);
		if (table == 0) {
			/* A new table, empty. */
			uint64_t pt = 0;
			IOLockLock(fVRAMLock);
			bool have = minkar_vram_alloc(&fVRAM, kSIVMPTBytes, kSIVMPTBytes, &pt) == 0;
			IOLockUnlock(fVRAMLock);
			volatile uint64_t *q = have ? vramPointer(pt, kSIVMPTBytes) : NULL;
			if (q == NULL) {
				if (have) {
					IOLockLock(fVRAMLock);
					minkar_vram_free(&fVRAM, pt, kSIVMPTBytes);
					IOLockUnlock(fVRAMLock);
				}
				err = kIOReturnNoMemory;
				break;
			}
			for (uint32_t i = 0; i < kSIVMPTEntries; i++)
				q[i] = 0;
			table = fVRAMStart + pt;
			dir[d] = si_vm_pde(table);
		}
		volatile uint64_t *ptes = vramPointer(table - fVRAMStart, kSIVMPTBytes);
		ptes[t] = si_vm_pte_vram(offset + done);
	}
	if (dir != NULL)
		vmFlush(vmid);
	IOLockUnlock(fDMALock);
	if (err != kIOReturnSuccess)
		vmUnmap(vmid, va, bytes);	/* what was mapped of it */
	return err;
}

/* The range's PTEs cleared (its tables stay until the context goes). */
void
MinkarAMDGPU::vmUnmap(unsigned vmid, uint64_t va, uint64_t bytes)
{
	if (vmid == 0 || vmid >= kSIVMContexts || !si_vm_range_ok(va, bytes))
		return;
	IOLockLock(fDMALock);
	uint64_t pd = fVMDirectory[vmid];
	volatile uint64_t *dir = pd != 0 ? vramPointer(pd, kSIVMPDBytes) : NULL;
	for (uint64_t done = 0; dir != NULL && done < bytes; done += kSIGPUPage) {
		uint32_t d, t;
		si_vm_index(va + done, &d, &t);
		uint64_t table = si_vm_pde_table(dir[d]);
		if (table != 0)
			vramPointer(table - fVRAMStart, kSIVMPTBytes)[t] = 0;
	}
	if (dir != NULL)
		vmFlush(vmid);
	IOLockUnlock(fDMALock);
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
	if (fGFXStarted)
		MinkarAMDGFXStop(&fGFX);
	OSSafeReleaseNULL(fGFXMap);
	OSSafeReleaseNULL(fWindowMap);
	stopInterrupts();
	MinkarAMDGARTDisable(&fGART);
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
