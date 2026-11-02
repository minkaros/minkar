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
#include <libkern/OSAtomic.h>
#include <kern/clock.h>
#include "MinkarAMDDMA.h"
#include "amd_si_dma.h"

#define LOG(fmt, ...)	IOLog("MinkarAMDDMA: " fmt "\n", ##__VA_ARGS__)

#define kRingBytes		(64 * 1024)
#define kWriteBackOffset	kRingBytes	/* [0] read pointer, [1] fence, [2] test */
#define kTimeoutNS		(100ULL * 1000 * 1000)

static inline uint32_t rd(struct MinkarAMDDMA *d, uint32_t reg) { return MinkarAMDRead(d->regs, reg + d->engine); }
static inline void wr(struct MinkarAMDDMA *d, uint32_t reg, uint32_t v) { MinkarAMDWrite(d->regs, reg + d->engine, v); }

/* Dwords of the region, at a byte offset (always a multiple of 4). */
static volatile uint32_t *
dwordsAt(struct MinkarAMDDMA *d, uint64_t offset)
{
	return (volatile uint32_t *)((uintptr_t)d->cpuBase + offset);
}

static volatile uint32_t *
writeBack(struct MinkarAMDDMA *d)
{
	return dwordsAt(d, kWriteBackOffset);
}

static uint64_t
now(void)
{
	uint64_t t, ns;

	clock_get_uptime(&t);
	absolutetime_to_nanoseconds(t, &ns);
	return ns;
}

/* What the CPU wrote through the window reaches VRAM before the engine
 * reads it; what the engine wrote is seen by the CPU's next reads. */
static void
flushHDP(struct MinkarAMDDMA *d)
{
	OSSynchronizeIO();
	MinkarAMDWrite(d->regs, mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL, 1);
	(void)MinkarAMDRead(d->regs, mmSI_HDP_MEM_COHERENCY_FLUSH_CNTL);
}

static void
invalidateHDP(struct MinkarAMDDMA *d)
{
	MinkarAMDWrite(d->regs, mmSI_HDP_DEBUG0, 1);
	(void)MinkarAMDRead(d->regs, mmSI_HDP_DEBUG0);
}

/*
 * Room for n dwords. The ring is used one submission at a time (each
 * waits for its fence), so the engine is idle when the ring wraps: the
 * rest is filled with NOPs and writing starts again at 0.
 */
static void
reserve(struct MinkarAMDDMA *d, uint32_t n)
{
	if (d->wptr + n < d->ringDwords)
		return;
	while (d->wptr < d->ringDwords) {
		uint32_t nop;
		si_dma_nop(&nop);
		d->ring[d->wptr++] = nop;
	}
	d->wptr = 0;
}

static void
emit(struct MinkarAMDDMA *d, const uint32_t *p, unsigned n)
{
	reserve(d, n);
	for (unsigned i = 0; i < n; i++)
		d->ring[d->wptr++] = p[i];
}

static void
commit(struct MinkarAMDDMA *d)
{
	flushHDP(d);
	wr(d, mmSI_DMA_RB_WPTR, (d->wptr << 2) & 0x3fffc);
}

/* Until the fence has seq (or a later one), at most kTimeoutNS. */
static bool
waitFence(struct MinkarAMDDMA *d, uint32_t seq, uint64_t *doneAt)
{
	uint64_t start = now(), t;

	do {
		invalidateHDP(d);
		if ((int32_t)(writeBack(d)[1] - seq) >= 0) {
			if (doneAt != NULL)
				*doneAt = now();
			return true;
		}
		IODelay(2);
		t = now();
	} while (t - start < kTimeoutNS);
	LOG("fence %u not reached: status 0x%08x, rptr 0x%x, wptr 0x%x, fence %u", seq,
	    rd(d, mmSI_DMA_STATUS_REG), rd(d, mmSI_DMA_RB_RPTR), rd(d, mmSI_DMA_RB_WPTR),
	    writeBack(d)[1]);
	return false;
}

static uint32_t
emitFence(struct MinkarAMDDMA *d)
{
	uint32_t p[4];

	d->seq++;
	emit(d, p, si_dma_fence(p, d->gpuBase + kWriteBackOffset + 4, d->seq));
	return d->seq;
}

/* si_dma_start's sequence, for one engine, its ring and write-back in
 * the region. */
bool
MinkarAMDDMAStart(struct MinkarAMDDMA *d, struct MinkarAMDRegs *regs, uint32_t engine,
    uint64_t gpuBase, volatile uint8_t *cpuBase, IOByteCount size)
{
	bzero(d, sizeof(*d));
	if (size < kMinkarAMDDMABuffers)
		return false;
	d->regs = regs;
	d->engine = engine;
	d->gpuBase = gpuBase;
	d->cpuBase = cpuBase;
	d->size = size;
	d->ring = dwordsAt(d, 0);
	d->ringDwords = kRingBytes / 4;

	for (uint32_t i = 0; i < d->ringDwords; i++)
		d->ring[i] = 0;
	writeBack(d)[0] = 0;
	writeBack(d)[1] = 0;
	flushHDP(d);

	wr(d, mmSI_DMA_RB_CNTL, rd(d, mmSI_DMA_RB_CNTL) & ~SI_DMA_RB_ENABLE);
	wr(d, mmSI_DMA_SEM_INCOMPLETE_TIMER_CNTL, 0);
	wr(d, mmSI_DMA_SEM_WAIT_FAIL_TIMER_CNTL, 0);
	uint32_t rbCntl = si_dma_rb_cntl(d->ringDwords);
	wr(d, mmSI_DMA_RB_CNTL, rbCntl);
	wr(d, mmSI_DMA_RB_RPTR, 0);
	wr(d, mmSI_DMA_RB_WPTR, 0);
	uint64_t rptrAddr = gpuBase + kWriteBackOffset;
	wr(d, mmSI_DMA_RB_RPTR_ADDR_LO, (uint32_t)rptrAddr);
	wr(d, mmSI_DMA_RB_RPTR_ADDR_HI, (uint32_t)(rptrAddr >> 32) & 0xff);
	rbCntl |= SI_DMA_RPTR_WRITEBACK_ENABLE;
	wr(d, mmSI_DMA_RB_BASE, (uint32_t)(gpuBase >> 8));
	wr(d, mmSI_DMA_IB_CNTL, SI_DMA_IB_ENABLE | SI_DMA_CMD_VMID_FORCE);
	wr(d, mmSI_DMA_CNTL, rd(d, mmSI_DMA_CNTL) & ~SI_DMA_CTXEMPTY_INT_ENABLE);
	d->wptr = 0;
	wr(d, mmSI_DMA_RB_WPTR, 0);
	wr(d, mmSI_DMA_RB_CNTL, rbCntl | SI_DMA_RB_ENABLE);

	/* amdgpu's ring test: the engine writes a value to memory. */
	volatile uint32_t *test = &writeBack(d)[2];
	uint32_t p[4];
	*test = 0xCAFEDEAD;
	unsigned n = si_dma_write(p, gpuBase + kWriteBackOffset + 8, 1);
	p[n++] = 0xDEADBEEF;
	emit(d, p, n);
	commit(d);
	uint64_t start = now();
	do {
		invalidateHDP(d);
		if (*test == 0xDEADBEEF)
			return true;
		IODelay(10);
	} while (now() - start < kTimeoutNS);
	LOG("engine 0x%x took no write: status 0x%08x, rptr 0x%x, wptr 0x%x", engine,
	    rd(d, mmSI_DMA_STATUS_REG), rd(d, mmSI_DMA_RB_RPTR), rd(d, mmSI_DMA_RB_WPTR));
	MinkarAMDDMAStop(d);
	return false;
}

void
MinkarAMDDMAStop(struct MinkarAMDDMA *d)
{
	if (d->regs != NULL)
		wr(d, mmSI_DMA_RB_CNTL, rd(d, mmSI_DMA_RB_CNTL) & ~SI_DMA_RB_ENABLE);
}

bool
MinkarAMDDMACopy(struct MinkarAMDDMA *d, uint64_t dst, uint64_t src, uint64_t bytes,
    uint64_t *ns)
{
	uint32_t p[5];

	if (dst + bytes > d->size || src + bytes > d->size)
		return false;
	for (uint64_t done = 0; done < bytes; ) {
		uint32_t chunk = bytes - done > kSIDMAMaxCopyBytes ? kSIDMAMaxCopyBytes :
		    (uint32_t)(bytes - done);
		emit(d, p, si_dma_copy(p, d->gpuBase + dst + done, d->gpuBase + src + done, chunk));
		done += chunk;
	}
	uint32_t seq = emitFence(d);
	uint64_t start = now(), end = 0;
	commit(d);
	if (!waitFence(d, seq, &end))
		return false;
	if (ns != NULL)
		*ns = end - start;
	return true;
}

static void
put(OSDictionary *results, const char *key, uint64_t value)
{
	OSNumber *n = OSNumber::withNumber((unsigned long long)value, 64);
	if (n != NULL) {
		results->setObject(key, n);
		n->release();
	}
}

static inline uint32_t
pattern(uint64_t i)
{
	return (uint32_t)(i * 2654435761u) ^ 0x5a5a5a5a;
}

bool
MinkarAMDDMASelfTest(struct MinkarAMDDMA *d, uint64_t bytes, OSDictionary *results)
{
	uint64_t src = kMinkarAMDDMABuffers, dst = src + bytes, ns = 0;

	if (dst + bytes > d->size)
		return false;
	/* A fence alone. */
	uint64_t done = 0, start = now();
	uint32_t seq = emitFence(d);
	commit(d);
	bool fence = waitFence(d, seq, &done);
	results->setObject("Fence", fence ? kOSBooleanTrue : kOSBooleanFalse);
	if (fence)
		put(results, "FenceNanoseconds", done - start);

	/* A copy between two surfaces, checked by the CPU: every 64th word
	 * and the last (reads through the window are slow). */
	volatile uint32_t *s = dwordsAt(d, src);
	volatile uint32_t *t = dwordsAt(d, dst);
	uint64_t words = bytes / 4;
	for (uint64_t i = 0; i < words; i++) {
		s[i] = pattern(i);
		t[i] = 0;
	}
	bool copied = fence && MinkarAMDDMACopy(d, dst, src, bytes, &ns);
	uint64_t bad = 0;
	if (copied) {
		invalidateHDP(d);
		for (uint64_t i = 0; i < words; i += 64)
			if (t[i] != pattern(i))
				bad++;
		if (t[words - 1] != pattern(words - 1))
			bad++;
	}
	put(results, "CopyBytes", bytes);
	results->setObject("Copy", copied ? kOSBooleanTrue : kOSBooleanFalse);
	if (copied) {
		put(results, "CopyNanoseconds", ns);
		put(results, "CopyMismatches", bad);
		if (ns > 0)
			put(results, "CopyMBPerSecond", bytes * 1000ULL / ns);
	}
	LOG("fence %s; copy of %llu KB %s in %llu us, %llu mismatches", fence ? "ok" : "FAILED",
	    (unsigned long long)bytes / 1024, copied ? "done" : "FAILED",
	    (unsigned long long)ns / 1000, (unsigned long long)bad);
	return fence && copied && bad == 0;
}
