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
 * administrators only, for now.
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
	kMinkarAMDGPUMethodCount
};

/* The most buffers a client holds at once. */
#define kMinkarAMDGPUMaxBuffers	64

#endif
