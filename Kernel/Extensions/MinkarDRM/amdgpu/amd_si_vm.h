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
 * GPU virtual memory for clients on GCN 1.0 GPUs (SI): VM contexts 1 to 15,
 * each with a two-level page table (a page directory of PDEs, each over a
 * page table of 512 PTEs, 2 MB), as amdgpu's gmc_v6_0.c and amdgpu_vm.c
 * (MIT) set them up: 64 GB of address space a context, VRAM pages by their
 * offset in VRAM, the tables by their address in the GPU's space. Plain C,
 * for the driver and for host tests.
 */

#ifndef _MINKAR_AMD_SI_VM_H
#define _MINKAR_AMD_SI_VM_H

#include <stdint.h>
#include "amd_si_gart.h"

#define mmSI_VM_CONTEXT1_CNTL			0x0505
#define mmSI_VM_CONTEXT1_CNTL2			0x050d
#define mmSI_VM_CONTEXT8_PAGE_TABLE_BASE_ADDR	0x050e	/* contexts 8-15 */
#define mmSI_VM_CONTEXT1_PROTECTION_FAULT_STATUS 0x0537
#define mmSI_VM_CONTEXT1_PROTECTION_FAULT_ADDR	0x053f
#define mmSI_VM_CONTEXT1_PROTECTION_FAULT_DEFAULT_ADDR 0x0547
#define mmSI_VM_CONTEXT1_PAGE_TABLE_START_ADDR	0x0558
#define mmSI_VM_CONTEXT1_PAGE_TABLE_END_ADDR	0x0560

#define kSIVMContexts		16		/* 0 is the kernel's (the GART) */
#define kSIVMBytes		(64ull << 30)
#define kSIVMPages		(kSIVMBytes / kSIGPUPage)
#define kSIVMPTEntries		512u		/* a page table: 2 MB, 4 KB of PTEs */
#define kSIVMPDEntries		((uint32_t)(kSIVMPages / kSIVMPTEntries))
#define kSIVMPDBytes		((uint64_t)kSIVMPDEntries * 8)	/* 256 KB */
#define kSIVMPTBytes		((uint64_t)kSIVMPTEntries * 8)	/* 4 KB */

/* The register that holds a context's page directory (its address >> 12). */
static inline uint32_t
si_vm_base_reg(unsigned vmid)
{
	return vmid < 8 ? mmSI_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR + vmid :
	    mmSI_VM_CONTEXT8_PAGE_TABLE_BASE_ADDR + (vmid - 8);
}

/* Contexts 1-15 on: two levels, 512-entry tables, every fault (range,
 * dummy page, PDE, valid, read, write) to the default page. */
static inline uint32_t
si_vm_context1_cntl(void)
{
	return 0x1u /* ENABLE_CONTEXT */ | (1u << 1) /* PAGE_TABLE_DEPTH */ |
	    (0u << 24) /* PAGE_TABLE_BLOCK_SIZE: 9 bits */ |
	    0x10u | 0x80u | 0x400u | 0x2000u | 0x10000u | 0x80000u;
}

/* A PDE: a page table's address in the GPU's space (below 1 TB, 4 KB
 * aligned), valid. */
static inline uint64_t
si_vm_pde(uint64_t table)
{
	return (table & 0x000000fffffff000ull) | SI_PTE_VALID;
}

/* The page table a PDE points at, or 0. */
static inline uint64_t
si_vm_pde_table(uint64_t pde)
{
	return (pde & SI_PTE_VALID) ? (pde & 0x000000fffffff000ull) : 0;
}

/* A PTE for a page of VRAM, by its offset in VRAM. */
static inline uint64_t
si_vm_pte_vram(uint64_t offset)
{
	return (offset & 0x000000fffffff000ull) | SI_PTE_VALID | SI_PTE_READABLE |
	    SI_PTE_WRITEABLE;
}

/* A PTE for a page of system memory, by its physical address. */
static inline uint64_t
si_vm_pte_system(uint64_t physical)
{
	return si_gart_pte(physical);
}

/* Where a virtual address's PTE is: its PDE and its PTE in that table;
 * 0, or -1 outside the address space or not page aligned. */
static inline int
si_vm_index(uint64_t va, uint32_t *pde, uint32_t *pte)
{
	if (va >= kSIVMBytes || (va & (kSIGPUPage - 1)) != 0)
		return -1;
	uint64_t page = va / kSIGPUPage;
	*pde = (uint32_t)(page / kSIVMPTEntries);
	*pte = (uint32_t)(page % kSIVMPTEntries);
	return 0;
}

/* A range of the address space, page aligned and inside it. */
static inline int
si_vm_range_ok(uint64_t va, uint64_t bytes)
{
	return bytes != 0 && (va & (kSIGPUPage - 1)) == 0 && (bytes & (kSIGPUPage - 1)) == 0 &&
	    va < kSIVMBytes && bytes <= kSIVMBytes - va;
}

/* The context of a protection fault, and whether there is one. */
static inline unsigned
si_vm_fault_vmid(uint32_t status)
{
	return (status >> 25) & 0xf;
}

#endif
