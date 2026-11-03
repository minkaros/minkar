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


#include <IOKit/IOLib.h>
#include <libkern/OSAtomic.h>
#include "MinkarAMDGART.h"
#include "amd_si_gart.h"
#include "amd_si_vm.h"
#include "amd_si_dma.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDGART: " fmt "\n", ##__VA_ARGS__)

/* The registers the GART changes, saved in this order. */
static const uint32_t kSaved[] = {
	mmSI_MC_VM_SYSTEM_APERTURE_LOW_ADDR, mmSI_MC_VM_SYSTEM_APERTURE_HIGH_ADDR,
	mmSI_MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR, mmSI_MC_VM_MX_L1_TLB_CNTL,
	mmSI_VM_L2_CNTL, mmSI_VM_L2_CNTL3,
	mmSI_VM_CONTEXT0_PAGE_TABLE_START_ADDR, mmSI_VM_CONTEXT0_PAGE_TABLE_END_ADDR,
	mmSI_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR, mmSI_VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR,
	mmSI_VM_CONTEXT0_CNTL2, mmSI_VM_CONTEXT0_CNTL,
};

static inline uint32_t rd(struct MinkarAMDGART *g, uint32_t r) { return MinkarAMDRead(g->regs, r); }
static inline void wr(struct MinkarAMDGART *g, uint32_t r, uint32_t v) { MinkarAMDWrite(g->regs, r, v); }

/* The table's writes through the window reach VRAM; then the TLBs drop
 * what they had of context 0 (gmc_v6_0_flush_gpu_tlb). */
static void
flush(struct MinkarAMDGART *g)
{
	OSSynchronizeIO();
	wr(g, mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL, 1);
	(void)rd(g, mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL);
	wr(g, mmSI_VM_INVALIDATE_REQUEST, 1u << 0);
}

bool
MinkarAMDGARTEnable(struct MinkarAMDGART *g, struct MinkarAMDRegs *regs, IODeviceMemory *window,
    uint64_t size, uint64_t tableOffset, uint64_t scratchOffset)
{
	bzero(g, sizeof(*g));
	g->regs = regs;
	if (size == 0 || (size & ((4ULL << 20) - 1)) != 0)
		return false;
	uint32_t fb = rd(g, mmSI_MC_VM_FB_LOCATION);
	uint64_t vramStart = si_vram_start(fb), vramEnd = si_vram_end(fb);
	si_gart_place(fb, size, &g->start, &g->end);
	g->entries = (uint32_t)(size / kSIGPUPage);
	g->tableOffset = tableOffset;
	g->scratchOffset = scratchOffset;

	uint64_t tableBytes = MinkarAMDGARTTableBytes(size);
	IODeviceMemory *t = tableOffset + tableBytes <= window->getLength() ?
	    IODeviceMemory::withSubRange(window, tableOffset, tableBytes) : NULL;
	g->tableMap = t != NULL ? t->map(kIOMapWriteCombineCache) : NULL;
	OSSafeReleaseNULL(t);
	if (g->tableMap == NULL)
		return false;
	g->table = (volatile uint64_t *)(uintptr_t)g->tableMap->getVirtualAddress();
	for (uint32_t i = 0; i < g->entries; i++)
		g->table[i] = 0;	/* invalid: a fault, to the scratch page */

	for (unsigned i = 0; i < sizeof(kSaved) / sizeof(kSaved[0]); i++)
		g->saved[i] = rd(g, kSaved[i]);

	/* VRAM outside the translation (gmc_v6_0_mc_program). */
	wr(g, mmSI_MC_VM_SYSTEM_APERTURE_LOW_ADDR, (uint32_t)(vramStart >> 12));
	wr(g, mmSI_MC_VM_SYSTEM_APERTURE_HIGH_ADDR, (uint32_t)(vramEnd >> 12));
	wr(g, mmSI_MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR, (uint32_t)((vramStart + scratchOffset) >> 12));
	/* gmc_v6_0_gart_enable. */
	wr(g, mmSI_MC_VM_MX_L1_TLB_CNTL, si_l1_tlb_cntl());
	wr(g, mmSI_VM_L2_CNTL, si_l2_cntl());
	wr(g, mmSI_VM_L2_CNTL2, si_l2_cntl2());
	wr(g, mmSI_VM_L2_CNTL3, si_l2_cntl3());
	wr(g, mmSI_VM_CONTEXT0_PAGE_TABLE_START_ADDR, (uint32_t)(g->start >> 12));
	wr(g, mmSI_VM_CONTEXT0_PAGE_TABLE_END_ADDR, (uint32_t)(g->end >> 12));
	wr(g, mmSI_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR, (uint32_t)((vramStart + tableOffset) >> 12));
	wr(g, mmSI_VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR,
	    (uint32_t)((vramStart + scratchOffset) >> 12));
	wr(g, mmSI_VM_CONTEXT0_CNTL2, 0);
	wr(g, mmSI_VM_CONTEXT0_CNTL, si_context0_cntl());
	/* Contexts 1-15, for clients' address spaces: their range, every
	 * directory at something valid until a client's takes its place,
	 * faults to the scratch page. */
	wr(g, mmSI_VM_CONTEXT1_PAGE_TABLE_START_ADDR, 0);
	wr(g, mmSI_VM_CONTEXT1_PAGE_TABLE_END_ADDR, (uint32_t)(kSIVMPages - 1));
	for (unsigned vmid = 1; vmid < kSIVMContexts; vmid++)
		wr(g, si_vm_base_reg(vmid), (uint32_t)((vramStart + tableOffset) >> 12));
	wr(g, mmSI_VM_CONTEXT1_PROTECTION_FAULT_DEFAULT_ADDR,
	    (uint32_t)((vramStart + scratchOffset) >> 12));
	wr(g, mmSI_VM_CONTEXT1_CNTL2, 4);
	wr(g, mmSI_VM_CONTEXT1_CNTL, si_vm_context1_cntl());
	flush(g);
	wr(g, mmSI_VM_INVALIDATE_REQUEST, 0xffffu);	/* every context */
	g->on = true;
	LOG("%llu MB at 0x%llx, table at VRAM 0x%llx", (unsigned long long)size >> 20,
	    (unsigned long long)g->start, (unsigned long long)tableOffset);
	return true;
}

void
MinkarAMDGARTDisable(struct MinkarAMDGART *g)
{
	if (!g->on)
		return;
	/* Context 0 off first, then the rest as it was. */
	wr(g, mmSI_VM_CONTEXT1_CNTL, 0);
	wr(g, mmSI_VM_CONTEXT0_CNTL, g->saved[11]);
	for (unsigned i = 0; i < sizeof(kSaved) / sizeof(kSaved[0]); i++)
		wr(g, kSaved[i], g->saved[i]);
	wr(g, mmSI_VM_L2_CNTL2, si_l2_cntl2());
	wr(g, mmSI_VM_INVALIDATE_REQUEST, 1u << 0);
	OSSafeReleaseNULL(g->tableMap);
	g->table = NULL;
	g->on = false;
}

bool
MinkarAMDGARTMap(struct MinkarAMDGART *g, uint32_t first, IOMemoryDescriptor *memory,
    uint64_t *gpuAddress)
{
	IOByteCount length = memory->getLength();
	uint32_t pages = (uint32_t)((length + kSIGPUPage - 1) / kSIGPUPage);

	if (!g->on || first >= g->entries || pages > g->entries - first)
		return false;
	for (uint32_t i = 0; i < pages; i++) {
		IOByteCount segment = 0;
		addr64_t phys = memory->getPhysicalSegment((IOByteCount)i * kSIGPUPage, &segment,
		    kIOMemoryMapperNone);
		if (phys == 0) {
			MinkarAMDGARTUnmap(g, first, i);
			return false;
		}
		g->table[first + i] = si_gart_pte(phys);
	}
	flush(g);
	*gpuAddress = g->start + (uint64_t)first * kSIGPUPage;
	return true;
}

void
MinkarAMDGARTUnmap(struct MinkarAMDGART *g, uint32_t first, uint32_t count)
{
	if (!g->on || first >= g->entries)
		return;
	if (count > g->entries - first)
		count = g->entries - first;
	for (uint32_t i = 0; i < count; i++)
		g->table[first + i] = 0;
	flush(g);
}
