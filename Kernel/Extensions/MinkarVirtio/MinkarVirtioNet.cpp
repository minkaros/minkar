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

#include "MinkarVirtioNet.h"
#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>
#include <sys/random.h>

#define super IOEthernetController
#define LOG(fmt, ...) IOLog("MinkarVirtioNet: " fmt "\n", ##__VA_ARGS__)

#define HDR_LEN		sizeof(struct virtio_net_hdr)

OSDefineMetaClassAndStructors(MinkarVirtioNet, IOEthernetController);

static void
addMedium(OSDictionary *dict, IOMediumType type, UInt64 mbps)
{
	IONetworkMedium *m = IONetworkMedium::medium(type, mbps * 1000000ULL);

	if (m != NULL) {
		IONetworkMedium::addMedium(dict, m);
		m->release();
	}
}

bool
MinkarVirtioNet::start(IOService *provider)
{
	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || !super::start(provider))
		return false;
	fPCI->retain();
	fOutQueue = getOutputQueue();
	if (fOutQueue == NULL || !setupDevice()) {
		freeResources();
		return false;
	}

	fMedia = OSDictionary::withCapacity(1);
	if (fMedia != NULL) {
		addMedium(fMedia, kIOMediumEthernetAuto, 0);
		publishMediumDictionary(fMedia);
		setSelectedMedium(IONetworkMedium::getMediumWithType(fMedia,
		    kIOMediumEthernetAuto));
	}

	LOG("%s, MAC %02x:%02x:%02x:%02x:%02x:%02x, queues of %u and %u",
	    fPolling ? "polling" : "legacy interrupt",
	    fMAC.bytes[0], fMAC.bytes[1], fMAC.bytes[2], fMAC.bytes[3],
	    fMAC.bytes[4], fMAC.bytes[5], fRxCount, fTxCount);

	if (!attachInterface((IONetworkInterface **)&fNetif, false))
		return false;
	fNetif->registerService();
	return true;
}

bool
MinkarVirtioNet::setupDevice()
{
	fVirtio = MinkarVirtioPCI::withDevice(fPCI);
	if (fVirtio == NULL)
		return false;
	if (!fVirtio->begin((1ULL << VIRTIO_NET_F_MAC) | (1ULL << VIRTIO_NET_F_STATUS),
	    &fFeatures))
		return false;

	if (fFeatures & (1ULL << VIRTIO_NET_F_MAC)) {
		for (unsigned i = 0; i < 6; i++)
			fMAC.bytes[i] = fVirtio->configRead8(VIRTIO_NET_CFG_MAC + i);
	} else {
		/* A random, locally administered unicast address. */
		read_random(fMAC.bytes, sizeof(fMAC.bytes));
		fMAC.bytes[0] = (uint8_t)((fMAC.bytes[0] & 0xFE) | 0x02);
	}

	fRxCount = fVirtio->getQueueMaxSize(VIRTIO_NET_RX_QUEUE);
	fTxCount = fVirtio->getQueueMaxSize(VIRTIO_NET_TX_QUEUE);
	if (fRxCount == 0 || fTxCount == 0) {
		LOG("no receive or transmit queue");
		fVirtio->fail();
		return false;
	}
	if (fRxCount > kMaxQueueSize)
		fRxCount = kMaxQueueSize;
	if (fTxCount > kMaxQueueSize)
		fTxCount = kMaxQueueSize;
	fRxQueue = MinkarVirtQueue::withSize(VIRTIO_NET_RX_QUEUE, fRxCount);
	fTxQueue = MinkarVirtQueue::withSize(VIRTIO_NET_TX_QUEUE, fTxCount);
	fRxMem = MinkarVirtioAllocDMA((IOByteCount)fRxCount * kBufferSize);
	fTxMem = MinkarVirtioAllocDMA((IOByteCount)fTxCount * kBufferSize);
	if (fRxQueue == NULL || fTxQueue == NULL || fRxMem == NULL || fTxMem == NULL ||
	    !fVirtio->setupQueue(fRxQueue) || !fVirtio->setupQueue(fTxQueue)) {
		LOG("cannot set up the queues");
		fVirtio->fail();
		return false;
	}
	fRxBufs = (uint8_t *)fRxMem->getBytesNoCopy();
	fTxBufs = (uint8_t *)fTxMem->getBytesNoCopy();
	fRxPhys = fRxMem->getPhysicalAddress();
	fTxPhys = fTxMem->getPhysicalAddress();
	for (UInt16 i = 0; i < fTxCount; i++)
		fTxFree[i] = (UInt16)(fTxCount - 1 - i);
	fTxFreeCount = fTxCount;

	IOWorkLoop *wl = getWorkLoop();
	fTimer = IOTimerEventSource::timerEventSource(this,
	    OSMemberFunctionCast(IOTimerEventSource::Action, this,
	    &MinkarVirtioNet::timerFired));
	if (wl == NULL || fTimer == NULL || wl->addEventSource(fTimer) != kIOReturnSuccess)
		return false;
	fIntSource = IOInterruptEventSource::interruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventSource::Action, this,
	    &MinkarVirtioNet::interruptOccurred), fPCI, MinkarVirtioInterruptIndex(fPCI));
	if (fIntSource == NULL || wl->addEventSource(fIntSource) != kIOReturnSuccess) {
		OSSafeReleaseNULL(fIntSource);
		fPolling = true;
	} else {
		fIntSource->enable();
	}
	fVirtio->driverOK();
	return true;
}

void
MinkarVirtioNet::freeResources()
{
	IOWorkLoop *wl = getWorkLoop();

	if (fVirtio != NULL)
		fVirtio->reset();
	if (wl != NULL) {
		if (fIntSource != NULL) {
			fIntSource->disable();
			wl->removeEventSource(fIntSource);
		}
		if (fTimer != NULL) {
			fTimer->cancelTimeout();
			wl->removeEventSource(fTimer);
		}
	}
	OSSafeReleaseNULL(fIntSource);
	OSSafeReleaseNULL(fTimer);
	OSSafeReleaseNULL(fRxQueue);
	OSSafeReleaseNULL(fTxQueue);
	OSSafeReleaseNULL(fRxMem);
	OSSafeReleaseNULL(fTxMem);
	OSSafeReleaseNULL(fVirtio);
	fRxPosted = false;
}

void
MinkarVirtioNet::stop(IOService *provider)
{
	if (fEnabled)
		disable(fNetif);
	if (fNetif != NULL)
		detachInterface(fNetif);
	freeResources();
	super::stop(provider);
}

void
MinkarVirtioNet::free()
{
	freeResources();
	OSSafeReleaseNULL(fNetif);
	OSSafeReleaseNULL(fMedia);
	OSSafeReleaseNULL(fPCI);
	super::free();
}

IOOutputQueue *
MinkarVirtioNet::createOutputQueue()
{
	return IOGatedOutputQueue::withTarget(this, getWorkLoop(), 256);
}

/* ---- Queues (on the work loop) ----------------------------------------- */

void
MinkarVirtioNet::postRxBuffer(UInt16 index)
{
	uint64_t addr = fRxPhys + (uint64_t)index * kBufferSize;
	uint32_t len = kBufferSize;

	(void)fRxQueue->addChain(&addr, &len, 0, 1, (void *)(uintptr_t)(index + 1));
}

void
MinkarVirtioNet::receivePackets()
{
	void *cookie;
	uint32_t len;
	bool reposted = false, received = false;

	while ((cookie = fRxQueue->getUsed(&len)) != NULL) {
		UInt16 index = (UInt16)((uintptr_t)cookie - 1);

		if (fEnabled && len > HDR_LEN + 14 && len <= kBufferSize) {
			UInt32 frameLen = len - (UInt32)HDR_LEN;
			const uint8_t *frame = fRxBufs + (size_t)index * kBufferSize + HDR_LEN;
			mbuf_t m = allocatePacket(frameLen);

			if (m != NULL && mbuf_copyback(m, 0, frameLen, frame, MBUF_DONTWAIT) == 0) {
				fNetif->inputPacket(m, 0, kInputOptionQueuePacket);
				received = true;
			} else if (m != NULL) {
				freePacket(m);
			}
		}
		postRxBuffer(index);
		reposted = true;
	}
	if (reposted)
		fVirtio->notify(fRxQueue);
	if (received)
		fNetif->flushInputQueue();
}

void
MinkarVirtioNet::reclaimTx()
{
	void *cookie;
	bool reclaimed = false;

	while ((cookie = fTxQueue->getUsed(NULL)) != NULL) {
		if (fTxFreeCount < fTxCount)
			fTxFree[fTxFreeCount++] = (UInt16)((uintptr_t)cookie - 1);
		reclaimed = true;
	}
	if (reclaimed && fTxStalled) {
		fTxStalled = false;
		fOutQueue->service(kServiceAsync);
	}
}

void
MinkarVirtioNet::checkLink()
{
	bool up = true;

	if (fFeatures & (1ULL << VIRTIO_NET_F_STATUS))
		up = (fVirtio->configRead16(VIRTIO_NET_CFG_STATUS) & VIRTIO_NET_S_LINK_UP) != 0;
	if (!up) {
		if (getLinkStatus() != kIONetworkLinkValid) {
			setLinkStatus(kIONetworkLinkValid);
			LOG("link down");
		}
		return;
	}
	if ((getLinkStatus() & kIONetworkLinkActive) == 0) {
		setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive,
		    IONetworkMedium::getMediumWithType(fMedia, kIOMediumEthernetAuto),
		    1000ULL * 1000000ULL);
		LOG("link up");
	}
}

void
MinkarVirtioNet::service()
{
	if (fVirtio == NULL)
		return;
	receivePackets();
	reclaimTx();
}

void
MinkarVirtioNet::interruptOccurred(IOInterruptEventSource *, int)
{
	if (fVirtio == NULL)
		return;
	uint8_t isr = fVirtio->readISR();	/* acknowledges the interrupt */

	service();
	if ((isr & VIRTIO_ISR_CONFIG) && fEnabled)
		checkLink();
}

void
MinkarVirtioNet::timerFired(IOTimerEventSource *)
{
	if (!fEnabled)
		return;
	service();
	checkLink();
	fTimer->setTimeoutMS(fPolling ? kPollMS : kWatchdogMS);
}

/* ---- IONetworkController ----------------------------------------------- */

IOReturn
MinkarVirtioNet::enable(IONetworkInterface *)
{
	if (fEnabled)
		return kIOReturnSuccess;
	if (fVirtio == NULL)
		return kIOReturnNotReady;
	if (!fRxPosted) {
		for (UInt16 i = 0; i < fRxCount; i++)
			postRxBuffer(i);
		fRxPosted = true;
		fVirtio->notify(fRxQueue);
	}
	fEnabled = true;
	checkLink();
	fTimer->setTimeoutMS(fPolling ? kPollMS : kWatchdogMS);
	fOutQueue->setCapacity(256);
	fOutQueue->start();
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioNet::disable(IONetworkInterface *)
{
	if (!fEnabled)
		return kIOReturnSuccess;
	fEnabled = false;
	fTimer->cancelTimeout();
	fOutQueue->stop();
	fOutQueue->flush();
	setLinkStatus(kIONetworkLinkValid);
	return kIOReturnSuccess;
}

UInt32
MinkarVirtioNet::outputPacket(mbuf_t m, void *)
{
	size_t len = mbuf_pkthdr_len(m);

	if (!fEnabled || len < 14 || len > kMaxFrame) {
		freePacket(m);
		return kIOReturnOutputDropped;
	}
	if (fTxFreeCount == 0)
		reclaimTx();
	if (fTxFreeCount == 0 || fTxQueue->getFreeCount() == 0) {
		fTxStalled = true;
		return kIOReturnOutputStall;
	}
	UInt16 slot = fTxFree[--fTxFreeCount];
	uint8_t *buf = fTxBufs + (size_t)slot * kBufferSize;

	bzero(buf, HDR_LEN);
	if (mbuf_copydata(m, 0, len, buf + HDR_LEN) != 0) {
		fTxFree[fTxFreeCount++] = slot;
		freePacket(m);
		return kIOReturnOutputDropped;
	}
	freePacket(m);

	uint64_t addr = fTxPhys + (uint64_t)slot * kBufferSize;
	uint32_t total = (uint32_t)(HDR_LEN + len);
	if (!fTxQueue->addChain(&addr, &total, 1, 0, (void *)(uintptr_t)(slot + 1))) {
		fTxFree[fTxFreeCount++] = slot;
		return kIOReturnOutputDropped;
	}
	fVirtio->notify(fTxQueue);
	return kIOReturnOutputSuccess;
}

IOReturn
MinkarVirtioNet::getHardwareAddress(IOEthernetAddress *addr)
{
	if (addr == NULL)
		return kIOReturnBadArgument;
	*addr = fMAC;
	return kIOReturnSuccess;
}

/*
 * Without the control queue the device delivers every frame for the
 * guest (QEMU does), so the receive filters need no programming.
 */
IOReturn
MinkarVirtioNet::setPromiscuousMode(bool)
{
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioNet::setMulticastMode(bool)
{
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioNet::setMulticastList(IOEthernetAddress *, UInt32)
{
	return kIOReturnSuccess;
}

IOReturn
MinkarVirtioNet::selectMedium(const IONetworkMedium *medium)
{
	setSelectedMedium(medium);
	return kIOReturnSuccess;
}

const OSString *
MinkarVirtioNet::newVendorString() const
{
	return OSString::withCString("VIRTIO");
}

const OSString *
MinkarVirtioNet::newModelString() const
{
	return OSString::withCString("Network Device");
}
