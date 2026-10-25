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

#include "MinkarE1000.h"
#include <IOKit/network/IONetworkData.h>
#include <IOKit/network/IOEthernetStats.h>
#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>
#include <netinet/in.h>

#define super IOEthernetController
OSDefineMetaClassAndStructors(MinkarE1000, IOEthernetController);

#define LOG(fmt, ...)	IOLog("MinkarE1000: " fmt "\n", ##__VA_ARGS__)

static inline void
compilerBarrier(void)
{
	__asm__ volatile("" ::: "memory");
}

/* ---- Registers --------------------------------------------------------- */

uint32_t
MinkarE1000::readReg(uint32_t reg) const
{
	return *(volatile uint32_t *)(fRegs + reg);
}

void
MinkarE1000::writeReg(uint32_t reg, uint32_t value)
{
	*(volatile uint32_t *)(fRegs + reg) = value;
}

/* Posted writes reach the device before a read of STATUS returns. */
void
MinkarE1000::flushWrites()
{
	(void)readReg(E1000_STATUS);
}

/* BAR 0 is the register window (128 KB, memory space). */
bool
MinkarE1000::mapRegisters()
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
MinkarE1000::readEEPROMWord(uint32_t addr, uint16_t *data)
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
MinkarE1000::readMACAddress()
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
MinkarE1000::allocateRings()
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
MinkarE1000::freeRings()
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
MinkarE1000::resetChip()
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
MinkarE1000::initRx()
{
	uint64_t ring = fRxRingMem->getPhysicalAddress();

	for (UInt32 i = 0; i < kRxRingSize; i++) {
		bzero(&fRxRing[i], sizeof(fRxRing[i]));
		fRxRing[i].addr = fRxBufsPhys + (uint64_t)i * kBufferSize;
	}
	for (UInt32 i = 0; i < 128; i++)
		writeReg(E1000_MTA + 4 * i, fMTA[i]);
	writeReg(E1000_RXCSUM, E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);

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
MinkarE1000::initTx()
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
MinkarE1000::updateRCTL()
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
MinkarE1000::publishMedia()
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
MinkarE1000::checkLink()
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

bool
MinkarE1000::writePHY(uint32_t reg, uint16_t value)
{
	writeReg(E1000_MDIC, value | reg << E1000_MDIC_REG_SHIFT |
	    E1000_PHY_ADDR << E1000_MDIC_PHY_SHIFT | E1000_MDIC_OP_WRITE);
	for (int i = 0; i < 640; i++) {
		uint32_t mdic = readReg(E1000_MDIC);

		if (mdic & E1000_MDIC_READY)
			return (mdic & E1000_MDIC_ERROR) == 0;
		IODelay(50);
	}
	return false;
}

/*
 * A fixed medium is still negotiated, advertising only that speed and
 * duplex (1000BASE-T needs autonegotiation); autoselect advertises all.
 */
bool
MinkarE1000::advertiseMedium(IOMediumType type)
{
	uint16_t anar = MII_ANAR_CSMA | MII_ANAR_PAUSE, gtcr = 0;
	bool fdx = (type & kIOMediumOptionFullDuplex) != 0;

	switch (type & ~(kIOMediumOptionFullDuplex | kIOMediumOptionHalfDuplex)) {
	case kIOMediumEthernetAuto:
		anar |= MII_ANAR_10_HD | MII_ANAR_10_FD | MII_ANAR_100_HD | MII_ANAR_100_FD;
		gtcr = MII_GTCR_1000_FD;
		break;
	case kIOMediumEthernet10BaseT:
		anar |= fdx ? MII_ANAR_10_FD : MII_ANAR_10_HD;
		break;
	case kIOMediumEthernet100BaseTX:
		anar |= fdx ? MII_ANAR_100_FD : MII_ANAR_100_HD;
		break;
	case kIOMediumEthernet1000BaseT:
		gtcr = MII_GTCR_1000_FD;
		break;
	default:
		return false;
	}
	return writePHY(MII_ANAR, anar) && writePHY(MII_GTCR, gtcr) &&
	    writePHY(MII_BMCR, MII_BMCR_AUTONEG | MII_BMCR_RESTART_AN);
}

IOReturn
MinkarE1000::selectMedium(const IONetworkMedium *medium)
{
	if (medium == NULL)
		return kIOReturnBadArgument;
	if (!advertiseMedium(medium->getType()))
		return kIOReturnIOError;
	setSelectedMedium(medium);
	return kIOReturnSuccess;
}

/* ---- Receive and transmit ---------------------------------------------- */

void
MinkarE1000::receivePackets()
{
	UInt32 count = 0;

	while (count < kRxRingSize) {
		volatile struct e1000_rx_desc *desc = &fRxRing[fRxNext];
		uint8_t status = desc->status;
		uint8_t errors;
		UInt32 len;

		if ((status & E1000_RXD_STAT_DD) == 0)
			break;
		compilerBarrier();
		len = desc->length;
		errors = desc->errors;
		if ((status & E1000_RXD_STAT_EOP) && !(errors & E1000_RXD_ERR_FRAME) &&
		    len >= 14 && len <= kBufferSize) {
			const uint8_t *frame = fRxBufs + fRxNext * kBufferSize;
			mbuf_t m = allocatePacket(len);

			if (m != NULL && mbuf_copyback(m, 0, len, frame, MBUF_DONTWAIT) == 0) {
				/* Checksums the hardware verified (IPv4 only). */
				if (!(status & E1000_RXD_STAT_IXSM) && frame[12] == 0x08 &&
				    frame[13] == 0x00) {
					UInt32 valid = 0, good = 0;

					if (status & E1000_RXD_STAT_IPCS) {
						valid |= kChecksumIP;
						if (!(errors & E1000_RXD_ERR_IPE))
							good |= kChecksumIP;
					}
					if (status & E1000_RXD_STAT_TCPCS) {
						valid |= kChecksumTCP | kChecksumUDP;
						if (!(errors & E1000_RXD_ERR_TCPE))
							good |= kChecksumTCP | kChecksumUDP;
					}
					if (valid != 0)
						setChecksumResult(m, kChecksumFamilyTCPIP, good, valid);
				}
				/* A VLAN tag the hardware took out. */
				if (status & E1000_RXD_STAT_VP)
					mbuf_set_vlan_tag(m, desc->special);
				fNetif->inputPacket(m, 0, kInputOptionQueuePacket);
			} else if (m != NULL) {
				freePacket(m);
			}
		} else if (fNetStats != NULL) {
			fNetStats->inputErrors++;
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
MinkarE1000::reclaimTx()
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

/*
 * The checksum and TSO context of an IPv4 packet: where the IP and TCP/UDP
 * headers are. Returns false for packets the hardware cannot help with.
 */
bool
MinkarE1000::buildContext(mbuf_t m, size_t len, bool tso, u_int32_t mss,
    volatile struct e1000_ctx_desc *ctx, size_t *tcpSumOffset, uint16_t *tcpLength)
{
	uint8_t hdr[128];
	size_t l2 = 14, ihl, l4len = 0, hdrlen;
	uint8_t proto;
	uint32_t tucmd;

	mbuf_copydata(m, 0, len < sizeof(hdr) ? len : sizeof(hdr), hdr);
	if (hdr[12] == 0x81 && hdr[13] == 0x00)
		l2 = 18;	/* a VLAN header in the frame (software tagging) */
	if (hdr[l2 - 2] != 0x08 || hdr[l2 - 1] != 0x00 || len < l2 + 20)
		return false;
	ihl = (size_t)(hdr[l2] & 0x0f) * 4;
	proto = hdr[l2 + 9];
	if (ihl < 20 || l2 + ihl + 8 > sizeof(hdr))
		return false;
	tucmd = E1000_TXD_TUCMD_IP | (proto == IPPROTO_TCP ? E1000_TXD_TUCMD_TCP : 0);

	ctx->ipcss = (uint8_t)l2;
	ctx->ipcso = (uint8_t)(l2 + 10);
	ctx->ipcse = (uint16_t)(l2 + ihl - 1);
	ctx->tucss = (uint8_t)(l2 + ihl);
	ctx->tucso = (uint8_t)(l2 + ihl + (proto == IPPROTO_TCP ? 16 : 6));
	ctx->tucse = 0;
	ctx->status = 0;
	ctx->hdrlen = 0;
	ctx->mss = 0;
	if (tso) {
		if (proto != IPPROTO_TCP || l2 + ihl + 13 > sizeof(hdr))
			return false;
		l4len = (size_t)(hdr[l2 + ihl + 12] >> 4) * 4;
		hdrlen = l2 + ihl + l4len;
		*tcpSumOffset = l2 + ihl + 16;
		*tcpLength = (uint16_t)(len - l2 - ihl);
		ctx->hdrlen = (uint8_t)hdrlen;
		ctx->mss = (uint16_t)mss;
		ctx->cmd_and_length = (uint32_t)(len - hdrlen) | E1000_TXD_DTYP_CTX | tucmd |
		    E1000_TXD_CMD_TSE | E1000_TXD_DCMD_DEXT | E1000_TXD_DCMD_RS;
	} else {
		ctx->cmd_and_length = E1000_TXD_DTYP_CTX | tucmd | E1000_TXD_DCMD_DEXT |
		    E1000_TXD_DCMD_RS;
	}
	return true;
}

/*
 * Called by the output queue, inside the command gate. The packet is
 * copied into 2 KB slots, one descriptor each. Checksum offload and TSO
 * use a context descriptor and extended data descriptors; a VLAN tag goes
 * in the descriptors' special field.
 */
UInt32
MinkarE1000::outputPacket(mbuf_t m, void *)
{
	size_t len = mbuf_pkthdr_len(m), off;
	UInt32 demand = 0, slots, need;
	u_int32_t tsoRequest = 0, mss = 0;
	u_int16_t vtag = 0, tcpLength = 0;
	size_t tcpSumOffset = 0;
	bool vlan, tso, offload;

	vlan = mbuf_get_vlan_tag(m, &vtag) == 0;
	getChecksumDemand(m, kChecksumFamilyTCPIP, &demand);
	tso = mbuf_get_tso_requested(m, &tsoRequest, &mss) == 0 &&
	    (tsoRequest & MBUF_TSO_IPV4) && mss != 0;
	offload = tso || (demand & (kChecksumIP | kChecksumTCP | kChecksumUDP));

	if (!fEnabled || len == 0 || len > (tso ? kMaxTSOSize : kBufferSize)) {
		freePacket(m);
		if (fNetStats != NULL)
			fNetStats->outputErrors++;
		return kIOReturnOutputDropped;
	}
	slots = (UInt32)((len + kBufferSize - 1) / kBufferSize);
	need = slots + (offload ? 1 : 0);
	reclaimTx();
	if (fTxInUse + need > kTxRingSize - 1) {
		fTxStalled = true;
		return kIOReturnOutputStall;
	}

	if (offload) {
		volatile struct e1000_ctx_desc *ctx =
		    (volatile struct e1000_ctx_desc *)&fTxRing[fTxNext];

		if (!buildContext(m, len, tso, mss, ctx, &tcpSumOffset, &tcpLength)) {
			/* The stack asked for offload on a packet we cannot parse. */
			freePacket(m);
			if (fNetStats != NULL)
				fNetStats->outputErrors++;
			return kIOReturnOutputDropped;
		}
		fTxNext = (fTxNext + 1) % kTxRingSize;
		fTxInUse++;
	}

	off = 0;
	for (UInt32 i = 0; i < slots; i++) {
		size_t chunk = len - off < kBufferSize ? len - off : kBufferSize;
		bool last = i == slots - 1;
		uint64_t phys = fTxBufsPhys + (uint64_t)fTxNext * kBufferSize;

		mbuf_copydata(m, off, chunk, fTxBufs + fTxNext * kBufferSize);
		if (tso && i == 0) {
			/*
			 * The stack's pseudo-header sum includes the TCP length;
			 * the hardware adds each segment's own, so take it out
			 * (one's complement subtraction).
			 */
			uint8_t *sum = fTxBufs + fTxNext * kBufferSize + tcpSumOffset;
			uint32_t v = ((uint32_t)sum[0] << 8 | sum[1]) + (uint16_t)~tcpLength;

			v = (v & 0xffff) + (v >> 16);
			v = (v & 0xffff) + (v >> 16);
			sum[0] = (uint8_t)(v >> 8);
			sum[1] = (uint8_t)v;
		}
		if (offload) {
			volatile struct e1000_data_desc *d =
			    (volatile struct e1000_data_desc *)&fTxRing[fTxNext];

			d->addr = phys;
			d->cmd_and_length = (uint32_t)chunk | E1000_TXD_DTYP_DATA |
			    E1000_TXD_DCMD_DEXT | E1000_TXD_DCMD_IFCS | E1000_TXD_DCMD_RS |
			    (last ? E1000_TXD_DCMD_EOP : 0) | (tso ? E1000_TXD_CMD_TSE : 0) |
			    (vlan ? E1000_TXD_DCMD_VLE : 0);
			d->status = 0;
			d->popts = (uint8_t)(((demand & kChecksumIP) || tso ? E1000_TXD_POPTS_IXSM : 0) |
			    ((demand & (kChecksumTCP | kChecksumUDP)) || tso ? E1000_TXD_POPTS_TXSM : 0));
			d->special = vlan ? vtag : 0;
		} else {
			volatile struct e1000_tx_desc *d = &fTxRing[fTxNext];

			d->addr = phys;
			d->length = (uint16_t)chunk;
			d->cso = 0;
			d->css = 0;
			d->special = vlan ? vtag : 0;
			d->status = 0;
			d->cmd = E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS |
			    (last ? E1000_TXD_CMD_EOP : 0) | (vlan ? E1000_TXD_CMD_VLE : 0);
		}
		fTxNext = (fTxNext + 1) % kTxRingSize;
		fTxInUse++;
		off += chunk;
	}
	freePacket(m);
	compilerBarrier();
	writeReg(E1000_TDT, fTxNext);
	return kIOReturnOutputSuccess;
}

/* ---- Statistics -------------------------------------------------------- */

/* The hardware counters (clear on read) added to the published statistics. */
void
MinkarE1000::updateStats()
{
	uint32_t crc = readReg(E1000_CRCERRS), algn = readReg(E1000_ALGNERRC);
	uint32_t rxerr = readReg(E1000_RXERRC), mpc = readReg(E1000_MPC);
	uint32_t scc = readReg(E1000_SCC), ecol = readReg(E1000_ECOL);
	uint32_t mcc = readReg(E1000_MCC), latecol = readReg(E1000_LATECOL);
	uint32_t colc = readReg(E1000_COLC), dc = readReg(E1000_DC);
	uint32_t tncrs = readReg(E1000_TNCRS), rnbc = readReg(E1000_RNBC);
	uint32_t ruc = readReg(E1000_RUC), roc = readReg(E1000_ROC);

	if (fEtherStats != NULL) {
		IODot3StatsEntry *e = &fEtherStats->dot3StatsEntry;

		e->fcsErrors += crc;
		e->alignmentErrors += algn;
		e->internalMacReceiveErrors += rxerr;
		e->missedFrames += mpc;
		e->singleCollisionFrames += scc;
		e->multipleCollisionFrames += mcc;
		e->excessiveCollisions += ecol;
		e->lateCollisions += latecol;
		e->deferredTransmissions += dc;
		e->carrierSenseErrors += tncrs;
		e->frameTooLongs += roc;
		fEtherStats->dot3RxExtraEntry.frameTooShorts += ruc;
		fEtherStats->dot3RxExtraEntry.resourceErrors += rnbc;
	}
	if (fNetStats != NULL) {
		fNetStats->inputErrors += crc + algn + rxerr + ruc + roc;
		fNetStats->collisions += colc;
		fNetStats->outputErrors += ecol + latecol;
	}
}

/* ---- Interrupts and polling -------------------------------------------- */

void
MinkarE1000::serviceInterrupt(UInt32 icr)
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
MinkarE1000::interruptFilter(IOFilterInterruptEventSource *)
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
MinkarE1000::interruptHandler(IOInterruptEventSource *, int)
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
MinkarE1000::timerFired(IOTimerEventSource *)
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
		if (++fPollCount % (kWatchdogMS / kPollMS) == 0)
			updateStats();
		fTimer->setTimeoutMS(kPollMS);
		return;
	}
	reclaimTx();
	checkLink();
	updateStats();
	fTimer->setTimeoutMS(kWatchdogMS);
}

/* ---- IONetworkController ----------------------------------------------- */

IOOutputQueue *
MinkarE1000::createOutputQueue()
{
	return IOGatedOutputQueue::withTarget(this, getWorkLoop(), 256);
}

bool
MinkarE1000::configureInterface(IONetworkInterface *netif)
{
	return super::configureInterface(netif);
}

bool
MinkarE1000::start(IOService *provider)
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
	    &MinkarE1000::interruptHandler),
	    OSMemberFunctionCast(IOFilterInterruptEventSource::Filter, this,
	    &MinkarE1000::interruptFilter), fPCI, intIndex);
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
	    &MinkarE1000::timerFired));
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
	fNetStats = fNetif->getNetworkStats();
	if (IONetworkData *d = fNetif->getNetworkData(kIOEthernetStatsKey))
		fEtherStats = (IOEthernetStats *)d->getBuffer();
	fNetif->registerService();
	return true;
}

void
MinkarE1000::stop(IOService *provider)
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
MinkarE1000::free()
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
MinkarE1000::enable(IONetworkInterface *)
{
	if (fEnabled)
		return kIOReturnSuccess;
	resetChip();
	/* VLAN tags: stripped on receive, inserted on transmit. */
	writeReg(E1000_CTRL, readReg(E1000_CTRL) | E1000_CTRL_VME);
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
MinkarE1000::disable(IONetworkInterface *)
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
MinkarE1000::getHardwareAddress(IOEthernetAddress *addr)
{
	if (addr == NULL)
		return kIOReturnBadArgument;
	*addr = fMAC;
	return kIOReturnSuccess;
}

IOReturn
MinkarE1000::setPromiscuousMode(bool active)
{
	fPromiscuous = active;
	if (fEnabled)
		updateRCTL();
	return kIOReturnSuccess;
}

IOReturn
MinkarE1000::setMulticastMode(bool active)
{
	fAllMulticast = active;
	if (fEnabled)
		updateRCTL();
	return kIOReturnSuccess;
}

/* No hash filter yet: any multicast group means all multicast. */
/*
 * The multicast table: bits 47:36 of each address (MO = 0) pick one of
 * 4096 bits. More groups than that can sensibly hash: all multicast.
 */
IOReturn
MinkarE1000::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
	bzero(fMTA, sizeof(fMTA));
	fMulticast = count > 256;
	for (UInt32 i = 0; addrs != NULL && i < count && !fMulticast; i++) {
		uint32_t hash = ((uint32_t)addrs[i].bytes[4] >> 4 |
		    (uint32_t)addrs[i].bytes[5] << 4) & 0xFFF;

		fMTA[hash >> 5] |= 1U << (hash & 31);
	}
	if (fEnabled) {
		for (UInt32 i = 0; i < 128; i++)
			writeReg(E1000_MTA + 4 * i, fMTA[i]);
		updateRCTL();
	}
	return kIOReturnSuccess;
}

/* Receive: IP, TCP and UDP (IPv4). Transmit: the same, with a context descriptor. */
IOReturn
MinkarE1000::getChecksumSupport(UInt32 *mask, UInt32 family, bool)
{
	if (mask == NULL || family != kChecksumFamilyTCPIP)
		return kIOReturnUnsupported;
	*mask = kChecksumIP | kChecksumTCP | kChecksumUDP;
	return kIOReturnSuccess;
}

UInt32
MinkarE1000::getFeatures() const
{
	return kIONetworkFeatureHardwareVlan | kIONetworkFeatureTSOIPv4;
}

const OSString *
MinkarE1000::newVendorString() const
{
	return OSString::withCString("Intel");
}

const OSString *
MinkarE1000::newModelString() const
{
	switch (fDeviceID) {
	case 0x10D3: return OSString::withCString("82574L Gigabit Ethernet");
	case 0x100F: return OSString::withCString("82545EM Gigabit Ethernet");
	default:     return OSString::withCString("82540EM Gigabit Ethernet");
	}
}
