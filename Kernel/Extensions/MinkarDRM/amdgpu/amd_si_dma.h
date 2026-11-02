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
 * The DMA engines of GCN 1.0 GPUs (SI, "async DMA"): their registers and
 * the packets of their rings, after the layout amdgpu's sid.h and
 * si_dma.c (MIT) describe. Plain C, for the driver and for host tests.
 */

#ifndef _MINKAR_AMD_SI_DMA_H
#define _MINKAR_AMD_SI_DMA_H

#include <stdint.h>

/* Register dword indices; DMA1's are DMA0's plus kSIDMA1Offset. */
#define kSIDMA1Offset			0x200
#define mmSI_DMA_RB_CNTL		0x3400
#define   SI_DMA_RB_ENABLE		(1u << 0)
#define   SI_DMA_RPTR_WRITEBACK_ENABLE	(1u << 12)
#define mmSI_DMA_RB_BASE		0x3401	/* address >> 8 */
#define mmSI_DMA_RB_RPTR		0x3402	/* in bytes */
#define mmSI_DMA_RB_WPTR		0x3403	/* in bytes */
#define mmSI_DMA_RB_RPTR_ADDR_HI	0x3407
#define mmSI_DMA_RB_RPTR_ADDR_LO	0x3408
#define mmSI_DMA_IB_CNTL		0x3409
#define   SI_DMA_IB_ENABLE		(1u << 0)
#define   SI_DMA_CMD_VMID_FORCE		(1u << 31)
#define mmSI_DMA_CNTL			0x340b
#define   SI_DMA_CTXEMPTY_INT_ENABLE	(1u << 28)
#define mmSI_DMA_STATUS_REG		0x340d
#define   SI_DMA_IDLE			(1u << 0)
#define mmSI_DMA_SEM_INCOMPLETE_TIMER_CNTL 0x3411
#define mmSI_DMA_SEM_WAIT_FAIL_TIMER_CNTL 0x3412

/* The host data path: CPU writes through the VRAM window reach VRAM
 * when flushed; the GPU's writes are seen once it is invalidated. */
#define mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL 0x1520
#define mmSI_HDP_DEBUG0			0x0bcc
#define mmSI_SRBM_STATUS2		0x0393

/* Packets. */
enum {
	kSIDMAPacketWrite	= 0x2,
	kSIDMAPacketCopy	= 0x3,
	kSIDMAPacketIndirectBuffer = 0x4,
	kSIDMAPacketFence	= 0x6,
	kSIDMAPacketTrap	= 0x7,
	kSIDMAPacketNop		= 0xf,
};
/* The most one copy packet moves (amdgpu's copy_max_bytes). */
#define kSIDMAMaxCopyBytes	0xffff8u

static inline uint32_t
si_dma_header(uint32_t cmd, uint32_t bytes, uint32_t count)
{
	return ((cmd & 0xf) << 28) | ((bytes & 0x1) << 26) | (count & 0xfffff);
}

/* Each writes its packet at p and returns how many dwords it took. */

/* count dwords of data follow; the caller writes them after. */
static inline unsigned
si_dma_write(uint32_t *p, uint64_t addr, uint32_t count)
{
	p[0] = si_dma_header(kSIDMAPacketWrite, 0, count);
	p[1] = (uint32_t)addr & ~3u;
	p[2] = (uint32_t)(addr >> 32) & 0xff;
	return 3;
}

/* bytes <= kSIDMAMaxCopyBytes, byte-aligned. */
static inline unsigned
si_dma_copy(uint32_t *p, uint64_t dst, uint64_t src, uint32_t bytes)
{
	p[0] = si_dma_header(kSIDMAPacketCopy, 1, bytes);
	p[1] = (uint32_t)dst;
	p[2] = (uint32_t)src;
	p[3] = (uint32_t)(dst >> 32) & 0xff;
	p[4] = (uint32_t)(src >> 32) & 0xff;
	return 5;
}

/* Writes seq at addr once everything before it is done. */
static inline unsigned
si_dma_fence(uint32_t *p, uint64_t addr, uint32_t seq)
{
	p[0] = si_dma_header(kSIDMAPacketFence, 0, 0);
	p[1] = (uint32_t)addr & ~3u;
	p[2] = (uint32_t)(addr >> 32) & 0xff;
	p[3] = seq;
	return 4;
}

/* Runs dwords of commands at addr (32-byte aligned) with the given VM
 * (0: the GPU's own addresses). In the ring, it must end on an 8-dword
 * boundary: the caller pads before it with NOPs (si_dma_ring_emit_ib). */
static inline unsigned
si_dma_indirect(uint32_t *p, uint64_t addr, uint32_t dwords, uint32_t vmid)
{
	p[0] = ((uint32_t)kSIDMAPacketIndirectBuffer << 28) | ((vmid & 0xf) << 20);
	p[1] = (uint32_t)addr & 0xffffffe0u;
	p[2] = (dwords << 12) | ((uint32_t)(addr >> 32) & 0xff);
	return 3;
}

static inline unsigned
si_dma_nop(uint32_t *p)
{
	p[0] = si_dma_header(kSIDMAPacketNop, 0, 0);
	return 1;
}

/* log2 of the ring's size in dwords, as DMA_RB_CNTL takes it. */
static inline uint32_t
si_dma_rb_cntl(uint32_t ringDwords)
{
	uint32_t order = 0;

	while ((1u << order) < ringDwords)
		order++;
	return order << 1;
}

#endif
