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

#include "MinkarVirtioBlock.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOKitKeys.h>

#define super IOBlockStorageDevice
#define LOG(fmt, ...) IOLog("MinkarVirtioBlock: " fmt "\n", ##__VA_ARGS__)

/* Layout of a slot's DMA area. */
#define SLOT_HDR	0
#define SLOT_STATUS	16
#define SLOT_DATA	4096

OSDefineMetaClassAndStructors(MinkarVirtioBlock, IOBlockStorageDevice);

bool
MinkarVirtioBlock::start(IOService *provider)
{
	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || !super::start(provider))
		return false;
	fPCI->retain();
	if (!setupDevice()) {
		freeResources();
		return false;
	}
	fStarted = true;
	registerService();
	return true;
}

bool
MinkarVirtioBlock::setupDevice()
{
	fVirtio = MinkarVirtioPCI::withDevice(fPCI);
	if (fVirtio == NULL)
		return false;
	if (!fVirtio->begin((1ULL << VIRTIO_BLK_F_RO) | (1ULL << VIRTIO_BLK_F_BLK_SIZE) |
	    (1ULL << VIRTIO_BLK_F_FLUSH), &fFeatures))
		return false;

	fCapacity = fVirtio->configRead64(VIRTIO_BLK_CFG_CAPACITY);
	fBlockSize = 512;
	if (fFeatures & (1ULL << VIRTIO_BLK_F_BLK_SIZE)) {
		UInt32 bs = fVirtio->configRead32(VIRTIO_BLK_CFG_BLK_SIZE);

		if (bs >= 512 && bs <= 4096 && (bs & (bs - 1)) == 0)
			fBlockSize = bs;
	}
	fReadOnly = (fFeatures & (1ULL << VIRTIO_BLK_F_RO)) != 0;

	UInt16 qsize = fVirtio->getQueueMaxSize(0);
	if (qsize == 0) {
		LOG("no request queue");
		fVirtio->fail();
		return false;
	}
	if (qsize > kQueueSize)
		qsize = kQueueSize;
	fQueue = MinkarVirtQueue::withSize(0, qsize);
	if (fQueue == NULL || !fVirtio->setupQueue(fQueue)) {
		LOG("cannot set up the request queue (size %u)", qsize);
		fVirtio->fail();
		return false;
	}
	for (unsigned i = 0; i < kSlots; i++) {
		fSlots[i].mem = MinkarVirtioAllocDMA(SLOT_DATA + kMaxBytes);
		if (fSlots[i].mem == NULL)
			return false;
		fSlots[i].bytes = (uint8_t *)fSlots[i].mem->getBytesNoCopy();
		fSlots[i].phys = fSlots[i].mem->getPhysicalAddress();
	}

	fWorkLoop = IOWorkLoop::workLoop();
	fGate = IOCommandGate::commandGate(this);
	fTimer = IOTimerEventSource::timerEventSource(this,
	    OSMemberFunctionCast(IOTimerEventSource::Action, this,
	    &MinkarVirtioBlock::timerFired));
	fIntSource = IOInterruptEventSource::interruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventSource::Action, this,
	    &MinkarVirtioBlock::interruptOccurred), fPCI, MinkarVirtioInterruptIndex(fPCI));
	if (fWorkLoop == NULL || fGate == NULL || fTimer == NULL ||
	    fWorkLoop->addEventSource(fGate) != kIOReturnSuccess ||
	    fWorkLoop->addEventSource(fTimer) != kIOReturnSuccess)
		return false;
	if (fIntSource == NULL || fWorkLoop->addEventSource(fIntSource) != kIOReturnSuccess) {
		LOG("no interrupt; polling every %d ms", kPollMS);
		OSSafeReleaseNULL(fIntSource);
	} else {
		fIntSource->enable();
	}
	fVirtio->driverOK();

	/* The largest request the storage stack may send us. */
	setProperty(kIOMaximumBlockCountReadKey, (UInt64)(kMaxBytes / fBlockSize), 64);
	setProperty(kIOMaximumBlockCountWriteKey, (UInt64)(kMaxBytes / fBlockSize), 64);
	setProperty(kIOMaximumByteCountReadKey, (UInt64)kMaxBytes, 64);
	setProperty(kIOMaximumByteCountWriteKey, (UInt64)kMaxBytes, 64);

	/* The device's serial number names the product, when it has one. */
	strlcpy(fProduct, "VIRTIO Block Device", sizeof(fProduct));
	IOBufferMemoryDescriptor *id = IOBufferMemoryDescriptor::withCapacity(
	    VIRTIO_BLK_ID_BYTES, kIODirectionIn);
	if (id != NULL) {
		Request req;

		bzero(&req, sizeof(req));
		bzero(id->getBytesNoCopy(), VIRTIO_BLK_ID_BYTES);
		id->setLength(VIRTIO_BLK_ID_BYTES);
		req.buffer = id;
		req.bytes = VIRTIO_BLK_ID_BYTES;
		req.type = VIRTIO_BLK_T_GET_ID;
		if (runSync(&req) == kIOReturnSuccess) {
			char serial[VIRTIO_BLK_ID_BYTES + 1];

			bzero(serial, sizeof(serial));
			id->readBytes(0, serial, VIRTIO_BLK_ID_BYTES);
			if (serial[0] != '\0')
				strlcpy(fProduct, serial, sizeof(fProduct));
		}
		id->release();
	}

	LOG("%llu sectors of 512 bytes, block size %u%s%s, queue of %u", fCapacity,
	    fBlockSize, fReadOnly ? ", read-only" : "",
	    (fFeatures & (1ULL << VIRTIO_BLK_F_FLUSH)) ? ", flush" : "", fQueue->getSize());
	return true;
}

void
MinkarVirtioBlock::freeResources()
{
	if (fVirtio != NULL)
		fVirtio->reset();
	if (fWorkLoop != NULL) {
		if (fIntSource != NULL) {
			fIntSource->disable();
			fWorkLoop->removeEventSource(fIntSource);
		}
		if (fTimer != NULL) {
			fTimer->cancelTimeout();
			fWorkLoop->removeEventSource(fTimer);
		}
		if (fGate != NULL)
			fWorkLoop->removeEventSource(fGate);
	}
	OSSafeReleaseNULL(fIntSource);
	OSSafeReleaseNULL(fTimer);
	OSSafeReleaseNULL(fGate);
	OSSafeReleaseNULL(fWorkLoop);
	for (unsigned i = 0; i < kSlots; i++)
		OSSafeReleaseNULL(fSlots[i].mem);
	OSSafeReleaseNULL(fQueue);
	OSSafeReleaseNULL(fVirtio);
}

void
MinkarVirtioBlock::stop(IOService *provider)
{
	fStarted = false;
	freeResources();
	super::stop(provider);
}

void
MinkarVirtioBlock::free()
{
	freeResources();
	OSSafeReleaseNULL(fPCI);
	super::free();
}

IOWorkLoop *
MinkarVirtioBlock::getWorkLoop() const
{
	return fWorkLoop;
}

/* ---- Requests (on the work loop) --------------------------------------- */

bool
MinkarVirtioBlock::submit(Request *req, Slot *slot)
{
	struct virtio_blk_req_hdr *hdr = (struct virtio_blk_req_hdr *)(slot->bytes + SLOT_HDR);
	uint64_t addrs[3];
	uint32_t lens[3];
	unsigned n = 0, nOut, nIn;

	hdr->type = req->type;
	hdr->reserved = 0;
	hdr->sector = req->sector;
	slot->bytes[SLOT_STATUS] = 0xFF;
	addrs[n] = slot->phys + SLOT_HDR;
	lens[n++] = sizeof(*hdr);
	if (req->type == VIRTIO_BLK_T_OUT) {
		if (req->buffer->readBytes(0, slot->bytes + SLOT_DATA, req->bytes) != req->bytes)
			return false;
		addrs[n] = slot->phys + SLOT_DATA;
		lens[n++] = req->bytes;
		nOut = 2;
	} else {
		nOut = 1;
		if (req->bytes > 0) {
			addrs[n] = slot->phys + SLOT_DATA;
			lens[n++] = req->bytes;
		}
	}
	addrs[n] = slot->phys + SLOT_STATUS;
	lens[n++] = 1;
	nIn = n - nOut;
	if (!fQueue->addChain(addrs, lens, nOut, nIn, slot))
		return false;
	slot->request = req;
	fInFlight++;
	fVirtio->notify(fQueue);
	if (fIntSource == NULL || fInFlight == 1)
		fTimer->setTimeoutMS(kPollMS);
	return true;
}

void
MinkarVirtioBlock::queueRequest(Request *req)
{
	req->next = NULL;
	if (fPendingTail != NULL)
		fPendingTail->next = req;
	else
		fPendingHead = req;
	fPendingTail = req;
	submitPending();
}

void
MinkarVirtioBlock::submitPending()
{
	while (fPendingHead != NULL && fQueue->getFreeCount() >= 3) {
		Slot *slot = NULL;

		for (unsigned i = 0; i < kSlots; i++) {
			if (fSlots[i].request == NULL) {
				slot = &fSlots[i];
				break;
			}
		}
		if (slot == NULL)
			return;
		Request *req = fPendingHead;
		fPendingHead = req->next;
		if (fPendingHead == NULL)
			fPendingTail = NULL;
		if (!submit(req, slot))
			complete(req, kIOReturnIOError, 0);
	}
}

void
MinkarVirtioBlock::complete(Request *req, IOReturn status, UInt64 bytes)
{
	if (req->sync) {
		req->status = status;
		req->done = true;
		fGate->commandWakeup(req);
		return;
	}
	IOMemoryDescriptor *buffer = req->buffer;
	IOStorageCompletion completion = req->completion;

	IOFree(req, sizeof(*req));
	IOStorage::complete(&completion, status, bytes);
	if (buffer != NULL)
		buffer->release();
}

void
MinkarVirtioBlock::serviceQueue()
{
	Slot *slot;
	uint32_t len;

	while ((slot = (Slot *)fQueue->getUsed(&len)) != NULL) {
		Request *req = slot->request;
		uint8_t st = slot->bytes[SLOT_STATUS];
		IOReturn status;

		slot->request = NULL;
		if (fInFlight > 0)
			fInFlight--;
		if (req == NULL)
			continue;
		if (st == VIRTIO_BLK_S_OK) {
			status = kIOReturnSuccess;
			if (req->type != VIRTIO_BLK_T_OUT && req->bytes > 0 &&
			    req->buffer->writeBytes(0, slot->bytes + SLOT_DATA, req->bytes) != req->bytes)
				status = kIOReturnIOError;
		} else if (st == VIRTIO_BLK_S_UNSUPP) {
			status = kIOReturnUnsupported;
		} else {
			status = kIOReturnIOError;
		}
		complete(req, status, status == kIOReturnSuccess ? req->bytes : 0);
	}
	submitPending();
}

void
MinkarVirtioBlock::interruptOccurred(IOInterruptEventSource *, int)
{
	if (fVirtio == NULL)
		return;
	(void)fVirtio->readISR();	/* acknowledges the interrupt */
	serviceQueue();
}

/* A safety net for a lost interrupt, and the work itself without one. */
void
MinkarVirtioBlock::timerFired(IOTimerEventSource *)
{
	if (fVirtio == NULL)
		return;
	serviceQueue();
	if (fInFlight > 0)
		fTimer->setTimeoutMS(kPollMS);
}

IOReturn
MinkarVirtioBlock::gatedEnqueue(OSObject *owner, void *req, void *, void *, void *)
{
	((MinkarVirtioBlock *)owner)->queueRequest((Request *)req);
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioBlock::gatedSync(OSObject *owner, void *arg, void *, void *, void *)
{
	MinkarVirtioBlock *self = (MinkarVirtioBlock *)owner;
	Request *req = (Request *)arg;

	self->queueRequest(req);
	while (!req->done)
		self->fGate->commandSleep(req, THREAD_UNINT);
	return req->status;
}

IOReturn
MinkarVirtioBlock::runSync(Request *req)
{
	req->sync = true;
	req->done = false;
	return fGate->runAction(gatedSync, req);
}

/* ---- IOBlockStorageDevice ---------------------------------------------- */

IOReturn
MinkarVirtioBlock::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
    UInt64 nblks, IOStorageAttributes *, IOStorageCompletion *completion)
{
	if (!fStarted)
		return kIOReturnNotAttached;
	if (buffer == NULL || completion == NULL || nblks == 0)
		return kIOReturnBadArgument;
	UInt64 bytes = nblks * fBlockSize;
	UInt64 sectors = fCapacity / (fBlockSize / 512);
	if (bytes > kMaxBytes || block + nblks > sectors || buffer->getLength() < bytes)
		return kIOReturnBadArgument;
	bool write = buffer->getDirection() == kIODirectionOut;
	if (write && fReadOnly)
		return kIOReturnNotWritable;

	Request *req = (Request *)IOMalloc(sizeof(Request));
	if (req == NULL)
		return kIOReturnNoMemory;
	bzero(req, sizeof(*req));
	req->buffer = buffer;
	buffer->retain();
	req->sector = block * (fBlockSize / 512);
	req->bytes = (UInt32)bytes;
	req->type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
	req->completion = *completion;
	fGate->runAction(gatedEnqueue, req);
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioBlock::doSynchronize(UInt64, UInt64, IOStorageSynchronizeOptions)
{
	if (!fStarted)
		return kIOReturnNotAttached;
	if ((fFeatures & (1ULL << VIRTIO_BLK_F_FLUSH)) == 0)
		return kIOReturnSuccess;	/* no write cache to flush */
	Request req;

	bzero(&req, sizeof(req));
	req.type = VIRTIO_BLK_T_FLUSH;
	return runSync(&req);
}

IOReturn
MinkarVirtioBlock::doEjectMedia(void)
{
	return kIOReturnUnsupported;
}

IOReturn
MinkarVirtioBlock::doFormatMedia(UInt64)
{
	return kIOReturnUnsupported;
}

UInt32
MinkarVirtioBlock::doGetFormatCapacities(UInt64 *capacities, UInt32 max) const
{
	if (capacities != NULL && max > 0)
		capacities[0] = fCapacity * 512;
	return 1;
}

char *
MinkarVirtioBlock::getVendorString(void)
{
	static char vendor[] = "VIRTIO";

	return vendor;
}

char *
MinkarVirtioBlock::getProductString(void)
{
	return fProduct;
}

char *
MinkarVirtioBlock::getRevisionString(void)
{
	static char revision[] = "1.0";

	return revision;
}

char *
MinkarVirtioBlock::getAdditionalDeviceInfoString(void)
{
	return NULL;
}

IOReturn
MinkarVirtioBlock::reportBlockSize(UInt64 *blockSize)
{
	*blockSize = fBlockSize;
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioBlock::reportEjectability(bool *isEjectable)
{
	*isEjectable = false;
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioBlock::reportMaxValidBlock(UInt64 *maxBlock)
{
	UInt64 blocks = fCapacity / (fBlockSize / 512);

	*maxBlock = blocks > 0 ? blocks - 1 : 0;
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioBlock::reportMediaState(bool *mediaPresent, bool *changedState)
{
	*mediaPresent = fCapacity > 0;
	if (changedState != NULL)
		*changedState = false;
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioBlock::reportRemovability(bool *isRemovable)
{
	*isRemovable = false;
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioBlock::reportWriteProtection(bool *isWriteProtected)
{
	*isWriteProtected = fReadOnly;
	return kIOReturnSuccess;
}
