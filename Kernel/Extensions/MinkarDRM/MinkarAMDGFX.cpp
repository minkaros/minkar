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
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <kern/clock.h>
#include "MinkarAMDGFX.h"
#include "amd_si_gfx.h"
#include "amd_si_dma.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDGFX: " fmt "\n", ##__VA_ARGS__)

#define kTimeoutNS	(100ULL * 1000 * 1000)

static inline uint32_t rd(struct MinkarAMDRegs *r, uint32_t reg) { return MinkarAMDRead(r, reg); }
static inline void wr(struct MinkarAMDRegs *r, uint32_t reg, uint32_t v) { MinkarAMDWrite(r, reg, v); }

static uint64_t
nowNS(void)
{
	uint64_t ns;
	absolutetime_to_nanoseconds(mach_absolute_time(), &ns);
	return ns;
}

static void
setNumber(OSDictionary *d, const char *key, uint64_t value, unsigned bits)
{
	if (d == NULL)
		return;
	OSNumber *n = OSNumber::withNumber((unsigned long long)value, bits);
	if (n != NULL) {
		d->setObject(key, n);
		n->release();
	}
}

/* The processor's state, for the results. */
static void
noteState(struct MinkarAMDRegs *r, OSDictionary *d, const char *prefix)
{
	char key[48];
	static const struct { const char *name; uint32_t reg; } regs[] = {
		{ "GRBM_STATUS", mmSI_GRBM_STATUS }, { "CP_STAT", mmSI_CP_STAT },
		{ "CP_ME_CNTL", mmSI_CP_ME_CNTL }, { "CP_RB0_RPTR", mmSI_CP_RB0_RPTR },
		{ "CP_RB0_WPTR", mmSI_CP_RB0_WPTR },
	};
	for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
		snprintf(key, sizeof(key), "%s%s", prefix, regs[i].name);
		setNumber(d, key, rd(r, regs[i].reg), 32);
	}
}

static void
enable(struct MinkarAMDRegs *r, bool on)
{
	if (on)
		wr(r, mmSI_CP_ME_CNTL, 0);
	else {
		wr(r, mmSI_CP_ME_CNTL, SI_CP_ME_HALT | SI_CP_PFP_HALT | SI_CP_CE_HALT);
		wr(r, mmSI_SCRATCH_UMSK, 0);
	}
	IODelay(50);
}

void
MinkarAMDGFXLoadMicrocode(struct MinkarAMDRegs *r, const struct amd_ucode *pfp,
    const struct amd_ucode *me, const struct amd_ucode *ce)
{
	enable(r, false);
	wr(r, mmSI_CP_PFP_UCODE_ADDR, 0);
	for (uint32_t i = 0; i < pfp->words; i++)
		wr(r, mmSI_CP_PFP_UCODE_DATA, amd_ucode_word(pfp, i));
	wr(r, mmSI_CP_PFP_UCODE_ADDR, 0);
	wr(r, mmSI_CP_CE_UCODE_ADDR, 0);
	for (uint32_t i = 0; i < ce->words; i++)
		wr(r, mmSI_CP_CE_UCODE_DATA, amd_ucode_word(ce, i));
	wr(r, mmSI_CP_CE_UCODE_ADDR, 0);
	wr(r, mmSI_CP_ME_RAM_WADDR, 0);
	for (uint32_t i = 0; i < me->words; i++)
		wr(r, mmSI_CP_ME_RAM_DATA, amd_ucode_word(me, i));
	wr(r, mmSI_CP_ME_RAM_WADDR, 0);
	wr(r, mmSI_CP_PFP_UCODE_ADDR, 0);
	wr(r, mmSI_CP_CE_UCODE_ADDR, 0);
	wr(r, mmSI_CP_ME_RAM_WADDR, 0);
	wr(r, mmSI_CP_ME_RAM_RADDR, 0);
}

/* Writes reach VRAM, then the processor is told how far to read. */
static void
commit(struct MinkarAMDGFX *g)
{
	OSSynchronizeIO();
	wr(g->regs, mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL, 1);
	(void)rd(g->regs, mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL);
	wr(g->regs, mmSI_CP_RB0_WPTR, g->wptr);
	(void)rd(g->regs, mmSI_CP_RB0_WPTR);
}

static void
emit(struct MinkarAMDGFX *g, const uint32_t *p, unsigned n)
{
	for (unsigned i = 0; i < n; i++) {
		g->ring[g->wptr] = p[i];
		g->wptr = (g->wptr + 1) & (g->ringDwords - 1);
	}
}

/* Until the processor has read all of the ring. */
static bool
drained(struct MinkarAMDGFX *g)
{
	uint64_t end = nowNS() + kTimeoutNS;
	while (nowNS() < end) {
		if (rd(g->regs, mmSI_CP_RB0_RPTR) == g->wptr)
			return true;
		IODelay(10);
	}
	return false;
}

bool
MinkarAMDGFXStart(struct MinkarAMDGFX *g, struct MinkarAMDRegs *r, uint64_t gpuBase,
    volatile uint8_t *cpuBase, OSDictionary *results)
{
	uint32_t p[16];

	g->regs = r;
	g->gpuBase = gpuBase;
	g->ring = (volatile uint32_t *)(uintptr_t)cpuBase;	/* page aligned */
	g->ringDwords = kMinkarAMDGFXRingBytes / 4;
	g->wptr = 0;
	for (uint32_t i = 0; i < g->ringDwords; i++)
		g->ring[i] = 0x80000000;	/* type 2 packets: padding */
	noteState(r, results, "Before");

	wr(r, mmSI_CP_SEM_WAIT_TIMER, 0);
	wr(r, mmSI_CP_SEM_INCOMPLETE_TIMER_CNTL, 0);
	wr(r, mmSI_CP_RB_WPTR_DELAY, 0);
	wr(r, mmSI_CP_DEBUG, 0);
	wr(r, mmSI_SCRATCH_ADDR, 0);

	/* Ring 0: its size, both pointers at 0, no read pointer write-back. */
	uint32_t cntl = amd_si_gfx_ring_cntl(kMinkarAMDGFXRingBytes);
	wr(r, mmSI_CP_RB0_CNTL, cntl | SI_CP_RB_RPTR_WR_ENA);
	wr(r, mmSI_CP_RB0_WPTR, 0);
	wr(r, mmSI_CP_RB0_RPTR_ADDR, 0);
	wr(r, mmSI_CP_RB0_RPTR_ADDR_HI, 0);
	wr(r, mmSI_SCRATCH_UMSK, 0);
	IODelay(1000);
	wr(r, mmSI_CP_RB0_CNTL, cntl);
	wr(r, mmSI_CP_RB0_BASE, (uint32_t)(gpuBase >> 8));

	/* The micro engine's start, then the processor on. */
	emit(g, p, amd_si_gfx_emit_start(p));
	commit(g);
	enable(r, true);
	bool ok = drained(g);
	noteState(r, results, "Started");
	if (!ok)
		LOG("the command processor did not take its start");
	return ok;
}

bool
MinkarAMDGFXRingTest(struct MinkarAMDGFX *g, uint64_t *ns, OSDictionary *results)
{
	uint32_t p[4];

	wr(g->regs, mmSI_SCRATCH_REG0, 0xcafedead);
	uint64_t start = nowNS();
	emit(g, p, amd_si_gfx_emit_set_config(p, mmSI_SCRATCH_REG0, 0xdeadbeef));
	commit(g);
	uint32_t value = 0;
	while (nowNS() - start < kTimeoutNS) {
		value = rd(g->regs, mmSI_SCRATCH_REG0);
		if (value == 0xdeadbeef)
			break;
		IODelay(1);
	}
	*ns = nowNS() - start;
	setNumber(results, "ScratchValue", value, 32);
	noteState(g->regs, results, "Tested");
	if (value != 0xdeadbeef) {
		LOG("ring test: scratch 0x%08x after %llu ns (CP_STAT 0x%08x, GRBM_STATUS 0x%08x, "
		    "rptr %u, wptr %u)", value, (unsigned long long)*ns, rd(g->regs, mmSI_CP_STAT),
		    rd(g->regs, mmSI_GRBM_STATUS), rd(g->regs, mmSI_CP_RB0_RPTR), g->wptr);
		return false;
	}
	return true;
}

void
MinkarAMDGFXStop(struct MinkarAMDGFX *g)
{
	enable(g->regs, false);
}
