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
		startGART(display);
		startInterrupts(display);
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
