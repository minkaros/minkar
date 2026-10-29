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
 * MinkarNVMe: an NVM Express controller's first namespace as an
 * IOBlockStorageDevice, under which IOBlockStorageDriver publishes the
 * disk's IOMedia.
 *
 * One admin queue, used by polling while the driver starts, and one I/O
 * queue pair. Each request uses one of the slots: a DMA area holding a PRP
 * list page and a bounce buffer of up to fMaxBytes, physically contiguous;
 * its command identifier is the slot's index. Requests beyond the free
 * slots wait in a list. Completions come by interrupt: MSI or MSI-X when
 * the device has it, else its pin interrupt, each kept only if a test read
 * at start brings one. A timer is the safety net, and does the work when
 * neither arrives.
 */

#ifndef _MINKARNVME_H
#define _MINKARNVME_H

#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include "NVMe.h"

class MinkarNVMe : public IOBlockStorageDevice
{
	OSDeclareDefaultStructors(MinkarNVMe);

	enum {
		kSlots		= 16,
		kMaxBytes	= 128 * 1024,
		kPage		= 4096,
		kAdminEntries	= 16,
		kIOEntries	= 64,
		kPollMS		= 10,	/* while requests are in flight */
	};

	struct Queue {
		IOBufferMemoryDescriptor *sqMem;
		IOBufferMemoryDescriptor *cqMem;
		volatile struct nvme_sqe *sq;
		volatile struct nvme_cqe *cq;
		uint16_t	entries;
		uint16_t	sqTail;
		uint16_t	cqHead;
		uint8_t		phase;
		uint16_t	id;
	};

	struct Request {
		IOMemoryDescriptor *	buffer;		/* NULL for a flush */
		UInt64			lba;
		UInt32			bytes;
		uint8_t			opcode;		/* NVME_CMD_* */
		IOStorageCompletion	completion;
		bool			sync;		/* a waiter sleeps on it */
		bool			done;
		IOReturn		status;
		Request *		next;
	};

	struct Slot {
		IOBufferMemoryDescriptor *mem;	/* PRP list page, then data */
		uint8_t *		bytes;
		uint64_t		phys;
		Request *		request;	/* NULL when free */
	};

	IOPCIDevice *			fPCI;
	IOMemoryMap *			fMap;
	IOMemoryDescriptor *		fDesc;		/* BAR0, when mapped by hand */
	volatile uint8_t *		fRegs;
	uint32_t			fDoorbellStride;
	uint32_t			fTimeoutMS;
	Queue				fAdmin;
	Queue				fIO;
	uint16_t			fAdminCID;
	IOBufferMemoryDescriptor *	fIdentify;	/* one page, for identify */

	IOWorkLoop *			fWorkLoop;
	IOCommandGate *			fGate;
	IOInterruptEventSource *	fIntSource;
	IOTimerEventSource *		fTimer;
	Slot				fSlots[kSlots];
	unsigned			fNumSlots;
	Request *			fPendingHead;
	Request *			fPendingTail;
	UInt32				fInFlight;
	UInt64				fInterrupts;
	UInt64				fCompletions;
	UInt64				fPolled;	/* completed by the timer */
	bool				fInTimer;

	UInt64				fBlocks;	/* logical blocks */
	UInt32				fBlockSize;
	UInt32				fMaxBytes;
	bool				fWriteCache;
	bool				fStarted;
	bool				fEnabled;
	char				fModel[41];
	char				fSerial[21];
	char				fFirmware[9];

	bool mapRegisters();
	uint32_t read32(uint32_t reg);
	uint64_t read64(uint32_t reg);
	void write32(uint32_t reg, uint32_t value);
	void write64(uint32_t reg, uint64_t value);
	void ringSQ(Queue *q);
	void ringCQ(Queue *q);

	bool waitReady(bool ready);
	bool resetController();
	bool enableController();
	void shutdownController();
	bool allocQueue(Queue *q, uint16_t id, uint16_t entries);
	void freeQueue(Queue *q);
	IOReturn adminCommand(struct nvme_sqe *cmd, uint32_t *result);
	bool identify();
	bool createIOQueues();
	bool setupDevice();
	void freeResources();

	void queueRequest(Request *req);
	void submitPending();
	bool submit(Request *req, unsigned slot);
	void complete(Request *req, IOReturn status, UInt64 bytes);
	void serviceQueue();
	IOReturn runSync(Request *req);

	static IOReturn gatedEnqueue(OSObject *owner, void *req, void *, void *, void *);
	static IOReturn gatedSync(OSObject *owner, void *req, void *, void *, void *);
	bool tryInterrupt(int index);
	void interruptOccurred(IOInterruptEventSource *src, int count);
	void timerFired(IOTimerEventSource *timer);
	void publishCounts();

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;
	virtual IOWorkLoop *getWorkLoop() const APPLE_KEXT_OVERRIDE;
	virtual void systemWillShutdown(IOOptionBits specifier) APPLE_KEXT_OVERRIDE;

	virtual IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
	    UInt64 nblks, IOStorageAttributes *attributes,
	    IOStorageCompletion *completion) APPLE_KEXT_OVERRIDE;
	virtual IOReturn doSynchronize(UInt64 block, UInt64 nblks,
	    IOStorageSynchronizeOptions options = 0) APPLE_KEXT_OVERRIDE;
	virtual IOReturn doEjectMedia(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn doFormatMedia(UInt64 byteCapacity) APPLE_KEXT_OVERRIDE;
	virtual UInt32 doGetFormatCapacities(UInt64 *capacities,
	    UInt32 capacitiesMaxCount) const APPLE_KEXT_OVERRIDE;

	virtual char *getVendorString(void) APPLE_KEXT_OVERRIDE;
	virtual char *getProductString(void) APPLE_KEXT_OVERRIDE;
	virtual char *getRevisionString(void) APPLE_KEXT_OVERRIDE;
	virtual char *getAdditionalDeviceInfoString(void) APPLE_KEXT_OVERRIDE;

	virtual IOReturn reportBlockSize(UInt64 *blockSize) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportEjectability(bool *isEjectable) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportMaxValidBlock(UInt64 *maxBlock) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportMediaState(bool *mediaPresent,
	    bool *changedState = 0) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportRemovability(bool *isRemovable) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportWriteProtection(bool *isWriteProtected) APPLE_KEXT_OVERRIDE;
};

#endif /* !_MINKARNVME_H */
