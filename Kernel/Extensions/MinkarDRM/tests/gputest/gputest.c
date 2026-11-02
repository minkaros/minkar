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
 * minkar-gputest: MinkarAMDGPU's user client from a program (ADR 0002,
 * G2). Opens an AMD GPU, prints its VRAM, makes two buffers, fills one
 * through its mapping, has the DMA engine copy it to the other, checks
 * the copy through the other's mapping, and frees them; with the GART on
 * (minkaramd_gart), also system buffers: one filled by the CPU, copied by
 * the engine to VRAM and on to another, compared in full. One line per
 * step; the exit status is the number of failures.
 *
 *   minkar-gputest [-g GPU] [-m MB]	GPU: 0 and up (default 0); MB: the
 *					size of the copy (default 8)
 *   minkar-gputest -l			lists the GPUs
 *   minkar-gputest [-g GPU] -r REG,...	prints registers (dword indices,
 *					hex), for bring-up
 */

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "MinkarAMDGPUInterface.h"

static int failures;

static void
check(int good, const char *what, const char *detail)
{
	printf("%s %s%s%s\n", good ? "ok  " : "FAIL", what, detail[0] ? ": " : "", detail);
	if (!good)
		failures++;
}

static uint32_t
pattern(uint64_t i)
{
	return (uint32_t)(i * 2654435761u) ^ 0xa5a5a5a5u;
}

int
main(int argc, char **argv)
{
	unsigned gpu = 0, megabytes = 8;
	int list = 0, c;
	const char *regs = NULL;
	char detail[256];

	while ((c = getopt(argc, argv, "g:m:lr:")) != -1)
		switch (c) {
		case 'g': gpu = (unsigned)strtoul(optarg, NULL, 0); break;
		case 'm': megabytes = (unsigned)strtoul(optarg, NULL, 0); break;
		case 'l': list = 1; break;
		case 'r': regs = optarg; break;
		default:
			fprintf(stderr, "usage: minkar-gputest [-g gpu] [-m megabytes] | -l | "
			    "[-g gpu] -r reg,...\n");
			return 2;
		}

	io_iterator_t it;
	if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("MinkarAMDGPU"),
	    &it) != KERN_SUCCESS) {
		check(0, "find the GPUs", "");
		return failures;
	}
	io_service_t service = IO_OBJECT_NULL, s;
	for (unsigned i = 0; (s = IOIteratorNext(it)) != IO_OBJECT_NULL; i++) {
		if (list) {
			io_string_t path;
			IORegistryEntryGetPath(s, kIOServicePlane, path);
			printf("%u %s\n", i, path);
		}
		if (i == gpu && service == IO_OBJECT_NULL)
			service = s;
		else
			IOObjectRelease(s);
	}
	IOObjectRelease(it);
	if (list)
		return 0;
	if (service == IO_OBJECT_NULL) {
		snprintf(detail, sizeof(detail), "no GPU %u", gpu);
		check(0, "find the GPU", detail);
		return failures;
	}

	io_connect_t conn;
	kern_return_t kr = IOServiceOpen(service, mach_task_self(), 0, &conn);
	IOObjectRelease(service);
	snprintf(detail, sizeof(detail), "0x%x", kr);
	check(kr == KERN_SUCCESS, "open the user client", kr == KERN_SUCCESS ? "" : detail);
	if (kr != KERN_SUCCESS)
		return failures;

	uint64_t out[3];
	uint32_t n = 3;
	if (regs != NULL) {
		char *copy = strdup(regs), *save = NULL;
		for (char *r = strtok_r(copy, ",", &save); r != NULL; r = strtok_r(NULL, ",", &save)) {
			uint64_t reg = strtoull(r, NULL, 16);
			n = 1;
			if (IOConnectCallScalarMethod(conn, kMinkarAMDGPUReadRegister, &reg, 1, out, &n) ==
			    KERN_SUCCESS)
				printf("0x%04llx 0x%08llx\n", (unsigned long long)reg,
				    (unsigned long long)out[0]);
			else {
				printf("0x%04llx unreadable\n", (unsigned long long)reg);
				failures++;
			}
		}
		free(copy);
		IOServiceClose(conn);
		return failures;
	}
	kr = IOConnectCallScalarMethod(conn, kMinkarAMDGPUInfo, NULL, 0, out, &n);
	snprintf(detail, sizeof(detail), "%llu MB, %llu MB free, %llu MB window",
	    (unsigned long long)out[0] >> 20, (unsigned long long)out[1] >> 20,
	    (unsigned long long)out[2] >> 20);
	check(kr == KERN_SUCCESS, "VRAM", kr == KERN_SUCCESS ? detail : "no answer");

	uint64_t bytes = (uint64_t)megabytes << 20, handle[2] = { 0, 0 }, gpuAddr[2] = { 0, 0 };
	uint64_t freeWithBuffers = 0;	/* before the gotos: read at out */
	for (int b = 0; b < 2; b++) {
		uint64_t in = bytes;
		n = 2;
		kr = IOConnectCallScalarMethod(conn, kMinkarAMDGPUCreateBuffer, &in, 1, out, &n);
		if (kr == KERN_SUCCESS) {
			handle[b] = out[0];
			gpuAddr[b] = out[1];
		}
		snprintf(detail, sizeof(detail), "%u MB at GPU address 0x%llx", megabytes,
		    (unsigned long long)gpuAddr[b]);
		check(kr == KERN_SUCCESS, b == 0 ? "create the source buffer" : "create the destination buffer",
		    kr == KERN_SUCCESS ? detail : "refused");
	}
	if (handle[0] == 0 || handle[1] == 0)
		goto out;
	/* What is free with the buffers made: freeing them gives it back.
	 * (Not what was free before: the first copy starts the DMA engine,
	 * which keeps 1 MB of VRAM for its ring.) */
	n = 3;
	if (IOConnectCallScalarMethod(conn, kMinkarAMDGPUInfo, NULL, 0, out, &n) == KERN_SUCCESS)
		freeWithBuffers = out[1];

	mach_vm_address_t addr[2] = { 0, 0 };
	mach_vm_size_t size[2] = { 0, 0 };
	for (int b = 0; b < 2; b++) {
		kr = IOConnectMapMemory64(conn, (uint32_t)handle[b], mach_task_self(), &addr[b],
		    &size[b], kIOMapAnywhere);
		check(kr == KERN_SUCCESS && size[b] >= bytes, b == 0 ? "map the source" : "map the destination", "");
	}
	if (addr[0] == 0 || addr[1] == 0)
		goto out;

	volatile uint32_t *src = (volatile uint32_t *)(uintptr_t)addr[0];
	volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)addr[1];
	uint64_t words = bytes / 4;
	for (uint64_t i = 0; i < words; i++) {
		src[i] = pattern(i);
		dst[i] = 0;
	}
	uint64_t in[5] = { handle[1], 0, handle[0], 0, bytes };
	n = 1;
	kr = IOConnectCallScalarMethod(conn, kMinkarAMDGPUCopy, in, 5, out, &n);
	snprintf(detail, sizeof(detail), "%u MB in %llu us, %llu MB/s", megabytes,
	    (unsigned long long)out[0] / 1000,
	    out[0] ? (unsigned long long)(bytes * 1000 / out[0]) : 0ULL);
	check(kr == KERN_SUCCESS, "the DMA engine copies", kr == KERN_SUCCESS ? detail : "failed");
	if (kr == KERN_SUCCESS) {
		/* Every 64th word and the last: reads through the window are slow. */
		uint64_t bad = 0;
		for (uint64_t i = 0; i < words; i += 64)
			if (dst[i] != pattern(i))
				bad++;
		if (dst[words - 1] != pattern(words - 1))
			bad++;
		snprintf(detail, sizeof(detail), "%llu mismatches", (unsigned long long)bad);
		check(bad == 0, "the copy is right", detail);
	}
	/* System memory through the GART: system -> VRAM -> system. */
	if (kr == KERN_SUCCESS) {
		uint64_t sys[2] = { 0, 0 }, sysAddr[2] = { 0, 0 };
		mach_vm_address_t sysMap[2] = { 0, 0 };
		mach_vm_size_t sysSize[2] = { 0, 0 };
		int haveSystem = 1;
		for (int b = 0; b < 2 && haveSystem; b++) {
			uint64_t want = bytes;
			n = 2;
			if (IOConnectCallScalarMethod(conn, kMinkarAMDGPUCreateSystemBuffer, &want, 1, out,
			    &n) != KERN_SUCCESS)
				haveSystem = 0;
			else {
				sys[b] = out[0];
				sysAddr[b] = out[1];
			}
		}
		if (!haveSystem && sys[0] == 0)
			printf("skip system buffers: no GART (boot with minkaramd_gart)\n");
		else {
			snprintf(detail, sizeof(detail), "at GPU addresses 0x%llx and 0x%llx",
			    (unsigned long long)sysAddr[0], (unsigned long long)sysAddr[1]);
			check(haveSystem, "create two system buffers", haveSystem ? detail : "refused");
		}
		for (int b = 0; b < 2 && haveSystem; b++)
			if (IOConnectMapMemory64(conn, (uint32_t)sys[b], mach_task_self(), &sysMap[b],
			    &sysSize[b], kIOMapAnywhere) != KERN_SUCCESS || sysSize[b] < bytes)
				haveSystem = 0;
		if (sys[0] != 0 && sys[1] != 0)
			check(haveSystem, "map the system buffers", "");
		if (haveSystem) {
			uint32_t *a = (uint32_t *)(uintptr_t)sysMap[0], *z = (uint32_t *)(uintptr_t)sysMap[1];
			for (uint64_t i = 0; i < words; i++) {
				a[i] = pattern(i) ^ 0x0f0f0f0fu;
				z[i] = 0;
			}
			uint64_t toVRAM[5] = { handle[1], 0, sys[0], 0, bytes };
			uint64_t back[5] = { sys[1], 0, handle[1], 0, bytes };
			uint64_t ns1 = 0, ns2 = 0;
			n = 1;
			kern_return_t k1 = IOConnectCallScalarMethod(conn, kMinkarAMDGPUCopy, toVRAM, 5, out, &n);
			ns1 = out[0];
			n = 1;
			kern_return_t k2 = k1 == KERN_SUCCESS ?
			    IOConnectCallScalarMethod(conn, kMinkarAMDGPUCopy, back, 5, out, &n) : k1;
			ns2 = out[0];
			snprintf(detail, sizeof(detail), "to VRAM %llu MB/s, back %llu MB/s",
			    ns1 ? (unsigned long long)(bytes * 1000 / ns1) : 0ULL,
			    ns2 ? (unsigned long long)(bytes * 1000 / ns2) : 0ULL);
			check(k1 == KERN_SUCCESS && k2 == KERN_SUCCESS,
			    "the engine copies system memory to VRAM and back", k2 == KERN_SUCCESS ? detail : "failed");
			if (k2 == KERN_SUCCESS) {
				uint64_t bad = 0;
				for (uint64_t i = 0; i < words; i++)
					if (z[i] != a[i])
						bad++;
				snprintf(detail, sizeof(detail), "%llu of %llu words differ",
				    (unsigned long long)bad, (unsigned long long)words);
				check(bad == 0, "the round trip is right", detail);
			}
		}
		for (int b = 0; b < 2; b++) {
			if (sysMap[b] != 0)
				IOConnectUnmapMemory64(conn, (uint32_t)sys[b], mach_task_self(), sysMap[b]);
			if (sys[b] != 0) {
				uint64_t h = sys[b];
				check(IOConnectCallScalarMethod(conn, kMinkarAMDGPUDestroyBuffer, &h, 1, NULL,
				    NULL) == KERN_SUCCESS, "free a system buffer", "");
			}
		}
	}

	/* A copy past a buffer's end is refused. */
	in[4] = bytes + 4096;
	n = 1;
	check(IOConnectCallScalarMethod(conn, kMinkarAMDGPUCopy, in, 5, out, &n) != KERN_SUCCESS,
	    "a copy past the end is refused", "");
	for (int b = 0; b < 2; b++)
		IOConnectUnmapMemory64(conn, (uint32_t)handle[b], mach_task_self(), addr[b]);

out:
	for (int b = 0; b < 2; b++)
		if (handle[b] != 0) {
			uint64_t h = handle[b];
			check(IOConnectCallScalarMethod(conn, kMinkarAMDGPUDestroyBuffer, &h, 1, NULL, NULL) ==
			    KERN_SUCCESS, "free a buffer", "");
		}
	n = 3;
	if (freeWithBuffers != 0 && IOConnectCallScalarMethod(conn, kMinkarAMDGPUInfo, NULL, 0, out,
	    &n) == KERN_SUCCESS) {
		/* The engine's ring may have been taken since (the first copy). */
		uint64_t back = out[1] + (1ULL << 20) >= freeWithBuffers + 2 * bytes &&
		    out[1] <= freeWithBuffers + 2 * bytes;
		snprintf(detail, sizeof(detail), "%llu MB free",
		    (unsigned long long)out[1] >> 20);
		check(back, "the buffers' VRAM came back", detail);
	}
	IOServiceClose(conn);
	return failures;
}
