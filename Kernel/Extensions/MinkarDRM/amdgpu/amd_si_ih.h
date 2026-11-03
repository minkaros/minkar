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
 * The interrupt handler ring (IH) of GCN 1.0 GPUs: the GPU writes one
 * 16-byte entry per interrupt to a ring in memory, moves its write
 * pointer (written back to memory too) and raises its interrupt; the
 * driver reads the entries and moves the read pointer. Registers, bits
 * and the entry's layout after amdgpu's sid.h and si_ih.c (MIT). Plain C,
 * for the driver and host tests.
 */

#ifndef _MINKAR_AMD_SI_IH_H
#define _MINKAR_AMD_SI_IH_H

#include <stdint.h>

#define mmSI_IH_RB_CNTL			0x0f80
#define   SI_IH_RB_ENABLE		(1u << 0)
#define   SI_IH_WPTR_WRITEBACK_ENABLE	(1u << 8)
#define   SI_IH_WPTR_OVERFLOW_ENABLE	(1u << 16)
#define   SI_IH_WPTR_OVERFLOW_CLEAR	(1u << 31)
#define mmSI_IH_RB_BASE			0x0f81	/* address >> 8 */
#define mmSI_IH_RB_RPTR			0x0f82	/* in bytes */
#define mmSI_IH_RB_WPTR			0x0f83
#define   SI_IH_RB_OVERFLOW		(1u << 0)
#define mmSI_IH_RB_WPTR_ADDR_HI		0x0f84
#define mmSI_IH_RB_WPTR_ADDR_LO		0x0f85
#define mmSI_IH_CNTL			0x0f86
#define   SI_IH_ENABLE_INTR		(1u << 0)
#define   SI_IH_RPTR_REARM		(1u << 4)
#define mmSI_INTERRUPT_CNTL		0x151a
#define   SI_IH_DUMMY_RD_OVERRIDE	(1u << 0)
#define   SI_IH_REQ_NONSNOOP_EN		(1u << 3)
#define mmSI_INTERRUPT_CNTL2		0x151b	/* dummy read address >> 8 */

/* DMA_CNTL's trap enable, and the DMA engines' trap sources. */
#define   SI_DMA_TRAP_ENABLE		(1u << 0)
#define kSISourceDMA0Trap		224
#define kSISourceDMA1Trap		244

#define kSIIHEntryBytes			16

/* si_ih_irq_init's IH_RB_CNTL: overflow handling, size, write-back. */
static inline uint32_t
si_ih_rb_cntl(uint32_t ringBytes)
{
	uint32_t order = 0;

	while ((1u << order) < ringBytes / 4)
		order++;
	return SI_IH_WPTR_OVERFLOW_ENABLE | SI_IH_WPTR_OVERFLOW_CLEAR | (order << 1) |
	    SI_IH_WPTR_WRITEBACK_ENABLE;
}

/* MC_WRREQ_CREDIT(0x10) | MC_WR_CLEAN_CNT(0x10) | MC_VMID(0), and the
 * read pointer re-arming the MSI. */
static inline uint32_t
si_ih_cntl(int msi)
{
	return (0x10u << 15) | (0x10u << 20) | (0u << 25) | (msi ? SI_IH_RPTR_REARM : 0);
}

struct si_ih_entry {
	uint32_t source;	/* src_id */
	uint32_t data;		/* src_data[0] */
	uint32_t ring;		/* ring_id */
	uint32_t vmid;
};

/* si_ih_decode_iv. */
static inline void
si_ih_decode(const volatile uint32_t *dw, struct si_ih_entry *e)
{
	e->source = dw[0] & 0xff;
	e->data = dw[1] & 0xfffffff;
	e->ring = dw[2] & 0xff;
	e->vmid = (dw[2] >> 8) & 0xff;
}

/*
 * The entries between rptr and the written-back write pointer, in a ring
 * of ringBytes (a power of two). Calls each(entry, info) for every one
 * and returns the new read pointer; *overflowed if the ring overflowed
 * (si_ih_get_wptr: reading starts again after the newest entry).
 */
static inline uint32_t
si_ih_drain(const volatile uint32_t *ring, uint32_t ringBytes, uint32_t rptr, uint32_t wptr,
    int *overflowed, void (*each)(const struct si_ih_entry *, void *), void *info)
{
	uint32_t mask = ringBytes - 1;
	struct si_ih_entry e;

	*overflowed = (wptr & SI_IH_RB_OVERFLOW) != 0;
	wptr &= ~SI_IH_RB_OVERFLOW;
	wptr &= mask;
	if (*overflowed)
		rptr = (wptr + kSIIHEntryBytes) & mask;
	while (rptr != wptr) {
		si_ih_decode(ring + rptr / 4, &e);
		each(&e, info);
		rptr = (rptr + kSIIHEntryBytes) & mask;
	}
	return rptr;
}

#endif
