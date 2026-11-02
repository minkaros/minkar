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
 * MinkarAMDDMA (ADR 0002, G2): a GCN 1.0 GPU's DMA engine, its ring in
 * VRAM, driven without interrupts: commands are written to the ring, the
 * write pointer moved, and a fence (a value the engine writes once
 * everything before it is done) waited for. The first part of G2: VRAM
 * to VRAM, so neither the GART nor any microcode is needed yet.
 */

#ifndef _MINKARAMDDMA_H
#define _MINKARAMDDMA_H

#include <IOKit/IOMemoryDescriptor.h>
#include "MinkarAMDDevice.h"

class OSDictionary;

struct MinkarAMDDMA {
	struct MinkarAMDRegs *	regs;
	uint32_t		engine;		/* register offset: DMA0 or DMA1 */
	uint64_t		gpuBase;	/* the region, in the GPU's space */
	volatile uint8_t *	cpuBase;	/* and through the VRAM window */
	IOByteCount		size;
	volatile uint32_t *	ring;
	uint32_t		ringDwords;
	uint32_t		wptr;		/* in dwords */
	uint32_t		seq;		/* the last fence emitted */
};

/*
 * A region of VRAM for the engine: the first 64 KB its ring, the next
 * 4 KB the read pointer's write-back and the fence, the rest for buffers
 * (from kMinkarAMDDMABuffers). Starts the engine; false if it does not
 * take a write within 100 ms.
 */
#define kMinkarAMDDMABuffers	(68 * 1024)
bool MinkarAMDDMAStart(struct MinkarAMDDMA *dma, struct MinkarAMDRegs *regs, uint32_t engine,
    uint64_t gpuBase, volatile uint8_t *cpuBase, IOByteCount size);
void MinkarAMDDMAStop(struct MinkarAMDDMA *dma);

/* Copies bytes within the region (offsets from its start) and waits for
 * the fence; the time from the write pointer to the fence in *ns. */
bool MinkarAMDDMACopy(struct MinkarAMDDMA *dma, uint64_t dst, uint64_t src, uint64_t bytes,
    uint64_t *ns);
/* The same between any two addresses in the GPU's space. */
bool MinkarAMDDMACopyAddresses(struct MinkarAMDDMA *dma, uint64_t dst, uint64_t src,
    uint64_t bytes, uint64_t *ns);

/* Runs a command buffer of dwords at offset in the region (32-byte
 * aligned), then a fence, and waits for it. */
bool MinkarAMDDMARunCommands(struct MinkarAMDDMA *dma, uint64_t offset, uint32_t dwords);

/* The engine's checks, for the bring-up: a write, a fence, a copy of
 * bytes verified by the CPU and timed, and a command buffer with a write
 * and a copy. Results in *results. */
bool MinkarAMDDMASelfTest(struct MinkarAMDDMA *dma, uint64_t bytes, OSDictionary *results);

#endif
