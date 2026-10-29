/*-
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
 * The system side of Intel's shared e1000 code (e1000/): PCI configuration
 * space, through the IOPCIDevice of the driver that owns the hardware.
 * hw->back points to a struct e1000_osdep whose driver is that IOPCIDevice.
 */

#include <IOKit/pci/IOPCIDevice.h>

extern "C" {
#include "e1000_api.h"
}

extern "C" int minkar_e1000_debug;
int minkar_e1000_debug = 0;

static IOPCIDevice *
pciOf(struct e1000_hw *hw)
{
	struct e1000_osdep *os = (struct e1000_osdep *)hw->back;

	return os != NULL ? (IOPCIDevice *)os->driver : NULL;
}

extern "C" void
e1000_read_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	IOPCIDevice *pci = pciOf(hw);

	*value = pci != NULL ? pci->configRead16((UInt8)reg) : 0;
}

extern "C" void
e1000_write_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	IOPCIDevice *pci = pciOf(hw);

	if (pci != NULL)
		pci->configWrite16((UInt8)reg, *value);
}

extern "C" void
e1000_pci_set_mwi(struct e1000_hw *hw)
{
	u16 cmd = hw->bus.pci_cmd_word | CMD_MEM_WRT_INVALIDATE;

	e1000_write_pci_cfg(hw, PCI_COMMAND_REGISTER, &cmd);
}

extern "C" void
e1000_pci_clear_mwi(struct e1000_hw *hw)
{
	u16 cmd = hw->bus.pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE;

	e1000_write_pci_cfg(hw, PCI_COMMAND_REGISTER, &cmd);
}

/* reg is an offset in the PCI Express capability. */
extern "C" s32
e1000_read_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	IOPCIDevice *pci = pciOf(hw);
	UInt8 cap = 0;

	if (pci == NULL || pci->findPCICapability(kIOPCIPCIExpressCapability, &cap) == 0)
		return -E1000_ERR_CONFIG;
	*value = pci->configRead16((UInt8)(cap + reg));
	return E1000_SUCCESS;
}

extern "C" s32
e1000_write_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value)
{
	IOPCIDevice *pci = pciOf(hw);
	UInt8 cap = 0;

	if (pci == NULL || pci->findPCICapability(kIOPCIPCIExpressCapability, &cap) == 0)
		return -E1000_ERR_CONFIG;
	pci->configWrite16((UInt8)(cap + reg), *value);
	return E1000_SUCCESS;
}

/* ---- The driver's interface (E1000Shared.h) ----------------------------- */

#include "E1000Shared.h"

struct E1000SharedChip {
	struct e1000_hw		hw;
	struct e1000_osdep	os;
};

E1000SharedChip *
E1000SharedCreate(IOPCIDevice *pci, volatile uint8_t *regs)
{
	E1000SharedChip *chip = IONew(E1000SharedChip, 1);

	if (chip == NULL)
		return NULL;
	bzero(chip, sizeof(*chip));
	chip->os.driver = pci;
	chip->hw.back = &chip->os;
	chip->hw.hw_addr = (u8 *)(uintptr_t)regs;
	chip->hw.vendor_id = pci->configRead16(kIOPCIConfigVendorID);
	chip->hw.device_id = pci->configRead16(kIOPCIConfigDeviceID);
	chip->hw.revision_id = pci->configRead8(kIOPCIConfigRevisionID);
	chip->hw.subsystem_vendor_id = pci->configRead16(kIOPCIConfigSubSystemVendorID);
	chip->hw.subsystem_device_id = pci->configRead16(kIOPCIConfigSubSystemID);
	chip->hw.bus.pci_cmd_word = pci->configRead16(kIOPCIConfigCommand);
	if (e1000_set_mac_type(&chip->hw) != E1000_SUCCESS) {
		IODelete(chip, E1000SharedChip, 1);
		return NULL;
	}
	/* From Sunrise Point on the NVM flash is in BAR 0. */
	if (chip->hw.mac.type >= e1000_pch_spt && chip->hw.mac.type <= e1000_pch_nvp)
		chip->hw.flash_address = chip->hw.hw_addr + E1000_FLASH_BASE_ADDR;
	return chip;
}

void
E1000SharedFree(E1000SharedChip *chip)
{
	if (chip != NULL)
		IODelete(chip, E1000SharedChip, 1);
}

E1000Family
E1000SharedFamily(const E1000SharedChip *chip)
{
	switch (chip->hw.mac.type) {
	case e1000_pchlan: case e1000_pch2lan: case e1000_pch_lpt:
	case e1000_pch_spt: case e1000_pch_cnp: case e1000_pch_tgp:
	case e1000_pch_adp: case e1000_pch_mtp: case e1000_pch_ptp:
	case e1000_pch_nvp:
		return kE1000FamilyPCH;
	case e1000_i210: case e1000_i211:
		return kE1000FamilyIGB;
	default:
		return kE1000FamilyOther;
	}
}

const char *
E1000SharedName(const E1000SharedChip *chip)
{
	switch (chip->hw.mac.type) {
	case e1000_pchlan:	return "82577/82578 (PCH)";
	case e1000_pch2lan:	return "82579 (PCH2)";
	case e1000_pch_lpt:	return "I217/I218 (Lynx Point)";
	case e1000_pch_spt:	return "I219 (Sunrise Point/Kaby Lake)";
	case e1000_pch_cnp:	return "I219 (Cannon Lake)";
	case e1000_pch_tgp:	return "I219 (Tiger Lake)";
	case e1000_pch_adp:	return "I219 (Alder Lake)";
	case e1000_pch_mtp:	return "I219 (Meteor Lake)";
	case e1000_pch_ptp:	return "I219 (Panther Lake)";
	case e1000_pch_nvp:	return "I219 (Nova Lake)";
	case e1000_i210:	return "I210";
	case e1000_i211:	return "I211";
	default:		return "8254x/8257x";
	}
}

bool
E1000SharedNeedsFlashBAR(const E1000SharedChip *chip)
{
	return chip->hw.mac.type >= e1000_pchlan && chip->hw.mac.type < e1000_pch_spt;
}

void
E1000SharedSetFlash(E1000SharedChip *chip, volatile uint8_t *flash)
{
	chip->hw.flash_address = (u8 *)(uintptr_t)flash;
}

/*
 * As FreeBSD's em/igb attach and reset do it: the packet buffer split, no
 * flow control (it needs water marks we do not tune), autonegotiation of
 * every speed, then reset and init.
 */
bool
E1000SharedInit(E1000SharedChip *chip, uint8_t mac[6])
{
	struct e1000_hw *hw = &chip->hw;
	u32 pba;

	if (e1000_setup_init_funcs(hw, true) != E1000_SUCCESS) {
		IOLog("MinkarE1000: the shared code cannot set up this %s\n", E1000SharedName(chip));
		return false;
	}
	(void)e1000_get_bus_info(hw);
	hw->mac.autoneg = true;
	hw->phy.autoneg_wait_to_complete = false;
	hw->phy.autoneg_advertised = ADVERTISE_10_HALF | ADVERTISE_10_FULL |
	    ADVERTISE_100_HALF | ADVERTISE_100_FULL | ADVERTISE_1000_FULL;
	hw->mac.report_tx_early = true;
	hw->fc.requested_mode = e1000_fc_none;
	hw->fc.current_mode = e1000_fc_none;

	switch (hw->mac.type) {
	case e1000_pchlan:		pba = E1000_PBA_10K; break;
	case e1000_i210: case e1000_i211: pba = E1000_PBA_34K; break;
	default:			pba = E1000_PBA_26K; break;
	}
	E1000_WRITE_REG(hw, E1000_PBA, pba);

	if (e1000_reset_hw(hw) != E1000_SUCCESS)
		IOLog("MinkarE1000: reset of the %s reported an error\n", E1000SharedName(chip));
	E1000_WRITE_REG(hw, E1000_WUC, 0);
	if (e1000_read_mac_addr(hw) != E1000_SUCCESS) {
		IOLog("MinkarE1000: cannot read the %s's MAC address\n", E1000SharedName(chip));
		return false;
	}
	if (e1000_init_hw(hw) != E1000_SUCCESS)
		IOLog("MinkarE1000: init of the %s reported an error\n", E1000SharedName(chip));
	hw->mac.get_link_status = true;
	bcopy(hw->mac.addr, mac, 6);
	return true;
}

void
E1000SharedReset(E1000SharedChip *chip)
{
	(void)e1000_reset_hw(&chip->hw);
}

void
E1000SharedLink(E1000SharedChip *chip, bool *up, uint32_t *mbps, bool *full)
{
	struct e1000_hw *hw = &chip->hw;
	u16 speed = 0, duplex = 0;

	hw->mac.get_link_status = true;
	(void)e1000_check_for_link(hw);
	*up = (E1000_READ_REG(hw, E1000_STATUS) & E1000_STATUS_LU) != 0;
	*mbps = 0;
	*full = false;
	if (*up && e1000_get_speed_and_duplex(hw, &speed, &duplex) == E1000_SUCCESS) {
		*mbps = speed;
		*full = duplex == FULL_DUPLEX;
	}
}
