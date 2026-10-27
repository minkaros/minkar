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
 * MinkarVirtioBlock: the virtio block device (QEMU virtio-blk-pci) as an
 * IOBlockStorageDevice, under which IOBlockStorageDriver publishes the
 * disk's IOMedia.
 *
 * Each request uses one of kSlots DMA areas: the request header, a
 * bounce buffer of up to kMaxBytes and the status byte. Data is copied
 * between the bounce buffer and the caller's memory descriptor; requests
 * beyond the free slots wait in a list. The queue, the interrupt and the
 * completions run on the driver's work loop.
 */

#ifndef _MINKARVIRTIOBLOCK_H
#define _MINKARVIRTIOBLOCK_H

#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include "MinkarVirtioPCI.h"

class MinkarVirtioBlock : public IOBlockStorageDevice
{
	OSDeclareDefaultStructors(MinkarVirtioBlock);

	enum {
		kSlots		= 16,
		kMaxBytes	= 64 * 1024,
		kQueueSize	= 64,		/* 3 descriptors per request */
		kPollMS		= 10,		/* while requests are in flight */
	};

	struct Request {
		IOMemoryDescriptor *	buffer;		/* NULL for a flush */
		UInt64			sector;
		UInt32			bytes;
		uint32_t		type;		/* VIRTIO_BLK_T_* */
		IOStorageCompletion	completion;
		bool			sync;		/* a waiter sleeps on it */
		bool			done;
		IOReturn		status;
		Request *		next;
	};

	struct Slot {
		IOBufferMemoryDescriptor *mem;	/* header, status, data */
		uint8_t *		bytes;
		uint64_t		phys;
		Request *		request;	/* NULL when free */
	};

	IOPCIDevice *			fPCI;
	MinkarVirtioPCI *		fVirtio;
	MinkarVirtQueue *		fQueue;
	IOWorkLoop *			fWorkLoop;
	IOCommandGate *			fGate;
	IOInterruptEventSource *	fIntSource;
	IOTimerEventSource *		fTimer;
	Slot				fSlots[kSlots];
	Request *			fPendingHead;
	Request *			fPendingTail;
	UInt32				fInFlight;

	uint64_t			fFeatures;
	UInt64				fCapacity;	/* 512-byte sectors */
	UInt32				fBlockSize;
	bool				fReadOnly;
	bool				fStarted;
	char				fProduct[VIRTIO_BLK_ID_BYTES + 1];

	bool setupDevice();
	void freeResources();
	void queueRequest(Request *req);
	void submitPending();
	bool submit(Request *req, Slot *slot);
	void complete(Request *req, IOReturn status, UInt64 bytes);
	void serviceQueue();
	IOReturn runSync(Request *req);

	static IOReturn gatedEnqueue(OSObject *owner, void *req, void *, void *, void *);
	static IOReturn gatedSync(OSObject *owner, void *req, void *, void *, void *);
	void interruptOccurred(IOInterruptEventSource *src, int count);
	void timerFired(IOTimerEventSource *timer);

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;
	virtual IOWorkLoop *getWorkLoop() const APPLE_KEXT_OVERRIDE;

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

#endif /* !_MINKARVIRTIOBLOCK_H */
