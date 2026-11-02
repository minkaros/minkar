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
 * The GART of GCN 1.0 GPUs (GMC 6): system memory in the GPU's address
 * space through VM context 0's one-level page table, and VRAM kept out of
 * the translation by the system aperture. The registers and values follow
 * amdgpu's gmc_v6_0.c (gmc_v6_0_mc_program, gmc_v6_0_gart_enable) and the
 * GMC 6 register headers (MIT). Plain C, for the driver and host tests.
 */

#ifndef _MINKAR_AMD_SI_GART_H
#define _MINKAR_AMD_SI_GART_H

#include <stdint.h>

/* Register dword indices (gmc_6_0_d.h). */
#define mmSI_MC_VM_FB_LOCATION			0x0809
#define mmSI_MC_VM_SYSTEM_APERTURE_LOW_ADDR	0x080d
#define mmSI_MC_VM_SYSTEM_APERTURE_HIGH_ADDR	0x080e
#define mmSI_MC_VM_SYSTEM_APERTURE_DEFAULT_ADDR	0x080f
#define mmSI_MC_VM_MX_L1_TLB_CNTL		0x0819
#define mmSI_VM_L2_CNTL				0x0500
#define mmSI_VM_L2_CNTL2			0x0501
#define mmSI_VM_L2_CNTL3			0x0502
#define mmSI_VM_CONTEXT0_CNTL			0x0504
#define mmSI_VM_CONTEXT0_CNTL2			0x050c
#define mmSI_VM_INVALIDATE_REQUEST		0x051e
#define mmSI_VM_CONTEXT0_PROTECTION_FAULT_DEFAULT_ADDR 0x0546
#define mmSI_VM_CONTEXT0_PAGE_TABLE_BASE_ADDR	0x054f
#define mmSI_VM_CONTEXT0_PAGE_TABLE_START_ADDR	0x0557
#define mmSI_VM_CONTEXT0_PAGE_TABLE_END_ADDR	0x055f

#define kSIGPUPage		4096ull
/* amdgpu's fragment size for GMC 6 (amdgpu_vm_adjust_size(adev, 64, 9, ...)). */
#define kSIFragmentSize		9u

/* Page table entries (amdgpu_vm.h): system pages, snooped by the CPU's
 * caches, readable and writable; the address as amdgpu_gmc_set_pte_pde
 * masks it. */
#define SI_PTE_VALID		(1ull << 0)
#define SI_PTE_SYSTEM		(1ull << 1)
#define SI_PTE_SNOOPED		(1ull << 2)
#define SI_PTE_READABLE		(1ull << 5)
#define SI_PTE_WRITEABLE	(1ull << 6)

static inline uint64_t
si_gart_pte(uint64_t physical)
{
	return (physical & 0x0000fffffffff000ull) | SI_PTE_VALID |
	    SI_PTE_SYSTEM | SI_PTE_SNOOPED | SI_PTE_READABLE | SI_PTE_WRITEABLE;
}

/* VRAM in the GPU's space, from MC_VM_FB_LOCATION (16 MB units). */
static inline uint64_t
si_vram_start(uint32_t fbLocation)
{
	return (uint64_t)(fbLocation & 0xffff) << 24;
}

static inline uint64_t
si_vram_end(uint32_t fbLocation)
{
	return ((uint64_t)(fbLocation >> 16) << 24) | 0xffffff;
}

/* The values gmc_v6_0_gart_enable writes. */
static inline uint32_t
si_l1_tlb_cntl(void)
{
	return (0xAu << 7) | 0x1u /* ENABLE_L1_TLB */ | 0x2u /* L1_FRAGMENT_PROCESSING */ |
	    0x18u /* SYSTEM_ACCESS_MODE */ | 0x40u /* ADVANCED_DRIVER_MODEL */;
}

static inline uint32_t
si_l2_cntl(void)
{
	return 0x1u /* ENABLE_L2_CACHE */ | 0x2u /* L2_FRAGMENT_PROCESSING */ |
	    0x200u /* PTE_CACHE_LRU_UPDATE_BY_WRITE */ | 0x400u /* PDE0_... */ |
	    (7u << 15) /* EFFECTIVE_L2_QUEUE_SIZE */ | (1u << 19) /* CONTEXT1_IDENTITY_ACCESS_MODE */;
}

static inline uint32_t
si_l2_cntl2(void)
{
	return 0x1u /* INVALIDATE_ALL_L1_TLBS */ | 0x2u /* INVALIDATE_L2_CACHE */;
}

static inline uint32_t
si_l2_cntl3(void)
{
	return 0x100000u /* L2_CACHE_BIGK_ASSOCIATIVITY */ | (kSIFragmentSize << 0) |
	    (kSIFragmentSize << 15);
}

/* Context 0 on, a one-level table, range faults to the default page. */
static inline uint32_t
si_context0_cntl(void)
{
	return 0x1u /* ENABLE_CONTEXT */ | (0u << 1) /* PAGE_TABLE_DEPTH */ |
	    0x10u /* RANGE_PROTECTION_FAULT_ENABLE_DEFAULT */;
}

/* The GART right after VRAM, size bytes (a multiple of 4 MB): its first
 * and last addresses in the GPU's space. */
static inline void
si_gart_place(uint32_t fbLocation, uint64_t size, uint64_t *start, uint64_t *end)
{
	*start = (si_vram_end(fbLocation) + 1 + (4ull << 20) - 1) & ~((4ull << 20) - 1);
	*end = *start + size - 1;
}

#endif
