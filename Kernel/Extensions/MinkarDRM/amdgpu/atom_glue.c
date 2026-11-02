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

void
minkar_atom_close(struct minkar_atom *a)
{
	if (a == NULL)
		return;
	mutex_destroy(&a->ctx->mutex);
	amdgpu_atom_destroy(a->ctx);
	kfree(a);
}
