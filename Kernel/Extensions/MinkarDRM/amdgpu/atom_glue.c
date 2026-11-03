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

#include "amdgpu.h"
#include "atom.h"
#include "atombios.h"
#include "ObjectID.h"
#include "atom_glue.h"

struct minkar_atom {
	struct card_info card;		/* first: the interpreter's callbacks get it */
	int auxExecute;			/* the last AUX transaction's, for bring-up */
	uint8_t auxDataOut;
	int tracing;
	unsigned traced;
	uint32_t trace[2 * MINKAR_ATOM_TRACE_MAX];
	void *gpu;
	minkar_reg_read_t read;
	minkar_reg_write_t write;
	struct atom_context *ctx;
};

static void note(struct minkar_atom *a, uint32_t reg, uint32_t val, int write)
{
	if (a->tracing && a->traced < MINKAR_ATOM_TRACE_MAX) {
		a->trace[2 * a->traced] = reg | (write ? 1u << 31 : 0);
		a->trace[2 * a->traced + 1] = val;
		a->traced++;
	}
}

static void reg_write(struct card_info *c, u32 reg, uint32_t val)
{
	struct minkar_atom *a = (struct minkar_atom *)c;
	note(a, reg, val, 1);
	a->write(a->gpu, reg, val);
}

static uint32_t reg_read(struct card_info *c, uint32_t reg)
{
	struct minkar_atom *a = (struct minkar_atom *)c;
	uint32_t v = a->read(a->gpu, reg);
	note(a, reg, v, 0);
	return v;
}

/* Memory controller and PLL access: not used by the tables run so far. */
static void nop_write(struct card_info *c, u32 reg, uint32_t val) { (void)c; (void)reg; (void)val; }
static uint32_t nop_read(struct card_info *c, uint32_t reg) { (void)c; (void)reg; return 0; }

static void copy_str(char *to, size_t size, const char *from)
{
	size_t i;
	for (i = 0; i + 1 < size && from[i] != 0; i++)
		to[i] = from[i];
	to[i] = 0;
}

struct minkar_atom *
minkar_atom_open(void *gpu, minkar_reg_read_t read, minkar_reg_write_t write, void *bios,
    struct minkar_vbios_info *info)
{
	struct minkar_atom *a = kzalloc(sizeof(*a), GFP_KERNEL);

	if (a == NULL)
		return NULL;
	a->gpu = gpu;
	a->read = read;
	a->write = write;
	a->card.reg_read = reg_read;
	a->card.reg_write = reg_write;
	a->card.mc_read = a->card.pll_read = nop_read;
	a->card.mc_write = a->card.pll_write = nop_write;
	a->ctx = amdgpu_atom_parse(&a->card, bios);
	if (a->ctx == NULL) {
		kfree(a);
		return NULL;
	}
	mutex_init(&a->ctx->mutex);
	/* The tables' scratch space (amdgpu_atombios_allocate_fb_scratch):
	 * what VRAM_UsageByFirmware asks for, else 20 KB. The AUX table
	 * passes its data through it. */
	{
		uint16_t data_offset;
		int usage = 0;
		if (amdgpu_atom_parse_data_header(a->ctx,
		    (int)(offsetof(ATOM_MASTER_LIST_OF_DATA_TABLES, VRAM_UsageByFirmware) / sizeof(uint16_t)),
		    NULL, NULL, NULL, &data_offset)) {
			const ATOM_VRAM_USAGE_BY_FIRMWARE *u =
			    (const ATOM_VRAM_USAGE_BY_FIRMWARE *)((const uint8_t *)a->ctx->bios + data_offset);
			uint32_t start = le32_to_cpu(u->asFirmwareVramReserveInfo[0].ulStartAddrUsedByFirmware);
			if ((start & ATOM_VRAM_OPERATION_FLAGS_MASK) !=
			    (ATOM_VRAM_BLOCK_SRIOV_MSG_SHARE_RESERVATION << ATOM_VRAM_OPERATION_FLAGS_SHIFT))
				usage = le16_to_cpu(u->asFirmwareVramReserveInfo[0].usFirmwareUseInKb) * 1024;
		}
		if (usage == 0)
			usage = 20 * 1024;
		a->ctx->scratch = kzalloc(usage, GFP_KERNEL);
		a->ctx->scratch_size_bytes = a->ctx->scratch != NULL ? usage : 0;
	}
	if (info != NULL) {
		copy_str(info->part_number, sizeof(info->part_number), (const char *)a->ctx->vbios_pn);
		copy_str(info->version, sizeof(info->version), (const char *)a->ctx->vbios_ver_str);
		copy_str(info->date, sizeof(info->date), (const char *)a->ctx->date);
		copy_str(info->name, sizeof(info->name), (const char *)a->ctx->name);
	}
	return a;
}

int
minkar_atom_execute(struct minkar_atom *a, int index, uint32_t *params, int size)
{
	return amdgpu_atom_execute_table(a->ctx, index, params, size);
}

#define TABLE(name) \
	(int)(offsetof(ATOM_MASTER_LIST_OF_COMMAND_TABLES, name) / sizeof(uint16_t))

int
minkar_atom_has_table(struct minkar_atom *a, enum minkar_atom_table t)
{
	uint8_t frev, crev;
	int index;

	switch (t) {
	case MINKAR_ATOM_ENABLE_SCALER: index = TABLE(EnableScaler); break;
	case MINKAR_ATOM_SET_CRTC_OVERSCAN: index = TABLE(SetCRTC_OverScan); break;
	case MINKAR_ATOM_UPDATE_CRTC_DOUBLE_BUFFER: index = TABLE(UpdateCRTC_DoubleBufferRegisters); break;
	default: return 0;
	}
	return amdgpu_atom_parse_cmd_header(a->ctx, index, &frev, &crev);
}

/* The order of amdgpu's atombios_crtc.c (MIT): the same tables, the same
 * parameters. */
int
minkar_atom_crtc_lock(struct minkar_atom *a, int crtc, int lock)
{
	union { ENABLE_CRTC_PS_ALLOCATION args; uint32_t dwords[(sizeof(ENABLE_CRTC_PS_ALLOCATION) + 3) / 4]; } u;

	memset(&u, 0, sizeof(u));
	u.args.ucCRTC = (uint8_t)crtc;
	u.args.ucEnable = (uint8_t)lock;
	return amdgpu_atom_execute_table(a->ctx, TABLE(UpdateCRTC_DoubleBufferRegisters),
	    u.dwords, sizeof(u.args));
}

int
minkar_atom_overscan(struct minkar_atom *a, int crtc, int left, int right, int top, int bottom)
{
	union { SET_CRTC_OVERSCAN_PS_ALLOCATION args; uint32_t dwords[(sizeof(SET_CRTC_OVERSCAN_PS_ALLOCATION) + 3) / 4]; } u;

	memset(&u, 0, sizeof(u));
	u.args.ucCRTC = (uint8_t)crtc;
	u.args.usOverscanLeft = cpu_to_le16((uint16_t)left);
	u.args.usOverscanRight = cpu_to_le16((uint16_t)right);
	u.args.usOverscanTop = cpu_to_le16((uint16_t)top);
	u.args.usOverscanBottom = cpu_to_le16((uint16_t)bottom);
	return amdgpu_atom_execute_table(a->ctx, TABLE(SetCRTC_OverScan),
	    u.dwords, sizeof(u.args));
}

int
minkar_atom_scaler(struct minkar_atom *a, int crtc, int expand)
{
	union { ENABLE_SCALER_PS_ALLOCATION args; uint32_t dwords[(sizeof(ENABLE_SCALER_PS_ALLOCATION) + 3) / 4]; } u;

	memset(&u, 0, sizeof(u));
	u.args.ucScaler = (uint8_t)crtc;
	u.args.ucEnable = expand ? ATOM_SCALER_EXPANSION : ATOM_SCALER_DISABLE;
	return amdgpu_atom_execute_table(a->ctx, TABLE(EnableScaler),
	    u.dwords, sizeof(u.args));
}

int
minkar_atom_aux(struct minkar_atom *a, uint8_t channel, uint8_t hpd, uint8_t request,
    uint32_t address, uint8_t *buffer, uint8_t size, uint8_t *reply)
{
	union {
		PROCESS_AUX_CHANNEL_TRANSACTION_PARAMETERS_V2 args;
		uint32_t dwords[(sizeof(PROCESS_AUX_CHANNEL_TRANSACTION_PARAMETERS_V2) + 3) / 4];
	} u;
	uint8_t tx[20];
	uint8_t send, *base;
	int write = (request & ~0x4) == 0x8 || (request & ~0x4) == 0x0;
	int n;

	if (size > 16 || a->ctx->scratch == NULL)
		return -EINVAL;
	/* The request's header: address, request, size - 1; bare when 0. */
	tx[0] = address & 0xff;
	tx[1] = (address >> 8) & 0xff;
	tx[2] = (uint8_t)((request << 4) | ((address >> 16) & 0xf));
	tx[3] = size ? size - 1 : 0;
	send = write ? 4 + size : 4;
	tx[3] |= (size == 0 ? 3 : send) << 4;
	if (write)
		memcpy(tx + 4, buffer, size);

	mutex_lock(&a->ctx->mutex);
	base = (uint8_t *)(a->ctx->scratch + 1);
	memcpy(base, tx, send);
	memset(&u, 0, sizeof(u));
	u.args.lpAuxRequest = cpu_to_le16(0 + 4);
	u.args.lpDataOut = cpu_to_le16(16 + 4);
	u.args.ucChannelID = channel;
	u.args.ucDelay = 0;
	u.args.ucHPD_ID = hpd;
	mutex_unlock(&a->ctx->mutex);
	a->auxExecute = amdgpu_atom_execute_table(a->ctx, TABLE(ProcessAuxChannelTransaction),
	    u.dwords, sizeof(u.args));
	a->auxDataOut = u.args.ucDataOutLen;
	*reply = u.args.ucReplyStatus;
	if (u.args.ucReplyStatus == 1)
		return -ETIMEDOUT;
	if (u.args.ucReplyStatus == 2 || u.args.ucReplyStatus == 3)
		return -EIO;
	if (write)
		n = size;
	else {
		n = u.args.ucDataOutLen;
		if (n > size)
			n = size;
		memcpy(buffer, base + 16, n);
	}
	*reply = u.args.ucReplyStatus;
	return n;
}

int
minkar_atom_i2c(struct minkar_atom *a, uint8_t line, uint8_t slave, int write, uint8_t reg,
    const uint8_t *more, uint8_t moreBytes, uint8_t *buffer, uint8_t readBytes)
{
	union {
		PROCESS_I2C_CHANNEL_TRANSACTION_PARAMETERS args;
		uint32_t dwords[(sizeof(PROCESS_I2C_CHANNEL_TRANSACTION_PARAMETERS) + 3) / 4];
	} u;
	uint16_t out = 0;

	if (a->ctx->scratch == NULL || (write && moreBytes > 2) ||
	    (!write && (readBytes == 0 || (int)readBytes > a->ctx->scratch_size_bytes)))
		return -EINVAL;
	memset(&u, 0, sizeof(u));
	if (write) {
		u.args.ucRegIndex = reg;
		if (moreBytes)
			memcpy(&out, more, moreBytes);
		u.args.lpI2CDataOut = cpu_to_le16(out);
		u.args.ucTransBytes = moreBytes;
		u.args.ucFlag = HW_I2C_WRITE;
	} else {
		/* A read starts at reg: the table writes it first. */
		u.args.ucRegIndex = reg;
		u.args.ucTransBytes = readBytes;
		u.args.ucFlag = HW_I2C_READ;
	}
	u.args.ucI2CSpeed = 50;		/* kHz, amdgpu's TARGET_HW_I2C_CLOCK */
	u.args.ucSlaveAddr = (uint8_t)(slave << 1);
	u.args.ucLineNumber = line;
	amdgpu_atom_execute_table(a->ctx, TABLE(ProcessI2cChannelTransaction), u.dwords, sizeof(u.args));
	if (u.args.ucStatus != HW_ASSISTED_I2C_STATUS_SUCCESS)
		return -EIO;
	if (!write)
		memcpy(buffer, a->ctx->scratch, readBytes);
	return 0;
}

void
minkar_atom_aux_last(struct minkar_atom *a, int *execute, uint8_t *dataOut)
{
	*execute = a->auxExecute;
	*dataOut = a->auxDataOut;
}

#define ARGS(type) union { type args; uint32_t dwords[(sizeof(type) + 3) / 4]; } u; \
	memset(&u, 0, sizeof(u))
#define RUN(table) amdgpu_atom_execute_table(a->ctx, TABLE(table), u.dwords, sizeof(u.args))

int
minkar_atom_dig_for(const struct minkar_atom_output *o)
{
	switch (o->encoderID) {
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:		return o->linkB ? 1 : 0;
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY1:	return o->linkB ? 3 : 2;
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY2:	return o->linkB ? 5 : 4;
	default:					return -1;
	}
}

int
minkar_atom_crtc_blank(struct minkar_atom *a, int crtc, int blank)
{
	ARGS(BLANK_CRTC_PARAMETERS);
	u.args.ucCRTC = (uint8_t)crtc;
	u.args.ucBlanking = blank ? ATOM_BLANKING : ATOM_BLANKING_OFF;
	return RUN(BlankCRTC);
}

int
minkar_atom_crtc_enable(struct minkar_atom *a, int crtc, int enable)
{
	ARGS(ENABLE_CRTC_PARAMETERS);
	u.args.ucCRTC = (uint8_t)crtc;
	u.args.ucEnable = enable ? ATOM_ENABLE : ATOM_DISABLE;
	return RUN(EnableCRTC);
}

/* amdgpu_atombios_crtc_program_pll, crev 6, without spread spectrum. */
int
minkar_atom_set_pixel_clock(struct minkar_atom *a, const struct minkar_atom_output *o,
    const struct minkar_atom_timing *t)
{
	ARGS(PIXEL_CLOCK_PARAMETERS_V6);
	u.args.ulDispEngClkFreq = cpu_to_le32((uint32_t)o->crtc << 24 | t->clock / 10);
	u.args.ucRefDiv = (uint8_t)t->refDiv;
	u.args.usFbDiv = cpu_to_le16(t->fbDiv);
	u.args.ulFbDivDecFrac = cpu_to_le32((uint32_t)t->fbFrac * 100000);
	u.args.ucPostDiv = (uint8_t)t->postDiv;
	u.args.ucMiscInfo = o->hdmi ? PIXEL_CLOCK_V6_MISC_HDMI_24BPP : 0;
	u.args.ucTransmitterID = o->encoderID;
	u.args.ucEncoderMode = o->hdmi ? ATOM_ENCODER_MODE_HDMI : ATOM_ENCODER_MODE_DVI;
	u.args.ucPpll = o->pll;
	return RUN(SetPixelClock);
}

/* amdgpu_atombios_crtc_set_dtd_timing, no borders. */
int
minkar_atom_set_timing(struct minkar_atom *a, int crtc, const struct minkar_atom_timing *t)
{
	ARGS(SET_CRTC_USING_DTD_TIMING_PARAMETERS);
	uint16_t misc = 0;
	u.args.usH_Size = cpu_to_le16(t->hactive);
	u.args.usH_Blanking_Time = cpu_to_le16(t->hfront + t->hsync + t->hback);
	u.args.usV_Size = cpu_to_le16(t->vactive);
	u.args.usV_Blanking_Time = cpu_to_le16(t->vfront + t->vsync + t->vback);
	u.args.usH_SyncOffset = cpu_to_le16(t->hfront);
	u.args.usH_SyncWidth = cpu_to_le16(t->hsync);
	u.args.usV_SyncOffset = cpu_to_le16(t->vfront);
	u.args.usV_SyncWidth = cpu_to_le16(t->vsync);
	if (!t->vpositive)
		misc |= ATOM_VSYNC_POLARITY;
	if (!t->hpositive)
		misc |= ATOM_HSYNC_POLARITY;
	u.args.susModeMiscInfo.usAccess = cpu_to_le16(misc);
	u.args.ucCRTC = (uint8_t)crtc;
	return RUN(SetCRTC_UsingDTDTiming);
}

/* amdgpu_atombios_encoder_set_crtc_source, crev 2. */
int
minkar_atom_select_crtc_source(struct minkar_atom *a, const struct minkar_atom_output *o)
{
	static const uint8_t digIDs[6] = {
		ASIC_INT_DIG1_ENCODER_ID, ASIC_INT_DIG2_ENCODER_ID, ASIC_INT_DIG3_ENCODER_ID,
		ASIC_INT_DIG4_ENCODER_ID, ASIC_INT_DIG5_ENCODER_ID, ASIC_INT_DIG6_ENCODER_ID,
	};
	int dig = minkar_atom_dig_for(o);
	ARGS(SELECT_CRTC_SOURCE_PARAMETERS_V2);
	if (dig < 0)
		return -EINVAL;
	u.args.ucCRTC = o->crtc;
	u.args.ucEncoderID = digIDs[dig];
	u.args.ucEncodeMode = o->hdmi ? ATOM_ENCODER_MODE_HDMI : ATOM_ENCODER_MODE_DVI;
	return RUN(SelectCRTC_Source);
}

/* amdgpu_atombios_encoder_setup_dig_encoder, crev 4, SETUP, a single-link
 * TMDS sink, 8 bits per colour. */
int
minkar_atom_dig_setup(struct minkar_atom *a, const struct minkar_atom_output *o, uint32_t clock)
{
	int dig = minkar_atom_dig_for(o);
	ARGS(DIG_ENCODER_CONTROL_PARAMETERS_V4);
	if (dig < 0)
		return -EINVAL;
	u.args.ucAction = ATOM_ENCODER_CMD_SETUP;
	u.args.usPixelClock = cpu_to_le16(clock / 10);
	u.args.ucEncoderMode = o->hdmi ? ATOM_ENCODER_MODE_HDMI : ATOM_ENCODER_MODE_DVI;
	u.args.ucLaneNum = 4;
	u.args.acConfig.ucDigSel = (uint8_t)dig;
	u.args.ucBitPerColor = PANEL_8BIT_PER_COLOR;
	u.args.ucHPD_ID = (uint8_t)(o->hpd + 1);
	return RUN(DIGxEncoderControl);
}

/* amdgpu_atombios_encoder_setup_dig_transmitter, crev 5, TMDS, coherent. */
int
minkar_atom_transmitter(struct minkar_atom *a, const struct minkar_atom_output *o, int action,
    uint32_t clock)
{
	int dig = minkar_atom_dig_for(o);
	ARGS(DIG_TRANSMITTER_CONTROL_PARAMETERS_V1_5);
	if (dig < 0)
		return -EINVAL;
	u.args.ucAction = (uint8_t)action;
	u.args.usSymClock = cpu_to_le16(clock / 10);
	u.args.ucPhyId = (uint8_t)dig;	/* UNIPHYA-F follow the DIGs: 2 * UNIPHYn + link */
	u.args.ucLaneNum = 4;
	u.args.ucConnObjId = o->connectorID;
	u.args.ucDigMode = o->hdmi ? ATOM_ENCODER_MODE_HDMI : ATOM_ENCODER_MODE_DVI;
	u.args.asConfig.ucPhyClkSrcId = o->pll;
	u.args.asConfig.ucCoherentMode = 1;
	u.args.asConfig.ucHPDSel = (uint8_t)(o->hpd + 1);
	u.args.ucDigEncoderSel = (uint8_t)(1u << dig);
	return RUN(UNIPHYTransmitterControl);
}

void
minkar_atom_trace(struct minkar_atom *a, int on)
{
	a->tracing = on;
	if (on)
		a->traced = 0;
}

unsigned
minkar_atom_trace_get(struct minkar_atom *a, uint32_t *entries, unsigned max)
{
	unsigned n = a->traced < max ? a->traced : max;
	memcpy(entries, a->trace, (size_t)n * 2 * sizeof(uint32_t));
	return n;
}

void
minkar_atom_close(struct minkar_atom *a)
{
	if (a == NULL)
		return;
	mutex_destroy(&a->ctx->mutex);
	kfree(a->ctx->scratch);
	amdgpu_atom_destroy(a->ctx);
	kfree(a);
}
