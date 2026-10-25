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
 * The generic statistics every network interface publishes, as an
 * IONetworkData named kIONetworkStatsKey. The family counts packets; the
 * driver adds its errors and collisions.
 */

#ifndef _IONETWORKSTATS_H
#define _IONETWORKSTATS_H

#include <IOKit/IOTypes.h>

typedef struct {
	UInt32	inputPackets;
	UInt32	inputErrors;
	UInt32	outputPackets;
	UInt32	outputErrors;
	UInt32	collisions;
} IONetworkStats;

#define kIONetworkStatsKey	"IONetworkStatsKey"

/* Output queue statistics, an IONetworkData named kIOOutputQueueStatsKey. */
typedef struct {
	UInt32	capacity;
	UInt32	size;
	UInt32	peakSize;
	UInt32	dropCount;
	UInt32	outputCount;
	UInt32	retryCount;
	UInt32	stallCount;
	UInt32	reserved[4];
} IOOutputQueueStats;

#define kIOOutputQueueStatsKey	"IOOutputQueueStatsKey"

#endif /* !_IONETWORKSTATS_H */
