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

#include "RavynE1000.h"
#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>

#define super IOEthernetController
OSDefineMetaClassAndStructors(RavynE1000, IOEthernetController);

#define LOG(fmt, ...)	IOLog("RavynE1000: " fmt "\n", ##__VA_ARGS__)

static inline void
compilerBarrier(void)
{
	__asm__ volatile("" ::: "memory");
}

/* ---- Registers --------------------------------------------------------- */

uint32_t
RavynE1000::readReg(uint32_t reg) const
{
	return *(volatile uint32_t *)(fRegs + reg);
}

void
RavynE1000::writeReg(uint32_t reg, uint32_t value)
{
	*(volatile uint32_t *)(fRegs + reg) = value;
}

/* Posted writes reach the device before a read of STATUS returns. */
void
RavynE1000::flushWrites()
{
	(void)readReg(E1000_STATUS);
}

/* BAR 0 is the register window (128 KB, memory space). */
bool
RavynE1000::mapRegisters()
{
	uint32_t bar, size;

	fRegsMap = fPCI->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0,
	    kIOMapInhibitCache);
	if (fRegsMap == NULL) {
		/* No IODeviceMemory for the BAR: map what the BAR holds. */
		bar = fPCI->configRead32(kIOPCIConfigBaseAddress0);
		if ((bar & 1) != 0 || (bar & ~0xFU) == 0)
			return false;
		fPCI->configWrite32(kIOPCIConfigBaseAddress0, 0xFFFFFFFF);
		size = ~(fPCI->configRead32(kIOPCIConfigBaseAddress0) & ~0xFU) + 1;
		fPCI->configWrite32(kIOPCIConfigBaseAddress0, bar);
		fRegsDesc = IOMemoryDescriptor::withPhysicalAddress(bar & ~0xFU, size,
		    kIODirectionNone);
		if (fRegsDesc == NULL)
			return false;
		fRegsMap = fRegsDesc->map(kIOMapAnywhere | kIOMapInhibitCache);
		if (fRegsMap == NULL)
			return false;
	}
	fRegs = (volatile uint8_t *)fRegsMap->getVirtualAddress();
	return fRegs != NULL;
}

bool
RavynE1000::readEEPROMWord(uint32_t addr, uint16_t *data)
{
	uint32_t done = fEERDv2 ? E1000_EERD_DONE_V2 : E1000_EERD_DONE;
	uint32_t shift = fEERDv2 ? E1000_EERD_ADDR_SHIFT_V2 : E1000_EERD_ADDR_SHIFT;

	writeReg(E1000_EERD, (addr << shift) | E1000_EERD_START);
	for (int i = 0; i < 1000; i++) {
		uint32_t v = readReg(E1000_EERD);

		if (v & done) {
			*data = (uint16_t)(v >> E1000_EERD_DATA_SHIFT);
			return true;
		}
		IODelay(5);
	}
	return false;
}

/* Receive address 0 holds the MAC once the EEPROM is loaded; else read it. */
bool
RavynE1000::readMACAddress()
{
	uint32_t ral = readReg(E1000_RAL0), rah = readReg(E1000_RAH0);

	if ((rah & E1000_RAH_AV) && (ral != 0 || (rah & 0xFFFF) != 0)) {
		for (int i = 0; i < 4; i++)
			fMAC.bytes[i] = (UInt8)(ral >> (8 * i));
		fMAC.bytes[4] = (UInt8)rah;
		fMAC.bytes[5] = (UInt8)(rah >> 8);
		return true;
	}
	for (uint32_t w = 0; w < 3; w++) {
		uint16_t v;

		if (!readEEPROMWord(w, &v))
			return false;
		fMAC.bytes[2 * w] = (UInt8)v;
		fMAC.bytes[2 * w + 1] = (UInt8)(v >> 8);
	}
	return true;
}

/* ---- DMA memory -------------------------------------------------------- */

static IOBufferMemoryDescriptor *
allocDMA(IOByteCount size)
{
	IOBufferMemoryDescriptor *mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
	    kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut, size,
	    0x00000000FFFFF000ULL);

	if (mem != NULL)
		bzero(mem->getBytesNoCopy(), size);
	return mem;
}

bool
RavynE1000::allocateRings()
{
	fRxRingMem = allocDMA(kRxRingSize * sizeof(struct e1000_rx_desc));
	fTxRingMem = allocDMA(kTxRingSize * sizeof(struct e1000_tx_desc));
	fRxBufMem = allocDMA(kRxRingSize * kBufferSize);
	fTxBufMem = allocDMA(kTxRingSize * kBufferSize);
	if (fRxRingMem == NULL || fTxRingMem == NULL || fRxBufMem == NULL ||
	    fTxBufMem == NULL)
		return false;
	fRxRing = (struct e1000_rx_desc *)fRxRingMem->getBytesNoCopy();
	fTxRing = (struct e1000_tx_desc *)fTxRingMem->getBytesNoCopy();
	fRxBufs = (uint8_t *)fRxBufMem->getBytesNoCopy();
	fTxBufs = (uint8_t *)fTxBufMem->getBytesNoCopy();
	fRxBufsPhys = fRxBufMem->getPhysicalAddress();
	fTxBufsPhys = fTxBufMem->getPhysicalAddress();
	return true;
}

void
RavynE1000::freeRings()
{
	OSSafeReleaseNULL(fRxRingMem);
	OSSafeReleaseNULL(fTxRingMem);
	OSSafeReleaseNULL(fRxBufMem);
	OSSafeReleaseNULL(fTxBufMem);
	fRxRing = NULL;
	fTxRing = NULL;
}

/* ---- Chip setup -------------------------------------------------------- */

void
RavynE1000::resetChip()
{
	uint32_t ctrl;

	writeReg(E1000_IMC, 0xFFFFFFFF);
	writeReg(E1000_RCTL, 0);
	writeReg(E1000_TCTL, E1000_TCTL_PSP);
	flushWrites();
	IOSleep(10);

	writeReg(E1000_CTRL, readReg(E1000_CTRL) | E1000_CTRL_RST);
	IOSleep(5);
	for (int i = 0; i < 100 && (readReg(E1000_CTRL) & E1000_CTRL_RST); i++)
		IOSleep(1);

	writeReg(E1000_IMC, 0xFFFFFFFF);
	(void)readReg(E1000_ICR);

	ctrl = readReg(E1000_CTRL);
	ctrl |= E1000_CTRL_SLU | E1000_CTRL_ASDE;
	ctrl &= ~(E1000_CTRL_LRST | E1000_CTRL_PHY_RST | E1000_CTRL_VME);
	writeReg(E1000_CTRL, ctrl);
	flushWrites();
}

void
RavynE1000::initRx()
{
	uint64_t ring = fRxRingMem->getPhysicalAddress();

	for (UInt32 i = 0; i < kRxRingSize; i++) {
		bzero(&fRxRing[i], sizeof(fRxRing[i]));
		fRxRing[i].addr = fRxBufsPhys + (uint64_t)i * kBufferSize;
	}
	for (UInt32 i = 0; i < 128; i++)
		writeReg(E1000_MTA + 4 * i, 0);

	writeReg(E1000_RDBAL, (uint32_t)ring);
	writeReg(E1000_RDBAH, (uint32_t)(ring >> 32));
	writeReg(E1000_RDLEN, kRxRingSize * sizeof(struct e1000_rx_desc));
	writeReg(E1000_RDH, 0);
	writeReg(E1000_RDT, kRxRingSize - 1);
	writeReg(E1000_RDTR, 0);
	fRxNext = 0;
	updateRCTL();
}

void
RavynE1000::initTx()
{
	uint64_t ring = fTxRingMem->getPhysicalAddress();

	bzero(fTxRing, kTxRingSize * sizeof(struct e1000_tx_desc));
	writeReg(E1000_TDBAL, (uint32_t)ring);
	writeReg(E1000_TDBAH, (uint32_t)(ring >> 32));
	writeReg(E1000_TDLEN, kTxRingSize * sizeof(struct e1000_tx_desc));
	writeReg(E1000_TDH, 0);
	writeReg(E1000_TDT, 0);
	writeReg(E1000_TIDV, 0);
	writeReg(E1000_TIPG, E1000_TIPG_COPPER);
	writeReg(E1000_TCTL, E1000_TCTL_EN | E1000_TCTL_PSP |
	    (0x0FU << E1000_TCTL_CT_SHIFT) | (0x40U << E1000_TCTL_COLD_SHIFT) |
	    E1000_TCTL_RTLC);
	fTxNext = fTxClean = fTxInUse = 0;
	fTxStalled = false;
}

void
RavynE1000::updateRCTL()
{
	uint32_t rctl = E1000_RCTL_BAM | E1000_RCTL_SECRC | E1000_RCTL_BSIZE_2048;

	if (fEnabled)
		rctl |= E1000_RCTL_EN;
	if (fPromiscuous)
		rctl |= E1000_RCTL_UPE | E1000_RCTL_MPE;
	if (fAllMulticast || fMulticast)
		rctl |= E1000_RCTL_MPE;
	writeReg(E1000_RCTL, rctl);
}

/* ---- Media and link ---------------------------------------------------- */

static void
addMedium(OSDictionary *dict, IOMediumType type, UInt64 mbps)
{
	IONetworkMedium *m = IONetworkMedium::medium(type, mbps * 1000000ULL);

	if (m != NULL) {
		IONetworkMedium::addMedium(dict, m);
		m->release();
	}
}

void
RavynE1000::publishMedia()
{
	fMedia = OSDictionary::withCapacity(6);
	if (fMedia == NULL)
		return;
	addMedium(fMedia, kIOMediumEthernetAuto, 0);
	addMedium(fMedia, kIOMediumEthernet10BaseT | kIOMediumOptionHalfDuplex, 10);
	addMedium(fMedia, kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex, 10);
	addMedium(fMedia, kIOMediumEthernet100BaseTX | kIOMediumOptionHalfDuplex, 100);
	addMedium(fMedia, kIOMediumEthernet100BaseTX | kIOMediumOptionFullDuplex, 100);
	addMedium(fMedia, kIOMediumEthernet1000BaseT | kIOMediumOptionFullDuplex, 1000);
	publishMediumDictionary(fMedia);
	setSelectedMedium(IONetworkMedium::getMediumWithType(fMedia, kIOMediumEthernetAuto));
}

void
RavynE1000::checkLink()
{
	uint32_t status = readReg(E1000_STATUS);
	IOMediumType type;
	UInt64 mbps;

	if ((status & E1000_STATUS_LU) == 0) {
		if (getLinkStatus() != kIONetworkLinkValid) {
			setLinkStatus(kIONetworkLinkValid);
			LOG("link down");
		}
		return;
	}
	switch ((status & E1000_STATUS_SPEED_MASK) >> E1000_STATUS_SPEED_SHIFT) {
	case 0:  type = kIOMediumEthernet10BaseT; mbps = 10; break;
	case 1:  type = kIOMediumEthernet100BaseTX; mbps = 100; break;
	default: type = kIOMediumEthernet1000BaseT; mbps = 1000; break;
	}
	type |= (status & E1000_STATUS_FD) ? kIOMediumOptionFullDuplex :
	    kIOMediumOptionHalfDuplex;
	if (getLinkStatus() == (kIONetworkLinkValid | kIONetworkLinkActive) &&
	    getLinkSpeed() == mbps * 1000000ULL)
		return;
	setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive,
	    IONetworkMedium::getMediumWithType(fMedia, type), mbps * 1000000ULL);
	LOG("link up, %llu Mb/s, %s duplex", mbps,
	    (status & E1000_STATUS_FD) ? "full" : "half");
}

IOReturn
RavynE1000::selectMedium(const IONetworkMedium *medium)
{
	/* Autonegotiation only; the hardware picks the medium. */
	if (medium == NULL || medium->getType() != kIOMediumEthernetAuto)
		return kIOReturnUnsupported;
	setSelectedMedium(medium);
	return kIOReturnSuccess;
}

/* ---- Receive and transmit ---------------------------------------------- */

void
RavynE1000::receivePackets()
{
	UInt32 count = 0;

	while (count < kRxRingSize) {
		volatile struct e1000_rx_desc *desc = &fRxRing[fRxNext];
		uint8_t status = desc->status;
		UInt32 len;

		if ((status & E1000_RXD_STAT_DD) == 0)
			break;
		compilerBarrier();
		len = desc->length;
		if ((status & E1000_RXD_STAT_EOP) && desc->errors == 0 &&
		    len >= 14 && len <= kBufferSize) {
			mbuf_t m = allocatePacket(len);

			if (m != NULL && mbuf_copyback(m, 0, len,
			    fRxBufs + fRxNext * kBufferSize, MBUF_DONTWAIT) == 0)
				fNetif->inputPacket(m, 0, kInputOptionQueuePacket);
			else if (m != NULL)
				freePacket(m);
		}
		desc->status = 0;
		desc->length = 0;
		compilerBarrier();
		writeReg(E1000_RDT, fRxNext);	/* hand the buffer back */
		fRxNext = (fRxNext + 1) % kRxRingSize;
		count++;
	}
	if (count > 0)
		fNetif->flushInputQueue();
}

void
RavynE1000::reclaimTx()
{
	bool reclaimed = false;

	while (fTxInUse > 0 && (fTxRing[fTxClean].status & E1000_TXD_STAT_DD)) {
		fTxRing[fTxClean].status = 0;
		fTxClean = (fTxClean + 1) % kTxRingSize;
		fTxInUse--;
		reclaimed = true;
	}
	if (reclaimed && fTxStalled) {
		fTxStalled = false;
		fTxQueue->service(kServiceAsync);
	}
}

/* Called by the output queue, inside the command gate. */
UInt32
RavynE1000::outputPacket(mbuf_t m, void *)
{
	size_t len = mbuf_pkthdr_len(m);
	volatile struct e1000_tx_desc *desc;

	if (!fEnabled || len > kBufferSize) {
		freePacket(m);
		return kIOReturnOutputDropped;
	}
	reclaimTx();
	if (fTxInUse >= kTxRingSize - 1) {
		fTxStalled = true;
		return kIOReturnOutputStall;
	}
	mbuf_copydata(m, 0, len, fTxBufs + fTxNext * kBufferSize);
	freePacket(m);

	desc = &fTxRing[fTxNext];
	desc->addr = fTxBufsPhys + (uint64_t)fTxNext * kBufferSize;
	desc->length = (uint16_t)len;
	desc->cso = 0;
	desc->css = 0;
	desc->special = 0;
	desc->status = 0;
	desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
	fTxNext = (fTxNext + 1) % kTxRingSize;
	fTxInUse++;
	compilerBarrier();
	writeReg(E1000_TDT, fTxNext);
	return kIOReturnOutputSuccess;
}

/* ---- Interrupts and polling -------------------------------------------- */

void
RavynE1000::serviceInterrupt(UInt32 icr)
{
	if (!fEnabled)
		return;
	receivePackets();
	reclaimTx();
	if (icr & (E1000_ICR_LSC | E1000_ICR_RXSEQ))
		checkLink();
}

/* Primary interrupt context: claim the interrupt if the device raised it. */
bool
RavynE1000::interruptFilter(IOFilterInterruptEventSource *)
{
	uint32_t icr;

	if (fRegs == NULL || fPolling)
		return false;
	icr = readReg(E1000_ICR);
	if (icr == 0)
		return false;
	/*
	 * Reading ICR clears it only when INT_ASSERTED is set, which is the
	 * case with INTx but not with MSI; writing the bits back clears them
	 * in both modes, so the next cause raises a new message.
	 */
	writeReg(E1000_ICR, icr);
	OSBitOrAtomic(icr, &fPendingICR);
	return true;
}

void
RavynE1000::interruptHandler(IOInterruptEventSource *, int)
{
	UInt32 icr = OSBitAndAtomic(0, &fPendingICR);

	fInterrupts++;
	serviceInterrupt(icr);
}

/*
 * The first timer after enable() checks that the test interrupt arrived;
 * if not, the driver polls. Later timers poll, or check the link and the
 * transmit ring.
 */
void
RavynE1000::timerFired(IOTimerEventSource *)
{
	if (!fEnabled)
		return;
	if (!fPolling && fInterrupts == 0) {
		LOG("no interrupts from the controller; polling every %d ms", kPollMS);
		fPolling = true;
		writeReg(E1000_IMC, 0xFFFFFFFF);
	}
	if (fPolling) {
		uint32_t icr = readReg(E1000_ICR);

		if (icr != 0)
			writeReg(E1000_ICR, icr);
		serviceInterrupt(icr);
		fTimer->setTimeoutMS(kPollMS);
		return;
	}
	reclaimTx();
	checkLink();
	fTimer->setTimeoutMS(kWatchdogMS);
}

/* ---- IONetworkController ----------------------------------------------- */

IOOutputQueue *
RavynE1000::createOutputQueue()
{
	return IOGatedOutputQueue::withTarget(this, getWorkLoop(), 256);
}

bool
RavynE1000::configureInterface(IONetworkInterface *netif)
{
	return super::configureInterface(netif);
}

bool
RavynE1000::start(IOService *provider)
{
	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || !super::start(provider))
		return false;
	fPCI->retain();
	fTxQueue = getOutputQueue();
	if (fTxQueue == NULL) {
		LOG("no output queue");
		return false;
	}

	fDeviceID = fPCI->configRead16(kIOPCIConfigDeviceID);
	fEERDv2 = fDeviceID == 0x10D3;	/* 82574L */
	fPCI->setMemoryEnable(true);
	fPCI->setBusMasterEnable(true);
	if (!mapRegisters()) {
		LOG("cannot map the registers");
		return false;
	}
	resetChip();
	if (!readMACAddress()) {
		LOG("cannot read the MAC address");
		return false;
	}
	if (!allocateRings()) {
		LOG("cannot allocate the DMA rings");
		return false;
	}
	publishMedia();

	/*
	 * Prefer a message-signalled interrupt (82574L); else the first
	 * source. Sources whose controller cannot serve them are skipped.
	 */
	int intIndex = 0;
	OSArray *controllers = OSDynamicCast(OSArray,
	    fPCI->getProperty(gIOInterruptControllersKey));
	for (unsigned i = 0; controllers != NULL && i < controllers->getCount(); i++) {
		int type;

		if (fPCI->getInterruptType((int)i, &type) == kIOReturnSuccess &&
		    (type & kIOInterruptTypePCIMessaged)) {
			intIndex = (int)i;
			fMSI = true;
			break;
		}
	}
	fIntSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
	    OSMemberFunctionCast(IOInterruptEventSource::Action, this,
	    &RavynE1000::interruptHandler),
	    OSMemberFunctionCast(IOFilterInterruptEventSource::Filter, this,
	    &RavynE1000::interruptFilter), fPCI, intIndex);
	if (fIntSource == NULL ||
	    getWorkLoop()->addEventSource(fIntSource) != kIOReturnSuccess) {
		LOG("no interrupt; polling every %d ms", kPollMS);
		OSSafeReleaseNULL(fIntSource);
		fPolling = true;
	} else {
		fIntSource->enable();
	}
	fTimer = IOTimerEventSource::timerEventSource(this,
	    OSMemberFunctionCast(IOTimerEventSource::Action, this,
	    &RavynE1000::timerFired));
	if (fTimer == NULL || getWorkLoop()->addEventSource(fTimer) != kIOReturnSuccess) {
		LOG("no timer");
		return false;
	}

	LOG("Intel %04x, %s, MAC %02x:%02x:%02x:%02x:%02x:%02x", fDeviceID,
	    fPolling ? "polling" : (fMSI ? "MSI" : "legacy interrupt"),
	    fMAC.bytes[0], fMAC.bytes[1], fMAC.bytes[2], fMAC.bytes[3],
	    fMAC.bytes[4], fMAC.bytes[5]);

	if (!attachInterface((IONetworkInterface **)&fNetif, false))
		return false;
	fNetif->registerService();
	return true;
}

void
RavynE1000::stop(IOService *provider)
{
	if (fEnabled)
		disable(fNetif);
	if (fNetif != NULL)
		detachInterface(fNetif);
	if (fTimer != NULL) {
		fTimer->cancelTimeout();
		getWorkLoop()->removeEventSource(fTimer);
	}
	if (fIntSource != NULL) {
		fIntSource->disable();
		getWorkLoop()->removeEventSource(fIntSource);
	}
	super::stop(provider);
}

void
RavynE1000::free()
{
	if (fRegs != NULL)
		writeReg(E1000_IMC, 0xFFFFFFFF);
	OSSafeReleaseNULL(fNetif);
	OSSafeReleaseNULL(fTimer);
	OSSafeReleaseNULL(fIntSource);
	OSSafeReleaseNULL(fMedia);
	freeRings();
	OSSafeReleaseNULL(fRegsMap);
	OSSafeReleaseNULL(fRegsDesc);
	OSSafeReleaseNULL(fPCI);
	super::free();
}

IOReturn
RavynE1000::enable(IONetworkInterface *)
{
	if (fEnabled)
		return kIOReturnSuccess;
	resetChip();
	writeReg(E1000_RAL0, (uint32_t)fMAC.bytes[0] | ((uint32_t)fMAC.bytes[1] << 8) |
	    ((uint32_t)fMAC.bytes[2] << 16) | ((uint32_t)fMAC.bytes[3] << 24));
	writeReg(E1000_RAH0, (uint32_t)fMAC.bytes[4] | ((uint32_t)fMAC.bytes[5] << 8) |
	    E1000_RAH_AV);
	fEnabled = true;
	initRx();
	initTx();

	fPendingICR = 0;
	fInterrupts = 0;
	if (!fPolling) {
		writeReg(E1000_IMS, E1000_ICR_TXDW | E1000_ICR_LSC | E1000_ICR_RXSEQ |
		    E1000_ICR_RXDMT0 | E1000_ICR_RXO | E1000_ICR_RXT0);
		writeReg(E1000_ICS, E1000_ICR_LSC);	/* test interrupt */
		fTimer->setTimeoutMS(500);
	} else {
		fTimer->setTimeoutMS(kPollMS);
	}
	checkLink();
	fTxQueue->setCapacity(256);
	fTxQueue->start();
	return kIOReturnSuccess;
}

IOReturn
RavynE1000::disable(IONetworkInterface *)
{
	if (!fEnabled)
		return kIOReturnSuccess;
	fEnabled = false;
	fTimer->cancelTimeout();
	fTxQueue->stop();
	fTxQueue->flush();
	writeReg(E1000_IMC, 0xFFFFFFFF);
	writeReg(E1000_RCTL, 0);
	writeReg(E1000_TCTL, 0);
	flushWrites();
	setLinkStatus(kIONetworkLinkValid);
	return kIOReturnSuccess;
}

IOReturn
RavynE1000::getHardwareAddress(IOEthernetAddress *addr)
{
	if (addr == NULL)
		return kIOReturnBadArgument;
	*addr = fMAC;
	return kIOReturnSuccess;
}

IOReturn
RavynE1000::setPromiscuousMode(bool active)
{
	fPromiscuous = active;
	if (fEnabled)
		updateRCTL();
	return kIOReturnSuccess;
}

IOReturn
RavynE1000::setMulticastMode(bool active)
{
	fAllMulticast = active;
	if (fEnabled)
		updateRCTL();
	return kIOReturnSuccess;
}

/* No hash filter yet: any multicast group means all multicast. */
IOReturn
RavynE1000::setMulticastList(IOEthernetAddress *, UInt32 count)
{
	fMulticast = count > 0;
	if (fEnabled)
		updateRCTL();
	return kIOReturnSuccess;
}

const OSString *
RavynE1000::newVendorString() const
{
	return OSString::withCString("Intel");
}

const OSString *
RavynE1000::newModelString() const
{
	switch (fDeviceID) {
	case 0x10D3: return OSString::withCString("82574L Gigabit Ethernet");
	case 0x100F: return OSString::withCString("82545EM Gigabit Ethernet");
	default:     return OSString::withCString("82540EM Gigabit Ethernet");
	}
}
