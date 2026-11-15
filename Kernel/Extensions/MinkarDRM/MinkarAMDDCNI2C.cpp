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
 * The display engine's I2C on Navi 23 (DCN 3.0.2; Docs/GPU-RDNA2.md, R2):
 * the hardware engine on one of the five DDC lines, as amdgpu's
 * dce_i2c_hw.c (MIT) drives it: the line's pads handed to the engine, a
 * write and a read queued as two transactions, both run with one GO, and
 * the reply read back from the engine's buffer. With it, the EDID of an
 * HDMI or DVI sink, which no firmware is needed for.
 */

#include <IOKit/IOLib.h>
#include <kern/clock.h>
#include <sys/errno.h>
#include "MinkarAMDFramebuffer.h"
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include "amd_dcn302_regs.h"
#include "atom_glue.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDFramebuffer: " fmt "\n", ##__VA_ARGS__)

#define M(reg, f)	DCN302_##reg##__##f##_MASK
#define S(reg, f)	DCN302_##reg##__##f##__SHIFT
#define GET(v, reg, f)	(((v) & M(reg, f)) >> S(reg, f))
#define PUT(reg, f, x)	(((uint32_t)(x) << S(reg, f)) & M(reg, f))

enum {
	kI2CIdle = 0,			/* DC_I2C_SW_STATUS, DC_I2C_DDCn_HW_STATUS */
	kI2CUsedBySW = 1,
	kI2CUsedByHW = 2,
	kI2CDMCUOnly = 2,		/* DC_I2C_REG_RW_CNTL_STATUS */
	kI2CSpeedKHz = 100,		/* dcn302's i2c_speed_in_khz */
	kI2CTimeLimit = 3,		/* I2C_SETUP_TIME_LIMIT_DCN */
	kI2CResetLength = 9,		/* I2C_SEND_RESET_LENGTH_9 */
	kI2CBuffer = 538,		/* I2C_HW_BUFFER_SIZE_DCE100 */
	kEDIDAddress = 0x50,
};

static uint64_t
nowNs(void)
{
	uint64_t ns;

	absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
	return ns;
}

static void
update(MinkarAMDFramebuffer *fb, uint32_t reg, uint32_t mask, uint32_t value)
{
	fb->writeReg(reg, (fb->readReg(reg) & ~mask) | (value & mask));
}

/*
 * One I2C exchange on DDC line (0 for DDC1): wlen bytes written to the
 * 7-bit address slave, then rlen read from it after a repeated start.
 * 0, or an errno: ENXIO when nothing answers, EBUSY when the engine is
 * someone else's, ETIMEDOUT.
 */
int
MinkarAMDFramebuffer::i2cDCN(unsigned line, uint8_t slave, const uint8_t *w, unsigned wlen,
    uint8_t *r, unsigned rlen)
{
	static const uint32_t transaction[4] = { dcn302_DC_I2C_TRANSACTION0,
	    dcn302_DC_I2C_TRANSACTION1, dcn302_DC_I2C_TRANSACTION2, dcn302_DC_I2C_TRANSACTION3 };
	const uint32_t txMask = M(DC_I2C_TRANSACTION0, DC_I2C_RW0) |
	    M(DC_I2C_TRANSACTION0, DC_I2C_STOP_ON_NACK0) | M(DC_I2C_TRANSACTION0, DC_I2C_START0) |
	    M(DC_I2C_TRANSACTION0, DC_I2C_STOP0) | M(DC_I2C_TRANSACTION0, DC_I2C_COUNT0);
	unsigned count = 0, index = 0;
	int err = 0;

	if (line >= DCN302_DDCS || (wlen == 0 && rlen == 0) || wlen + rlen + 2 >= kI2CBuffer)
		return EINVAL;

	/* The engine: not the hardware's (an EDID read by itself) nor the
	 * display microcontroller's. */
	if (GET(readReg(dcn302_DC_I2C_DDC_HW_STATUS[line]), DC_I2C_DDC1_HW_STATUS,
	    DC_I2C_DDC1_HW_STATUS) == kI2CUsedByHW ||
	    GET(readReg(dcn302_DC_I2C_ARBITRATION), DC_I2C_ARBITRATION,
	    DC_I2C_REG_RW_CNTL_STATUS) == kI2CDMCUOnly)
		return EBUSY;

	/* The line's pads to the engine, with their pull-downs (amdgpu's
	 * hw_ddc set_config and the GPIO's hardware mode). */
	uint32_t pads = readReg(dcn302_DC_GPIO_DDC_MASK[line]);
	writeReg(dcn302_DC_GPIO_DDC_MASK[line],
	    (pads & ~(M(DC_GPIO_DDC1_MASK, DC_GPIO_DDC1CLK_MASK) | M(DC_GPIO_DDC1_MASK, DC_GPIO_DDC1DATA_MASK))) |
	    M(DC_GPIO_DDC1_MASK, DC_GPIO_DDC1CLK_PD_EN) | M(DC_GPIO_DDC1_MASK, DC_GPIO_DDC1DATA_PD_EN));

	/* Set up: software asks for the engine, its speed, the line. */
	update(this, dcn302_DC_I2C_ARBITRATION, M(DC_I2C_ARBITRATION, DC_I2C_SW_USE_I2C_REG_REQ),
	    M(DC_I2C_ARBITRATION, DC_I2C_SW_USE_I2C_REG_REQ));
	uint32_t base = readReg(dcn302_MICROSECOND_TIME_BASE_DIV);
	uint32_t refKHz = GET(base, MICROSECOND_TIME_BASE_DIV, MICROSECOND_TIME_BASE_DIV) * 1000;
	uint32_t xtalDiv = GET(base, MICROSECOND_TIME_BASE_DIV, XTAL_REF_DIV);
	if (refKHz == 0)
		refKHz = 100000;	/* the crystal: 100 MHz */
	if (xtalDiv == 0)
		xtalDiv = 2;
	update(this, dcn302_DC_I2C_DDC_SPEED[line],
	    M(DC_I2C_DDC1_SPEED, DC_I2C_DDC1_PRESCALE) | M(DC_I2C_DDC1_SPEED, DC_I2C_DDC1_THRESHOLD) |
	    M(DC_I2C_DDC1_SPEED, DC_I2C_DDC1_START_STOP_TIMING_CNTL),
	    PUT(DC_I2C_DDC1_SPEED, DC_I2C_DDC1_PRESCALE, refKHz / xtalDiv / kI2CSpeedKHz) |
	    PUT(DC_I2C_DDC1_SPEED, DC_I2C_DDC1_THRESHOLD, 2) |
	    PUT(DC_I2C_DDC1_SPEED, DC_I2C_DDC1_START_STOP_TIMING_CNTL, kI2CSpeedKHz > 50 ? 2 : 1));
	update(this, dcn302_DC_I2C_CONTROL,
	    M(DC_I2C_CONTROL, DC_I2C_GO) | M(DC_I2C_CONTROL, DC_I2C_SOFT_RESET) |
	    M(DC_I2C_CONTROL, DC_I2C_SEND_RESET) | M(DC_I2C_CONTROL, DC_I2C_SW_STATUS_RESET) |
	    M(DC_I2C_CONTROL, DC_I2C_TRANSACTION_COUNT) | M(DC_I2C_CONTROL, DC_I2C_DDC_SELECT),
	    M(DC_I2C_CONTROL, DC_I2C_SW_STATUS_RESET) | PUT(DC_I2C_CONTROL, DC_I2C_DDC_SELECT, line));
	update(this, dcn302_DC_I2C_DDC_SETUP[line],
	    M(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_TIME_LIMIT) | M(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_SEND_RESET_LENGTH) |
	    M(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_ENABLE),
	    PUT(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_TIME_LIMIT, kI2CTimeLimit) |
	    PUT(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_SEND_RESET_LENGTH, kI2CResetLength) |
	    M(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_ENABLE));
	update(this, dcn302_DC_I2C_ARBITRATION, M(DC_I2C_ARBITRATION, DC_I2C_NO_QUEUED_SW_GO), 0);

	/* The transactions, and the bytes they send into the engine's
	 * buffer: each one's address first. */
	if (wlen != 0) {
		update(this, transaction[count], txMask,
		    M(DC_I2C_TRANSACTION0, DC_I2C_STOP_ON_NACK0) | M(DC_I2C_TRANSACTION0, DC_I2C_START0) |
		    PUT(DC_I2C_TRANSACTION0, DC_I2C_COUNT0, wlen) |
		    (rlen == 0 ? M(DC_I2C_TRANSACTION0, DC_I2C_STOP0) : 0));
		writeReg(dcn302_DC_I2C_DATA, PUT(DC_I2C_DATA, DC_I2C_DATA, slave << 1) |
		    M(DC_I2C_DATA, DC_I2C_INDEX_WRITE));
		for (unsigned i = 0; i < wlen; i++)
			writeReg(dcn302_DC_I2C_DATA, PUT(DC_I2C_DATA, DC_I2C_DATA, w[i]));
		index += 1 + wlen;
		count++;
	}
	if (rlen != 0) {
		update(this, transaction[count], txMask,
		    M(DC_I2C_TRANSACTION0, DC_I2C_STOP_ON_NACK0) | M(DC_I2C_TRANSACTION0, DC_I2C_START0) |
		    M(DC_I2C_TRANSACTION0, DC_I2C_RW0) | PUT(DC_I2C_TRANSACTION0, DC_I2C_COUNT0, rlen) |
		    M(DC_I2C_TRANSACTION0, DC_I2C_STOP0));
		writeReg(dcn302_DC_I2C_DATA, PUT(DC_I2C_DATA, DC_I2C_DATA, (slave << 1) | 1) |
		    (count == 0 ? M(DC_I2C_DATA, DC_I2C_INDEX_WRITE) : 0));
		index += 1;
		count++;
	}

	/* Run them. */
	update(this, dcn302_DC_I2C_CONTROL, M(DC_I2C_CONTROL, DC_I2C_SW_STATUS_RESET),
	    M(DC_I2C_CONTROL, DC_I2C_SW_STATUS_RESET));
	update(this, dcn302_DC_I2C_DDC_SETUP[line],
	    M(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_DATA_DRIVE_EN) | M(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_CLK_DRIVE_EN) |
	    M(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_DATA_DRIVE_SEL) |
	    M(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_INTRA_TRANSACTION_DELAY) |
	    M(DC_I2C_DDC1_SETUP, DC_I2C_DDC1_INTRA_BYTE_DELAY), 0);
	update(this, dcn302_DC_I2C_CONTROL,
	    M(DC_I2C_CONTROL, DC_I2C_SOFT_RESET) | M(DC_I2C_CONTROL, DC_I2C_SW_STATUS_RESET) |
	    M(DC_I2C_CONTROL, DC_I2C_SEND_RESET) | M(DC_I2C_CONTROL, DC_I2C_GO) |
	    M(DC_I2C_CONTROL, DC_I2C_TRANSACTION_COUNT),
	    PUT(DC_I2C_CONTROL, DC_I2C_TRANSACTION_COUNT, count - 1));
	update(this, dcn302_DC_I2C_CONTROL, M(DC_I2C_CONTROL, DC_I2C_GO), M(DC_I2C_CONTROL, DC_I2C_GO));

	/* Done once software no longer holds it: about 10 bit times a byte
	 * at 100 kHz, waited for up to half a second. */
	uint64_t start = nowNs();
	uint32_t status;
	for (;;) {
		status = readReg(dcn302_DC_I2C_SW_STATUS);
		if (GET(status, DC_I2C_SW_STATUS, DC_I2C_SW_STATUS) != kI2CUsedBySW)
			break;
		if (nowNs() - start > 500000000ULL) {
			err = ETIMEDOUT;
			break;
		}
		IODelay(20);
	}
	if (err == 0) {
		if (status & M(DC_I2C_SW_STATUS, DC_I2C_SW_STOPPED_ON_NACK))
			err = ENXIO;
		else if (status & M(DC_I2C_SW_STATUS, DC_I2C_SW_TIMEOUT))
			err = ETIMEDOUT;
		else if (status & M(DC_I2C_SW_STATUS, DC_I2C_SW_ABORTED))
			err = EIO;
	}
	if (err == 0 && rlen != 0) {
		writeReg(dcn302_DC_I2C_DATA, PUT(DC_I2C_DATA, DC_I2C_INDEX, index) |
		    M(DC_I2C_DATA, DC_I2C_DATA_RW) | M(DC_I2C_DATA, DC_I2C_INDEX_WRITE));
		for (unsigned i = 0; i < rlen; i++)
			r[i] = (uint8_t)GET(readReg(dcn302_DC_I2C_DATA), DC_I2C_DATA, DC_I2C_DATA);
	}

	/* Release: reset what software left, give the engine back, the pads
	 * as they were. The engine stays enabled, as amdgpu keeps it. */
	if (GET(readReg(dcn302_DC_I2C_SW_STATUS), DC_I2C_SW_STATUS, DC_I2C_SW_STATUS) == kI2CUsedBySW)
		update(this, dcn302_DC_I2C_CONTROL,
		    M(DC_I2C_CONTROL, DC_I2C_SOFT_RESET) | M(DC_I2C_CONTROL, DC_I2C_SW_STATUS_RESET),
		    M(DC_I2C_CONTROL, DC_I2C_SOFT_RESET) | M(DC_I2C_CONTROL, DC_I2C_SW_STATUS_RESET));
	else
		update(this, dcn302_DC_I2C_CONTROL, M(DC_I2C_CONTROL, DC_I2C_SW_STATUS_RESET),
		    M(DC_I2C_CONTROL, DC_I2C_SW_STATUS_RESET));
	update(this, dcn302_DC_I2C_ARBITRATION,
	    M(DC_I2C_ARBITRATION, DC_I2C_SW_DONE_USING_I2C_REG) | M(DC_I2C_ARBITRATION, DC_I2C_SW_USE_I2C_REG_REQ),
	    M(DC_I2C_ARBITRATION, DC_I2C_SW_DONE_USING_I2C_REG));
	writeReg(dcn302_DC_GPIO_DDC_MASK[line], pads);
	return err;
}

/* The EDID on one DDC line: its base block and its first extension. */
bool
MinkarAMDFramebuffer::readEDIDDCN(unsigned line, uint8_t *edid, unsigned *bytes)
{
	*bytes = 0;
	for (unsigned block = 0; block < 2; block++) {
		uint8_t offset = (uint8_t)(block * 128);
		if (i2cDCN(line, kEDIDAddress, &offset, 1, edid + block * 128, 128) != 0)
			break;
		*bytes = (block + 1) * 128;
		if (block == 0 && edid[126] == 0)
			break;
	}
	return *bytes >= 128 && edid[0] == 0x00 && edid[1] == 0xff && edid[7] == 0x00;
}

/*
 * The display's EDID: the DDC lines tried in turn, those in AUX mode (a
 * DisplayPort sink's, R2 later) left alone. The line that answers is the
 * display's.
 */
void
MinkarAMDFramebuffer::readSinkDCN()
{
	uint8_t edid[256];
	unsigned bytes;

	fDDC = -1;
	fEDIDBytes = 0;
	for (unsigned line = 0; line < DCN302_DDCS; line++) {
		if (GET(readReg(dcn302_DC_GPIO_DDC_MASK[line]), DC_GPIO_DDC1_MASK, AUX_PAD1_MODE))
			continue;
		if (!readEDIDDCN(line, edid, &bytes))
			continue;
		fDDC = (int)line;
		bcopy(edid, fEDID, bytes);
		fEDIDBytes = bytes;
		break;
	}
	if (fDDC < 0) {
		LOG("no EDID on the DDC lines");
		return;
	}
	setProperty("MinkarEDID", fEDID, fEDIDBytes);
	setProperty("MinkarDisplayDDCLine", (unsigned long long)fDDC + 1, 32);
	LOG("EDID on DDC%d: %u bytes, vendor %c%c%c, product 0x%04x", fDDC + 1, fEDIDBytes,
	    '@' + ((fEDID[8] >> 2) & 0x1f), '@' + (((fEDID[8] & 3) << 3) | (fEDID[9] >> 5)),
	    '@' + (fEDID[9] & 0x1f), fEDID[10] | (fEDID[11] << 8));
}

/* ---- The output ------------------------------------------------------ */

/*
 * The output that carries the pipe: the stream encoder (DIG front end)
 * fed by its timing generator, the link encoder (back end) fed by that,
 * and the VBIOS's display path on the DDC line that answered, which names
 * the transmitter (PHY) and the connector. Published as MinkarDisplayOutput.
 */
void
MinkarAMDFramebuffer::describeOutputDCN()
{
	struct minkar_atom_fw_path paths[8];
	int n = fAtom != NULL ? minkar_atom_fw_paths(fAtom, paths, 8) : -1;

	fDIGFE = fDIGBE = fPHY = -1;
	fDIGMode = fHPDSelect = 0;
	fConnectorID = 0;
	for (unsigned d = 0; d < DCN302_DIGS; d++) {
		uint32_t fe = readReg(dcn302_DIG_DIG_FE_CNTL[d]);
		if (GET(fe, DIG_DIG_FE_CNTL, DIG_SYMCLK_FE_ON) &&
		    GET(fe, DIG_DIG_FE_CNTL, DIG_SOURCE_SELECT) == fCRTC) {
			fDIGFE = (int)d;
			break;
		}
	}
	for (unsigned d = 0; fDIGFE >= 0 && d < DCN302_DIGS; d++) {
		uint32_t be = readReg(dcn302_DIG_DIG_BE_CNTL[d]);
		if (GET(be, DIG_DIG_BE_CNTL, DIG_FE_SOURCE_SELECT) & (1U << fDIGFE)) {
			fDIGBE = (int)d;
			fDIGMode = GET(be, DIG_DIG_BE_CNTL, DIG_MODE);
			fHPDSelect = GET(be, DIG_DIG_BE_CNTL, DIG_HPD_SELECT);
			break;
		}
	}

	OSArray *list = OSArray::withCapacity(n > 0 ? (unsigned)n : 1);
	for (int i = 0; i < n; i++) {
		const struct minkar_atom_fw_path &p = paths[i];
		OSDictionary *d = OSDictionary::withCapacity(6);
		if (d == NULL)
			continue;
		const struct { const char *key; unsigned value; } v[] = {
			{ "Connector", p.connector }, { "Encoder", p.encoder },
			{ "DeviceTag", p.device_tag }, { "I2CID", p.i2c_id }, { "HPDPin", p.hpd_pin },
		};
		for (unsigned k = 0; k < sizeof(v) / sizeof(v[0]); k++) {
			OSNumber *num = OSNumber::withNumber(v[k].value, 32);
			if (num != NULL) {
				d->setObject(v[k].key, num);
				num->release();
			}
		}
		if (list != NULL)
			list->setObject(d);
		d->release();
		LOG("VBIOS path %d: connector 0x%04x, encoder 0x%04x, device 0x%04x, I2C 0x%02x, HPD pin %u",
		    i, p.connector, p.encoder, p.device_tag, p.i2c_id, p.hpd_pin);
		/* The display's: its DDC is the line that answered (the I2C id's
		 * low bits, when the hardware engine serves it). */
		if (fDDC >= 0 && (p.i2c_id & 0x80) && (p.i2c_id & 0x0F) == (unsigned)fDDC &&
		    ((p.encoder >> 12) & 0xF) == 2 /* GRAPH_OBJECT_TYPE_ENCODER */) {
			unsigned id = p.encoder & 0xFF, link = (p.encoder >> 8) & 0x7;
			int base = id == 0x1E ? 0 : id == 0x20 ? 2 : id == 0x21 ? 4 : -1;
			if (base >= 0 && (link == 1 || link == 2)) {
				fPHY = base + (int)link - 1;
				fConnectorID = (uint8_t)(p.connector & 0xFF);
			}
		}
	}
	if (list != NULL) {
		setProperty("MinkarVBIOSDisplayPaths", list);
		list->release();
	}
	/* The display's command tables the VBIOS has, by name: their content
	 * revision (absent: the display microcontroller runs it, or none). */
	static const struct { const char *name; int index; } tables[] = {
		{ "digxencodercontrol", 4 }, { "setpixelclock", 12 }, { "enabledisppowergating", 13 },
		{ "blankcrtc", 34 }, { "enablecrtc", 35 }, { "selectcrtc_source", 42 },
		{ "setdceclock", 46 }, { "setcrtc_usingdtdtiming", 49 },
		{ "processi2cchanneltransaction", 54 }, { "dig1transmittercontrol", 76 },
		{ "processauxchanneltransaction", 78 },
	};
	OSDictionary *td = OSDictionary::withCapacity(sizeof(tables) / sizeof(tables[0]));
	for (unsigned k = 0; td != NULL && fAtom != NULL && k < sizeof(tables) / sizeof(tables[0]); k++) {
		uint8_t frev, crev;
		if (!minkar_atom_cmd_header(fAtom, tables[k].index, &frev, &crev))
			continue;
		OSNumber *num = OSNumber::withNumber((unsigned long long)(frev << 8 | crev), 32);
		if (num != NULL) {
			td->setObject(tables[k].name, num);
			num->release();
		}
	}
	if (td != NULL) {
		setProperty("MinkarVBIOSCommandTables", td);
		td->release();
	}
	/* The display microcontroller, as the firmware left it: whether it
	 * runs (its timer moves) and its mailbox. */
	static const struct { const char *name; uint32_t reg; } dmcub[] = {
		{ "CNTL", dcn302_DMCUB_CNTL }, { "SEC_CNTL", dcn302_DMCUB_SEC_CNTL },
		{ "MEM_CNTL", dcn302_DMCUB_MEM_CNTL }, { "SCRATCH0", dcn302_DMCUB_SCRATCH0 },
		{ "SCRATCH7", dcn302_DMCUB_SCRATCH7 }, { "TIMER_CURRENT", dcn302_DMCUB_TIMER_CURRENT },
		{ "INBOX1_BASE_ADDRESS", dcn302_DMCUB_INBOX1_BASE_ADDRESS },
		{ "INBOX1_SIZE", dcn302_DMCUB_INBOX1_SIZE }, { "INBOX1_RPTR", dcn302_DMCUB_INBOX1_RPTR },
		{ "INBOX1_WPTR", dcn302_DMCUB_INBOX1_WPTR }, { "OUTBOX1_RPTR", dcn302_DMCUB_OUTBOX1_RPTR },
		{ "OUTBOX1_WPTR", dcn302_DMCUB_OUTBOX1_WPTR },
		{ "REGION3_CW0_BASE_ADDRESS", dcn302_DMCUB_REGION3_CW0_BASE_ADDRESS },
		{ "INST_FETCH_FAULT_ADDR", dcn302_DMCUB_INST_FETCH_FAULT_ADDR },
		{ "UNDEFINED_ADDRESS_FAULT_ADDR", dcn302_DMCUB_UNDEFINED_ADDRESS_FAULT_ADDR },
	};
	OSDictionary *md = OSDictionary::withCapacity(sizeof(dmcub) / sizeof(dmcub[0]) + 1);
	uint32_t t0 = readReg(dcn302_DMCUB_TIMER_CURRENT);
	IODelay(100);
	uint32_t t1 = readReg(dcn302_DMCUB_TIMER_CURRENT);
	for (unsigned k = 0; md != NULL && k < sizeof(dmcub) / sizeof(dmcub[0]); k++) {
		OSNumber *num = OSNumber::withNumber(readReg(dmcub[k].reg), 32);
		if (num != NULL) {
			md->setObject(dmcub[k].name, num);
			num->release();
		}
	}
	if (md != NULL) {
		md->setObject("Running", t1 != t0 ? kOSBooleanTrue : kOSBooleanFalse);
		setProperty("MinkarDMCUB", md);
		md->release();
	}
	LOG("display microcontroller: CNTL 0x%08x, SCRATCH0 0x%08x, timer %s", readReg(dcn302_DMCUB_CNTL),
	    readReg(dcn302_DMCUB_SCRATCH0), t1 != t0 ? "running" : "stopped");
	OSDictionary *d = OSDictionary::withCapacity(6);
	if (d != NULL) {
		const struct { const char *key; int value; } v[] = {
			{ "DIGFrontEnd", fDIGFE }, { "DIGBackEnd", fDIGBE }, { "DIGMode", (int)fDIGMode },
			{ "HPDSelect", (int)fHPDSelect }, { "PHY", fPHY }, { "ConnectorID", fConnectorID },
		};
		for (unsigned k = 0; k < sizeof(v) / sizeof(v[0]); k++) {
			OSNumber *num = OSNumber::withNumber((unsigned long long)(long long)v[k].value, 32);
			if (num != NULL) {
				d->setObject(v[k].key, num);
				num->release();
			}
		}
		setProperty("MinkarDisplayOutput", d);
		d->release();
	}
	LOG("output: DIG front end %d, back end %d (mode %u, HPD %u), PHY %d, connector 0x%02x",
	    fDIGFE, fDIGBE, fDIGMode, fHPDSelect, fPHY, fConnectorID);
}

/* The pixel clock of the mode on screen, from the EDID's detailed timing
 * of the timing generator's totals; 0 if none matches. kHz. */
uint32_t
MinkarAMDFramebuffer::pixelClockDCN()
{
	uint32_t htotal = (readReg(dcn302_OTG_OTG_H_TOTAL[fCRTC]) & DCN302_OTG_OTG_H_TOTAL__OTG_H_TOTAL_MASK) + 1;
	uint32_t vtotal = (readReg(dcn302_OTG_OTG_V_TOTAL[fCRTC]) & DCN302_OTG_OTG_V_TOTAL__OTG_V_TOTAL_MASK) + 1;
	struct edid_mode modes[32];
	unsigned n = fEDIDBytes >= 128 ? edid_modes(fEDID, fEDIDBytes, modes, 32) : 0;

	for (unsigned i = 0; i < n; i++) {
		const struct edid_mode &m = modes[i];
		if (m.hactive == fModes[0].width && m.vactive == fModes[0].height &&
		    (uint32_t)(m.hactive + m.hfront + m.hsync + m.hback) == htotal &&
		    (uint32_t)(m.vactive + m.vfront + m.vsync + m.vback) == vtotal)
			return m.clock;
	}
	return 0;
}

/*
 * The display's power: its transmitter turned off (an HDMI or DVI sink
 * sees no signal and sleeps) or on again at the mode's pixel clock,
 * through the VBIOS's DIG1TransmitterControl. DisplayPort waits for its
 * link training.
 */
IOReturn
MinkarAMDFramebuffer::setDisplayPowerDCN(bool on)
{
	if (fPHY < 0 || fDIGFE < 0)
		return kIOReturnUnsupported;
	if (fDIGMode != 2 /* DIG_MODE: HDMI */ && fDIGMode != 1 /* DVI */)
		return kIOReturnUnsupported;
	if (on == fDisplayOn)
		return kIOReturnSuccess;
	uint32_t clock = pixelClockDCN();
	if (on && clock == 0)
		return kIOReturnNotReady;
	struct minkar_atom_fw_transmitter_args t;
	bzero(&t, sizeof(t));
	t.phy = (uint8_t)fPHY;
	t.action = on ? 1 : 0;		/* ATOM_TRANSMITTER_ACTION_ENABLE, DISABLE */
	t.digmode = fDIGMode == 2 ? 3 : 2;	/* ATOM_ENCODER_MODE_HDMI, DVI */
	t.lanes = 4;
	t.symclk_10khz = clock / 10;
	t.hpdsel = (uint8_t)(fHPDSelect + 1);
	t.digfe_sel = (uint8_t)(1U << fDIGFE);
	t.connobj_id = fConnectorID;
	/* The VBIOS's table if it has one; else the microcontroller, which
	 * runs it for the VBIOS on DCN 3. */
	int rc = minkar_atom_fw_transmitter(fAtom, &t);
	if (rc == -1)
		rc = dmcubTransmitter(&t) ? 0 : -3;
	LOG("display %s: transmitter %d, %u kHz: %d", on ? "on" : "off", fPHY, clock, rc);
	if (rc != 0)
		return kIOReturnIOError;
	fDisplayOn = on;
	setProperty("MinkarDisplayPowered", on);
	return kIOReturnSuccess;
}
