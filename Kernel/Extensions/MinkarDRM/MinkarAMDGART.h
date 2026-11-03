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
 * MinkarAMDGART (ADR 0002, G2): a GCN 1.0 GPU's GART, system memory pages
 * at GPU addresses right after VRAM, through VM context 0's page table in
 * VRAM. Turned on as gmc_v6_0_gart_enable does, with the system aperture
 * over VRAM so that VRAM (the display's scanout too) stays untranslated;
 * turned off by putting back the registers it changed.
 */

#ifndef _MINKARAMDGART_H
#define _MINKARAMDGART_H

#include <IOKit/IOBufferMemoryDescriptor.h>
#include "MinkarAMDDevice.h"

struct MinkarAMDGART {
	struct MinkarAMDRegs *	regs;
	uint64_t		start, end;	/* in the GPU's space */
	volatile uint64_t *	table;		/* one entry per 4 KB page */
	IOMemoryMap *		tableMap;
	uint64_t		tableOffset;	/* VRAM offsets, from the allocator */
	uint64_t		scratchOffset;
	uint32_t		entries;
	uint32_t		saved[16];	/* the registers as they were */
	bool			on;
};

/*
 * Turns the GART on: size bytes (a multiple of 4 MB) after VRAM; the
 * table and a scratch page at tableOffset and scratchOffset in VRAM (the
 * caller's, from its allocator: the table inside the CPU's window). All
 * entries start invalid.
 */
bool MinkarAMDGARTEnable(struct MinkarAMDGART *gart, struct MinkarAMDRegs *regs,
    IODeviceMemory *window, uint64_t size, uint64_t tableOffset, uint64_t scratchOffset);
void MinkarAMDGARTDisable(struct MinkarAMDGART *gart);
/* The table's size for size bytes of GART. */
static inline uint64_t MinkarAMDGARTTableBytes(uint64_t size) { return size / 4096 * 8; }

/* Points pages [first, first + count) at the memory's pages, from its
 * start; its GPU address in *gpuAddress. The memory must stay wired. */
bool MinkarAMDGARTMap(struct MinkarAMDGART *gart, uint32_t first, IOMemoryDescriptor *memory,
    uint64_t *gpuAddress);
void MinkarAMDGARTUnmap(struct MinkarAMDGART *gart, uint32_t first, uint32_t count);

#endif
