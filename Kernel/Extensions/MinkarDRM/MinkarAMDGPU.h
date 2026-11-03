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
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include "MinkarAMDDevice.h"
#include "minkar_vram.h"
#include "MinkarAMDDMA.h"
#include "MinkarAMDGFX.h"
#include "MinkarAMDGART.h"

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

	/* For the user client (MinkarAMDGPUInterface.h). Buffers are VRAM
	 * offsets the CPU's window reaches. */
	IOReturn vramInfo(uint64_t *total, uint64_t *available, uint64_t *window);
	/* cpuVisible: in the CPU's window on VRAM; else anywhere, from the top. */
	IOReturn createBuffer(uint64_t size, uint64_t *offset, bool cpuVisible = true);
	void destroyBuffer(uint64_t offset, uint64_t size);
	IODeviceMemory *bufferMemory(uint64_t offset, uint64_t size);
	uint64_t gpuAddress(uint64_t offset) { return fVRAMStart + offset; }
	/* Between two GPU addresses (VRAM or the GART). */
	IOReturn copy(uint64_t dst, uint64_t src, uint64_t bytes, uint64_t *ns);
	/* System memory through the GART: pages from the GART's allocator. */
	IOReturn createSystemBuffer(uint64_t size, IOBufferMemoryDescriptor **memory,
	    uint64_t *gpuAddress);
	void destroySystemBuffer(IOBufferMemoryDescriptor *memory, uint64_t gpuAddress);
	/* Prepared (wired) memory into the GART, and out. */
	IOReturn mapSystemMemory(IOMemoryDescriptor *memory, uint64_t *gpuAddress);
	void unmapSystemMemory(uint64_t gpuAddress, uint64_t size);
	IOReturn readWindowRegister(uint32_t reg, uint32_t *value);
	IOReturn i2cRead(const uint64_t *in, uint64_t *out);
	IOReturn modeSet(const uint64_t *in, uint64_t *out);
	/* One AUX transaction through this GPU's VBIOS, for bring-up. */
	IOReturn auxTransaction(const uint64_t *in, uint64_t *out, uint32_t *trace,
	    uint32_t *traceBytes);
	/* Shared VRAM buffers, by global ID, freed with their last holder. */
	IOReturn createShared(uint64_t size, uint32_t *id, uint64_t *offset, uint64_t *rounded);
	IOReturn retainShared(uint32_t id, uint64_t *offset, uint64_t *size);
	void releaseShared(uint32_t id);
	/* An indirect buffer of the graphics command processor at a GPU
	 * address, run and its fence waited for. */
	IOReturn runGFX(uint64_t address, uint32_t dwords, unsigned vmid, uint64_t *ns,
	    uint32_t *fault = NULL);
	/* Clients' GPU address spaces (VM contexts 1-15; with the GART on):
	 * one made, VRAM mapped into it and out, and gone. */
	IOReturn createVM(unsigned *vmid);
	void destroyVM(unsigned vmid);
	IOReturn vmMapVRAM(unsigned vmid, uint64_t va, uint64_t offset, uint64_t bytes);
	IOReturn vmMapSystem(unsigned vmid, uint64_t va, IOMemoryDescriptor *memory, uint64_t offset,
	    uint64_t bytes);
	void vmUnmap(unsigned vmid, uint64_t va, uint64_t bytes);
	/* Into the framebuffer the display shows, at offset. */
	IOReturn copyToScanout(uint64_t src, uint64_t offset, uint64_t bytes, uint64_t *ns);
	/* The display shows bytes of VRAM at offset from its next vertical
	 * blank (0, 0: its framebuffer again); ns: how long it took. */
	IOReturn flip(uint64_t offset, uint64_t bytes, uint64_t *ns);

private:
	void dmaSelfTest();
	void startGFXTest(const char *family);
	static void gfxTestThread(void *gpu, wait_result_t);
	void gfxSelfTest();
	/* pfp, me, ce, rlc */
	bool readMicrocode(uint8_t *file[4], size_t bytes[4], struct amd_ucode code[4]);
	void freeMicrocode(uint8_t *file[4], size_t bytes[4]);
	bool startGFX();
	void resetGFX();
	void initEngine(bool dryRun);
	volatile uint64_t *vramPointer(uint64_t offset, uint64_t bytes);
	void vmFlush(unsigned vmid);
	IOReturn vmMap(unsigned vmid, uint64_t va, uint64_t bytes, uint64_t offset,
	    IOMemoryDescriptor *memory);
	void gartSelfTest(struct MinkarAMDDMA *dma, IODeviceMemory *window, uint64_t region,
	    uint64_t vramStart);
	void setUpVRAM(uint32_t megabytes);
	bool startDMA();
	void startGART(bool display);
	void startInterrupts(bool display);
	void stopInterrupts();
	bool interruptFilter(IOFilterInterruptEventSource *source);
	void interruptHandler(IOInterruptEventSource *source, int count);
	static void sleepForTrap(void *gpu);
	bool lightsDisplay();

	enum { kVRAMRanges = 16384, kGARTRanges = 16384, kSharedBuffers = 512 };
	struct Shared {
		uint32_t id, holders;	/* holders 0: the slot is empty */
		uint64_t offset, size;
	};

	IOPCIDevice *		fPCI;
	struct MinkarAMDRegs	fRegs;
	uint8_t *		fBIOS;
	IOByteCount		fBIOSSize;
	struct minkar_atom *	fAtom;
	IODeviceMemory *	fWindow;	/* VRAM through BAR 0 */
	uint64_t		fVRAMStart;	/* VRAM in the GPU's space */
	IOLock *		fDMALock;	/* one submission at a time */
	char			fMicrocodePrefix[16];	/* "tahiti", for its files */
	const char *		fFamily;		/* "Tahiti" */
	struct MinkarAMDGFX	fGFX;		/* started at the first submission */
	bool			fGFXStarted;
	IOMemoryMap *		fGFXMap;	/* its ring and write-back */
	uint64_t		fGFXOffset;
	uint32_t		fGFXResets;
	uint32_t		fRasterConfig;	/* for the RLC's clear state */
	/* Clients' address spaces: each context's page directory in VRAM
	 * (0: the context is free); written through all of the VRAM window,
	 * mapped once. */
	uint64_t		fVMDirectory[16];
	IOMemoryMap *		fWindowMap;
	void *			fNode;		/* its DRM render node (devfs) */
	struct MinkarAMDDMA	fDMA;
	bool			fDMAStarted;
	IOMemoryMap *		fDMAMap;	/* its ring and write-back */
	uint64_t		fDMAOffset;
	struct MinkarAMDGART	fGART;		/* on while the driver runs */
	uint64_t		fGARTTable, fGARTScratch;	/* its VRAM */
	struct minkar_vram	fGARTPages;	/* what is free of it, by offset */
	struct minkar_vram_range *fGARTRanges;	/* kGARTRanges of them */
	IOLock *		fVRAMLock;	/* the VRAM's and the GART's allocators */
	Shared			fShared[kSharedBuffers];	/* under fVRAMLock */
	uint32_t		fNextSharedID;

	/* Interrupts: the IH ring and its write-back in system memory. */
	IOWorkLoop *		fWorkLoop;
	IOFilterInterruptEventSource *fInterrupt;
	IOBufferMemoryDescriptor *fIHMemory;
	uint64_t		fIHGPU;
	uint32_t		fIHRptr;
	bool			fIHOn;
	IOLock *		fTrapLock;	/* traps, and waits for them */
	uint64_t		fInterrupts, fDMATraps, fIHOverflows;
	struct minkar_vram	fVRAM;		/* what is free of VRAM */
	struct minkar_vram_range *fVRAMRanges;	/* kVRAMRanges of them */
};

#endif
