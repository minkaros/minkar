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
 * MinkarAMDGFX (ADR 0002, G2): a GCN 1.0 GPU's graphics command processor,
 * its microcode loaded from AMD's files, its ring (ring 0) in VRAM. The
 * processor started with the RLC and the clear state, as amdgpu's
 * gfx_v6_0.c does, its ring tested by a register write.
 */

#ifndef _MINKARAMDGFX_H
#define _MINKARAMDGFX_H

#include "MinkarAMDDevice.h"

class OSDictionary;
struct amd_ucode;

struct MinkarAMDGFX {
	struct MinkarAMDRegs *	regs;
	uint64_t		gpuBase;	/* the ring, in the GPU's space */
	volatile uint32_t *	ring;		/* and through the VRAM window */
	uint32_t		ringDwords;
	uint32_t		wptr;		/* in dwords */
	volatile uint32_t *	writeBack;	/* [0] the last fence */
	uint32_t		seq;		/* the last fence emitted */
};

/* The region the processor needs: its ring, then a page for write-back
 * (fences) and the self-test's indirect buffer, then the RLC's: its save
 * and restore list (a page) and the clear state buffer (a page). */
#define kMinkarAMDGFXRingBytes		(64 * 1024)
#define kMinkarAMDGFXRegionBytes	(kMinkarAMDGFXRingBytes + 3 * 4096)

/* Halts the processor and loads its three microcodes. */
void MinkarAMDGFXLoadMicrocode(struct MinkarAMDRegs *regs, const struct amd_ucode *pfp,
    const struct amd_ucode *me, const struct amd_ucode *ce);
/* The RLC (the graphics block's run list controller) started with its
 * microcode, its save and restore list and the clear state buffer in the
 * region (rasterConfig in it), as gfx_v6_0_rlc_init and _rlc_resume do
 * (no power gating: GCN 1.0's discrete chips have none on). */
void MinkarAMDGFXStartRLC(struct MinkarAMDRegs *regs, const struct amd_ucode *rlc, uint64_t gpuBase,
    volatile uint8_t *cpuBase, uint32_t rasterConfig);
/* Sets up ring 0 at gpuBase (kMinkarAMDGFXRegionBytes, mapped at cpuBase) and
 * starts the processor on it, then (clearState, the RLC started) loads
 * the context registers' clear state as gfx_v6_0_cp_gfx_start does; false
 * if it does not take its start. */
bool MinkarAMDGFXStart(struct MinkarAMDGFX *gfx, struct MinkarAMDRegs *regs, uint64_t gpuBase,
    volatile uint8_t *cpuBase, bool clearState, OSDictionary *results);
/* A scratch register written through the ring; the time it took. */
bool MinkarAMDGFXRingTest(struct MinkarAMDGFX *gfx, uint64_t *ns, OSDictionary *results);
/* An indirect buffer at a GPU address (VRAM or the GART in context 0, a
 * client's address in its context vmid) run, then a fence waited for;
 * the time it took. */
bool MinkarAMDGFXRunCommands(struct MinkarAMDGFX *gfx, uint64_t address, uint32_t dwords,
    unsigned vmid, uint64_t *ns);
/* An indirect buffer that writes a scratch register, run with its fence. */
bool MinkarAMDGFXIBTest(struct MinkarAMDGFX *gfx, OSDictionary *results);
/* After a command buffer that did not finish: the processor halted and
 * the graphics block reset (CP and the GFX pipeline); it starts again
 * with MinkarAMDGFXLoadMicrocode and MinkarAMDGFXStart. */
void MinkarAMDGFXReset(struct MinkarAMDRegs *regs);
/* Halts the processor. */
void MinkarAMDGFXStop(struct MinkarAMDGFX *gfx);

#endif
