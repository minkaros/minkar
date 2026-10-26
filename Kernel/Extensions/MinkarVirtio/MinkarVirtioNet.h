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
 * MinkarVirtioNet: the virtio network device (QEMU virtio-net-pci) as an
 * Ethernet controller.
 *
 * One receive and one transmit queue. Every descriptor owns a 2 KB DMA
 * buffer: the virtio-net header followed by one Ethernet frame, copied
 * to or from an mbuf. No offloads are negotiated, so the header is all
 * zeroes on transmit and ignored on receive. Queues, interrupt and timer
 * run on the controller's work loop.
 */

#ifndef _MINKARVIRTIONET_H
#define _MINKARVIRTIONET_H

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOOutputQueue.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include "MinkarVirtioPCI.h"

class MinkarVirtioNet : public IOEthernetController
{
	OSDeclareDefaultStructors(MinkarVirtioNet);

	enum {
		kMaxQueueSize	= 128,
		kBufferSize	= 2048,
		kMaxFrame	= 1514,		/* without the FCS */
		kPollMS		= 5,		/* without an interrupt */
		kWatchdogMS	= 1000,
	};

	IOPCIDevice *			fPCI;
	MinkarVirtioPCI *		fVirtio;
	MinkarVirtQueue *		fRxQueue;
	MinkarVirtQueue *		fTxQueue;
	IOBufferMemoryDescriptor *	fRxMem;
	IOBufferMemoryDescriptor *	fTxMem;
	uint8_t *			fRxBufs;
	uint8_t *			fTxBufs;
	uint64_t			fRxPhys;
	uint64_t			fTxPhys;
	UInt16				fRxCount;
	UInt16				fTxCount;
	UInt16				fTxFree[kMaxQueueSize];
	UInt16				fTxFreeCount;
	bool				fTxStalled;
	bool				fRxPosted;

	IOEthernetInterface *		fNetif;
	IOOutputQueue *			fOutQueue;
	IOInterruptEventSource *	fIntSource;
	IOTimerEventSource *		fTimer;
	OSDictionary *			fMedia;
	IOEthernetAddress		fMAC;
	uint64_t			fFeatures;
	bool				fEnabled;
	bool				fPolling;

	bool setupDevice();
	void freeResources();
	void postRxBuffer(UInt16 index);
	void receivePackets();
	void reclaimTx();
	void checkLink();
	void service();

	void interruptOccurred(IOInterruptEventSource *src, int count);
	void timerFired(IOTimerEventSource *timer);

protected:
	virtual IOOutputQueue *createOutputQueue() APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

	virtual IOReturn enable(IONetworkInterface *netif) APPLE_KEXT_OVERRIDE;
	virtual IOReturn disable(IONetworkInterface *netif) APPLE_KEXT_OVERRIDE;
	virtual UInt32 outputPacket(mbuf_t packet, void *param) APPLE_KEXT_OVERRIDE;

	virtual IOReturn getHardwareAddress(IOEthernetAddress *addr) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setPromiscuousMode(bool active) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setMulticastMode(bool active) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setMulticastList(IOEthernetAddress *addrs, UInt32 count) APPLE_KEXT_OVERRIDE;
	virtual IOReturn selectMedium(const IONetworkMedium *medium) APPLE_KEXT_OVERRIDE;
	virtual const OSString *newVendorString() const APPLE_KEXT_OVERRIDE;
	virtual const OSString *newModelString() const APPLE_KEXT_OVERRIDE;

	using IOEthernetController::getHardwareAddress;
};

#endif /* !_MINKARVIRTIONET_H */
