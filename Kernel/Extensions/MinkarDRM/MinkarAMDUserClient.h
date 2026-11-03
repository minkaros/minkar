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
 * MinkarAMDUserClient (ADR 0002, G2): a program's way to an AMD GPU's
 * VRAM and DMA engine (MinkarAMDGPUInterface.h). Each client's buffers
 * are its own and freed when it goes.
 */

#ifndef _MINKARAMDUSERCLIENT_H
#define _MINKARAMDUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "MinkarAMDGPUInterface.h"

class MinkarAMDGPU;

class MinkarAMDUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(MinkarAMDUserClient);

public:
	virtual bool initWithTask(task_t owningTask, void *securityID, UInt32 type) APPLE_KEXT_OVERRIDE;
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) APPLE_KEXT_OVERRIDE;
	virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
	    IOMemoryDescriptor **memory) APPLE_KEXT_OVERRIDE;

private:
	struct Buffer {
		uint64_t offset, size;	/* VRAM offset; size 0: the slot is empty */
		uint64_t gpu;		/* its GPU address */
		IOBufferMemoryDescriptor *system;	/* a system buffer's memory */
		IOMemoryDescriptor *imported;		/* the client's own, wired */
		uint32_t shared;			/* a shared buffer's global ID */
	};

	static IOReturn sInfo(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sCreate(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sDestroy(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sCopy(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sReadRegister(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sCreateSystem(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sImport(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sCopyToScanout(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sCreateShared(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sRetainShared(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sAUX(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sI2C(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sModeSet(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sSubmitGFX(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sMapBuffer(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sUnmapBuffer(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn sSubmitGFXVM(OSObject *target, void *reference, IOExternalMethodArguments *args);
	/* A buffer at an address of the client's GPU address space. */
	struct Mapping {
		uint64_t va, bytes;	/* bytes 0: the slot is empty */
		Buffer *buffer;
	};

	void destroy(Buffer *b);
	void unmapAll(Buffer *b);
	Buffer *buffer(uint64_t handle);
	void releaseAll();

	MinkarAMDGPU *		fGPU;
	task_t			fTask;		/* the client's, for ImportMemory */
	bool			fAdministrator;
	bool			fEntitled;	/* gpu-client, or an administrator */
	IOLock *		fLock;
	Buffer			fBuffers[kMinkarAMDGPUMaxBuffers];
	unsigned		fVMID;		/* its address space's context, or 0 */
	Mapping			fMappings[kMinkarAMDGPUMaxMappings];
};

#endif
