/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 1997, 1998, 1999, 2001
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
 * Copyright (c) 2026 The Minkar contributors
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * MinkarBGE: the Broadcom 57765 family on IOKit.
 *
 * The chip setup is FreeBSD's bge(4) (sys/dev/bge/if_bge.c) reduced to
 * this family: bge_reset, bge_chipinit, bge_blockinit, bge_init_locked
 * and bge_stop keep their order and their values, with the branches for
 * other chips left out; the PHY is set up as brgphy(4) does for the
 * BCM57765 PHY. The IOKit side (buffers, interrupts, polling, media) is
 * Minkar's, as in MinkarE1000.
 *
 * QEMU does not emulate these chips: the driver logs each step of its
 * setup (bge_debug=1 adds the register values) so a hardware test shows
 * where it stops.
 */

#include "MinkarBGE.h"
#include <IOKit/network/IONetworkData.h>
#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>
#include <pexpert/pexpert.h>

#include "bge/if_bgereg.h"
#include "bge/brgphyreg.h"

#define super IOEthernetController
OSDefineMetaClassAndStructors(MinkarBGE, IOEthernetController);

#define LOG(fmt, ...)	IOLog("MinkarBGE: " fmt "\n", ##__VA_ARGS__)
#define DEBUG(fmt, ...)	do { if (gBGEDebug) LOG(fmt, ##__VA_ARGS__); } while (0)

static int gBGEDebug;

/* if_bgereg.h leaves these to the driver. */
#define BGE_TIMEOUT_LOOPS	100000		/* BGE_TIMEOUT: 10 us steps */
#ifndef ETHER_CRC_LEN
#define ETHER_CRC_LEN		4
#endif

/* Standard MII advertisement bits (brgphyreg.h names only the Broadcom ones). */
#define MII_ANAR_CSMA		0x0001
#define MII_ANAR_10_HD		0x0020
#define MII_ANAR_10_FD		0x0040
#define MII_ANAR_100_HD		0x0080
#define MII_ANAR_100_FD		0x0100

/* Management firmware (bge_asf_mode). */
#define ASF_ENABLE		1
#define ASF_NEW_HANDSHAKE	2
#define ASF_STACKUP		4

/* bge_sig_* reset kinds. */
#define BGE_RESET_SHUTDOWN	0
#define BGE_RESET_START		1

/* PCI Express device control and status (offsets in the capability). */
#define PCIE_DEVICE_CTL		0x08
#define PCIE_DEVICE_STA		0x0A
#define PCIE_CTL_RELAXED_ORD	0x0010
#define PCIE_CTL_NOSNOOP	0x0800
#define PCIE_CTL_MAX_READ_MASK	0x7000
#define PCIE_CTL_MAX_READ_4096	0x5000
#define PCIE_STA_ERRORS		0x000F

static inline void
compilerBarrier(void)
{
	__asm__ volatile("" ::: "memory");
}

/* ---- Registers, NIC memory and PHY -------------------------------------- */

uint32_t
MinkarBGE::readReg(uint32_t reg) const
{
	return *(volatile uint32_t *)(fRegs + reg);
}

void
MinkarBGE::writeReg(uint32_t reg, uint32_t value)
{
	*(volatile uint32_t *)(fRegs + reg) = value;
}

void
MinkarBGE::setBits(uint32_t reg, uint32_t bits)
{
	writeReg(reg, readReg(reg) | bits);
}

void
MinkarBGE::clearBits(uint32_t reg, uint32_t bits)
{
	writeReg(reg, readReg(reg) & ~bits);
}

/* bge_writembx */
void
MinkarBGE::writeMailbox(uint32_t reg, uint32_t value)
{
	writeReg(reg, value);
}

/* bge_readmem_ind / bge_writemem_ind: NIC memory through PCI config space. */
uint32_t
MinkarBGE::readMemory(uint32_t offset)
{
	uint32_t value;

	fPCI->configWrite32(BGE_PCI_MEMWIN_BASEADDR, offset);
	value = fPCI->configRead32(BGE_PCI_MEMWIN_DATA);
	fPCI->configWrite32(BGE_PCI_MEMWIN_BASEADDR, 0);
	return value;
}

void
MinkarBGE::writeMemory(uint32_t offset, uint32_t value)
{
	fPCI->configWrite32(BGE_PCI_MEMWIN_BASEADDR, offset);
	fPCI->configWrite32(BGE_PCI_MEMWIN_DATA, value);
	fPCI->configWrite32(BGE_PCI_MEMWIN_BASEADDR, 0);
}

/* bge_miibus_readreg (no autopoll and no APE on this family) */
uint16_t
MinkarBGE::readPHY(uint32_t reg)
{
	uint32_t value = 0;
	int i;

	writeReg(BGE_MI_COMM, BGE_MICMD_READ | BGE_MICOMM_BUSY |
	    BGE_MIPHY(kPHYAddress) | BGE_MIREG(reg));
	for (i = 0; i < BGE_TIMEOUT_LOOPS; i++) {
		IODelay(10);
		value = readReg(BGE_MI_COMM);
		if ((value & BGE_MICOMM_BUSY) == 0) {
			IODelay(5);
			value = readReg(BGE_MI_COMM);
			break;
		}
	}
	if (i == BGE_TIMEOUT_LOOPS) {
		LOG("PHY read timed out (register %u)", reg);
		return 0;
	}
	if (value & BGE_MICOMM_READFAIL)
		return 0;
	return (uint16_t)value;
}

/* bge_miibus_writereg */
void
MinkarBGE::writePHY(uint32_t reg, uint16_t value)
{
	int i;

	writeReg(BGE_MI_COMM, BGE_MICMD_WRITE | BGE_MICOMM_BUSY |
	    BGE_MIPHY(kPHYAddress) | BGE_MIREG(reg) | value);
	for (i = 0; i < BGE_TIMEOUT_LOOPS; i++) {
		IODelay(10);
		if ((readReg(BGE_MI_COMM) & BGE_MICOMM_BUSY) == 0) {
			IODelay(5);
			(void)readReg(BGE_MI_COMM);
			break;
		}
	}
	if (i == BGE_TIMEOUT_LOOPS)
		LOG("PHY write timed out (register %u)", reg);
}

/* ---- Registers window ------------------------------------------------------ */

/*
 * BAR 0: the registers (64 KB, memory space, 64-bit on this family, BAR 1
 * holding the high half). When the PCI family made no IODeviceMemory for
 * it (as for these chips behind the Mac Pro 6,1's bridges), the BAR's own
 * address and size are mapped, as MinkarE1000 does.
 */
bool
MinkarBGE::mapRegisters()
{
	uint32_t lo, hi = 0, szlo, szhi = 0xFFFFFFFF;
	uint64_t base, size;
	bool is64;

	fRegsMap = fPCI->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0, kIOMapInhibitCache);
	if (fRegsMap != NULL) {
		fRegs = (volatile uint8_t *)fRegsMap->getVirtualAddress();
		return fRegs != NULL;
	}
	lo = fPCI->configRead32(kIOPCIConfigBaseAddress0);
	is64 = (lo & 0x6) == 0x4;
	if (is64)
		hi = fPCI->configRead32(kIOPCIConfigBaseAddress1);
	if (lo & 1) {
		LOG("BAR 0 is I/O space (0x%08x): cannot map the registers", lo);
		return false;
	}
	fPCI->configWrite32(kIOPCIConfigBaseAddress0, 0xFFFFFFFF);
	szlo = fPCI->configRead32(kIOPCIConfigBaseAddress0);
	fPCI->configWrite32(kIOPCIConfigBaseAddress0, lo);
	if (is64) {
		fPCI->configWrite32(kIOPCIConfigBaseAddress1, 0xFFFFFFFF);
		szhi = fPCI->configRead32(kIOPCIConfigBaseAddress1);
		fPCI->configWrite32(kIOPCIConfigBaseAddress1, hi);
	}
	base = ((uint64_t)hi << 32) | (lo & ~0xFULL);
	size = ~((((uint64_t)szhi << 32) | (szlo & ~0xFULL))) + 1;
	if (!is64)
		size &= 0xFFFFFFFFULL;
	LOG("BAR 0 0x%08x%08x (%s, %llu KB) without IODeviceMemory; mapping it",
	    hi, lo, is64 ? "64-bit" : "32-bit", size / 1024);
	if (base == 0 || size == 0 || size > (16ULL << 20)) {
		LOG("BAR 0 not assigned (address 0x%llx, size 0x%llx): cannot map the registers",
		    base, size);
		return false;
	}
	fRegsDesc = IOMemoryDescriptor::withPhysicalAddress(base, size, kIODirectionNone);
	if (fRegsDesc == NULL || (fRegsMap = fRegsDesc->map(kIOMapAnywhere | kIOMapInhibitCache)) == NULL) {
		LOG("cannot map BAR 0 at 0x%llx", base);
		return false;
	}
	fRegs = (volatile uint8_t *)fRegsMap->getVirtualAddress();
	return fRegs != NULL;
}

/* ---- Chip identification ------------------------------------------------- */

/* bge_chipid: the 57765 family keeps its id in the GEN15 product register. */
bool
MinkarBGE::identifyChip()
{
	uint32_t id = fPCI->configRead32(BGE_PCI_MISC_CTL) >> BGE_PCIMISCCTL_ASICREV_SHIFT;

	if (BGE_ASICREV(id) == BGE_ASICREV_USE_PRODID_REG)
		id = fPCI->configRead32(BGE_PCI_GEN15_PRODID_ASICREV);
	fChipID = id;
	fAsicRev = BGE_ASICREV(id);
	fChipRev = BGE_CHIPREV(id);
	return fAsicRev == BGE_ASICREV_BCM57765 || fAsicRev == BGE_ASICREV_BCM57766;
}

/* ---- Management firmware handshake (bge_sig_*, bge_stop_fw) -------------- */

void
MinkarBGE::signalPreReset(int type)
{
	if (fASF)
		writeMemory(BGE_SRAM_FW_MB, BGE_SRAM_FW_MB_MAGIC);
	if (fASF & ASF_NEW_HANDSHAKE)
		writeMemory(BGE_SRAM_FW_DRV_STATE_MB, type == BGE_RESET_START ?
		    BGE_FW_DRV_STATE_START : BGE_FW_DRV_STATE_UNLOAD);
}

void
MinkarBGE::signalPostReset(int type)
{
	if (fASF & ASF_NEW_HANDSHAKE)
		writeMemory(BGE_SRAM_FW_DRV_STATE_MB, type == BGE_RESET_START ?
		    BGE_FW_DRV_STATE_START_DONE : BGE_FW_DRV_STATE_UNLOAD_DONE);
}

void
MinkarBGE::stopFirmware()
{
	if (fASF == 0)
		return;
	writeMemory(BGE_SRAM_FW_CMD_MB, BGE_FW_CMD_PAUSE);
	setBits(BGE_RX_CPU_EVENT, BGE_RX_CPU_DRV_EVENT);
	for (int i = 0; i < 100; i++) {
		if ((readReg(BGE_RX_CPU_EVENT) & BGE_RX_CPU_DRV_EVENT) == 0)
			break;
		IODelay(10);
	}
}

/* ---- Reset (bge_reset) ---------------------------------------------------- */

static uint32_t
swapOptions(void)
{
	/* bge_dma_swap_options, little endian */
	return BGE_MODECTL_WORDSWAP_NONFRAME | BGE_MODECTL_BYTESWAP_DATA |
	    BGE_MODECTL_WORDSWAP_DATA;
}

bool
MinkarBGE::resetChip()
{
	uint32_t macModeMask = BGE_MACMODE_HALF_DUPLEX | BGE_MACMODE_PORTMODE;
	uint32_t macMode = readReg(BGE_MAC_MODE) & macModeMask;
	uint32_t cacheSize, command, reset, value;
	int i;

	/* Take the NVRAM arbitration lock (the bootcode may be using it). */
	writeReg(BGE_NVRAM_SWARB, BGE_NVRAMSWARB_SET1);
	for (i = 0; i < 8000; i++) {
		if (readReg(BGE_NVRAM_SWARB) & BGE_NVRAMSWARB_GNT1)
			break;
		IODelay(20);
	}
	if (i == 8000)
		DEBUG("NVRAM lock timed out");

	/* Save some important PCI state. */
	cacheSize = fPCI->configRead32(BGE_PCI_CACHESZ);
	command = fPCI->configRead32(BGE_PCI_CMD);
	fPCI->configWrite32(BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_INDIRECT_ACCESS |
	    BGE_PCIMISCCTL_MASK_PCI_INTR | BGE_HIF_SWAP_OPTIONS |
	    BGE_PCIMISCCTL_PCISTATE_RW);

	/* Disable fastboot. */
	writeReg(BGE_FASTBOOT_PC, 0);

	/* The bootcode writes ~MAGIC here when it has finished. */
	writeMemory(BGE_SRAM_FW_MB, BGE_SRAM_FW_MB_MAGIC);

	reset = BGE_MISCCFG_RESET_CORE_CLOCKS | BGE_32BITTIME_66MHZ;
	/* Prevent PCIe link training during the global reset. */
	writeReg(BGE_MISC_CFG, 1U << 29);
	reset |= 1U << 29;
	/* The CPMU keeps the PHY powered: no GPHY power down override. */

	/* Issue global reset (PCIe 575X+: a direct register write). */
	writeReg(BGE_MISC_CFG, reset);
	IOSleep(100);

	if (fPCIeCap != 0) {
		uint16_t devctl = fPCI->configRead16(fPCIeCap + PCIE_DEVICE_CTL);

		/* No snoop and relaxed ordering off; 4 KB read requests. */
		devctl &= ~(PCIE_CTL_RELAXED_ORD | PCIE_CTL_NOSNOOP | PCIE_CTL_MAX_READ_MASK);
		devctl |= PCIE_CTL_MAX_READ_4096;
		fPCI->configWrite16(fPCIeCap + PCIE_DEVICE_CTL, devctl);
		fPCI->configWrite16(fPCIeCap + PCIE_DEVICE_STA, PCIE_STA_ERRORS);
	}

	/* Reset some of the PCI state that got zapped by the reset. */
	fPCI->configWrite32(BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_INDIRECT_ACCESS |
	    BGE_PCIMISCCTL_MASK_PCI_INTR | BGE_HIF_SWAP_OPTIONS |
	    BGE_PCIMISCCTL_PCISTATE_RW);
	fPCI->configWrite32(BGE_PCI_PCISTATE, BGE_PCISTATE_ROM_ENABLE |
	    BGE_PCISTATE_ROM_RETRY_ENABLE);
	fPCI->configWrite32(BGE_PCI_CACHESZ, cacheSize);
	fPCI->configWrite32(BGE_PCI_CMD, command);

	/* Enable the memory arbiter; fix up byte swapping. */
	writeReg(BGE_MARB_MODE, BGE_MARBMODE_ENABLE);
	writeReg(BGE_MODE_CTL, swapOptions());
	value = readReg(BGE_MAC_MODE);
	writeReg(BGE_MAC_MODE, (value & ~macModeMask) | macMode);
	IODelay(40);

	/* Wait for the bootcode: it writes the one's complement of the magic. */
	for (i = 0; i < BGE_TIMEOUT_LOOPS; i++) {
		IODelay(10);
		value = readMemory(BGE_SRAM_FW_MB);
		if (value == (uint32_t)~BGE_SRAM_FW_MB_MAGIC)
			break;
	}
	if (i == BGE_TIMEOUT_LOOPS) {
		LOG("firmware handshake timed out (found 0x%08x)", value);
		return false;
	}
	/* BCM57765 A0 needs additional time before accessing. */
	if (fChipID == BGE_CHIPID_BCM57765_A0)
		IOSleep(10);
	return true;
}

/* ---- Chip init (bge_chipinit) ----------------------------------------------- */

void
MinkarBGE::initChip()
{
	uint32_t dmaRW, modeCtl;

	/* Set endianness before we access any non-PCI registers. */
	fPCI->configWrite32(BGE_PCI_MISC_CTL, BGE_INIT |
	    (fTaggedStatus ? BGE_PCIMISCCTL_TAGGED_STATUS : 0));

	/* Clear the statistics and status blocks in NIC memory (window 0). */
	fPCI->configWrite32(BGE_PCI_MEMWIN_BASEADDR, 0);
	for (uint32_t i = BGE_STATS_BLOCK; i < BGE_STATS_BLOCK_END + 1; i += 4)
		writeReg(BGE_MEMWIN_START + i, 0);
	for (uint32_t i = BGE_STATUS_BLOCK; i < BGE_STATUS_BLOCK_END + 1; i += 4)
		writeReg(BGE_MEMWIN_START + i, 0);

	/*
	 * For the 57766 and non Ax versions of the 57765, the bootcode needs
	 * the PCIe fast training sequence value to prevent transmit hangs.
	 */
	if (fChipRev != BGE_CHIPREV_57765_AX)
		setBits(BGE_CPMU_PADRNG_CTL, BGE_CPMU_PADRNG_CTL_RDIV2);

	/* PCI DMA control. */
	dmaRW = BGE_PCIDMARWCTL_RD_CMD_SHIFT(6) | BGE_PCIDMARWCTL_WR_CMD_SHIFT(7);
	dmaRW |= fMaxPayload >= 256 ? BGE_PCIDMARWCTL_WR_WAT_SHIFT(7) :
	    BGE_PCIDMARWCTL_WR_WAT_SHIFT(3);
	dmaRW &= ~BGE_PCIDMARWCTL_DIS_CACHE_ALIGNMENT;
	if (fChipID == BGE_CHIPID_BCM57765_A0)
		dmaRW &= ~BGE_PCIDMARWCTL_CRDRDR_RDMA_MRRS_MSK;
	fPCI->configWrite32(BGE_PCI_DMA_RW_CTL, dmaRW);

	/* General mode. */
	modeCtl = swapOptions() | BGE_MODECTL_MAC_ATTN_INTR | BGE_MODECTL_HOST_SEND_BDS |
	    BGE_MODECTL_TX_NO_PHDR_CSUM;
	if (fASF & ASF_STACKUP)
		modeCtl |= BGE_MODECTL_STACKUP;
	writeReg(BGE_MODE_CTL, modeCtl);

	/* Memory write invalidate is not supported properly. */
	fPCI->configWrite32(BGE_PCI_CMD, fPCI->configRead32(BGE_PCI_CMD) & ~PCIM_CMD_MWIEN);

	/* Timer prescaler (always 66 MHz). */
	writeReg(BGE_MISC_CFG, BGE_32BITTIME_66MHZ);
}

/* ---- Block init (bge_blockinit) ------------------------------------------------- */

#define RCB_WRITE(base, field, value) \
	writeReg((base) + offsetof(struct bge_rcb, field), (value))

bool
MinkarBGE::initBlocks()
{
	uint32_t rcb, value;
	uint64_t phys;
	int i;

	writeReg(BGE_PCI_MEMWIN_BASEADDR, 0);

	/* Mbuf pool watermarks (5717 and later, standard MTU). */
	writeReg(BGE_BMAN_MBUFPOOL_READDMA_LOWAT, 0x0);
	writeReg(BGE_BMAN_MBUFPOOL_MACRX_LOWAT, 0x2a);
	writeReg(BGE_BMAN_MBUFPOOL_HIWAT, 0xa0);
	writeReg(BGE_BMAN_DMA_DESCPOOL_LOWAT, 5);
	writeReg(BGE_BMAN_DMA_DESCPOOL_HIWAT, 10);

	/* Buffer manager. */
	writeReg(BGE_BMAN_MODE, BGE_BMANMODE_ENABLE | BGE_BMANMODE_LOMBUF_ATTN);
	for (i = 0; i < BGE_TIMEOUT_LOOPS; i++) {
		IODelay(10);
		if (readReg(BGE_BMAN_MODE) & BGE_BMANMODE_ENABLE)
			break;
	}
	if (i == BGE_TIMEOUT_LOOPS) {
		LOG("buffer manager failed to start");
		return false;
	}

	/* Flow-through queues. */
	writeReg(BGE_FTQ_RESET, 0xFFFFFFFF);
	writeReg(BGE_FTQ_RESET, 0);
	for (i = 0; i < BGE_TIMEOUT_LOOPS; i++) {
		IODelay(10);
		if (readReg(BGE_FTQ_RESET) == 0)
			break;
	}
	if (i == BGE_TIMEOUT_LOOPS) {
		LOG("flow-through queue init failed");
		return false;
	}

	/* Standard receive producer ring. */
	phys = fStdRingMem->getPhysicalAddress();
	writeReg(BGE_RX_STD_RCB_HADDR_HI, (uint32_t)BGE_ADDR_HI(phys));
	writeReg(BGE_RX_STD_RCB_HADDR_LO, (uint32_t)BGE_ADDR_LO(phys));
	writeReg(BGE_RX_STD_RCB_MAXLEN_FLAGS,
	    BGE_RCB_MAXLEN_FLAGS(kStdRingSize, BGE_MAX_FRAMELEN << 2));
	writeReg(BGE_RX_STD_RCB_NICADDR, BGE_STD_RX_RINGS);
	writeMailbox(BGE_MBX_RX_STD_PROD_LO, 0);

	/* Jumbo ring: disabled (standard MTU only). */
	writeReg(BGE_RX_JUMBO_RCB_HADDR_HI, 0);
	writeReg(BGE_RX_JUMBO_RCB_HADDR_LO, 0);
	writeReg(BGE_RX_JUMBO_RCB_MAXLEN_FLAGS, BGE_RCB_MAXLEN_FLAGS(0,
	    BGE_RCB_FLAG_USE_EXT_RX_BD | BGE_RCB_FLAG_RING_DISABLED));
	writeReg(BGE_RX_JUMBO_RCB_NICADDR, BGE_JUMBO_RX_RINGS);
	writeMailbox(BGE_MBX_RX_JUMBO_PROD_LO, 0);

	/* Replenish thresholds. */
	writeReg(BGE_RBDI_STD_REPL_THRESH, 8);
	writeReg(BGE_RBDI_JUMBO_REPL_THRESH, BGE_JUMBO_RX_RING_CNT / 8);
	writeReg(BGE_STD_REPLENISH_LWM, 32);
	writeReg(BGE_JMB_REPLENISH_LWM, 16);

	/* Send rings: disable both, then set up ring 0 in host memory. */
	rcb = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	for (i = 0; i < 2; i++) {
		RCB_WRITE(rcb, bge_maxlen_flags, BGE_RCB_MAXLEN_FLAGS(0, BGE_RCB_FLAG_RING_DISABLED));
		RCB_WRITE(rcb, bge_nicaddr, 0);
		rcb += sizeof(struct bge_rcb);
	}
	rcb = BGE_MEMWIN_START + BGE_SEND_RING_RCB;
	phys = fTxRingMem->getPhysicalAddress();
	RCB_WRITE(rcb, bge_hostaddr.bge_addr_hi, (uint32_t)BGE_ADDR_HI(phys));
	RCB_WRITE(rcb, bge_hostaddr.bge_addr_lo, (uint32_t)BGE_ADDR_LO(phys));
	RCB_WRITE(rcb, bge_nicaddr, BGE_NIC_TXRING_ADDR(0, BGE_TX_RING_CNT));
	RCB_WRITE(rcb, bge_maxlen_flags, BGE_RCB_MAXLEN_FLAGS(kTxRingSize, 0));

	/* Receive return rings: disable all four, then set up ring 0. */
	rcb = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	for (i = 0; i < 4; i++) {
		RCB_WRITE(rcb, bge_hostaddr.bge_addr_hi, 0);
		RCB_WRITE(rcb, bge_hostaddr.bge_addr_lo, 0);
		RCB_WRITE(rcb, bge_maxlen_flags, BGE_RCB_FLAG_RING_DISABLED);
		RCB_WRITE(rcb, bge_nicaddr, 0);
		writeMailbox(BGE_MBX_RX_CONS0_LO + (uint32_t)(i * sizeof(uint64_t)), 0);
		rcb += sizeof(struct bge_rcb);
	}
	rcb = BGE_MEMWIN_START + BGE_RX_RETURN_RING_RCB;
	phys = fReturnRingMem->getPhysicalAddress();
	RCB_WRITE(rcb, bge_hostaddr.bge_addr_hi, (uint32_t)BGE_ADDR_HI(phys));
	RCB_WRITE(rcb, bge_hostaddr.bge_addr_lo, (uint32_t)BGE_ADDR_LO(phys));
	RCB_WRITE(rcb, bge_nicaddr, 0);
	RCB_WRITE(rcb, bge_maxlen_flags, BGE_RCB_MAXLEN_FLAGS(kReturnRingSize, 0));

	/* Random backoff seed for transmit; inter-packet gap. */
	value = 0;
	for (i = 0; i < 6; i++)
		value += fMAC.bytes[i];
	writeReg(BGE_TX_RANDOM_BACKOFF, value & BGE_TX_BACKOFF_SEED_MASK);
	writeReg(BGE_TX_LENGTHS, 0x2620);

	/* The ring for packets that match no receive rule; receive lists. */
	writeReg(BGE_RX_RULES_CFG, 0x08);
	writeReg(BGE_RXLP_CFG, 0x181);
	writeReg(BGE_RXLP_STATS_ENABLE_MASK, 0x007FFFFF);
	writeReg(BGE_RXLP_STATS_CTL, 0x1);

	/* Host coalescing: off while it is set up. */
	writeReg(BGE_HCC_MODE, 0);
	for (i = 0; i < BGE_TIMEOUT_LOOPS; i++) {
		IODelay(10);
		if ((readReg(BGE_HCC_MODE) & BGE_HCCMODE_ENABLE) == 0)
			break;
	}
	if (i == BGE_TIMEOUT_LOOPS) {
		LOG("host coalescing engine failed to idle");
		return false;
	}
	writeReg(BGE_HCC_RX_COAL_TICKS, 150);
	writeReg(BGE_HCC_TX_COAL_TICKS, 150);
	writeReg(BGE_HCC_RX_MAX_COAL_BDS, 10);
	writeReg(BGE_HCC_TX_MAX_COAL_BDS, 10);
	writeReg(BGE_HCC_RX_MAX_COAL_BDS_INT, 1);
	writeReg(BGE_HCC_TX_MAX_COAL_BDS_INT, 1);

	/* Status block (32 bytes). */
	phys = fStatusMem->getPhysicalAddress();
	writeReg(BGE_HCC_STATUSBLK_ADDR_HI, (uint32_t)BGE_ADDR_HI(phys));
	writeReg(BGE_HCC_STATUSBLK_ADDR_LO, (uint32_t)BGE_ADDR_LO(phys));
	bzero((void *)fStatus, 32);
	writeReg(BGE_HCC_MODE, BGE_STATBLKSZ_32BYTE | BGE_HCCMODE_ENABLE);

	/* Receive BD completion, list placement. */
	writeReg(BGE_RBDC_MODE, BGE_RBDCMODE_ENABLE | BGE_RBDCMODE_ATTN);
	writeReg(BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);

	/* DMA on, statistics cleared; MII port mode until the link is known. */
	writeReg(BGE_MAC_MODE, BGE_MACMODE_TXDMA_ENB | BGE_MACMODE_RXDMA_ENB |
	    BGE_MACMODE_RX_STATS_CLEAR | BGE_MACMODE_TX_STATS_CLEAR |
	    BGE_MACMODE_RX_STATS_ENB | BGE_MACMODE_TX_STATS_ENB |
	    BGE_MACMODE_FRMHDR_DMA_ENB | BGE_PORTMODE_MII);
	IODelay(40);

	/* Interrupts on attentions. */
	setBits(BGE_MISC_LOCAL_CTL, BGE_MLC_INTR_ONATTN);

	/* Write DMA, with the host coalescing (status tag) fix. */
	writeReg(BGE_WDMA_MODE, BGE_WDMAMODE_ENABLE | BGE_WDMAMODE_ALL_ATTNS |
	    BGE_WDMAMODE_STATUS_TAG_FIX);
	IODelay(40);

	/* Read DMA, with the fix for read DMA FIFO overruns. */
	value = readReg(BGE_RDMA_RSRVCTRL);
	writeReg(BGE_RDMA_RSRVCTRL, value | BGE_RDMA_RSRVCTRL_FIFO_OFLW_FIX);
	writeReg(BGE_RDMA_MODE, BGE_RDMAMODE_ENABLE | BGE_RDMAMODE_ALL_ATTNS |
	    (fPCIeCap != 0 ? BGE_RDMAMODE_FIFO_LONG_BURST : 0));
	IODelay(40);

	/* The remaining receive and send state machines. */
	writeReg(BGE_RDC_MODE, BGE_RDCMODE_ENABLE);
	writeReg(BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);
	writeReg(BGE_RDBDI_MODE, BGE_RDBDIMODE_ENABLE);
	writeReg(BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);
	writeReg(BGE_SDC_MODE, BGE_SDCMODE_ENABLE);
	writeReg(BGE_SDI_MODE, BGE_SDIMODE_ENABLE);
	writeReg(BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);
	writeReg(BGE_SRS_MODE, BGE_SRSMODE_ENABLE);
	writeReg(BGE_SDI_STATS_ENABLE_MASK, 0x007FFFFF);
	writeReg(BGE_SDI_STATS_CTL, BGE_SDISTATSCTL_ENABLE | BGE_SDISTATSCTL_FASTER);

	/* Clear link change events; attention on link changes. */
	writeReg(BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED | BGE_MACSTAT_CFG_CHANGED |
	    BGE_MACSTAT_MI_COMPLETE | BGE_MACSTAT_LINK_CHANGED);
	writeReg(BGE_MI_STS, 0);
	writeReg(BGE_MI_MODE, fMIMode);
	IODelay(80);
	writeReg(BGE_MAC_STS, BGE_MACSTAT_SYNC_CHANGED | BGE_MACSTAT_CFG_CHANGED |
	    BGE_MACSTAT_MI_COMPLETE | BGE_MACSTAT_LINK_CHANGED);
	setBits(BGE_MAC_EVT_ENB, BGE_EVTENB_LINK_CHANGED);
	return true;
}

/* ---- Stop (bge_stop) --------------------------------------------------------------- */

void
MinkarBGE::stopBlock(uint32_t reg, uint32_t bit)
{
	clearBits(reg, bit);
	for (int i = 0; i < BGE_TIMEOUT_LOOPS; i++) {
		if ((readReg(reg) & bit) == 0)
			return;
		IODelay(100);
	}
}

void
MinkarBGE::stopChip()
{
	/* Disable host interrupts. */
	setBits(BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
	writeMailbox(BGE_MBX_IRQ0_LO, 1);

	stopFirmware();
	signalPreReset(BGE_RESET_SHUTDOWN);

	/* Receive blocks. */
	stopBlock(BGE_RX_MODE, BGE_RXMODE_ENABLE);
	stopBlock(BGE_RBDI_MODE, BGE_RBDIMODE_ENABLE);
	stopBlock(BGE_RXLP_MODE, BGE_RXLPMODE_ENABLE);
	stopBlock(BGE_RDBDI_MODE, BGE_RBDIMODE_ENABLE);
	stopBlock(BGE_RDC_MODE, BGE_RDCMODE_ENABLE);
	stopBlock(BGE_RBDC_MODE, BGE_RBDCMODE_ENABLE);

	/* Transmit blocks. */
	stopBlock(BGE_SRS_MODE, BGE_SRSMODE_ENABLE);
	stopBlock(BGE_SBDI_MODE, BGE_SBDIMODE_ENABLE);
	stopBlock(BGE_SDI_MODE, BGE_SDIMODE_ENABLE);
	stopBlock(BGE_RDMA_MODE, BGE_RDMAMODE_ENABLE);
	stopBlock(BGE_SDC_MODE, BGE_SDCMODE_ENABLE);
	stopBlock(BGE_SBDC_MODE, BGE_SBDCMODE_ENABLE);

	/* Memory managers. */
	stopBlock(BGE_HCC_MODE, BGE_HCCMODE_ENABLE);
	stopBlock(BGE_WDMA_MODE, BGE_WDMAMODE_ENABLE);
	writeReg(BGE_FTQ_RESET, 0xFFFFFFFF);
	writeReg(BGE_FTQ_RESET, 0);

	(void)resetChip();
	signalPostReset(BGE_RESET_SHUTDOWN);

	/* Keep the management firmware running if it was. */
	if (fASF & ASF_STACKUP)
		setBits(BGE_MODE_CTL, BGE_MODECTL_STACKUP);
	else
		clearBits(BGE_MODE_CTL, BGE_MODECTL_STACKUP);
}

/* ---- MAC address (bge_get_eaddr) ----------------------------------------------------- */

/* bge_eeprom_getbyte: the auto access interface. */
bool
MinkarBGE::readEEPROMByte(uint32_t addr, uint8_t *byte)
{
	int i;

	setBits(BGE_MISC_LOCAL_CTL, BGE_MLC_AUTO_EEPROM);
	writeReg(BGE_EE_ADDR, BGE_EEADDR_RESET | BGE_EEHALFCLK(BGE_HALFCLK_384SCL));
	IODelay(20);
	writeReg(BGE_EE_ADDR, BGE_EE_READCMD | addr);
	for (i = 0; i < BGE_TIMEOUT_LOOPS * 10; i++) {
		IODelay(10);
		if (readReg(BGE_EE_ADDR) & BGE_EEADDR_DONE)
			break;
	}
	if (i == BGE_TIMEOUT_LOOPS * 10)
		return false;
	*byte = (uint8_t)(readReg(BGE_EE_DATA) >> ((addr % 4) * 8));
	return true;
}

/*
 * The bootcode leaves the address in NIC memory ("HK" signature); else
 * the EEPROM has it; else the address registers as the firmware set them
 * before the reset (beforeReset).
 */
bool
MinkarBGE::readMACAddress(const uint8_t *beforeReset)
{
	uint32_t hi = readMemory(BGE_SRAM_MAC_ADDR_HIGH_MB);
	bool ok = true;

	if ((hi >> 16) == 0x484b) {
		uint32_t lo = readMemory(BGE_SRAM_MAC_ADDR_LOW_MB);

		fMAC.bytes[0] = (UInt8)(hi >> 8);
		fMAC.bytes[1] = (UInt8)hi;
		fMAC.bytes[2] = (UInt8)(lo >> 24);
		fMAC.bytes[3] = (UInt8)(lo >> 16);
		fMAC.bytes[4] = (UInt8)(lo >> 8);
		fMAC.bytes[5] = (UInt8)lo;
		DEBUG("MAC address from NIC memory");
		return true;
	}
	for (int i = 0; i < 6 && ok; i++)
		ok = readEEPROMByte(BGE_EE_MAC_OFFSET + 2 + i, &fMAC.bytes[i]);
	if (ok && (fMAC.bytes[0] | fMAC.bytes[1] | fMAC.bytes[2] | fMAC.bytes[3] |
	    fMAC.bytes[4] | fMAC.bytes[5]) != 0 && (fMAC.bytes[0] & 1) == 0) {
		DEBUG("MAC address from the EEPROM");
		return true;
	}
	bcopy(beforeReset, fMAC.bytes, 6);
	if ((fMAC.bytes[0] | fMAC.bytes[1] | fMAC.bytes[2] | fMAC.bytes[3] |
	    fMAC.bytes[4] | fMAC.bytes[5]) != 0 && (fMAC.bytes[0] & 1) == 0) {
		DEBUG("MAC address from the address registers");
		return true;
	}
	return false;
}

/* ---- DMA memory ------------------------------------------------------------------------ */

static IOBufferMemoryDescriptor *
allocDMA(IOByteCount size)
{
	/*
	 * Below 4 GB and physically contiguous: these chips hang on a DMA
	 * that crosses a 4 GB boundary.
	 */
	IOBufferMemoryDescriptor *mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
	    kernel_task, kIOMemoryPhysicallyContiguous | kIODirectionInOut, size,
	    0x00000000FFFFF000ULL);

	if (mem != NULL)
		bzero(mem->getBytesNoCopy(), size);
	return mem;
}

bool
MinkarBGE::allocateRings()
{
	fStdRingMem = allocDMA(kStdRingSize * sizeof(struct bge_rx_bd));
	fReturnRingMem = allocDMA(kReturnRingSize * sizeof(struct bge_rx_bd));
	fTxRingMem = allocDMA(kTxRingSize * sizeof(struct bge_tx_bd));
	fStatusMem = allocDMA(PAGE_SIZE);
	fRxBufMem = allocDMA(kStdRingSize * kBufferSize);
	fTxBufMem = allocDMA(kTxRingSize * kBufferSize);
	if (fStdRingMem == NULL || fReturnRingMem == NULL || fTxRingMem == NULL ||
	    fStatusMem == NULL || fRxBufMem == NULL || fTxBufMem == NULL)
		return false;
	fStdRing = (volatile struct bge_rx_bd *)fStdRingMem->getBytesNoCopy();
	fReturnRing = (volatile struct bge_rx_bd *)fReturnRingMem->getBytesNoCopy();
	fTxRing = (volatile struct bge_tx_bd *)fTxRingMem->getBytesNoCopy();
	fStatus = (volatile struct bge_status_block *)fStatusMem->getBytesNoCopy();
	fRxBufs = (uint8_t *)fRxBufMem->getBytesNoCopy();
	fTxBufs = (uint8_t *)fTxBufMem->getBytesNoCopy();
	fRxBufsPhys = fRxBufMem->getPhysicalAddress();
	fTxBufsPhys = fTxBufMem->getPhysicalAddress();
	return true;
}

void
MinkarBGE::freeRings()
{
	OSSafeReleaseNULL(fStdRingMem);
	OSSafeReleaseNULL(fReturnRingMem);
	OSSafeReleaseNULL(fTxRingMem);
	OSSafeReleaseNULL(fStatusMem);
	OSSafeReleaseNULL(fRxBufMem);
	OSSafeReleaseNULL(fTxBufMem);
	fStdRing = fReturnRing = NULL;
	fTxRing = NULL;
	fStatus = NULL;
}

/* bge_init_rx_ring_std: buffer i in slot i; the producer index stays one behind. */
void
MinkarBGE::initRxRing()
{
	bzero((void *)fStdRing, kStdRingSize * sizeof(struct bge_rx_bd));
	for (UInt32 i = 0; i < kStdRingSize; i++) {
		volatile struct bge_rx_bd *r = &fStdRing[i];
		uint64_t phys = fRxBufsPhys + (uint64_t)i * kBufferSize;

		r->bge_addr.bge_addr_lo = (uint32_t)BGE_ADDR_LO(phys);
		r->bge_addr.bge_addr_hi = (uint32_t)BGE_ADDR_HI(phys);
		r->bge_flags = BGE_RXBDFLAG_END;
		r->bge_len = kBufferSize;
		r->bge_idx = (uint16_t)i;
	}
	compilerBarrier();
	fStdProd = 0;
	fReturnCons = 0;
	writeMailbox(BGE_MBX_RX_STD_PROD_LO, kStdRingSize - 1);
}

/* bge_init_tx_ring */
void
MinkarBGE::initTxRing()
{
	bzero((void *)fTxRing, kTxRingSize * sizeof(struct bge_tx_bd));
	fTxProd = fTxCons = fTxInUse = 0;
	fTxStalled = false;
	writeMailbox(BGE_MBX_TX_HOST_PROD0_LO, 0);
	writeMailbox(BGE_MBX_TX_NIC_PROD0_LO, 0);
}

/* bge_setpromisc, bge_setmulti */
void
MinkarBGE::programFilters()
{
	if (fPromiscuous)
		setBits(BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
	else
		clearBits(BGE_RX_MODE, BGE_RXMODE_RX_PROMISC);
	for (int i = 0; i < 4; i++)
		writeReg(BGE_MAR0 + 4 * i, (fAllMulticast || fPromiscuous) ?
		    0xFFFFFFFF : fMulticastHash[i]);
}

/* ---- Link ------------------------------------------------------------------------------ */

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
MinkarBGE::publishMedia()
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

/*
 * brgphy_reset (the BCM57765 PHY needs no fixups) and brgphy_mii_phy_auto.
 * A fixed medium is still negotiated, advertising only that one.
 */
void
MinkarBGE::startAutonegotiation(IOMediumType type)
{
	uint16_t anar = MII_ANAR_CSMA, gtcr = 0;
	bool fdx = (type & kIOMediumOptionFullDuplex) != 0;

	writePHY(BRGPHY_MII_BMCR, BRGPHY_BMCR_RESET);
	for (int i = 0; i < 100; i++) {
		if ((readPHY(BRGPHY_MII_BMCR) & BRGPHY_BMCR_RESET) == 0)
			break;
		IODelay(1000);
	}
	switch (type & ~(kIOMediumOptionFullDuplex | kIOMediumOptionHalfDuplex)) {
	case kIOMediumEthernet10BaseT:
		anar |= fdx ? MII_ANAR_10_FD : MII_ANAR_10_HD;
		break;
	case kIOMediumEthernet100BaseTX:
		anar |= fdx ? MII_ANAR_100_FD : MII_ANAR_100_HD;
		break;
	case kIOMediumEthernet1000BaseT:
		gtcr = BRGPHY_1000CTL_AFD;
		break;
	default:
		anar |= MII_ANAR_10_HD | MII_ANAR_10_FD | MII_ANAR_100_HD | MII_ANAR_100_FD;
		gtcr = BRGPHY_1000CTL_AFD | BRGPHY_1000CTL_AHD;
		break;
	}
	writePHY(BRGPHY_MII_ANAR, anar);
	writePHY(BRGPHY_MII_1000CTL, gtcr);
	(void)readPHY(BRGPHY_MII_1000CTL);
	writePHY(BRGPHY_MII_BMCR, BRGPHY_BMCR_AUTOEN | BRGPHY_BMCR_STARTNEG);
	writePHY(BRGPHY_MII_IMR, 0xFF00);
}

/*
 * brgphy_status (copper): the link from BMSR, speed and duplex from the
 * auxiliary status register; then the MAC port mode as bge_miibus_statchg
 * sets it (GMII for 1000 Mb/s, MII below; half duplex bit).
 */
void
MinkarBGE::checkLink()
{
	uint16_t bmsr, aux;

	/*
	 * PCI Express errors the device reported (correctable, non-fatal,
	 * fatal, unsupported request), logged when they change and cleared.
	 */
	if (fPCIeCap != 0) {
		uint16_t sta = fPCI->configRead16(fPCIeCap + PCIE_DEVICE_STA) & PCIE_STA_ERRORS;

		if (sta != 0) {
			fPCIeErrors++;
			if (fPCIeErrors <= 10 || (fPCIeErrors % 100) == 0)
				LOG("PCIe error status 0x%x (%u times)", sta, (unsigned)fPCIeErrors);
			fPCI->configWrite16(fPCIeCap + PCIE_DEVICE_STA, sta);
		}
	}
	IOMediumType type;
	UInt64 mbps;
	bool fdx;
	uint32_t macMode;

	(void)readPHY(BRGPHY_MII_BMSR);		/* the link bit latches low */
	bmsr = readPHY(BRGPHY_MII_BMSR);
	writeReg(BGE_MAC_STS, BGE_MACSTAT_LINK_CHANGED);
	if ((bmsr & BRGPHY_BMSR_LINK) == 0) {
		if (fLinkUp)
			LOG("link down");
		fLinkUp = false;
		setLinkStatus(kIONetworkLinkValid);
		return;
	}
	aux = readPHY(BRGPHY_MII_AUXSTS);
	switch (aux & BRGPHY_AUXSTS_AN_RES) {
	case BRGPHY_RES_1000FD: type = kIOMediumEthernet1000BaseT; mbps = 1000; fdx = true; break;
	case BRGPHY_RES_1000HD: type = kIOMediumEthernet1000BaseT; mbps = 1000; fdx = false; break;
	case BRGPHY_RES_100FD:  type = kIOMediumEthernet100BaseTX; mbps = 100; fdx = true; break;
	case BRGPHY_RES_100HD:  type = kIOMediumEthernet100BaseTX; mbps = 100; fdx = false; break;
	case BRGPHY_RES_10FD:   type = kIOMediumEthernet10BaseT; mbps = 10; fdx = true; break;
	case BRGPHY_RES_10HD:   type = kIOMediumEthernet10BaseT; mbps = 10; fdx = false; break;
	default:
		return;		/* still negotiating */
	}
	if (fLinkUp && getLinkSpeed() == mbps * 1000000ULL)
		return;

	macMode = readReg(BGE_MAC_MODE) & ~(BGE_MACMODE_PORTMODE | BGE_MACMODE_HALF_DUPLEX);
	macMode |= mbps == 1000 ? BGE_PORTMODE_GMII : BGE_PORTMODE_MII;
	if (!fdx)
		macMode |= BGE_MACMODE_HALF_DUPLEX;
	writeReg(BGE_MAC_MODE, macMode);
	IODelay(40);
	clearBits(BGE_TX_MODE, BGE_TXMODE_FLOWCTL_ENABLE);
	clearBits(BGE_RX_MODE, BGE_RXMODE_FLOWCTL_ENABLE);

	fLinkUp = true;
	type |= fdx ? kIOMediumOptionFullDuplex : kIOMediumOptionHalfDuplex;
	setLinkStatus(kIONetworkLinkValid | kIONetworkLinkActive,
	    IONetworkMedium::getMediumWithType(fMedia, type), mbps * 1000000ULL);
	LOG("link up, %llu Mb/s, %s duplex", mbps, fdx ? "full" : "half");
}

IOReturn
MinkarBGE::selectMedium(const IONetworkMedium *medium)
{
	if (medium == NULL)
		return kIOReturnBadArgument;
	if (fEnabled)
		startAutonegotiation(medium->getType());
	setSelectedMedium(medium);
	return kIOReturnSuccess;
}

/* ---- Receive and transmit -------------------------------------------------------------- */

/* bge_rxeof: every returned buffer is copied out and posted again. */
void
MinkarBGE::receivePackets(uint16_t producer)
{
	UInt32 count = 0;

	while (fReturnCons != producer && count < kReturnRingSize) {
		volatile struct bge_rx_bd *cur = &fReturnRing[fReturnCons];
		uint16_t flags = cur->bge_flags, idx = cur->bge_idx;
		UInt32 len = cur->bge_len;

		fReturnCons = (fReturnCons + 1) % kReturnRingSize;
		count++;
		if (idx >= kStdRingSize || (flags & BGE_RXBDFLAG_JUMBO_RING))
			continue;		/* not one of ours */
		if ((flags & BGE_RXBDFLAG_ERROR) == 0 && len >= 14 + ETHER_CRC_LEN &&
		    len <= kBufferSize) {
			mbuf_t m;

			len -= ETHER_CRC_LEN;
			m = allocatePacket(len);
			if (m != NULL &&
			    mbuf_copyback(m, 0, len, fRxBufs + (size_t)idx * kBufferSize,
			    MBUF_DONTWAIT) == 0) {
				if (flags & BGE_RXBDFLAG_VLAN_TAG)
					mbuf_set_vlan_tag(m, cur->bge_vlan_tag);
				fNetif->inputPacket(m, 0, kInputOptionQueuePacket);
			} else if (m != NULL) {
				freePacket(m);
			}
		} else if (fNetStats != NULL) {
			fNetStats->inputErrors++;
		}
		/* bge_rxreuse_std: the freed buffer goes in the next slot. */
		volatile struct bge_rx_bd *r = &fStdRing[fStdProd];
		uint64_t phys = fRxBufsPhys + (uint64_t)idx * kBufferSize;

		r->bge_addr.bge_addr_lo = (uint32_t)BGE_ADDR_LO(phys);
		r->bge_addr.bge_addr_hi = (uint32_t)BGE_ADDR_HI(phys);
		r->bge_flags = BGE_RXBDFLAG_END;
		r->bge_len = kBufferSize;
		r->bge_idx = idx;
		fStdProd = (fStdProd + 1) % kStdRingSize;
	}
	if (count == 0)
		return;
	compilerBarrier();
	writeMailbox(BGE_MBX_RX_CONS0_LO, fReturnCons);
	writeMailbox(BGE_MBX_RX_STD_PROD_LO, (fStdProd + kStdRingSize - 1) % kStdRingSize);
	fNetif->flushInputQueue();
}

/* bge_txeof */
void
MinkarBGE::reclaimTx(uint16_t consumer)
{
	bool reclaimed = false;

	while (fTxCons != consumer && fTxInUse > 0) {
		if (fNetStats != NULL)
			fNetStats->outputPackets++;
		fTxCons = (fTxCons + 1) % kTxRingSize;
		fTxInUse--;
		reclaimed = true;
	}
	if (reclaimed && fTxStalled) {
		fTxStalled = false;
		fTxQueue->service(kServiceAsync);
	}
}

UInt32
MinkarBGE::outputPacket(mbuf_t m, void *)
{
	size_t len = mbuf_pkthdr_len(m);
	u_int16_t vtag = 0;
	bool vlan;

	if (!fEnabled || len == 0 || len > kBufferSize) {
		freePacket(m);
		if (fNetStats != NULL)
			fNetStats->outputErrors++;
		return kIOReturnOutputDropped;
	}
	if (fStatus != NULL)
		reclaimTx(fStatus->bge_idx[0].bge_tx_cons_idx);
	if (fTxInUse + 1 >= kTxRingSize - 1) {
		fTxStalled = true;
		return kIOReturnOutputStall;
	}
	vlan = mbuf_get_vlan_tag(m, &vtag) == 0;

	uint8_t *buf = fTxBufs + (size_t)fTxProd * kBufferSize;
	uint64_t phys = fTxBufsPhys + (uint64_t)fTxProd * kBufferSize;
	volatile struct bge_tx_bd *d = &fTxRing[fTxProd];

	mbuf_copydata(m, 0, len, buf);
	/* Short frames padded to the minimum, as bge_cksum_pad does. */
	if (len < BGE_MIN_FRAMELEN) {
		bzero(buf + len, BGE_MIN_FRAMELEN - len);
		len = BGE_MIN_FRAMELEN;
	}
	d->bge_addr.bge_addr_lo = (uint32_t)BGE_ADDR_LO(phys);
	d->bge_addr.bge_addr_hi = (uint32_t)BGE_ADDR_HI(phys);
	d->bge_len = (uint16_t)len;
	d->bge_flags = BGE_TXBDFLAG_END | (vlan ? BGE_TXBDFLAG_VLAN_TAG : 0);
	d->bge_vlan_tag = vlan ? vtag : 0;
	d->bge_mss = 0;
	fTxProd = (fTxProd + 1) % kTxRingSize;
	fTxInUse++;
	freePacket(m);
	compilerBarrier();
	writeMailbox(BGE_MBX_TX_HOST_PROD0_LO, fTxProd);	/* bge_start_tx */
	return kIOReturnOutputSuccess;
}

/* ---- Interrupts and polling ------------------------------------------------------------- */

/* bge_intr / bge_intr_task: the status block says what changed. */
void
MinkarBGE::service()
{
	uint32_t status, tag;
	uint16_t rxProd, txCons;

	if (!fEnabled || fStatus == NULL)
		return;
	compilerBarrier();
	rxProd = fStatus->bge_idx[0].bge_rx_prod_idx;
	txCons = fStatus->bge_idx[0].bge_tx_cons_idx;
	status = fStatus->bge_status;
	tag = fTaggedStatus ? fStatus->bge_status_tag << 24 : 0;
	fStatus->bge_status = 0;
	compilerBarrier();
	if (status & BGE_STATFLAG_LINKSTATE_CHANGED)
		checkLink();
	/* Let the controller work (and interrupt again for what follows). */
	writeMailbox(BGE_MBX_IRQ0_LO, tag);
	receivePackets(rxProd);
	reclaimTx(txCons);
}

/* Primary interrupt context: claim it if the status block was updated. */
bool
MinkarBGE::interruptFilter(IOFilterInterruptEventSource *)
{
	if (fRegs == NULL || fPolling || !fEnabled || fStatus == NULL)
		return false;
	if (!fMSI) {
		if ((fStatus->bge_status & BGE_STATFLAG_UPDATED) == 0)
			return false;	/* a shared line, not ours */
		/* A level interrupt: quiet the chip until service() acks. */
		writeMailbox(BGE_MBX_IRQ0_LO, 1);
	}
	/* With MSI the chip sends no other message until it is acked. */
	return true;
}

void
MinkarBGE::interruptHandler(IOInterruptEventSource *, int)
{
	fInterrupts++;
	service();
}

/*
 * The first timer after enable() checks that the test interrupt came; if
 * not, the driver polls. Later ones poll, or check the link.
 */
void
MinkarBGE::timerFired(IOTimerEventSource *)
{
	if (!fEnabled)
		return;
	if (!fPolling && fInterrupts == 0) {
		LOG("no interrupts from the controller; polling every %d ms", kPollMS);
		fPolling = true;
	}
	if (fPolling) {
		service();
		if (++fPollCount % (kWatchdogMS / kPollMS) == 0)
			checkLink();
		fTimer->setTimeoutMS(kPollMS);
		return;
	}
	service();	/* a safety net for a lost interrupt */
	checkLink();
	fTimer->setTimeoutMS(kWatchdogMS);
}

/* ---- IONetworkController ------------------------------------------------------------------ */

IOOutputQueue *
MinkarBGE::createOutputQueue()
{
	return IOGatedOutputQueue::withTarget(this, getWorkLoop(), 256);
}

IOService *
MinkarBGE::probe(IOService *provider, SInt32 *score)
{
	IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);

	uint32_t disable = 0;

	if (pci == NULL || super::probe(provider, score) == NULL)
		return NULL;
	/* bge_disable=1: leave the chip alone (to tell its faults from others'). */
	if (PE_parse_boot_argn("bge_disable", &disable, sizeof(disable)) && disable) {
		LOG("%04x: disabled by bge_disable", pci->configRead16(kIOPCIConfigDeviceID));
		return NULL;
	}
	fPCI = pci;
	if (!identifyChip()) {
		LOG("%04x: ASIC revision 0x%x is not of the 57765 family",
		    pci->configRead16(kIOPCIConfigDeviceID), fAsicRev);
		fPCI = NULL;
		return NULL;
	}
	fPCI = NULL;
	return this;
}

bool
MinkarBGE::start(IOService *provider)
{
	uint8_t beforeReset[6];
	uint32_t lo, hi;

	(void)PE_parse_boot_argn("bge_debug", &gBGEDebug, sizeof(gBGEDebug));
	fPCI = OSDynamicCast(IOPCIDevice, provider);
	if (fPCI == NULL || !super::start(provider))
		return false;
	fPCI->retain();
	fTxQueue = getOutputQueue();
	if (fTxQueue == NULL)
		return false;

	fDeviceID = fPCI->configRead16(kIOPCIConfigDeviceID);
	fPCI->setMemoryEnable(true);
	fPCI->setBusMasterEnable(true);
	if (!mapRegisters())
		return false;
	if (!identifyChip()) {
		LOG("not a 57765 family chip (ASIC revision 0x%x)", fAsicRev);
		return false;
	}
	LOG("BCM%04x, chip id 0x%08x", fDeviceID, fChipID);

	/*
	 * PCI Express: the maximum payload size the firmware set (device
	 * control bits 7:5), as bge_attach reads it. The device's capability
	 * can be larger than what its root port accepts.
	 */
	fMaxPayload = 128;
	if (fPCI->findPCICapability(kIOPCIPCIExpressCapability, &fPCIeCap) != 0 && fPCIeCap != 0)
		fMaxPayload = 128U << ((fPCI->configRead16(fPCIeCap + PCIE_DEVICE_CTL) >> 5) & 7);
	else
		fPCIeCap = 0;
	/* The family has a CPMU: the MDIO clock is constant. */
	fMIMode = BGE_MIMODE_500KHZ_CONST;

	/* The address registers as the firmware left them (a last resort). */
	lo = readReg(BGE_MAC_ADDR1_LO);
	hi = readReg(BGE_MAC_ADDR1_HI);
	beforeReset[0] = (uint8_t)(lo >> 8);
	beforeReset[1] = (uint8_t)lo;
	beforeReset[2] = (uint8_t)(hi >> 24);
	beforeReset[3] = (uint8_t)(hi >> 16);
	beforeReset[4] = (uint8_t)(hi >> 8);
	beforeReset[5] = (uint8_t)hi;

	/* Interrupt: MSI when there is one (tagged status then, as bge does). */
	int intIndex = 0;
	OSArray *controllers = OSDynamicCast(OSArray, fPCI->getProperty(gIOInterruptControllersKey));
	for (unsigned i = 0; controllers != NULL && i < controllers->getCount(); i++) {
		int type;

		if (fPCI->getInterruptType((int)i, &type) == kIOReturnSuccess &&
		    (type & kIOInterruptTypePCIMessaged)) {
			intIndex = (int)i;
			fMSI = true;
			break;
		}
	}
	fTaggedStatus = fMSI;
	/* bge_poll=1: no interrupt at all; the timer polls the status block. */
	uint32_t poll = 0;
	if (PE_parse_boot_argn("bge_poll", &poll, sizeof(poll)) && poll) {
		fPolling = true;
		fMSI = false;
		fTaggedStatus = false;
	}

	/* Management firmware (ASF): only when the bootcode says so. */
	fASF = 0;
	if (readMemory(BGE_SRAM_DATA_SIG) == BGE_SRAM_DATA_SIG_MAGIC &&
	    (readMemory(BGE_SRAM_DATA_CFG) & BGE_HWCFG_ASF))
		fASF = ASF_ENABLE | ASF_STACKUP | ASF_NEW_HANDSHAKE;
	DEBUG("PCIe capability 0x%x, max payload %u, ASF 0x%x", fPCIeCap, fMaxPayload, fASF);

	stopFirmware();
	signalPreReset(BGE_RESET_SHUTDOWN);
	if (!resetChip()) {
		LOG("chip reset failed");
		return false;
	}
	signalPostReset(BGE_RESET_SHUTDOWN);
	initChip();
	if (!readMACAddress(beforeReset)) {
		LOG("cannot read the MAC address");
		return false;
	}
	if (!allocateRings()) {
		LOG("cannot allocate the DMA rings");
		return false;
	}
	publishMedia();

	if (fPolling) {
		LOG("polling every %d ms (bge_poll)", kPollMS);
	} else {
		fIntSource = IOFilterInterruptEventSource::filterInterruptEventSource(this,
		    OSMemberFunctionCast(IOInterruptEventSource::Action, this, &MinkarBGE::interruptHandler),
		    OSMemberFunctionCast(IOFilterInterruptEventSource::Filter, this, &MinkarBGE::interruptFilter),
		    fPCI, intIndex);
		if (fIntSource == NULL || getWorkLoop()->addEventSource(fIntSource) != kIOReturnSuccess) {
			LOG("no interrupt; polling every %d ms", kPollMS);
			OSSafeReleaseNULL(fIntSource);
			fPolling = true;
			fMSI = false;
			fTaggedStatus = false;
		} else {
			fIntSource->enable();
		}
	}
	fTimer = IOTimerEventSource::timerEventSource(this,
	    OSMemberFunctionCast(IOTimerEventSource::Action, this, &MinkarBGE::timerFired));
	if (fTimer == NULL || getWorkLoop()->addEventSource(fTimer) != kIOReturnSuccess) {
		LOG("no timer");
		return false;
	}

	LOG("%s, MAC %02x:%02x:%02x:%02x:%02x:%02x",
	    fPolling ? "polling" : (fMSI ? "MSI" : "legacy interrupt"),
	    fMAC.bytes[0], fMAC.bytes[1], fMAC.bytes[2], fMAC.bytes[3], fMAC.bytes[4], fMAC.bytes[5]);

	if (!attachInterface((IONetworkInterface **)&fNetif, false))
		return false;
	fNetStats = fNetif->getNetworkStats();
	if (IONetworkData *d = fNetif->getNetworkData(kIOEthernetStatsKey))
		fEtherStats = (IOEthernetStats *)d->getBuffer();
	fNetif->registerService();
	return true;
}

void
MinkarBGE::stop(IOService *provider)
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
MinkarBGE::free()
{
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

/* bge_init_locked */
IOReturn
MinkarBGE::enable(IONetworkInterface *)
{
	uint32_t mode;

	if (fEnabled)
		return kIOReturnSuccess;

	stopFirmware();
	signalPreReset(BGE_RESET_START);
	if (!resetChip()) {
		LOG("chip reset failed");
		return kIOReturnIOError;
	}
	signalPostReset(BGE_RESET_START);
	initChip();
	if (!initBlocks()) {
		LOG("initialization failure");
		return kIOReturnIOError;
	}
	DEBUG("blocks initialized");

	/* MTU with a VLAN tag. */
	writeReg(BGE_RX_MTU, 1500 + 14 + ETHER_CRC_LEN + 4);

	/* Our MAC address. */
	writeReg(BGE_MAC_ADDR1_LO, (uint32_t)fMAC.bytes[0] << 8 | fMAC.bytes[1]);
	writeReg(BGE_MAC_ADDR1_HI, (uint32_t)fMAC.bytes[2] << 24 |
	    (uint32_t)fMAC.bytes[3] << 16 | (uint32_t)fMAC.bytes[4] << 8 | fMAC.bytes[5]);
	programFilters();
	/* VLAN tags: stripped on receive (bge_setvlan with hardware tagging). */
	clearBits(BGE_RX_MODE, BGE_RXMODE_RX_KEEP_VLAN_DIAG);

	initRxRing();
	initTxRing();
	fEnabled = true;

	/* Transmitter, with the TX MAC lockup fix. */
	mode = readReg(BGE_TX_MODE) | BGE_TXMODE_MBUF_LOCKUP_FIX;
	writeReg(BGE_TX_MODE, mode | BGE_TXMODE_ENABLE);
	IODelay(100);
	/* Receiver. */
	mode = readReg(BGE_RX_MODE) | BGE_RXMODE_IPV6_ENABLE;
	writeReg(BGE_RX_MODE, mode | BGE_RXMODE_ENABLE);
	IODelay(10);
	writeReg(BGE_MAX_RX_FRAME_LOWAT, 1);

	/* Tell the firmware we are alive. */
	setBits(BGE_MODE_CTL, BGE_MODECTL_STACKUP);

	/* Host interrupts on, unless polling (then masked: nobody handles them). */
	fInterrupts = 0;
	if (fPolling) {
		setBits(BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
		writeMailbox(BGE_MBX_IRQ0_LO, 1);
	} else {
		setBits(BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_CLEAR_INTA);
		clearBits(BGE_PCI_MISC_CTL, BGE_PCIMISCCTL_MASK_PCI_INTR);
		writeMailbox(BGE_MBX_IRQ0_LO, 0);
	}

	fLinkUp = false;
	setLinkStatus(kIONetworkLinkValid);
	startAutonegotiation(getSelectedMedium() != NULL ?
	    getSelectedMedium()->getType() : kIOMediumEthernetAuto);
	if (!fPolling) {
		/* A status block update now: the test interrupt. */
		setBits(BGE_HCC_MODE, BGE_HCCMODE_COAL_NOW);
		fTimer->setTimeoutMS(500);
	} else {
		fTimer->setTimeoutMS(kPollMS);
	}
	fTxQueue->setCapacity(256);
	fTxQueue->start();
	DEBUG("enabled");
	return kIOReturnSuccess;
}

IOReturn
MinkarBGE::disable(IONetworkInterface *)
{
	if (!fEnabled)
		return kIOReturnSuccess;
	fEnabled = false;
	fTimer->cancelTimeout();
	fTxQueue->stop();
	fTxQueue->flush();
	stopChip();
	fLinkUp = false;
	setLinkStatus(kIONetworkLinkValid);
	return kIOReturnSuccess;
}

IOReturn
MinkarBGE::getHardwareAddress(IOEthernetAddress *addr)
{
	if (addr == NULL)
		return kIOReturnBadArgument;
	*addr = fMAC;
	return kIOReturnSuccess;
}

IOReturn
MinkarBGE::setPromiscuousMode(bool active)
{
	fPromiscuous = active;
	if (fEnabled)
		programFilters();
	return kIOReturnSuccess;
}

IOReturn
MinkarBGE::setMulticastMode(bool active)
{
	fAllMulticast = active;
	if (fEnabled)
		programFilters();
	return kIOReturnSuccess;
}

/* Little-endian CRC-32 of an address (ether_crc32_le). */
static uint32_t
etherCRC32LE(const uint8_t *buf, size_t len)
{
	uint32_t crc = 0xFFFFFFFF;

	for (size_t i = 0; i < len; i++) {
		crc ^= buf[i];
		for (int bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0);
	}
	return crc;
}

/* bge_hash_maddr: 7 bits of the CRC pick one of 128 hash bits. */
IOReturn
MinkarBGE::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
	bzero(fMulticastHash, sizeof(fMulticastHash));
	for (UInt32 i = 0; addrs != NULL && i < count; i++) {
		uint32_t h = etherCRC32LE(addrs[i].bytes, 6) & 0x7F;

		fMulticastHash[(h & 0x60) >> 5] |= 1U << (h & 0x1F);
	}
	if (fEnabled)
		programFilters();
	return kIOReturnSuccess;
}

UInt32
MinkarBGE::getFeatures() const
{
	return kIONetworkFeatureHardwareVlan;
}

const OSString *
MinkarBGE::newVendorString() const
{
	return OSString::withCString("Broadcom");
}

const OSString *
MinkarBGE::newModelString() const
{
	char name[64];

	snprintf(name, sizeof(name), "BCM%04x NetXtreme Gigabit Ethernet", fDeviceID);
	return OSString::withCString(name);
}
