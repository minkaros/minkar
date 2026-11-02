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
#include "atom_glue.h"

struct minkar_atom {
	struct card_info card;		/* first: the interpreter's callbacks get it */
	void *gpu;
	minkar_reg_read_t read;
	minkar_reg_write_t write;
	struct atom_context *ctx;
};

static void reg_write(struct card_info *c, u32 reg, uint32_t val)
{
	struct minkar_atom *a = (struct minkar_atom *)c;
	a->write(a->gpu, reg, val);
}

static uint32_t reg_read(struct card_info *c, uint32_t reg)
{
	struct minkar_atom *a = (struct minkar_atom *)c;
	return a->read(a->gpu, reg);
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

void
minkar_atom_close(struct minkar_atom *a)
{
	if (a == NULL)
		return;
	mutex_destroy(&a->ctx->mutex);
	amdgpu_atom_destroy(a->ctx);
	kfree(a);
}
