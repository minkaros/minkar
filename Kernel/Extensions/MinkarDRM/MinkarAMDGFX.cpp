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
#include "amd_si_gfx_tables.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDGFX: " fmt "\n", ##__VA_ARGS__)

#define kTimeoutNS	(100ULL * 1000 * 1000)
/* GRBM_GFX_INDEX: every shader engine, array and instance. */
#define mmSI_GRBM_GFX_INDEX_BROADCAST_REG	SI_mmGRBM_GFX_INDEX
#define mmSI_GRBM_GFX_INDEX_BROADCAST		(SI_GRBM_GFX_INDEX__INSTANCE_BROADCAST_WRITES_MASK | \
	SI_GRBM_GFX_INDEX__SH_BROADCAST_WRITES_MASK | SI_GRBM_GFX_INDEX__SE_BROADCAST_WRITES_MASK)

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

#define kWriteBackOffset	kMinkarAMDGFXRingBytes
#define kTestIBOffset		(kWriteBackOffset + 256)
#define kSaveRestoreOffset	(kWriteBackOffset + 4096)
#define kClearStateOffset	(kSaveRestoreOffset + 4096)

/* Room for n more dwords: the processor is waited for when the ring would
 * catch up with its read pointer. */
static bool
room(struct MinkarAMDGFX *g, unsigned n)
{
	uint64_t end = nowNS() + kTimeoutNS;
	for (;;) {
		uint32_t rptr = rd(g->regs, mmSI_CP_RB0_RPTR) & (g->ringDwords - 1);
		uint32_t used = (g->wptr - rptr) & (g->ringDwords - 1);
		if (g->ringDwords - 1 - used >= n)
			return true;
		if (nowNS() >= end)
			return false;
		IODelay(10);
	}
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

/* Until RLC's serial buses are idle. */
static void
rlcSerdesIdle(struct MinkarAMDRegs *r)
{
	for (int i = 0; i < 100000 && rd(r, SI_mmRLC_SERDES_MASTER_BUSY_0) != 0; i++)
		IODelay(1);
	for (int i = 0; i < 100000 && rd(r, SI_mmRLC_SERDES_MASTER_BUSY_1) != 0; i++)
		IODelay(1);
}

static void
guiIdleInterrupt(struct MinkarAMDRegs *r, bool on)
{
	uint32_t v = rd(r, SI_mmCP_INT_CNTL_RING0);
	uint32_t bits = SI_CP_INT_CNTL__CNTX_BUSY_INT_ENABLE_MASK | SI_CP_INT_CNTL__CNTX_EMPTY_INT_ENABLE_MASK;
	wr(r, SI_mmCP_INT_CNTL_RING0, on ? v | bits : v & ~bits);
	if (!on) {
		/* A graphics register read, then the clocks and power up, the
		 * RLC idle. */
		(void)rd(r, SI_mmDB_DEPTH_INFO);
		uint32_t mask = SI_RLC_BUSY_STATUS | SI_GFX_POWER_STATUS | SI_GFX_CLOCK_STATUS | SI_GFX_LS_STATUS;
		for (int i = 0; i < 100000; i++) {
			if ((rd(r, SI_mmRLC_STAT) & mask) == (SI_GFX_CLOCK_STATUS | SI_GFX_POWER_STATUS))
				break;
			IODelay(1);
		}
	}
}

void
MinkarAMDGFXStartRLC(struct MinkarAMDRegs *r, const struct amd_ucode *rlc, uint64_t gpuBase,
    volatile uint8_t *cpuBase, uint32_t rasterConfig)
{
	/* The save and restore list, and the clear state buffer: its
	 * commands' address and size, then the commands. */
	volatile uint32_t *sr = (volatile uint32_t *)(uintptr_t)(cpuBase + kSaveRestoreOffset);
	for (size_t i = 0; i < sizeof(si_rlc_save_restore) / 4; i++)
		sr[i] = si_rlc_save_restore[i];
	volatile uint32_t *cs = (volatile uint32_t *)(uintptr_t)(cpuBase + kClearStateOffset);
	uint64_t commands = gpuBase + kClearStateOffset + 256;
	uint32_t count = (uint32_t)(sizeof(si_clear_state) / 4);
	cs[0] = (uint32_t)(commands >> 32);
	cs[1] = (uint32_t)commands;
	cs[2] = count;
	for (uint32_t i = 0; i < count; i++)
		cs[64 + i] = i == SI_CLEAR_STATE_RASTER_INDEX ? rasterConfig : si_clear_state[i];
	OSSynchronizeIO();
	wr(r, mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL, 1);
	(void)rd(r, mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL);

	/* Stopped and reset (gfx_v6_0_rlc_stop, _rlc_reset). */
	wr(r, SI_mmRLC_CNTL, 0);
	guiIdleInterrupt(r, false);
	rlcSerdesIdle(r);
	wr(r, mmSI_GRBM_SOFT_RESET, rd(r, mmSI_GRBM_SOFT_RESET) | SI_GRBM_SOFT_RESET__SOFT_RESET_RLC_MASK);
	IODelay(50);
	wr(r, mmSI_GRBM_SOFT_RESET, rd(r, mmSI_GRBM_SOFT_RESET) & ~SI_GRBM_SOFT_RESET__SOFT_RESET_RLC_MASK);
	IODelay(50);
	/* Its buffers (gfx_v6_0_init_pg, no power gating). */
	wr(r, SI_mmRLC_SAVE_AND_RESTORE_BASE, (uint32_t)((gpuBase + kSaveRestoreOffset) >> 8));
	wr(r, SI_mmRLC_CLEAR_STATE_RESTORE_BASE, (uint32_t)((gpuBase + kClearStateOffset) >> 8));
	wr(r, SI_mmRLC_RL_BASE, 0);
	wr(r, SI_mmRLC_RL_SIZE, 0);
	wr(r, SI_mmRLC_LB_CNTL, 0);
	wr(r, SI_mmRLC_LB_CNTR_MAX, 0xffffffff);
	wr(r, SI_mmRLC_LB_CNTR_INIT, 0);
	wr(r, SI_mmRLC_LB_INIT_CU_MASK, 0xffffffff);
	wr(r, SI_mmRLC_MC_CNTL, 0);
	wr(r, SI_mmRLC_UCODE_CNTL, 0);
	for (uint32_t i = 0; i < rlc->words; i++) {
		wr(r, SI_mmRLC_UCODE_ADDR, i);
		wr(r, SI_mmRLC_UCODE_DATA, amd_ucode_word(rlc, i));
	}
	wr(r, SI_mmRLC_UCODE_ADDR, 0);
	/* Load balancing only with DDR3 (gfx_v6_0_lbpw_supported): off. */
	if ((rd(r, SI_mmMC_SEQ_MISC0) & 0xf0000000) == 0xb0000000)
		wr(r, SI_mmRLC_LB_CNTL, rd(r, SI_mmRLC_LB_CNTL) | SI_RLC_LB_CNTL__LOAD_BALANCE_ENABLE_MASK);
	else {
		wr(r, SI_mmRLC_LB_CNTL, rd(r, SI_mmRLC_LB_CNTL) & ~SI_RLC_LB_CNTL__LOAD_BALANCE_ENABLE_MASK);
		wr(r, mmSI_GRBM_GFX_INDEX_BROADCAST_REG, mmSI_GRBM_GFX_INDEX_BROADCAST);
		wr(r, SI_mmSPI_LB_CU_MASK, 0x00ff);
	}
	/* On (gfx_v6_0_rlc_start). */
	wr(r, SI_mmRLC_CNTL, SI_RLC_CNTL__RLC_ENABLE_F32_MASK);
	guiIdleInterrupt(r, true);
	IODelay(50);
	LOG("RLC started: microcode %u, clear state of %u dwords", rlc->version, count);
}

bool
MinkarAMDGFXStart(struct MinkarAMDGFX *g, struct MinkarAMDRegs *r, uint64_t gpuBase,
    volatile uint8_t *cpuBase, bool clearState, OSDictionary *results)
{
	uint32_t p[16];

	g->regs = r;
	g->gpuBase = gpuBase;
	g->ring = (volatile uint32_t *)(uintptr_t)cpuBase;	/* page aligned */
	g->ringDwords = kMinkarAMDGFXRingBytes / 4;
	g->wptr = 0;
	g->writeBack = (volatile uint32_t *)(uintptr_t)(cpuBase + kWriteBackOffset);
	g->writeBack[0] = 0;
	g->seq = 0;
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
	/* The context registers' clear state, as the RLC keeps it. */
	if (ok && clearState) {
		const unsigned n = sizeof(si_cp_start1) / 4;
		if (room(g, n)) {
			emit(g, si_cp_start1, n);
			commit(g);
			ok = drained(g);
		} else
			ok = false;
		if (!ok)
			LOG("the command processor did not take the clear state");
	}
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
	if (!room(g, 3))
		return false;
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

/* The GPU's writes reach what the CPU reads through the window. */
static void
invalidateHDP(struct MinkarAMDGFX *g)
{
	wr(g->regs, mmSI_HDP_DEBUG0, 1);
	(void)rd(g->regs, mmSI_HDP_DEBUG0);
}

/* Fences count up; one is passed when the written value has reached it. */
static bool
fenceWait(struct MinkarAMDGFX *g, uint32_t seq)
{
	uint64_t end = nowNS() + kTimeoutNS;
	for (;;) {
		invalidateHDP(g);
		if ((int32_t)(g->writeBack[0] - seq) >= 0)
			return true;
		if (nowNS() >= end)
			return false;
		IODelay(2);
	}
}

bool
MinkarAMDGFXRunCommands(struct MinkarAMDGFX *g, uint64_t address, uint32_t dwords, unsigned vmid,
    uint64_t *ns)
{
	uint32_t p[kSIFenceDwords + 4];
	unsigned n;

	if (dwords == 0 || dwords > 0xfffff || (address & 3) != 0)
		return false;
	if (!room(g, kSIFenceDwords + 4))
		return false;
	uint64_t start = nowNS();
	n = amd_si_gfx_emit_ib(p, address, dwords, vmid);
	n += amd_si_gfx_emit_fence(p + n, g->gpuBase + kWriteBackOffset, ++g->seq, 0);
	emit(g, p, n);
	commit(g);
	bool ok = fenceWait(g, g->seq);
	if (ns != NULL)
		*ns = nowNS() - start;
	if (!ok)
		LOG("fence %u not passed (at %u; CP_STAT 0x%08x, GRBM_STATUS 0x%08x, rptr %u, wptr %u)",
		    g->seq, g->writeBack[0], rd(g->regs, mmSI_CP_STAT), rd(g->regs, mmSI_GRBM_STATUS),
		    rd(g->regs, mmSI_CP_RB0_RPTR), g->wptr);
	return ok;
}

bool
MinkarAMDGFXIBTest(struct MinkarAMDGFX *g, OSDictionary *results)
{
	volatile uint32_t *ib = (volatile uint32_t *)(uintptr_t)((volatile uint8_t *)g->ring +
	    kTestIBOffset);
	uint32_t p[4];
	uint64_t ns = 0;

	wr(g->regs, mmSI_SCRATCH_REG0, 0xcafedead);
	unsigned n = amd_si_gfx_emit_set_config(p, mmSI_SCRATCH_REG0, 0x600dcafe);
	for (unsigned i = 0; i < n; i++)
		ib[i] = p[i];
	bool fenced = MinkarAMDGFXRunCommands(g, g->gpuBase + kTestIBOffset, n, 0, &ns);
	uint32_t value = rd(g->regs, mmSI_SCRATCH_REG0);
	setNumber(results, "IBScratchValue", value, 32);
	setNumber(results, "IBFenceValue", g->writeBack[0], 32);
	setNumber(results, "IBTestNanoseconds", ns, 64);
	bool ok = fenced && value == 0x600dcafe;
	if (results != NULL)
		results->setObject("IBPassed", ok ? kOSBooleanTrue : kOSBooleanFalse);
	if (!ok)
		LOG("IB test: fence %s, scratch 0x%08x", fenced ? "passed" : "not passed", value);
	return ok;
}

void
MinkarAMDGFXReset(struct MinkarAMDRegs *r)
{
	/* The command processor and the pipeline behind it: a wave that never
	 * ends holds the shader blocks, and the processor cannot start again
	 * while they are busy. */
	const uint32_t mask = SI_SOFT_RESET_CP | SI_SOFT_RESET_GFX | SI_SOFT_RESET_CPF |
	    SI_SOFT_RESET_CPC | SI_SOFT_RESET_CPG | SI_SOFT_RESET_PIPELINE;

	enable(r, false);
	wr(r, mmSI_GRBM_SOFT_RESET, rd(r, mmSI_GRBM_SOFT_RESET) | mask);
	(void)rd(r, mmSI_GRBM_SOFT_RESET);
	IODelay(50);
	wr(r, mmSI_GRBM_SOFT_RESET, rd(r, mmSI_GRBM_SOFT_RESET) & ~mask);
	(void)rd(r, mmSI_GRBM_SOFT_RESET);
	IODelay(50);
	LOG("reset: GRBM_STATUS 0x%08x, CP_STAT 0x%08x", rd(r, mmSI_GRBM_STATUS), rd(r, mmSI_CP_STAT));
}

void
MinkarAMDGFXStop(struct MinkarAMDGFX *g)
{
	enable(g->regs, false);
}
