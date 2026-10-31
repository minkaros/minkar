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

#ifndef _MINKARBGE_H
#define _MINKARBGE_H

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOOutputQueue.h>
#include <IOKit/network/IOEthernetStats.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include <machine/endian.h>

struct bge_rx_bd;
struct bge_tx_bd;
struct bge_status_block;

/*
 * Broadcom NetXtreme 57765 family (BCM57761, 57762, 57765, 57766, 57781,
 * 57782, 57785, 57786, 57791, 57795): the Ethernet of the Mac Pro 6,1
 * and other machines, on our IONetworkingFamily. The chip setup follows
 * FreeBSD's bge(4) for this family (MinkarBGE.cpp).
 *
 * Packets are copied to and from fixed DMA buffers, one per descriptor,
 * as in MinkarE1000. A message-signalled interrupt is used when there is
 * one; without interrupts the driver polls.
 */
class MinkarBGE : public IOEthernetController
{
	OSDeclareDefaultStructors(MinkarBGE);

	enum {
		kStdRingSize	= 512,		/* BGE_STD_RX_RING_CNT */
		kReturnRingSize	= 1024,		/* BGE_RETURN_RING_CNT */
		kTxRingSize	= 512,		/* BGE_TX_RING_CNT */
		kBufferSize	= 2048,
		kPollMS		= 5,		/* polling period without interrupts */
		kWatchdogMS	= 1000,
		kPHYAddress	= 1,		/* the 57765 family's copper PHY */
	};

	IOPCIDevice *			fPCI;
	IOMemoryMap *			fRegsMap;
	volatile uint8_t *		fRegs;
	uint16_t			fDeviceID;
	uint32_t			fChipID;
	uint32_t			fAsicRev;
	uint32_t			fChipRev;
	uint8_t				fPCIeCap;	/* PCI Express capability */
	uint32_t			fMaxPayload;
	uint32_t			fMIMode;
	uint32_t			fASF;		/* management firmware */
	bool				fTaggedStatus;

	IOEthernetInterface *		fNetif;
	IOOutputQueue *			fTxQueue;
	IOFilterInterruptEventSource *	fIntSource;
	IOTimerEventSource *		fTimer;
	UInt32				fInterrupts;	/* handler runs */
	UInt32				fPollCount;
	bool				fPolling;
	bool				fMSI;
	bool				fEnabled;
	bool				fLinkUp;

	IOEthernetAddress		fMAC;
	OSDictionary *			fMedia;
	IONetworkStats *		fNetStats;
	IOEthernetStats *		fEtherStats;

	IOBufferMemoryDescriptor *	fStdRingMem;
	IOBufferMemoryDescriptor *	fReturnRingMem;
	IOBufferMemoryDescriptor *	fTxRingMem;
	IOBufferMemoryDescriptor *	fStatusMem;
	IOBufferMemoryDescriptor *	fRxBufMem;
	IOBufferMemoryDescriptor *	fTxBufMem;
	volatile struct bge_rx_bd *	fStdRing;
	volatile struct bge_rx_bd *	fReturnRing;
	volatile struct bge_tx_bd *	fTxRing;
	volatile struct bge_status_block *fStatus;
	uint8_t *			fRxBufs;
	uint8_t *			fTxBufs;
	uint64_t			fRxBufsPhys;
	uint64_t			fTxBufsPhys;
	UInt32				fStdProd;	/* next standard ring slot to fill */
	UInt32				fReturnCons;	/* next return ring entry */
	UInt32				fTxProd;
	UInt32				fTxCons;
	UInt32				fTxInUse;
	bool				fTxStalled;

	uint32_t			fMulticastHash[4];
	bool				fPromiscuous;
	bool				fAllMulticast;

	/* Registers, NIC memory, PHY. */
	uint32_t readReg(uint32_t reg) const;
	void writeReg(uint32_t reg, uint32_t value);
	void setBits(uint32_t reg, uint32_t bits);
	void clearBits(uint32_t reg, uint32_t bits);
	void writeMailbox(uint32_t reg, uint32_t value);
	uint32_t readMemory(uint32_t offset);
	void writeMemory(uint32_t offset, uint32_t value);
	uint16_t readPHY(uint32_t reg);
	void writePHY(uint32_t reg, uint16_t value);

	/* Chip setup (if_bge.c: bge_reset, bge_chipinit, bge_blockinit...). */
	bool identifyChip();
	void signalPreReset(int type);
	void signalPostReset(int type);
	void stopFirmware();
	bool resetChip();
	void initChip();
	bool initBlocks();
	void stopBlock(uint32_t reg, uint32_t bit);
	void stopChip();
	bool readEEPROMByte(uint32_t addr, uint8_t *byte);
	bool readMACAddress(const uint8_t *beforeReset);
	bool allocateRings();
	void freeRings();
	void initRxRing();
	void initTxRing();
	void programFilters();

	/* Link. */
	void publishMedia();
	void startAutonegotiation(IOMediumType type);
	void checkLink();

	/* Data path. */
	void service();
	void receivePackets(uint16_t producer);
	void reclaimTx(uint16_t consumer);

	bool interruptFilter(IOFilterInterruptEventSource *src);
	void interruptHandler(IOInterruptEventSource *src, int count);
	void timerFired(IOTimerEventSource *timer);

protected:
	virtual IOOutputQueue *createOutputQueue() APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;

public:
	virtual IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
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
	virtual UInt32 getFeatures() const APPLE_KEXT_OVERRIDE;
	virtual const OSString *newVendorString() const APPLE_KEXT_OVERRIDE;
	virtual const OSString *newModelString() const APPLE_KEXT_OVERRIDE;

	using IOEthernetController::getHardwareAddress;
};

#endif /* !_MINKARBGE_H */
