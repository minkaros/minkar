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
 * The VBIOS's tables of the newer format (atomfirmware.h, MIT; Navi 23's):
 * the display paths of the object table, each with its connector, its
 * encoder and the DDC and hot-plug records, and the DIG transmitter
 * control table (v1.6), as amdgpu's bios_parser2.c and command_table2.c
 * use them.
 */

#include "amdgpu.h"
#include "atomfirmware.h"
#include "atom_glue.h"

#define DATA_TABLE(name) \
	(int)(offsetof(struct atom_master_list_of_data_tables_v2_1, name) / sizeof(uint16_t))
#define CMD_TABLE(name) \
	(int)(offsetof(struct atom_master_list_of_command_functions_v2_1, name) / sizeof(uint16_t))

/* The records after offset (from the object table's start): the DDC's and
 * the hot-plug pin's. */
static void
records(const uint8_t *bios, uint32_t size, uint32_t at, struct minkar_atom_fw_path *p)
{
	for (int i = 0; i < 32 && at + sizeof(struct atom_common_record_header) <= size; i++) {
		const struct atom_common_record_header *h =
		    (const struct atom_common_record_header *)(bios + at);
		if (h->record_type == ATOM_RECORD_END_TYPE || h->record_size == 0 ||
		    at + h->record_size > size)
			break;
		if (h->record_type == ATOM_I2C_RECORD_TYPE &&
		    h->record_size >= sizeof(struct atom_i2c_record))
			p->i2c_id = ((const struct atom_i2c_record *)h)->i2c_id;
		if (h->record_type == ATOM_HPD_INT_RECORD_TYPE &&
		    h->record_size >= sizeof(struct atom_hpd_int_record))
			p->hpd_pin = ((const struct atom_hpd_int_record *)h)->pin_id;
		at += h->record_size;
	}
}

int
minkar_atom_fw_paths(struct minkar_atom *a, struct minkar_atom_fw_path *out, int max)
{
	uint16_t size, offset;
	uint8_t frev, crev;
	int n = 0;

	if (!minkar_atom_data_header(a, DATA_TABLE(displayobjectinfo), &size, &frev, &crev, &offset))
		return -1;
	const uint8_t *bios = minkar_atom_bios(a);
	uint32_t limit = (uint32_t)offset + size + 4096;	/* records may follow the table */
	if (frev != 1 || (crev != 4 && crev != 5))
		return -2;
	const struct display_object_info_table_v1_4 *t =
	    (const struct display_object_info_table_v1_4 *)(bios + offset);
	for (int i = 0; i < t->number_of_path && n < max; i++) {
		struct minkar_atom_fw_path *p = &out[n];
		uint16_t display, encoder, tag, recordOffset;
		memset(p, 0, sizeof(*p));
		if (crev == 4) {
			const struct atom_display_object_path_v2 *o = &t->display_path[i];
			display = o->display_objid;
			encoder = o->encoderobjid;
			tag = o->device_tag;
			recordOffset = o->disp_recordoffset;
		} else {
			const struct atom_display_object_path_v3 *o =
			    &((const struct display_object_info_table_v1_5 *)t)->display_path[i];
			display = o->display_objid;
			encoder = o->encoderobjid;
			tag = o->device_tag;
			recordOffset = o->disp_recordoffset;
		}
		p->connector = le16_to_cpu(display);
		p->encoder = le16_to_cpu(encoder);
		p->device_tag = le16_to_cpu(tag);
		p->i2c_id = 0;
		p->hpd_pin = 0xff;
		records(bios, limit, (uint32_t)offset + le16_to_cpu(recordOffset), p);
		n++;
	}
	return n;
}

int
minkar_atom_fw_transmitter(struct minkar_atom *a, const struct minkar_atom_fw_transmitter_args *t)
{
	union {
		struct dig_transmitter_control_ps_allocation_v1_6 ps;
		uint32_t dwords[(sizeof(struct dig_transmitter_control_ps_allocation_v1_6) + 3) / 4];
	} u;
	uint8_t frev, crev;

	if (!minkar_atom_cmd_header(a, CMD_TABLE(dig1transmittercontrol), &frev, &crev))
		return -1;
	if (crev != 6)
		return -2;
	memset(&u, 0, sizeof(u));
	u.ps.param.phyid = t->phy;
	u.ps.param.action = t->action;
	u.ps.param.mode_laneset.digmode = t->digmode;
	u.ps.param.lanenum = t->lanes;
	u.ps.param.symclk_10khz = cpu_to_le32(t->symclk_10khz);
	u.ps.param.hpdsel = t->hpdsel;
	u.ps.param.digfe_sel = t->digfe_sel;
	u.ps.param.connobj_id = t->connobj_id;
	return minkar_atom_execute(a, CMD_TABLE(dig1transmittercontrol), u.dwords, sizeof(u.ps));
}
