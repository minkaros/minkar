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
 * first step: the processor started and its ring tested by a register
 * write, as amdgpu's gfx_v6_0.c does before anything else.
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
};

#define kMinkarAMDGFXRingBytes	(64 * 1024)

/* Halts the processor and loads its three microcodes. */
void MinkarAMDGFXLoadMicrocode(struct MinkarAMDRegs *regs, const struct amd_ucode *pfp,
    const struct amd_ucode *me, const struct amd_ucode *ce);
/* Sets up ring 0 at gpuBase (kMinkarAMDGFXRingBytes, mapped at cpuBase) and
 * starts the processor on it; false if it does not take its start. */
bool MinkarAMDGFXStart(struct MinkarAMDGFX *gfx, struct MinkarAMDRegs *regs, uint64_t gpuBase,
    volatile uint8_t *cpuBase, OSDictionary *results);
/* A scratch register written through the ring; the time it took. */
bool MinkarAMDGFXRingTest(struct MinkarAMDGFX *gfx, uint64_t *ns, OSDictionary *results);
/* Halts the processor. */
void MinkarAMDGFXStop(struct MinkarAMDGFX *gfx);

#endif
