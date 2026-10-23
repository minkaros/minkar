/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * RavynE1000: driver for the Intel 8254x and 82574L Gigabit Ethernet
 * controllers that QEMU, VirtualBox and VMware emulate (e1000, e1000e).
 *
 * Packets are copied to and from fixed 2 KB DMA buffers, one per
 * descriptor; the transmit and receive rings and the interrupt handler run
 * on the controller's work loop. If the interrupt never arrives (no INTx
 * routing), a timer polls the controller instead.
 */

#ifndef _RAVYNE1000_H
#define _RAVYNE1000_H

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include "E1000Regs.h"

class RavynE1000 : public IOEthernetController
{
	OSDeclareDefaultStructors(RavynE1000);

	enum {
		kRxRingSize	= 128,
		kTxRingSize	= 128,
		kBufferSize	= 2048,
		kPollMS		= 5,	/* polling period without interrupts */
		kWatchdogMS	= 1000,
	};

	IOPCIDevice *			fPCI;
	IOMemoryMap *			fRegsMap;
	IOMemoryDescriptor *		fRegsDesc;
	volatile uint8_t *		fRegs;
	uint16_t			fDeviceID;
	bool				fEERDv2;	/* 82541/82547/82574 EERD layout */

	IOEthernetInterface *		fNetif;
	IOOutputQueue *			fTxQueue;
	IOFilterInterruptEventSource *	fIntSource;
	IOTimerEventSource *		fTimer;
	volatile UInt32			fPendingICR;	/* set by the filter */
	UInt32				fInterrupts;	/* handler runs */
	bool				fPolling;
	bool				fEnabled;

	IOEthernetAddress		fMAC;
	OSDictionary *			fMedia;

	IOBufferMemoryDescriptor *	fRxRingMem;
	IOBufferMemoryDescriptor *	fRxBufMem;
	IOBufferMemoryDescriptor *	fTxRingMem;
	IOBufferMemoryDescriptor *	fTxBufMem;
	struct e1000_rx_desc *		fRxRing;
	struct e1000_tx_desc *		fTxRing;
	uint8_t *			fRxBufs;
	uint8_t *			fTxBufs;
	uint64_t			fRxBufsPhys;
	uint64_t			fTxBufsPhys;
	UInt32				fRxNext;	/* next descriptor to look at */
	UInt32				fTxNext;	/* next free descriptor */
	UInt32				fTxClean;	/* oldest descriptor in use */
	UInt32				fTxInUse;
	bool				fTxStalled;

	uint32_t readReg(uint32_t reg) const;
	void writeReg(uint32_t reg, uint32_t value);
	void flushWrites();

	bool mapRegisters();
	bool readEEPROMWord(uint32_t addr, uint16_t *data);
	bool readMACAddress();
	bool allocateRings();
	void freeRings();
	void resetChip();
	void initRx();
	void initTx();
	void updateRCTL();
	void publishMedia();
	void checkLink();

	void receivePackets();
	void reclaimTx();
	void serviceInterrupt(UInt32 icr);

	bool interruptFilter(IOFilterInterruptEventSource *src);
	void interruptHandler(IOInterruptEventSource *src, int count);
	void timerFired(IOTimerEventSource *timer);

	bool fPromiscuous;
	bool fAllMulticast;	/* IFF_ALLMULTI */
	bool fMulticast;	/* multicast groups joined */

protected:
	virtual IOOutputQueue *createOutputQueue() APPLE_KEXT_OVERRIDE;
	virtual bool configureInterface(IONetworkInterface *netif) APPLE_KEXT_OVERRIDE;
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

#endif /* !_RAVYNE1000_H */
