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
 * Navi 23's security processor (PSP, MP0 11.0.11; Docs/GPU.md), as
 * amdgpu's psp_v11_0.c and amdgpu_psp.c (MIT) start it: the bootloader
 * given, in turn, the key database, the SPL table, the system driver and
 * the secure OS, each copied to the firmware buffer in VRAM; then the
 * secure OS's sign of life. Its region of VRAM: amd_psp.h. Run by hand
 * for now ("MinkarPSPStep", administrators): AMD's files are on the root
 * file system, which the driver starts before.
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <pexpert/pexpert.h>
#include <libkern/c++/OSNumber.h>
#include "MinkarAMDGPU.h"
#include "MinkarAMDFramebuffer.h"
#include "MinkarAMDFirmware.h"
#include "amd_psp.h"
#include "amd_smu11_layout.h"
#include "atom_glue.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDGPU: PSP: " fmt "\n", ##__VA_ARGS__)

enum {
	kMP0 = 0x16000,			/* MP0_BASE__INST0_SEG0 */
	kC2PMSG35 = kMP0 + 0x63,	/* the bootloader's command and status */
	kC2PMSG36 = kMP0 + 0x64,	/* the firmware buffer, in MB */
	kC2PMSG64 = kMP0 + 0x80,	/* the ring's control */
	kC2PMSG67 = kMP0 + 0x83,	/* the ring's write pointer, in dwords */
	kC2PMSG69 = kMP0 + 0x85, kC2PMSG70 = kMP0 + 0x86, kC2PMSG71 = kMP0 + 0x87,
	kC2PMSG81 = kMP0 + 0x91,	/* the secure OS's sign of life */
	kBLLoadSysDrv = 0x10000, kBLLoadSOS = 0x20000, kBLLoadKeyDatabase = 0x80000,
	kBLLoadSPL = 0x10000000,
	kRingTypeKM = 2,
	kCmdSetupTMR = 0x5, kCmdLoadIPFW = 0x6, kCmdLoadTOC = 0x20,
	kFWTypeSMU = 18,
	/* VRAM for the PSP: past both framebuffers, in the CPU's window. */
	kPSPRegionOffset = 128 << 20,
};

static uint64_t
nowNs(void)
{
	uint64_t ns;

	absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
	return ns;
}

/* A register's bits at a value, polled for up to ms (amdgpu's psp_wait_for). */
bool
MinkarAMDGPU::pspWait(uint32_t reg, uint32_t value, uint32_t mask, unsigned ms)
{
	uint64_t end = nowNs() + (uint64_t)ms * 1000000;

	do {
		if ((readReg(reg) & mask) == value)
			return true;
		IODelay(100);
	} while (nowNs() < end);
	return false;
}

bool
MinkarAMDGPU::pspSOSAlive()
{
	return readReg(kC2PMSG81) != 0;
}

/* One part to the bootloader: once it is ready, the part in the firmware
 * buffer, its address (in MB) and the command; then ready again. */
bool
MinkarAMDGPU::pspLoadPart(const uint8_t *data, uint32_t bytes, uint32_t command, const char *name)
{
	if (pspSOSAlive())
		return true;
	if (!pspWait(kC2PMSG35, 0x80000000, 0x80000000, 1000)) {
		LOG("%s: the bootloader is not ready (0x%08x)", name, readReg(kC2PMSG35));
		return false;
	}
	volatile uint32_t *fw = (volatile uint32_t *)(fPSPMap->getVirtualAddress() + AMD_PSP_FW_OFFSET);
	for (uint32_t i = 0; i < AMD_PSP_FW_BYTES / 4; i++) {
		uint32_t w = 0;
		for (uint32_t k = 0; k < 4 && 4 * i + k < bytes; k++)
			w |= (uint32_t)data[4 * i + k] << (8 * k);
		fw[i] = w;
	}
	OSSynchronizeIO();
	(void)readReg(kC2PMSG35);
	writeReg(kC2PMSG36, (uint32_t)(gpuAddress(fPSPOffset + AMD_PSP_FW_OFFSET) >> 20));
	writeReg(kC2PMSG35, command);
	if (command == kBLLoadSOS) {
		/* The secure OS signs its life (amdgpu: the register changes). */
		IOSleep(30);
		for (int i = 0; i < 2000 && !pspSOSAlive(); i++)
			IOSleep(1);
		LOG("%s: %u bytes; secure OS %s (0x%08x)", name, bytes,
		    pspSOSAlive() ? "alive" : "silent", readReg(kC2PMSG81));
		return pspSOSAlive();
	}
	bool ready = pspWait(kC2PMSG35, 0x80000000, 0x80000000, 1000);
	LOG("%s: %u bytes; bootloader %s (0x%08x)", name, bytes, ready ? "ready" : "busy",
	    readReg(kC2PMSG35));
	return ready;
}

/* Step 1: the secure OS up, from amdgpu/<chip>_sos.bin. */
IOReturn
MinkarAMDGPU::pspStartSOS()
{
	uint8_t *file = NULL;
	size_t bytes = 0;
	struct amd_psp_sos parts;

	if (fPSPMap == NULL)
		return kIOReturnNoResources;
	if (pspSOSAlive()) {
		LOG("the secure OS is already alive (0x%08x)", readReg(kC2PMSG81));
		return kIOReturnSuccess;
	}
	if (!MinkarAMDReadFirmware("dimgrey_cavefish_sos.bin", &file, &bytes))
		return kIOReturnNotReady;
	IOReturn err = kIOReturnIOError;
	if (amd_psp_parse_sos(file, bytes, &parts) != 0) {
		LOG("the sos file is not one it reads");
		err = kIOReturnUnsupported;
	} else if ((parts.kdb.bytes == 0 || pspLoadPart(file + parts.kdb.offset, parts.kdb.bytes,
	    kBLLoadKeyDatabase, "key database")) &&
	    (parts.spl.bytes == 0 || pspLoadPart(file + parts.spl.offset, parts.spl.bytes,
	    kBLLoadSPL, "SPL table")) &&
	    pspLoadPart(file + parts.sys.offset, parts.sys.bytes, kBLLoadSysDrv, "system driver") &&
	    pspLoadPart(file + parts.sos.offset, parts.sos.bytes, kBLLoadSOS, "secure OS")) {
		err = kIOReturnSuccess;
		LOG("the secure OS is up: version 0x%08x", parts.version);
	}
	setProperty("MinkarPSPSecureOS", readReg(kC2PMSG81), 32);
	MinkarAMDFreeFirmware(file, bytes);
	return err;
}

/* The region's words, by byte offset. */
volatile uint32_t *
MinkarAMDGPU::pspWords(uint32_t offset)
{
	return (volatile uint32_t *)(fPSPMap->getVirtualAddress() + offset);
}

static void
copyWords(volatile uint32_t *to, const uint8_t *data, uint32_t bytes, uint32_t clear)
{
	for (uint32_t i = 0; i < clear / 4; i++) {
		uint32_t w = 0;
		for (uint32_t k = 0; k < 4 && 4 * i + k < bytes; k++)
			w |= (uint32_t)data[4 * i + k] << (8 * k);
		to[i] = w;
	}
}

/* The kernel mode ring (psp_v11_0_ring_create): its address and size,
 * then the command, answered in bit 31 with no error. */
bool
MinkarAMDGPU::pspCreateRing()
{
	if (!pspWait(kC2PMSG64, 0x80000000, 0x80000000, 1000)) {
		LOG("the secure OS is not ready for a ring (0x%08x)", readReg(kC2PMSG64));
		return false;
	}
	volatile uint32_t *ring = pspWords(AMD_PSP_RING_OFFSET);
	for (uint32_t i = 0; i < (AMD_PSP_RING_BYTES + 8192) / 4; i++)	/* ring, fence, command */
		ring[i] = 0;
	uint64_t at = gpuAddress(fPSPOffset + AMD_PSP_RING_OFFSET);
	writeReg(kC2PMSG69, (uint32_t)at);
	writeReg(kC2PMSG70, (uint32_t)(at >> 32));
	writeReg(kC2PMSG71, AMD_PSP_RING_BYTES);
	writeReg(kC2PMSG64, kRingTypeKM << 16);
	IOSleep(20);
	bool ok = pspWait(kC2PMSG64, 0x80000000, 0x8000FFFF, 2000);
	LOG("ring at 0x%llx: %s (0x%08x)", (unsigned long long)at, ok ? "created" : "refused",
	    readReg(kC2PMSG64));
	fPSPFence = 0;
	return ok;
}

/* A command (psp_cmd_submit_buf, psp_ring_cmd_submit): the 1 KB command
 * buffer, a frame at the write pointer, the pointer moved, the fence
 * waited for. Its response's status, or -1 when the fence never came;
 * reply: the response's first five words. */
int64_t
MinkarAMDGPU::pspCommand(uint32_t id, const uint32_t *args, unsigned words, uint32_t reply[5])
{
	volatile uint32_t *cmd = pspWords(AMD_PSP_CMD_OFFSET);
	volatile uint32_t *fence = pspWords(AMD_PSP_FENCE_OFFSET);
	for (uint32_t i = 0; i < AMD_PSP_CMD_BYTES / 4; i++)
		cmd[i] = 0;
	cmd[2] = id;				/* +8 */
	for (unsigned i = 0; i < words; i++)
		cmd[7 + i] = args[i];		/* +28: the command's own words */

	uint32_t wptr = readReg(kC2PMSG67), ringDwords = AMD_PSP_RING_BYTES / 4;
	volatile uint32_t *frame = pspWords(AMD_PSP_RING_OFFSET + (wptr % ringDwords) * 4);
	for (unsigned i = 0; i < 16; i++)
		frame[i] = 0;
	uint64_t cmdAt = gpuAddress(fPSPOffset + AMD_PSP_CMD_OFFSET);
	uint64_t fenceAt = gpuAddress(fPSPOffset + AMD_PSP_FENCE_OFFSET);
	uint32_t value = ++fPSPFence;
	frame[0] = (uint32_t)cmdAt;
	frame[1] = (uint32_t)(cmdAt >> 32);
	frame[3] = (uint32_t)fenceAt;
	frame[4] = (uint32_t)(fenceAt >> 32);
	frame[5] = value;
	OSSynchronizeIO();
	(void)readReg(kC2PMSG67);
	writeReg(kC2PMSG67, (wptr + 16) % ringDwords);

	for (int i = 0; i < 20000 && *fence != value; i++)
		IODelay(100);
	if (*fence != value) {
		LOG("command 0x%x: no fence (0x%x), write pointer %u", id, *fence, readReg(kC2PMSG67));
		return -1;
	}
	for (unsigned i = 0; reply != NULL && i < 5; i++)
		reply[i] = cmd[216 + i];	/* +864: the response */
	return cmd[216];
}

/* Step 2: the ring, then the trusted memory region at the size the TOC
 * asks (psp_tmr_init, psp_tmr_load). */
IOReturn
MinkarAMDGPU::pspStartRing()
{
	uint8_t *file = NULL;
	size_t bytes = 0;
	struct amd_psp_sos parts;
	uint32_t reply[5], tmrBytes = 4 << 20;

	if (!pspSOSAlive())
		return kIOReturnNotReady;
	if (!pspCreateRing())
		return kIOReturnIOError;
	if (MinkarAMDReadFirmware("dimgrey_cavefish_sos.bin", &file, &bytes) &&
	    amd_psp_parse_sos(file, bytes, &parts) == 0 && parts.toc.bytes != 0) {
		copyWords(pspWords(AMD_PSP_FW_OFFSET), file + parts.toc.offset, parts.toc.bytes,
		    AMD_PSP_FW_BYTES);
		uint64_t at = gpuAddress(fPSPOffset + AMD_PSP_FW_OFFSET);
		uint32_t args[3] = { (uint32_t)at, (uint32_t)(at >> 32), parts.toc.bytes };
		int64_t status = pspCommand(kCmdLoadTOC, args, 3, reply);
		LOG("LOAD_TOC: status 0x%llx, TMR %u bytes", (long long)status, reply[4]);
		if (status == 0 && reply[4] != 0)
			tmrBytes = reply[4];
	}
	MinkarAMDFreeFirmware(file, bytes);
	if (tmrBytes > AMD_PSP_TMR_BYTES) {
		LOG("the TMR wants %u bytes, more than the region's", tmrBytes);
		return kIOReturnNoSpace;
	}
	uint64_t tmr = gpuAddress(fPSPOffset + AMD_PSP_TMR_OFFSET);
	uint64_t physical = fPSPOffset + AMD_PSP_TMR_OFFSET;	/* its offset in VRAM */
	uint32_t args[6] = { (uint32_t)tmr, (uint32_t)(tmr >> 32), tmrBytes, 0x2 /* virt_phy_addr */,
	    (uint32_t)physical, (uint32_t)(physical >> 32) };
	int64_t status = pspCommand(kCmdSetupTMR, args, 6, reply);
	LOG("SETUP_TMR at 0x%llx, %u bytes: status 0x%llx", (unsigned long long)tmr, tmrBytes,
	    (long long)status);
	setProperty("MinkarPSPTMRBytes", tmrBytes, 32);
	return status == 0 ? kIOReturnSuccess : kIOReturnIOError;
}

/* Step 3: the SMU's firmware loaded (psp_load_smu_fw): its code, from
 * amdgpu/<chip>_smc.bin, in VRAM, and LOAD_IP_FW of type SMU. */
IOReturn
MinkarAMDGPU::pspLoadSMU()
{
	uint8_t *file = NULL;
	size_t bytes = 0;
	uint32_t reply[5];

	if (!MinkarAMDReadFirmware("dimgrey_cavefish_smc.bin", &file, &bytes))
		return kIOReturnNotReady;
	/* common_firmware_header: ucode_size_bytes at +20, its offset at +24. */
	uint32_t size = bytes >= 32 ? (uint32_t)(file[20] | file[21] << 8 | file[22] << 16 | (uint32_t)file[23] << 24) : 0;
	uint32_t offset = bytes >= 32 ? (uint32_t)(file[24] | file[25] << 8 | file[26] << 16 | (uint32_t)file[27] << 24) : 0;
	IOReturn err = kIOReturnUnsupported;
	if (size != 0 && (uint64_t)offset + size <= bytes && size <= AMD_PSP_IP_FW_BYTES) {
		copyWords(pspWords(AMD_PSP_IP_FW_OFFSET), file + offset, size, (size + 4095) & ~4095U);
		uint64_t at = gpuAddress(fPSPOffset + AMD_PSP_IP_FW_OFFSET);
		uint32_t args[4] = { (uint32_t)at, (uint32_t)(at >> 32), size, kFWTypeSMU };
		int64_t status = pspCommand(kCmdLoadIPFW, args, 4, reply);
		LOG("LOAD_IP_FW SMU: %u bytes, status 0x%llx, at 0x%x%08x in the TMR", size,
		    (long long)status, reply[3], reply[2]);
		err = status == 0 ? kIOReturnSuccess : kIOReturnIOError;
	}
	MinkarAMDFreeFirmware(file, bytes);
	return err;
}

/* A message to the SMU on one of its mailboxes (smu_cmn.c): the last
 * one answered, the response cleared, the argument and the message
 * written, the response waited for. The response (1 OK; 0 no answer);
 * the argument read back. */
uint32_t
MinkarAMDGPU::smuMessage(uint32_t box, uint32_t message, uint32_t argument, uint32_t *reply)
{
	static const struct { uint32_t msg, arg, resp; } boxes[2] = {
		{ 0x16282, 0x16292, 0x1629a },	/* MP1 C2PMSG_66/82/90: the driver's */
		{ 0x1628a, 0x16273, 0x16274 },	/* C2PMSG_74/51/52: the display's (DAL) */
	};
	const auto &b = boxes[box & 1];
	/* The last message done (up to 2 s, amdgpu's wait), then this one
	 * (up to 5 s: the DC calibration takes its time). */
	for (int i = 0; i < 2000 && readReg(b.resp) == 0; i++)
		IODelay(1000);
	writeReg(b.resp, 0);
	writeReg(b.arg, argument);
	writeReg(b.msg, message);
	uint32_t result = 0;
	for (int i = 0; i < 5000 && (result = readReg(b.resp)) == 0; i++)
		IODelay(1000);
	if (reply != NULL)
		*reply = readReg(b.arg);
	return result;
}

/* Step 4: whether the SMU answers, on each mailbox (GetSmuVersion, 0x2). */
IOReturn
MinkarAMDGPU::smuProbe()
{
	uint32_t version = 0, dal = 0;
	uint32_t r = smuMessage(0, 0x2, 0, &version);
	LOG("SMU: GetSmuVersion: response 0x%x, version 0x%08x", r, version);
	uint32_t rd = smuMessage(1, 0x2, 0, &dal);
	LOG("SMU (display mailbox): GetSmuVersion: response 0x%x, version 0x%08x", rd, dal);
	setProperty("MinkarSMUVersion", version, 32);
	return r == 1 ? kIOReturnSuccess : kIOReturnNotResponding;
}

/*
 * Step 5: the SMU set up as amdgpu's smu_smc_hw_setup does for SMU 11.0.7
 * (sienna_cichlid_ppt.c, smu_v11_0.c): the driver table's place in VRAM,
 * the power play table (the VBIOS's powerplayinfo, its board part from
 * smc_dpm_info) passed through it, the features allowed, and all of
 * them enabled. The features: amdgpu's that need no
 * other block, and the SOC and display clocks' DPM. Not, until the
 * graphics block runs (R4): its clocks, GFXOFF, its spread spectrum, the
 * DFLL, the D-states, the power and current limits (EnableAllSmuFeatures
 * never answers with them and no RLC); nor BACO, power gating, or the
 * memory clock's DPM (the display's watermarks are not set).
 */
enum {
	kSMUGetDriverIfVersion = 0x3, kSMUSetAllowedFeaturesMaskLow = 0x4,
	kSMUSetAllowedFeaturesMaskHigh = 0x5, kSMUEnableAllSmuFeatures = 0x6,
	kSMUSetDriverDramAddrHigh = 0xE, kSMUSetDriverDramAddrLow = 0xF,
	kSMUTransferTableDram2Smu = 0x13, kSMURunDcBtc = 0x36,
	kSMUEnableSmuFeaturesLow = 0x8, kSMUGetRunningSmuFeaturesLow = 0xC,
	kSMUEnabledNow = (1U << 5) | (1U << 8),	/* DPM_SOCCLK, DPM_DCEFCLK */
	kTablePPTable = 0,
};

static const uint64_t kSMUFeatures =
    (1ULL << 0) |	/* DPM_PREFETCHER */
    (1ULL << 4) |	/* DPM_FCLK */
    (1ULL << 5) |	/* DPM_SOCCLK */
    (1ULL << 6) |	/* DPM_MP0CLK */
    (1ULL << 7) |	/* DPM_LINK */
    (1ULL << 8) |	/* DPM_DCEFCLK: the display's clocks */
    (1ULL << 13) |	/* DS_SOCCLK */
    (1ULL << 14) |	/* DS_FCLK */
    (1ULL << 16) |	/* DS_DCEFCLK */
    (1ULL << 31) |	/* FW_CTF */
    (1ULL << 32) |	/* FAN_CONTROL */
    (1ULL << 33) |	/* THERMAL */
    (1ULL << 44);	/* RSMU_SMN_CG */

bool
MinkarAMDGPU::smuSend(uint32_t message, uint32_t argument, uint32_t *reply, const char *name)
{
	uint32_t r = smuMessage(0, message, argument, reply);
	if (r != 1)
		LOG("SMU: %s (0x%08x): response 0x%x", name, argument, r);
	return r == 1;
}

IOReturn
MinkarAMDGPU::smuSetUp()
{
	uint32_t version = 0;
	uint16_t size, offset, dpmSize, dpmOffset;
	uint8_t frev, crev;

	if (fAtom == NULL)
		return kIOReturnNotReady;
	if (!smuSend(kSMUGetDriverIfVersion, 0, &version, "GetDriverIfVersion"))
		return kIOReturnNotResponding;
	LOG("SMU: driver interface version 0x%x (amdgpu's 0x%x)", version, AMD_SMU11_DRIVER_IF_VERSION);

	/* The power play table from the VBIOS. */
	if (!minkar_atom_data_header(fAtom, AMD_ATOM_DATA_POWERPLAYINFO, &size, &frev, &crev, &offset) ||
	    size < AMD_SMU11_PPTABLE_IN_POWERPLAY + AMD_SMU11_PPTABLE_BYTES ||
	    !minkar_atom_data_header(fAtom, AMD_ATOM_DATA_SMC_DPM_INFO, &dpmSize, &frev, &crev, &dpmOffset) ||
	    dpmSize < AMD_SMU11_DPM_INFO_BOARD_OFFSET + AMD_SMU11_BOARD_BYTES) {
		LOG("SMU: no power play table in the VBIOS (%u bytes)", size);
		return kIOReturnUnsupported;
	}
	const uint8_t *bios = minkar_atom_bios(fAtom);
	uint8_t table[AMD_SMU11_PPTABLE_BYTES];
	bcopy(bios + offset + AMD_SMU11_PPTABLE_IN_POWERPLAY, table, sizeof(table));
	bcopy(bios + dpmOffset + AMD_SMU11_DPM_INFO_BOARD_OFFSET, table + AMD_SMU11_BOARD_OFFSET,
	    AMD_SMU11_BOARD_BYTES);
	copyWords(pspWords(AMD_PSP_SMU_TABLE_OFFSET), table, sizeof(table), AMD_PSP_SMU_TABLE_BYTES);
	OSSynchronizeIO();

	uint64_t at = gpuAddress(fPSPOffset + AMD_PSP_SMU_TABLE_OFFSET);
	if (!smuSend(kSMUSetDriverDramAddrHigh, (uint32_t)(at >> 32), NULL, "SetDriverDramAddrHigh") ||
	    !smuSend(kSMUSetDriverDramAddrLow, (uint32_t)at, NULL, "SetDriverDramAddrLow") ||
	    !smuSend(kSMUTransferTableDram2Smu, kTablePPTable, NULL, "TransferTableDram2Smu"))
		return kIOReturnIOError;
	LOG("SMU: power play table given (%u bytes, VBIOS table %u bytes)", (unsigned)sizeof(table), size);
	/* RunDcBtc, the graphics block's calibration, is left for when that
	 * block is started (R4): without the RLC's firmware it never answers. */
	(void)kSMURunDcBtc;
	if (!smuSend(kSMUSetAllowedFeaturesMaskLow, (uint32_t)kSMUFeatures, NULL, "SetAllowedFeaturesMaskLow") ||
	    !smuSend(kSMUSetAllowedFeaturesMaskHigh, (uint32_t)(kSMUFeatures >> 32), NULL,
	    "SetAllowedFeaturesMaskHigh"))
		return kIOReturnIOError;
	uint32_t running = 0;
	smuSend(kSMUGetRunningSmuFeaturesLow, 0, &running, "GetRunningSmuFeaturesLow");
	LOG("SMU: features running 0x%08x", running);
	/* Enabled one by one, the SOC's and the display's clocks: with every
	 * allowed feature, EnableAllSmuFeatures never answers while the
	 * graphics block has no firmware. */
	(void)kSMUEnableAllSmuFeatures;
	if (!smuSend(kSMUEnableSmuFeaturesLow, kSMUEnabledNow, NULL, "EnableSmuFeaturesLow"))
		return kIOReturnIOError;
	smuSend(kSMUGetRunningSmuFeaturesLow, 0, &running, "GetRunningSmuFeaturesLow");
	LOG("SMU: features running 0x%08x", running);
	setProperty("MinkarSMUFeatures", running, 32);
	return (running & kSMUEnabledNow) == kSMUEnabledNow ? kIOReturnSuccess : kIOReturnIOError;
}

/* The PSP's region of VRAM, mapped. */
bool
MinkarAMDGPU::pspMap()
{
	if (fWindow == NULL)
		fWindow = MinkarAMDBARRange(fPCI, kIOPCIConfigBaseAddress0);
	if (fWindow == NULL || kPSPRegionOffset + AMD_PSP_REGION_BYTES > fWindow->getLength())
		return false;
	fPSPOffset = kPSPRegionOffset;
	IODeviceMemory *range = IODeviceMemory::withSubRange(fWindow, fPSPOffset, AMD_PSP_REGION_BYTES);
	fPSPMap = range != NULL ? range->map(kIOMapInhibitCache) : NULL;
	OSSafeReleaseNULL(range);
	return fPSPMap != NULL;
}

/*
 * At start: once AMD's files can be read (the root file system up), the
 * PSP started, the SMU's firmware loaded and set up, and the display
 * driver told, which may then offer modes faster clocks carry. On a
 * thread of its own; a failure leaves the display as the firmware set
 * it. "minkaramd_psp=0" leaves the PSP alone.
 */
void
MinkarAMDGPU::pspStart()
{
	UInt32 enabled = 1;
	thread_t thread;

	PE_parse_boot_argn("minkaramd_psp", &enabled, sizeof(enabled));
	if (enabled == 0 || fPSPMap == NULL)
		return;
	retain();
	if (kernel_thread_start(&MinkarAMDGPU::pspThread, this, &thread) != KERN_SUCCESS) {
		release();
		return;
	}
	thread_deallocate(thread);
}

void
MinkarAMDGPU::pspThread(void *gpu, wait_result_t)
{
	MinkarAMDGPU *self = (MinkarAMDGPU *)gpu;
	uint8_t *file = NULL;
	size_t bytes = 0;
	int tries = 0;

	/* The root file system: up to two minutes. */
	while (!MinkarAMDReadFirmware("dimgrey_cavefish_sos.bin", &file, &bytes) && ++tries < 120)
		IOSleep(1000);
	if (file == NULL) {
		LOG("AMD's files cannot be read: the PSP is left alone");
		self->release();
		return;
	}
	MinkarAMDFreeFirmware(file, bytes);
	IOReturn err = self->pspStartSOS();
	if (err == kIOReturnSuccess)
		err = self->pspStartRing();
	if (err == kIOReturnSuccess)
		err = self->pspLoadSMU();
	if (err == kIOReturnSuccess)
		err = self->smuProbe();
	if (err == kIOReturnSuccess)
		err = self->smuSetUp();
	self->setProperty("MinkarSMUReady", err == kIOReturnSuccess);
	LOG("%s (0x%x)", err == kIOReturnSuccess ? "the SMU is set up" : "the SMU is not set up", err);
	if (err == kIOReturnSuccess) {
		OSIterator *it = self->fPCI->getClientIterator();
		OSObject *o;
		while (it != NULL && (o = it->getNextObject()) != NULL) {
			MinkarAMDFramebuffer *f = OSDynamicCast(MinkarAMDFramebuffer, o);
			if (f != NULL)
				f->displayClocksReady();
		}
		OSSafeReleaseNULL(it);
	}
	self->release();
}

IOReturn
MinkarAMDGPU::setProperties(OSObject *properties)
{
	OSDictionary *d = OSDynamicCast(OSDictionary, properties);
	OSNumber *step = d != NULL ? OSDynamicCast(OSNumber, d->getObject("MinkarPSPStep")) : NULL;

	if (step == NULL)
		return IOService::setProperties(properties);
	if (IOUserClient::clientHasPrivilege(current_task(), kIOClientPrivilegeAdministrator) != kIOReturnSuccess)
		return kIOReturnNotPrivileged;
	if (!fNavi23)
		return kIOReturnUnsupported;
	switch (step->unsigned32BitValue()) {
	case 1:
		return pspStartSOS();
	case 2:
		return pspStartRing();
	case 3:
		return pspLoadSMU();
	case 4:
		return smuProbe();
	case 5:
		return smuSetUp();
	default:
		return kIOReturnBadArgument;
	}
}
