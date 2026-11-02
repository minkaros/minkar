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
 * MinkarAMDGPU's user client (ADR 0002, G2): what a program asks of an AMD
 * GPU through IOServiceOpen and IOConnectCall*. Shared by the driver and
 * its tools; the selectors and their scalars are the interface. For
 * administrators, and for programs signed with the org.minkar.gpu-client
 * entitlement (restricted: in the trust cache only), the window server.
 *
 *   Info		out: VRAM, free VRAM, the CPU's window on it (bytes)
 *   CreateBuffer	in: size; out: buffer (1 and up), its GPU address
 *			A buffer in VRAM the CPU can reach: IOConnectMapMemory64
 *			with the buffer as the memory type maps it.
 *   DestroyBuffer	in: buffer
 *   Copy		in: destination buffer and offset, source buffer and
 *			offset, bytes; out: nanoseconds from the submission
 *			to the fence. The DMA engine copies; the call returns
 *			once the fence has signalled.
 *   CreateSystemBuffer	in: size; out: buffer, its GPU address. A buffer in
 *			system memory the GPU reaches through the GART (when
 *			the driver turned it on: minkaramd_gart); mapped the
 *			same way, cached.
 *   ImportMemory	in: address, size (page-aligned, the caller's own
 *			memory, an IOSurface's for one); out: buffer, its GPU
 *			address. The pages are wired and put in the GART while
 *			the buffer lives; DestroyBuffer gives them back. It is
 *			not mapped again (the caller has it).
 *   CopyToScanout	in: source buffer and offset, offset into the
 *			framebuffer the display shows, bytes; out: nanoseconds.
 *			The DMA engine copies into the scanout (the window
 *			server's present); the range must lie in it.
 *   CreateSharedBuffer	in: size; out: buffer, its GPU address, its global ID.
 *			A VRAM buffer other clients may hold too: its VRAM is
 *			freed when the last holder lets it go (IOSurfaces in
 *			VRAM: iosurfaced holds one for each).
 *   RetainSharedBuffer	in: global ID; out: buffer, its GPU address, size.
 *   AUXTransaction	in: AUX channel, HPD (0-5), request, address, size;
 *			out: bytes or a negative errno, the reply byte, the
 *			interpreter's result, the data length, the first 8
 *			and next 8 bytes read. For bring-up (administrators).
 *			The structure output, if given, gets the registers the
 *			table read and wrote: pairs of (reg | write << 31, value).
 *   I2CRead		in: DDC line, slave (7-bit), offset, count (<= 16);
 *			out: 0 or a negative errno, the first 8 and next 8
 *			bytes. Writes the offset, then reads. For bring-up.
 *   ReadRegister	in: dword index; out: its value. For bring-up: the
 *			register window only, reading only (no indexed
 *			registers, which take two writes).
 *
 * A client's buffers are freed when it closes or exits.
 */

#ifndef _MINKARAMDGPUINTERFACE_H
#define _MINKARAMDGPUINTERFACE_H

enum {
	kMinkarAMDGPUInfo,
	kMinkarAMDGPUCreateBuffer,
	kMinkarAMDGPUDestroyBuffer,
	kMinkarAMDGPUCopy,
	kMinkarAMDGPUReadRegister,
	kMinkarAMDGPUCreateSystemBuffer,
	kMinkarAMDGPUImportMemory,
	kMinkarAMDGPUCopyToScanout,
	kMinkarAMDGPUCreateSharedBuffer,
	kMinkarAMDGPURetainSharedBuffer,
	kMinkarAMDGPUAUXTransaction,
	kMinkarAMDGPUI2CRead,
	kMinkarAMDGPUMethodCount
};

/* The entitlement that opens the user client without administrator
 * rights. */
#define kMinkarGPUClientEntitlement	"org.minkar.gpu-client"

/* The most buffers a client holds at once. */
#define kMinkarAMDGPUMaxBuffers	64

#endif
