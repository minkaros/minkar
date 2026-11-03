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
#include "MinkarAMDUserClient.h"
#include "amd_si_vm.h"
#include "MinkarAMDGPU.h"
#include "atom_glue.h"

#define super IOUserClient
OSDefineMetaClassAndStructors(MinkarAMDUserClient, IOUserClient);

bool
MinkarAMDUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
	if (!super::initWithTask(owningTask, securityID, type))
		return false;
	/* Any program: each works in its own buffers and its own GPU address
	 * space (the maintainer's choice, 2026-11-03). What reaches past them
	 * is for administrators (the kernel's GPU space, registers, the
	 * display's AUX and DDC lines, mode setting), and the display's
	 * framebuffer for programs signed with the restricted gpu-client
	 * entitlement (the window server). */
	fAdministrator = clientHasPrivilege(securityID, kIOClientPrivilegeAdministrator) ==
	    kIOReturnSuccess;
	OSObject *e = copyClientEntitlement(owningTask, kMinkarGPUClientEntitlement);
	fEntitled = fAdministrator || e == kOSBooleanTrue;
	OSSafeReleaseNULL(e);
	fTask = owningTask;
	fLock = IOLockAlloc();
	return fLock != NULL;
}

bool
MinkarAMDUserClient::start(IOService *provider)
{
	fGPU = OSDynamicCast(MinkarAMDGPU, provider);
	return fGPU != NULL && super::start(provider);
}

/* Called locked: the buffer's mappings in the client's address space. */
void
MinkarAMDUserClient::unmapAll(Buffer *b)
{
	uint32_t handle = b != NULL ? (uint32_t)(b - fBuffers) + 1 : 0;
	for (unsigned i = 0; i < fMappingCap; i++)
		if (fMappings[i].bytes != 0 && (b == NULL || fMappings[i].buffer == handle)) {
			fGPU->vmUnmap(fVMID, fMappings[i].va, fMappings[i].bytes);
			fMappings[i].bytes = 0;
		}
}

/* Called locked. */
void
MinkarAMDUserClient::destroy(Buffer *b)
{
	unmapAll(b);
	if (b->shared != 0)
		fGPU->releaseShared(b->shared);
	else if (b->system != NULL)
		fGPU->destroySystemBuffer(b->system, b->gpu);
	else if (b->imported != NULL) {
		fGPU->unmapSystemMemory(b->gpu, b->size);
		b->imported->complete();
		b->imported->release();
	} else
		fGPU->destroyBuffer(b->offset, b->size);
	b->system = NULL;
	b->imported = NULL;
	b->shared = 0;
	b->hidden = false;
	b->size = 0;
}

void
MinkarAMDUserClient::releaseAll()
{
	if (fGPU == NULL || fLock == NULL)
		return;
	IOLockLock(fLock);
	/* The display shows the framebuffer again before its buffers go. */
	if (fFlipped) {
		uint64_t ns;
		fGPU->flip(0, 0, &ns);
		fFlipped = false;
	}
	for (unsigned i = 0; i < fBufferCap; i++)
		if (fBuffers[i].size != 0)
			destroy(&fBuffers[i]);
	if (fVMID != 0) {
		fGPU->destroyVM(fVMID);
		fVMID = 0;
	}
	IOLockUnlock(fLock);
}

IOReturn
MinkarAMDUserClient::clientClose(void)
{
	releaseAll();
	terminate();
	return kIOReturnSuccess;
}

void
MinkarAMDUserClient::free()
{
	releaseAll();	/* a client that died without closing */
	if (fBuffers != NULL)
		IOFree(fBuffers, fBufferCap * sizeof(*fBuffers));
	if (fMappings != NULL)
		IOFree(fMappings, fMappingCap * sizeof(*fMappings));
	if (fLock != NULL)
		IOLockFree(fLock);
	super::free();
}

/* Handles are 1 and up; called locked. */
MinkarAMDUserClient::Buffer *
MinkarAMDUserClient::buffer(uint64_t handle)
{
	if (handle < 1 || handle > fBufferCap || fBuffers[handle - 1].size == 0)
		return NULL;
	return &fBuffers[handle - 1];
}

/* A table twice as large (64 entries at first), up to max; the entries
 * kept, the new ones zero. Called locked. */
static bool
grow(void **table, unsigned *cap, size_t entry, unsigned max)
{
	unsigned n = *cap == 0 ? 64 : *cap * 2;
	if (n > max)
		n = max;
	if (n <= *cap)
		return false;
	void *t = IOMallocZero(n * entry);
	if (t == NULL)
		return false;
	if (*table != NULL) {
		memcpy(t, *table, *cap * entry);
		IOFree(*table, *cap * entry);
	}
	*table = t;
	*cap = n;
	return true;
}

/* An empty buffer slot, the table grown if needed, or kNoSlot. Called
 * locked. */
unsigned
MinkarAMDUserClient::bufferSlot()
{
	for (unsigned i = 0; i < fBufferCap; i++)
		if (fBuffers[i].size == 0)
			return i;
	unsigned first = fBufferCap;
	return grow((void **)&fBuffers, &fBufferCap, sizeof(*fBuffers), kMinkarAMDGPUMaxBuffers) ? first :
	    kNoSlot;
}

/* The mapping table grown: its first new slot, or kNoSlot. Called locked. */
unsigned
MinkarAMDUserClient::growMappings()
{
	unsigned first = fMappingCap;
	return grow((void **)&fMappings, &fMappingCap, sizeof(*fMappings), kMinkarAMDGPUMaxMappings) ? first :
	    kNoSlot;
}

IOReturn
MinkarAMDUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	static IOExternalMethodDispatch methods[kMinkarAMDGPUMethodCount] = {
		{ (IOExternalMethodAction)&sInfo, 0, 0, 3, 0 },
		{ (IOExternalMethodAction)&sCreate, 1, 0, 2, 0 },
		{ (IOExternalMethodAction)&sDestroy, 1, 0, 0, 0 },
		{ (IOExternalMethodAction)&sCopy, 5, 0, 1, 0 },
		{ (IOExternalMethodAction)&sReadRegister, 1, 0, 1, 0 },
		{ (IOExternalMethodAction)&sCreateSystem, 1, 0, 2, 0 },
		{ (IOExternalMethodAction)&sImport, 2, 0, 2, 0 },
		{ (IOExternalMethodAction)&sCopyToScanout, 4, 0, 1, 0 },
		{ (IOExternalMethodAction)&sCreateShared, 1, 0, 3, 0 },
		{ (IOExternalMethodAction)&sRetainShared, 1, 0, 3, 0 },
		{ (IOExternalMethodAction)&sAUX, 5, 0, 6, kIOUCVariableStructureSize },
		{ (IOExternalMethodAction)&sI2C, 4, 0, 3, 0 },
		{ (IOExternalMethodAction)&sModeSet, 2, 0, 6, 0 },
		{ (IOExternalMethodAction)&sSubmitGFX, 3, 0, 1, 0 },
		{ (IOExternalMethodAction)&sMapBuffer, 4, 0, 0, 0 },
		{ (IOExternalMethodAction)&sUnmapBuffer, 1, 0, 0, 0 },
		{ (IOExternalMethodAction)&sSubmitGFXVM, 2, 0, 2, 0 },
		{ (IOExternalMethodAction)&sCreateHidden, 1, 0, 2, 0 },
		{ (IOExternalMethodAction)&sFlip, 1, 0, 1, 0 },
	};

	(void)dispatch; (void)target;
	if (selector >= kMinkarAMDGPUMethodCount)
		return kIOReturnUnsupported;
	return super::externalMethod(selector, args, &methods[selector], this, reference);
}

IOReturn
MinkarAMDUserClient::sInfo(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	uint64_t total, available, window;

	IOReturn err = me->fGPU->vramInfo(&total, &available, &window);
	if (err == kIOReturnSuccess) {
		args->scalarOutput[0] = total;
		args->scalarOutput[1] = available;
		args->scalarOutput[2] = window;
	}
	return err;
}

/* A buffer's size in whole pages, as clients map them; 0 when it is 0 or
 * too large to round. */
static uint64_t
pages(uint64_t bytes)
{
	return bytes == 0 || bytes > UINT64_MAX - 4095 ? 0 : (bytes + 4095) & ~4095ull;
}

IOReturn
MinkarAMDUserClient::sCreate(OSObject *target, void *, IOExternalMethodArguments *args)
{
	return create((MinkarAMDUserClient *)target, args, false);
}

/* size: a buffer anywhere in VRAM, which the CPU cannot map. */
IOReturn
MinkarAMDUserClient::sCreateHidden(OSObject *target, void *, IOExternalMethodArguments *args)
{
	return create((MinkarAMDUserClient *)target, args, true);
}

IOReturn
MinkarAMDUserClient::create(MinkarAMDUserClient *me, IOExternalMethodArguments *args, bool hidden)
{
	uint64_t size = pages(args->scalarInput[0]), offset;
	unsigned slot;

	if (size == 0)
		return kIOReturnBadArgument;
	IOLockLock(me->fLock);
	slot = me->bufferSlot();
	IOReturn err = slot == kNoSlot ? kIOReturnNoResources :
	    me->fGPU->createBuffer(size, &offset, !hidden);
	if (err == kIOReturnSuccess) {
		me->fBuffers[slot].offset = offset;
		me->fBuffers[slot].size = size;
		me->fBuffers[slot].hidden = hidden;
		me->fBuffers[slot].gpu = me->fGPU->gpuAddress(offset);
		me->fBuffers[slot].system = NULL;
		me->fBuffers[slot].imported = NULL;
		me->fBuffers[slot].shared = 0;
		args->scalarOutput[0] = slot + 1;
		args->scalarOutput[1] = me->fBuffers[slot].gpu;
	}
	IOLockUnlock(me->fLock);
	return err;
}

IOReturn
MinkarAMDUserClient::sDestroy(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;

	IOLockLock(me->fLock);
	Buffer *b = me->buffer(args->scalarInput[0]);
	/* The one the display shows: the framebuffer first. */
	if (b != NULL && me->fFlipped && b->offset == me->fFlipOffset) {
		uint64_t ns;
		me->fGPU->flip(0, 0, &ns);
		me->fFlipped = false;
	}
	if (b != NULL)
		me->destroy(b);
	IOLockUnlock(me->fLock);
	return b != NULL ? kIOReturnSuccess : kIOReturnBadArgument;
}

/* dst buffer, dst offset, src buffer, src offset, bytes: both ranges
 * inside their buffers. */
IOReturn
MinkarAMDUserClient::sCopy(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	const uint64_t *in = args->scalarInput;
	uint64_t bytes = in[4], ns = 0;

	IOLockLock(me->fLock);
	Buffer *d = me->buffer(in[0]), *s = me->buffer(in[2]);
	bool good = d != NULL && s != NULL && bytes > 0 &&
	    in[1] <= d->size && bytes <= d->size - in[1] &&
	    in[3] <= s->size && bytes <= s->size - in[3];
	uint64_t dst = good ? d->gpu + in[1] : 0, src = good ? s->gpu + in[3] : 0;
	/* Held over the copy: the buffers cannot go while the engine uses them. */
	IOReturn err = good ? me->fGPU->copy(dst, src, bytes, &ns) : kIOReturnBadArgument;
	IOLockUnlock(me->fLock);
	if (err == kIOReturnSuccess)
		args->scalarOutput[0] = ns;
	return err;
}

/* buffer, offset, dwords: an indirect buffer inside the buffer. */
IOReturn
MinkarAMDUserClient::sSubmitGFX(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	const uint64_t *in = args->scalarInput;
	uint64_t ns = 0;

	if (!me->fAdministrator)
		return kIOReturnNotPrivileged;
	IOLockLock(me->fLock);
	Buffer *b = me->buffer(in[0]);
	bool good = b != NULL && in[2] > 0 && in[2] <= 0xfffff && (in[1] & 3) == 0 &&
	    in[1] <= b->size && in[2] * 4 <= b->size - in[1];
	/* Held over the run: the buffer cannot go while the processor reads it. */
	IOReturn err = good ? me->fGPU->runGFX(b->gpu + in[1], (uint32_t)in[2], 0, &ns) :
	    kIOReturnBadArgument;
	IOLockUnlock(me->fLock);
	if (err == kIOReturnSuccess)
		args->scalarOutput[0] = ns;
	return err;
}

/* buffer, va, offset, bytes: that part of a buffer at va in the client's
 * address space, made at the first. */
IOReturn
MinkarAMDUserClient::sMapBuffer(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	const uint64_t *in = args->scalarInput;
	IOReturn err = kIOReturnSuccess;

	IOLockLock(me->fLock);
	Buffer *b = me->buffer(in[0]);
	uint64_t va = in[1], offset = in[2], bytes = in[3];
	/* bytes 0: the whole buffer from offset; page aligned, inside it. */
	if (b != NULL && bytes == 0 && offset <= b->size)
		bytes = b->size - offset;
	if (b == NULL || (offset & 4095) != 0 || offset > b->size || bytes > b->size - offset ||
	    !si_vm_range_ok(va, bytes))
		err = kIOReturnBadArgument;
	unsigned slot = kNoSlot;
	for (unsigned i = 0; err == kIOReturnSuccess && i < me->fMappingCap; i++) {
		Mapping *m = &me->fMappings[i];
		if (m->bytes == 0) {
			if (slot == kNoSlot)
				slot = i;
		} else if (va < m->va + m->bytes && m->va < va + bytes)
			err = kIOReturnExclusiveAccess;	/* overlaps one */
	}
	if (err == kIOReturnSuccess && slot == kNoSlot && (slot = me->growMappings()) == kNoSlot)
		err = kIOReturnNoResources;
	if (err == kIOReturnSuccess && me->fVMID == 0)
		err = me->fGPU->createVM(&me->fVMID);
	/* System and imported buffers by their pages; VRAM ones (shared ones
	 * too) by their offset. */
	if (err == kIOReturnSuccess)
		err = b->system != NULL ? me->fGPU->vmMapSystem(me->fVMID, va, b->system, offset, bytes) :
		    b->imported != NULL ? me->fGPU->vmMapSystem(me->fVMID, va, b->imported, offset, bytes) :
		    me->fGPU->vmMapVRAM(me->fVMID, va, b->offset + offset, bytes);
	if (err == kIOReturnSuccess) {
		me->fMappings[slot].va = va;
		me->fMappings[slot].bytes = bytes;
		me->fMappings[slot].buffer = (uint32_t)(b - me->fBuffers) + 1;
	}
	IOLockUnlock(me->fLock);
	return err;
}

/* va: the mapping that starts there. */
IOReturn
MinkarAMDUserClient::sUnmapBuffer(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	IOReturn err = kIOReturnNotFound;

	IOLockLock(me->fLock);
	for (unsigned i = 0; i < me->fMappingCap; i++) {
		Mapping *m = &me->fMappings[i];
		if (m->bytes != 0 && m->va == args->scalarInput[0]) {
			me->fGPU->vmUnmap(me->fVMID, m->va, m->bytes);
			m->bytes = 0;
			err = kIOReturnSuccess;
			break;
		}
	}
	IOLockUnlock(me->fLock);
	return err;
}

/* va, dwords: an indirect buffer inside one of the client's mappings, run
 * in its address space; out: nanoseconds, the protection fault status (0
 * when it touched only what it maps). */
IOReturn
MinkarAMDUserClient::sSubmitGFXVM(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	const uint64_t *in = args->scalarInput;
	uint64_t ns = 0;
	uint32_t fault = 0;
	bool inside = false;

	IOLockLock(me->fLock);
	for (unsigned i = 0; i < me->fMappingCap && !inside; i++) {
		Mapping *m = &me->fMappings[i];
		inside = m->bytes != 0 && in[1] > 0 && in[1] <= 0xfffff && (in[0] & 3) == 0 &&
		    in[0] >= m->va && in[0] - m->va < m->bytes && in[1] * 4 <= m->bytes - (in[0] - m->va);
	}
	/* Held over the run: the mapping cannot go while the processor reads it. */
	IOReturn err = inside && me->fVMID != 0 ?
	    me->fGPU->runGFX(in[0], (uint32_t)in[1], me->fVMID, &ns, &fault) : kIOReturnBadArgument;
	IOLockUnlock(me->fLock);
	if (err == kIOReturnSuccess) {
		args->scalarOutput[0] = ns;
		args->scalarOutput[1] = fault;
	}
	return err;
}

IOReturn
MinkarAMDUserClient::sReadRegister(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	uint32_t value = 0;

	if (!me->fAdministrator)
		return kIOReturnNotPrivileged;
	if (args->scalarInput[0] > UINT32_MAX)
		return kIOReturnBadArgument;
	IOReturn err = me->fGPU->readWindowRegister((uint32_t)args->scalarInput[0], &value);
	if (err == kIOReturnSuccess)
		args->scalarOutput[0] = value;
	return err;
}

IOReturn
MinkarAMDUserClient::sCreateSystem(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	IOBufferMemoryDescriptor *m = NULL;
	uint64_t gpu = 0;
	unsigned slot;

	IOLockLock(me->fLock);
	slot = me->bufferSlot();
	uint64_t size = pages(args->scalarInput[0]);
	IOReturn err = slot == kNoSlot ? kIOReturnNoResources :
	    size == 0 ? kIOReturnBadArgument : me->fGPU->createSystemBuffer(size, &m, &gpu);
	if (err == kIOReturnSuccess) {
		me->fBuffers[slot].offset = 0;
		me->fBuffers[slot].size = m->getLength();
		me->fBuffers[slot].gpu = gpu;
		me->fBuffers[slot].system = m;
		me->fBuffers[slot].imported = NULL;
		me->fBuffers[slot].shared = 0;
		args->scalarOutput[0] = slot + 1;
		args->scalarOutput[1] = gpu;
	}
	IOLockUnlock(me->fLock);
	return err;
}

/* The client's own memory into the GART: page-aligned, wired for the
 * buffer's life. */
IOReturn
MinkarAMDUserClient::sImport(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	mach_vm_address_t address = args->scalarInput[0];
	mach_vm_size_t size = args->scalarInput[1];
	uint64_t gpu = 0;
	unsigned slot;

	if (size == 0 || (address & 4095) != 0 || (size & 4095) != 0 || address + size < address)
		return kIOReturnBadArgument;
	IOMemoryDescriptor *m = IOMemoryDescriptor::withAddressRange(address, size,
	    kIODirectionInOut, me->fTask);
	if (m == NULL)
		return kIOReturnNoMemory;
	if (m->prepare() != kIOReturnSuccess) {
		m->release();
		return kIOReturnVMError;
	}
	IOLockLock(me->fLock);
	slot = me->bufferSlot();
	IOReturn err = slot == kNoSlot ? kIOReturnNoResources :
	    me->fGPU->mapSystemMemory(m, &gpu);
	if (err == kIOReturnSuccess) {
		me->fBuffers[slot].offset = 0;
		me->fBuffers[slot].size = size;
		me->fBuffers[slot].gpu = gpu;
		me->fBuffers[slot].system = NULL;
		me->fBuffers[slot].imported = m;
		me->fBuffers[slot].shared = 0;
		args->scalarOutput[0] = slot + 1;
		args->scalarOutput[1] = gpu;
	}
	IOLockUnlock(me->fLock);
	if (err != kIOReturnSuccess) {
		m->complete();
		m->release();
	}
	return err;
}

/* buffer (0: the framebuffer again). */
IOReturn
MinkarAMDUserClient::sFlip(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	uint64_t ns = 0;
	IOReturn err;

	if (!me->fEntitled)
		return kIOReturnNotPrivileged;
	IOLockLock(me->fLock);
	if (args->scalarInput[0] == 0) {
		err = me->fGPU->flip(0, 0, &ns);
		if (err == kIOReturnSuccess)
			me->fFlipped = false;
	} else {
		Buffer *b = me->buffer(args->scalarInput[0]);
		/* VRAM only: neither system memory nor another task's pages. */
		err = b == NULL || b->system != NULL || b->imported != NULL ? kIOReturnBadArgument :
		    me->fGPU->flip(b->offset, b->size, &ns);
		if (err == kIOReturnSuccess) {
			me->fFlipped = true;
			me->fFlipOffset = b->offset;
		}
	}
	IOLockUnlock(me->fLock);
	args->scalarOutput[0] = ns;
	return err;
}

/* source buffer, source offset, framebuffer offset, bytes. */
IOReturn
MinkarAMDUserClient::sCopyToScanout(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	const uint64_t *in = args->scalarInput;
	uint64_t ns = 0;

	if (!me->fEntitled)
		return kIOReturnNotPrivileged;

	IOLockLock(me->fLock);
	Buffer *s = me->buffer(in[0]);
	bool good = s != NULL && in[3] > 0 && in[1] <= s->size && in[3] <= s->size - in[1];
	IOReturn err = good ? me->fGPU->copyToScanout(s->gpu + in[1], in[2], in[3], &ns) :
	    kIOReturnBadArgument;
	IOLockUnlock(me->fLock);
	if (err == kIOReturnSuccess)
		args->scalarOutput[0] = ns;
	return err;
}

/* A shared VRAM buffer, held by this client. */
IOReturn
MinkarAMDUserClient::sCreateShared(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	uint64_t offset = 0, size = 0;
	uint32_t id = 0;
	unsigned slot;

	IOLockLock(me->fLock);
	slot = me->bufferSlot();
	IOReturn err = slot == kNoSlot ? kIOReturnNoResources :
	    me->fGPU->createShared(args->scalarInput[0], &id, &offset, &size);
	if (err == kIOReturnSuccess) {
		Buffer *b = &me->fBuffers[slot];
		b->offset = offset;
		b->size = size;
		b->gpu = me->fGPU->gpuAddress(offset);
		b->system = NULL;
		b->imported = NULL;
		b->shared = id;
		args->scalarOutput[0] = slot + 1;
		args->scalarOutput[1] = b->gpu;
		args->scalarOutput[2] = id;
	}
	IOLockUnlock(me->fLock);
	return err;
}

/* Another client's shared buffer, by its global ID: held by this one too. */
IOReturn
MinkarAMDUserClient::sRetainShared(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;
	uint64_t offset = 0, size = 0;
	unsigned slot;

	if (args->scalarInput[0] == 0 || args->scalarInput[0] > UINT32_MAX)
		return kIOReturnBadArgument;
	IOLockLock(me->fLock);
	slot = me->bufferSlot();
	IOReturn err = slot == kNoSlot ? kIOReturnNoResources :
	    me->fGPU->retainShared((uint32_t)args->scalarInput[0], &offset, &size);
	if (err == kIOReturnSuccess) {
		Buffer *b = &me->fBuffers[slot];
		b->offset = offset;
		b->size = size;
		b->gpu = me->fGPU->gpuAddress(offset);
		b->system = NULL;
		b->imported = NULL;
		b->shared = (uint32_t)args->scalarInput[0];
		args->scalarOutput[0] = slot + 1;
		args->scalarOutput[1] = b->gpu;
		args->scalarOutput[2] = size;
	}
	IOLockUnlock(me->fLock);
	return err;
}

IOReturn
MinkarAMDUserClient::sAUX(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;

	/* Administrators only: it drives the display's link. */
	if (!me->fAdministrator)
		return kIOReturnNotPrivileged;
	uint32_t *trace = (uint32_t *)args->structureOutput;
	uint32_t bytes = args->structureOutputSize;
	if (bytes > MINKAR_ATOM_TRACE_MAX * 8)
		bytes = MINKAR_ATOM_TRACE_MAX * 8;
	IOReturn err = me->fGPU->auxTransaction(args->scalarInput, args->scalarOutput,
	    bytes >= 8 ? trace : NULL, &bytes);
	args->structureOutputSize = bytes >= 8 ? bytes : 0;
	return err;
}

IOReturn
MinkarAMDUserClient::sI2C(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;

	if (!me->fAdministrator)
		return kIOReturnNotPrivileged;
	return me->fGPU->i2cRead(args->scalarInput, args->scalarOutput);
}

IOReturn
MinkarAMDUserClient::sModeSet(OSObject *target, void *, IOExternalMethodArguments *args)
{
	MinkarAMDUserClient *me = (MinkarAMDUserClient *)target;

	if (!me->fAdministrator)
		return kIOReturnNotPrivileged;
	return me->fGPU->modeSet(args->scalarInput, args->scalarOutput);
}

/* A buffer mapped into the client, the memory type being the buffer:
 * VRAM through the window, write-combined; system memory cached. */
IOReturn
MinkarAMDUserClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
    IOMemoryDescriptor **memory)
{
	IOMemoryDescriptor *m = NULL;

	IOLockLock(fLock);
	Buffer *b = buffer(type);
	if (b != NULL && b->imported != NULL)
		m = NULL;	/* the client has it already */
	else if (b != NULL && b->system != NULL) {
		m = b->system;
		m->retain();
		*options = kIOMapDefaultCache;
	} else if (b != NULL && !b->hidden) {
		m = fGPU->bufferMemory(b->offset, b->size);
		*options = kIOMapWriteCombineCache;
	}
	IOLockUnlock(fLock);
	if (m == NULL)
		return kIOReturnBadArgument;
	*memory = m;	/* the caller releases it */
	return kIOReturnSuccess;
}
