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
 * the engine to VRAM and on to another, compared in full; and an
 * IOSurface, imported into the GART, copied by the engine to VRAM and
 * back into it, its pixels checked through the surface. One line per
 * step; the exit status is the number of failures.
 *
 *   minkar-gputest [-g GPU] [-m MB]	GPU: 0 and up (default 0); MB: the
 *					size of the copy (default 8)
 *   minkar-gputest -l			lists the GPUs
 *   minkar-gputest [-g GPU] -r REG,...	prints registers (dword indices,
 *					hex), for bring-up
 *   minkar-gputest -v			an IOSurface in VRAM (IOSurfaceMinkar-
 *					GPUMemory): made and filled here, looked
 *					up by a child after this process let it
 *					go, its VRAM back once both are done
 *   minkar-gputest -V ID -F FD		(the child: looks up surface ID, says
 *					so on FD, checks once the maker let go)
 *   minkar-gputest [-g GPU] -a CH,HPD,REQ,ADDR,SIZE
 *					one AUX transaction (hex), for bring-up
 *   minkar-gputest [-g GPU] -i LINE,SLAVE,OFFSET,COUNT
 *					one DDC (I2C) read (hex), for bring-up
 *   minkar-gputest [-g GPU] -e		whether VRAM mapped from the user
 *					client can be shared as a Mach memory
 *					entry (IOSurfaces in VRAM)
 */

#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurfaceRef.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

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

extern uint32_t IOSurfaceGetMinkarGPUBuffer(IOSurfaceRef buffer);

/* The VRAM free on the GPU IOSurface uses, through a client of our own. */
static uint64_t
freeVRAM(void)
{
	io_iterator_t it;
	io_service_t s;
	uint64_t result = 0, out[3];
	uint32_t n = 3;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("MinkarAMDGPU"), &it) !=
	    KERN_SUCCESS)
		return 0;
	while ((s = IOIteratorNext(it)) != IO_OBJECT_NULL) {
		CFTypeRef lights = IORegistryEntryCreateCFProperty(s, CFSTR("MinkarLightsDisplay"),
		    kCFAllocatorDefault, 0);
		io_connect_t c;
		if (lights == kCFBooleanTrue && IOServiceOpen(s, mach_task_self(), 0, &c) == KERN_SUCCESS) {
			if (IOConnectCallScalarMethod(c, kMinkarAMDGPUInfo, NULL, 0, out, &n) == KERN_SUCCESS)
				result = out[1];
			IOServiceClose(c);
		}
		if (lights != NULL)
			CFRelease(lights);
		IOObjectRelease(s);
	}
	IOObjectRelease(it);
	return result;
}

static uint32_t
surfacePixel(int x, int y)
{
	return 0xff000000u | (uint32_t)(x * 2654435761u + y * 40503u);
}

static int
surfaceInVRAM(const char *self, unsigned childID, int childFD)
{
	const int w = 512, h = 512;
	char detail[128];

	if (childID != 0) {
		/* The child: looks it up, tells the maker, which then lets it go;
		 * the pixels are read after that. */
		IOSurfaceRef s = IOSurfaceLookup(childID);
		check(s != NULL, "the child looks the surface up", "");
		if (childFD >= 0) {
			(void)write(childFD, "x", 1);
			close(childFD);
		}
		if (s == NULL)
			return failures;
		usleep(500 * 1000);
		IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL);
		const uint8_t *px = IOSurfaceGetBaseAddress(s);
		size_t rb = IOSurfaceGetBytesPerRow(s);
		uint64_t bad = 0;
		for (int y = 0; y < h; y += 7)
			for (int x = 0; x < w; x += 3)
				if (((const uint32_t *)(px + (size_t)y * rb))[x] != surfacePixel(x, y))
					bad++;
		IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL);
		snprintf(detail, sizeof(detail), "%llu pixels differ", (unsigned long long)bad);
		check(bad == 0, "and sees the maker's pixels, in VRAM", detail);
		CFRelease(s);
		return failures;
	}

	uint64_t before = freeVRAM();
	CFMutableDictionaryRef props = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	int values[3] = { w, h, 4 };
	CFStringRef keys[3] = { kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement };
	for (int k = 0; k < 3; k++) {
		CFNumberRef v = CFNumberCreate(NULL, kCFNumberIntType, &values[k]);
		CFDictionarySetValue(props, keys[k], v);
		CFRelease(v);
	}
	CFDictionarySetValue(props, CFSTR("IOSurfaceMinkarGPUMemory"), kCFBooleanTrue);
	IOSurfaceRef s = IOSurfaceCreate(props);
	CFRelease(props);
	uint32_t buffer = s ? IOSurfaceGetMinkarGPUBuffer(s) : 0;
	snprintf(detail, sizeof(detail), "surface %u, GPU buffer %u", s ? IOSurfaceGetID(s) : 0, buffer);
	check(s != NULL && buffer != 0 && IOSurfaceGetID(s) != 0, "make an IOSurface in VRAM", detail);
	if (s == NULL || buffer == 0)
		return failures;
	IOSurfaceLock(s, 0, NULL);
	uint8_t *px = IOSurfaceGetBaseAddress(s);
	size_t rb = IOSurfaceGetBytesPerRow(s);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			((uint32_t *)(px + (size_t)y * rb))[x] = surfacePixel(x, y);
	IOSurfaceUnlock(s, 0, NULL);
	uint64_t during = freeVRAM();
	snprintf(detail, sizeof(detail), "%llu KB taken", (unsigned long long)(before - during) / 1024);
	check(during < before, "its pixels are in VRAM", detail);

	/* A child looks it up after this process has let it go. */
	char idText[16], fdText[16];
	int p[2];
	if (pipe(p) != 0)
		return ++failures;
	snprintf(idText, sizeof(idText), "%u", IOSurfaceGetID(s));
	snprintf(fdText, sizeof(fdText), "%d", p[1]);
	pid_t pid = fork();
	if (pid == 0) {
		close(p[0]);
		execl(self, self, "-V", idText, "-F", fdText, (char *)NULL);
		_exit(127);
	}
	close(p[1]);
	char c;
	(void)read(p[0], &c, 1);	/* the child holds it now */
	close(p[0]);
	CFRelease(s);			/* the maker goes */
	int status = 0;
	waitpid(pid, &status, 0);
	check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "the child's checks pass", "");
	usleep(300 * 1000);
	uint64_t after = freeVRAM();
	snprintf(detail, sizeof(detail), "%llu KB free before, %llu after",
	    (unsigned long long)before / 1024, (unsigned long long)after / 1024);
	check(after == before, "the VRAM is back once both are done", detail);
	return failures;
}

int
main(int argc, char **argv)
{
	unsigned gpu = 0, megabytes = 8;
	int list = 0, entry = 0, vram = 0, c;
	unsigned childID = 0;
	int childFD = -1;
	const char *regs = NULL, *auxSpec = NULL, *i2cSpec = NULL;
	char detail[256];

	while ((c = getopt(argc, argv, "g:m:lr:evV:F:a:i:")) != -1)
		switch (c) {
		case 'g': gpu = (unsigned)strtoul(optarg, NULL, 0); break;
		case 'm': megabytes = (unsigned)strtoul(optarg, NULL, 0); break;
		case 'l': list = 1; break;
		case 'r': regs = optarg; break;
		case 'a': auxSpec = optarg; break;
		case 'i': i2cSpec = optarg; break;
		case 'e': entry = 1; break;
		case 'v': vram = 1; break;
		case 'V': childID = (unsigned)strtoul(optarg, NULL, 0); break;
		case 'F': childFD = (int)strtol(optarg, NULL, 0); break;
		default:
			fprintf(stderr, "usage: minkar-gputest [-g gpu] [-m megabytes] | -l | "
			    "[-g gpu] -r reg,...\n");
			return 2;
		}

	if (vram || childID)
		return surfaceInVRAM(argv[0], childID, childFD);

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
	if (i2cSpec != NULL) {
		uint64_t in[4] = { 0 }, o[3] = { 0 };
		uint32_t on = 3;
		char *copy = strdup(i2cSpec), *save = NULL, *t;
		int k = 0;
		for (t = strtok_r(copy, ",", &save); t != NULL && k < 4; t = strtok_r(NULL, ",", &save))
			in[k++] = strtoull(t, NULL, 16);
		free(copy);
		kern_return_t r = IOConnectCallScalarMethod(conn, kMinkarAMDGPUI2CRead, in, 4, o, &on);
		printf("call 0x%x result %lld data", r, (long long)o[0]);
		uint8_t *bytes = (uint8_t *)&o[1];
		for (unsigned i = 0; i < 16; i++)
			printf(" %02x", bytes[i]);
		printf("\n");
		IOServiceClose(conn);
		return r != KERN_SUCCESS;
	}
	if (auxSpec != NULL) {
		uint64_t in[5] = { 0 }, o[6] = { 0 };
		uint32_t on = 6;
		char *copy = strdup(auxSpec), *save = NULL, *t;
		int k = 0;
		for (t = strtok_r(copy, ",", &save); t != NULL && k < 5; t = strtok_r(NULL, ",", &save))
			in[k++] = strtoull(t, NULL, 16);
		free(copy);
		static uint32_t trace[2 * 512];
		size_t traceBytes = sizeof(trace);
		kern_return_t r = IOConnectCallMethod(conn, kMinkarAMDGPUAUXTransaction, in, 5, NULL, 0,
		    o, &on, trace, &traceBytes);
		printf("call 0x%x result %lld reply 0x%02llx execute %lld dataout %llu data", r,
		    (long long)o[0], (unsigned long long)o[1], (long long)o[2], (unsigned long long)o[3]);
		uint8_t *bytes = (uint8_t *)&o[4];
		for (int i = 0; i < 16; i++)
			printf(" %02x", bytes[i]);
		printf("\n");
		for (size_t i = 0; i + 1 < traceBytes / 4; i += 2)
			printf("  %s 0x%04x 0x%08x\n", trace[i] >> 31 ? "W" : "R", trace[i] & 0x7fffffff,
			    trace[i + 1]);
		IOServiceClose(conn);
		return r != KERN_SUCCESS;
	}
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
	if (entry) {
		/* A VRAM buffer, mapped; a memory entry of that mapping, mapped
		 * again: do both see the same VRAM, the GPU's writes too? */
		uint64_t in = 1 << 20, h, sz = 1 << 20;
		n = 2;
		kr = IOConnectCallScalarMethod(conn, kMinkarAMDGPUCreateBuffer, &in, 1, out, &n);
		check(kr == KERN_SUCCESS, "create a 1 MB VRAM buffer", "");
		if (kr != KERN_SUCCESS)
			return failures;
		h = out[0];
		mach_vm_address_t a = 0, b = 0;
		mach_vm_size_t as = 0;
		kr = IOConnectMapMemory64(conn, (uint32_t)h, mach_task_self(), &a, &as, kIOMapAnywhere);
		check(kr == KERN_SUCCESS, "map it", "");
		memory_object_size_t esz = sz;
		mach_port_t me = MACH_PORT_NULL;
		kr = mach_make_memory_entry_64(mach_task_self(), &esz, a, VM_PROT_READ | VM_PROT_WRITE,
		    &me, MACH_PORT_NULL);
		snprintf(detail, sizeof(detail), "0x%x, %llu bytes", kr, (unsigned long long)esz);
		check(kr == KERN_SUCCESS, "make a memory entry of the mapping", detail);
		if (kr == KERN_SUCCESS) {
			kr = mach_vm_map(mach_task_self(), &b, sz, 0, VM_FLAGS_ANYWHERE, me, 0, FALSE,
			    VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, VM_INHERIT_NONE);
			snprintf(detail, sizeof(detail), "0x%x", kr);
			check(kr == KERN_SUCCESS, "map the entry again", kr == KERN_SUCCESS ? "" : detail);
		}
		if (b != 0) {
			volatile uint32_t *pa = (volatile uint32_t *)(uintptr_t)a, *pb = (volatile uint32_t *)(uintptr_t)b;
			pa[0] = 0x12345678;
			pb[100] = 0x9abcdef0;
			__sync_synchronize();
			check(pb[0] == 0x12345678 && pa[100] == 0x9abcdef0, "both see the same memory", "");
			/* The GPU copies the first half to the second: seen through b. */
			for (int i = 0; i < 1024; i++)
				pa[i] = pattern(i);
			uint64_t cp[5] = { h, sz / 2, h, 0, 4096 };
			n = 1;
			kr = IOConnectCallScalarMethod(conn, kMinkarAMDGPUCopy, cp, 5, out, &n);
			int same = kr == KERN_SUCCESS;
			for (int i = 0; i < 1024 && same; i++)
				same = pb[sz / 8 + i] == pattern(i);
			check(same, "the GPU's copy is seen through the entry's mapping", "");
			mach_vm_deallocate(mach_task_self(), b, sz);
		}
		if (me != MACH_PORT_NULL)
			mach_port_deallocate(mach_task_self(), me);
		IOConnectUnmapMemory64(conn, (uint32_t)h, mach_task_self(), a);
		IOConnectCallScalarMethod(conn, kMinkarAMDGPUDestroyBuffer, &h, 1, NULL, NULL);
		IOServiceClose(conn);
		return failures;
	}
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

	/* An IOSurface, imported: its own pages, reached through the GART. */
	if (kr == KERN_SUCCESS) {
		const int w = 1024, h = 1024, bpe = 4;
		CFMutableDictionaryRef props = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		int values[3] = { w, h, bpe };
		CFStringRef keys[3] = { kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement };
		for (int k = 0; k < 3; k++) {
			CFNumberRef v = CFNumberCreate(NULL, kCFNumberIntType, &values[k]);
			CFDictionarySetValue(props, keys[k], v);
			CFRelease(v);
		}
		IOSurfaceRef surface = IOSurfaceCreate(props);
		CFRelease(props);
		check(surface != NULL, "create an IOSurface", surface != NULL ? "1024x1024, 4 bytes per pixel" : "");
		if (surface != NULL) {
			IOSurfaceLock(surface, 0, NULL);
			uint32_t *px = (uint32_t *)IOSurfaceGetBaseAddress(surface);
			size_t rowBytes = IOSurfaceGetBytesPerRow(surface);
			uint64_t span = ((uint64_t)rowBytes * h + 4095) & ~4095ULL;
			for (int y = 0; y < h; y++)
				for (int x = 0; x < w; x++)
					((uint32_t *)((char *)px + (size_t)y * rowBytes))[x] =
					    0xff000000u | (uint32_t)(x * 131 + y * 7919);
			uint64_t imp[2] = { (uint64_t)(uintptr_t)px, span }, ih = 0;
			n = 2;
			kern_return_t ki = IOConnectCallScalarMethod(conn, kMinkarAMDGPUImportMemory, imp, 2, out, &n);
			if (ki == KERN_SUCCESS)
				ih = out[0];
			snprintf(detail, sizeof(detail), "%llu KB at GPU address 0x%llx",
			    (unsigned long long)span / 1024, (unsigned long long)out[1]);
			if (ki == kIOReturnUnsupported)
				printf("skip IOSurface: no GART (boot with minkaramd_gart)\n");
			else
				check(ki == KERN_SUCCESS, "import the surface into the GPU", ki == KERN_SUCCESS ? detail : "refused");
			if (ki == KERN_SUCCESS && span <= bytes) {
				uint64_t toVRAM[5] = { handle[1], 0, ih, 0, span };
				uint64_t back[5] = { ih, 0, handle[1], 0, span };
				n = 1;
				kern_return_t k1 = IOConnectCallScalarMethod(conn, kMinkarAMDGPUCopy, toVRAM, 5, out, &n);
				uint64_t ns1 = out[0];
				for (int y = 0; y < h; y++)
					memset((char *)px + (size_t)y * rowBytes, 0, rowBytes);
				n = 1;
				kern_return_t k2 = k1 == KERN_SUCCESS ?
				    IOConnectCallScalarMethod(conn, kMinkarAMDGPUCopy, back, 5, out, &n) : k1;
				uint64_t bad = 0;
				for (int y = 0; y < h && k2 == KERN_SUCCESS; y++)
					for (int x = 0; x < w; x++)
						if (((uint32_t *)((char *)px + (size_t)y * rowBytes))[x] !=
						    (0xff000000u | (uint32_t)(x * 131 + y * 7919)))
							bad++;
				snprintf(detail, sizeof(detail), "4 MB to VRAM at %llu MB/s, back; %llu pixels differ",
				    ns1 ? (unsigned long long)(span * 1000 / ns1) : 0ULL, (unsigned long long)bad);
				check(k1 == KERN_SUCCESS && k2 == KERN_SUCCESS && bad == 0,
				    "the engine copies the surface to VRAM and back", detail);
			}
			if (ih != 0) {
				uint64_t hh = ih;
				check(IOConnectCallScalarMethod(conn, kMinkarAMDGPUDestroyBuffer, &hh, 1, NULL, NULL) ==
				    KERN_SUCCESS, "release the surface's pages", "");
			}
			IOSurfaceUnlock(surface, 0, NULL);
			CFRelease(surface);
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
