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
 * Navi 23's display microcontroller (DMCUB; Docs/GPU-RDNA2.md, R2). On
 * DCN 3.0.2 the VBIOS keeps none of the display's command tables but the
 * power gating one: the transmitters, the pixel clocks and the timings
 * are the microcontroller's, which runs AMD's firmware. The firmware's GOP
 * leaves it running, with its mailbox ready; this reads where its memory
 * windows and its mailbox lie in VRAM, as amdgpu's dmub_dcn20.c sets them
 * (MIT), so that the driver keeps clear of them and can talk to it.
 */

#include <IOKit/IOLib.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include "MinkarAMDFramebuffer.h"
#include "amd_dcn302_regs.h"
#include "atom_glue.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDFramebuffer: " fmt "\n", ##__VA_ARGS__)

static void
putNumber(OSDictionary *d, const char *key, uint64_t value)
{
	OSNumber *n = OSNumber::withNumber((unsigned long long)value, 64);

	if (n != NULL) {
		d->setObject(key, n);
		n->release();
	}
}

void
MinkarAMDFramebuffer::describeDMCUB()
{
	static const struct { uint32_t offset, high, base, top; } cw[8] = {
		{ dcn302_DMCUB_REGION3_CW0_OFFSET, dcn302_DMCUB_REGION3_CW0_OFFSET_HIGH,
		  dcn302_DMCUB_REGION3_CW0_BASE_ADDRESS, dcn302_DMCUB_REGION3_CW0_TOP_ADDRESS },
		{ dcn302_DMCUB_REGION3_CW1_OFFSET, dcn302_DMCUB_REGION3_CW1_OFFSET_HIGH,
		  dcn302_DMCUB_REGION3_CW1_BASE_ADDRESS, dcn302_DMCUB_REGION3_CW1_TOP_ADDRESS },
		{ dcn302_DMCUB_REGION3_CW2_OFFSET, dcn302_DMCUB_REGION3_CW2_OFFSET_HIGH,
		  dcn302_DMCUB_REGION3_CW2_BASE_ADDRESS, dcn302_DMCUB_REGION3_CW2_TOP_ADDRESS },
		{ dcn302_DMCUB_REGION3_CW3_OFFSET, dcn302_DMCUB_REGION3_CW3_OFFSET_HIGH,
		  dcn302_DMCUB_REGION3_CW3_BASE_ADDRESS, dcn302_DMCUB_REGION3_CW3_TOP_ADDRESS },
		{ dcn302_DMCUB_REGION3_CW4_OFFSET, dcn302_DMCUB_REGION3_CW4_OFFSET_HIGH,
		  dcn302_DMCUB_REGION3_CW4_BASE_ADDRESS, dcn302_DMCUB_REGION3_CW4_TOP_ADDRESS },
		{ dcn302_DMCUB_REGION3_CW5_OFFSET, dcn302_DMCUB_REGION3_CW5_OFFSET_HIGH,
		  dcn302_DMCUB_REGION3_CW5_BASE_ADDRESS, dcn302_DMCUB_REGION3_CW5_TOP_ADDRESS },
		{ dcn302_DMCUB_REGION3_CW6_OFFSET, dcn302_DMCUB_REGION3_CW6_OFFSET_HIGH,
		  dcn302_DMCUB_REGION3_CW6_BASE_ADDRESS, dcn302_DMCUB_REGION3_CW6_TOP_ADDRESS },
		{ dcn302_DMCUB_REGION3_CW7_OFFSET, dcn302_DMCUB_REGION3_CW7_OFFSET_HIGH,
		  dcn302_DMCUB_REGION3_CW7_BASE_ADDRESS, dcn302_DMCUB_REGION3_CW7_TOP_ADDRESS },
	};
	/* The windows' offsets are the engine's addresses of VRAM, which
	 * starts at fVRAMStart there. */
	uint64_t fbOffset = fVRAMStart;
	OSArray *windows = OSArray::withCapacity(9);

	for (unsigned i = 0; windows != NULL && i < 8; i++) {
		uint64_t at = readReg(cw[i].offset) | (uint64_t)readReg(cw[i].high) << 32;
		uint32_t base = readReg(cw[i].base), top = readReg(cw[i].top);
		OSDictionary *d = OSDictionary::withCapacity(5);
		if (d == NULL)
			continue;
		putNumber(d, "Window", i);
		putNumber(d, "Base", base);
		putNumber(d, "Top", top);
		putNumber(d, "Offset", at);
		if (at != 0)
			putNumber(d, "VRAMOffset", at - fbOffset);
		windows->setObject(d);
		d->release();
		if (at != 0 || base != 0)
			LOG("DMCUB window %u: 0x%08x..0x%08x at VRAM 0x%llx", i, base, top,
			    (unsigned long long)(at - fbOffset));
	}
	uint64_t mail = readReg(dcn302_DMCUB_REGION4_OFFSET) |
	    (uint64_t)readReg(dcn302_DMCUB_REGION4_OFFSET_HIGH) << 32;
	uint32_t size = readReg(dcn302_DMCUB_INBOX1_SIZE);
	uint32_t wptr = readReg(dcn302_DMCUB_INBOX1_WPTR);
	if (windows != NULL) {
		OSDictionary *d = OSDictionary::withCapacity(4);
		if (d != NULL) {
			putNumber(d, "Window", 4 + 0x100);	/* region 4: the mailbox */
			putNumber(d, "Offset", mail);
			putNumber(d, "Top", readReg(dcn302_DMCUB_REGION4_TOP_ADDRESS));
			if (mail != 0)
				putNumber(d, "VRAMOffset", mail - fbOffset);
			windows->setObject(d);
			d->release();
		}
		setProperty("MinkarDMCUBWindows", windows);
		windows->release();
	}
	fDMCUBMail = mail != 0 ? mail - fbOffset : ~0ULL;
	LOG("DMCUB mailbox: VRAM 0x%llx, %u bytes, %u written", (unsigned long long)fDMCUBMail,
	    size, wptr);

	/* What the firmware asked of it, for bring-up: the mailbox lies
	 * past the CPU's window, read through the index registers. */
	uint32_t bytes = wptr < size ? wptr : size;
	if (fDMCUBMail != ~0ULL && bytes != 0 && bytes <= 8192) {
		uint32_t *copy = (uint32_t *)IOMalloc(bytes);
		if (copy != NULL) {
			MinkarAMDVRAMAccess(&fRegs, fDMCUBMail, copy, bytes / 4, false);
			OSData *data = OSData::withBytes(copy, bytes);
			if (data != NULL) {
				setProperty("MinkarDMCUBInbox", data);
				data->release();
			}
			IOFree(copy, bytes);
		}
	}
}

/*
 * A command to the microcontroller (64 bytes, dmub_cmd.h's layout) put in
 * its mailbox, and waited for until it has read it; as amdgpu's
 * dmub_srv_cmd_queue and dmub_srv_wait_for_idle do. The mailbox lies past
 * the CPU's window: written through the index registers.
 */
bool
MinkarAMDFramebuffer::dmcubSend(const uint32_t cmd[16])
{
	if (fDMCUBMail == ~0ULL)
		return false;
	uint32_t size = readReg(dcn302_DMCUB_INBOX1_SIZE);
	uint32_t rptr = readReg(dcn302_DMCUB_INBOX1_RPTR);
	uint32_t wptr = readReg(dcn302_DMCUB_INBOX1_WPTR);
	if (size < 128 || size % 64 != 0 || wptr % 64 != 0 || wptr >= size)
		return false;
	uint32_t next = (wptr + 64) % size;
	if (next == rptr) {
		LOG("DMCUB mailbox full");
		return false;
	}
	MinkarAMDVRAMAccess(&fRegs, fDMCUBMail + wptr, const_cast<uint32_t *>(cmd), 16, true);
	(void)readReg(dcn302_DMCUB_INBOX1_RPTR);	/* the writes done before the pointer moves */
	writeReg(dcn302_DMCUB_INBOX1_WPTR, next);
	for (int i = 0; i < 1000; i++) {
		if (readReg(dcn302_DMCUB_INBOX1_RPTR) == next)
			return true;
		IODelay(100);
	}
	LOG("DMCUB did not take command 0x%08x (read pointer %u, written %u)", cmd[0],
	    readReg(dcn302_DMCUB_INBOX1_RPTR), next);
	return false;
}

/* DIG1TransmitterControl, as the microcontroller runs it for the VBIOS
 * (DMUB_CMD__VBIOS, DMUB_CMD__VBIOS_DIG1_TRANSMITTER_CONTROL): its
 * parameters are v1.6's, in a 60-byte payload. */
bool
MinkarAMDFramebuffer::dmcubTransmitter(const struct minkar_atom_fw_transmitter_args *t)
{
	uint32_t cmd[16];
	uint8_t *p = (uint8_t *)&cmd[1];

	bzero(cmd, sizeof(cmd));
	cmd[0] = 128 | (1 << 8) | (60u << 24);		/* type, sub type, payload bytes */
	p[0] = t->phy;
	p[1] = t->action;
	p[2] = t->digmode;
	p[3] = t->lanes;
	p[4] = (uint8_t)t->symclk_10khz;
	p[5] = (uint8_t)(t->symclk_10khz >> 8);
	p[6] = (uint8_t)(t->symclk_10khz >> 16);
	p[7] = (uint8_t)(t->symclk_10khz >> 24);
	p[8] = t->hpdsel;
	p[9] = t->digfe_sel;
	p[10] = t->connobj_id;
	return dmcubSend(cmd);
}

