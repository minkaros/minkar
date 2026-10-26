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
 * The virtio PCI transport (VIRTIO 1.x, "modern" interface) and split
 * virtqueues, shared by the block and network drivers.
 *
 * The transport finds the configuration areas through the device's
 * vendor-specific PCI capabilities and maps the BARs they point into. It
 * uses the INTx line: queues and configuration changes report through
 * the ISR register; no MSI-X vectors are programmed. Callers serialize
 * all queue work (the drivers do it on their work loop).
 */

#ifndef _MINKARVIRTIOPCI_H
#define _MINKARVIRTIOPCI_H

#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/c++/OSObject.h>
#include "VirtioSpec.h"

/* Contiguous, zeroed memory a device can reach with 32-bit addresses. */
IOBufferMemoryDescriptor *MinkarVirtioAllocDMA(IOByteCount size);

/* The INTx interrupt source of the device (no MSI-X vectors are used). */
int MinkarVirtioInterruptIndex(IOPCIDevice *pci);

class MinkarVirtQueue : public OSObject
{
	OSDeclareDefaultStructors(MinkarVirtQueue);

public:
	static MinkarVirtQueue *withSize(UInt16 index, UInt16 size);

	UInt16 getIndex() const { return fIndex; }
	UInt16 getSize() const { return fSize; }
	UInt16 getFreeCount() const { return fNumFree; }
	uint64_t getDescPhys() const { return fPhys; }
	uint64_t getAvailPhys() const { return fPhys + fAvailOffset; }
	uint64_t getUsedPhys() const { return fPhys + fUsedOffset; }

	/*
	 * Adds one buffer chain: nOut device-readable segments followed by
	 * nIn device-writable ones. The cookie comes back from getUsed().
	 * False when the queue has not enough free descriptors.
	 */
	bool addChain(const uint64_t *addrs, const uint32_t *lens,
	    unsigned nOut, unsigned nIn, void *cookie);

	/* The next chain the device finished, or NULL; *len is what it wrote. */
	void *getUsed(uint32_t *len);
	bool hasUsed() const;

	virtual void free() APPLE_KEXT_OVERRIDE;

private:
	bool initWithSize(UInt16 index, UInt16 size);

	UInt16				fIndex;
	UInt16				fSize;
	IOBufferMemoryDescriptor *	fMem;
	uint64_t			fPhys;
	IOByteCount			fAvailOffset;
	IOByteCount			fUsedOffset;
	volatile struct virtq_desc *	fDesc;
	volatile uint16_t *		fAvail;	/* flags, idx, ring[size] */
	volatile uint16_t *		fUsedHdr; /* flags, idx */
	volatile struct virtq_used_elem *fUsedRing;
	void **				fCookies;
	UInt16				fFreeHead;
	UInt16				fNumFree;
	UInt16				fAvailIdx;
	UInt16				fLastUsed;
};

class MinkarVirtioPCI : public OSObject
{
	OSDeclareDefaultStructors(MinkarVirtioPCI);

public:
	static MinkarVirtioPCI *withDevice(IOPCIDevice *pci);

	/*
	 * Resets the device and negotiates features: VERSION_1 plus those of
	 * `wanted` the device offers. Leaves the device in FEATURES_OK.
	 */
	bool begin(uint64_t wanted, uint64_t *negotiated);
	UInt16 getQueueMaxSize(UInt16 index);
	bool setupQueue(MinkarVirtQueue *queue);
	void notify(MinkarVirtQueue *queue);
	void driverOK();
	void fail();
	void reset();

	/* Reading the ISR acknowledges the interrupt (VIRTIO_ISR_*). */
	uint8_t readISR();

	uint8_t configRead8(UInt32 offset);
	uint16_t configRead16(UInt32 offset);
	uint32_t configRead32(UInt32 offset);
	uint64_t configRead64(UInt32 offset);

	virtual void free() APPLE_KEXT_OVERRIDE;

private:
	bool initWithDevice(IOPCIDevice *pci);
	bool locateCapabilities();
	volatile uint8_t *mapBAR(unsigned bar);
	volatile uint8_t *areaAt(unsigned bar, uint32_t offset, uint32_t length);
	void setStatus(uint8_t bits);

	IOPCIDevice *			fPCI;
	IOMemoryMap *			fMaps[6];
	IOMemoryDescriptor *		fDescs[6];
	volatile uint8_t *		fBARs[6];
	volatile uint8_t *		fCommon;
	volatile uint8_t *		fNotify;
	uint32_t			fNotifyMultiplier;
	volatile uint8_t *		fISR;
	volatile uint8_t *		fDevice;
	volatile uint16_t *		fQueueNotify[8];
};

#endif /* !_MINKARVIRTIOPCI_H */
