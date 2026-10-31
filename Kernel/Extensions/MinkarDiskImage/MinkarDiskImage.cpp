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
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/mount.h>

#include "MinkarDiskImage.h"

#define LOG(fmt, ...)	IOLog("MinkarDiskImage: " fmt "\n", ##__VA_ARGS__)

static const UInt32 kBlock = 512;
static const size_t kChunk = 128 * 1024;	/* bytes per file transfer */

/* ---- The controller ------------------------------------------------------- */

OSDefineMetaClassAndStructors(MinkarDiskImageController, IOService);

bool
MinkarDiskImageController::start(IOService *provider)
{
	if (!IOService::start(provider))
		return false;
	registerService();
	return true;
}

/* ---- A disk image ------------------------------------------------------- */

#undef super
#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(MinkarDiskImageDevice, IOBlockStorageDevice);

bool
MinkarDiskImageDevice::initWithVnode(vnode_t vp, int mode, UInt64 offset,
    UInt64 size, bool readOnly, const char *path)
{
	OSDictionary *dc, *pc;
	const OSSymbol *s;

	if (!super::init(NULL))
		return false;
	fLock = IOLockAlloc();
	if (fLock == NULL)
		return false;
	fVnode = vp;
	fOpenMode = mode;
	fOffset = offset;
	fSize = size;
	fReadOnly = readOnly;
	strlcpy(fProduct, "Disk Image", sizeof(fProduct));
	setProperty("image-path", path);
	setProperty("image-offset", offset, 64);
	setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeGeneric);

	/* What macOS's disk images say of themselves. */
	dc = OSDictionary::withCapacity(3);
	pc = OSDictionary::withCapacity(2);
	if (dc != NULL) {
		s = OSSymbol::withCString("Minkar");
		dc->setObject(kIOPropertyVendorNameKey, s);
		s->release();
		s = OSSymbol::withCString(fProduct);
		dc->setObject(kIOPropertyProductNameKey, s);
		s->release();
		s = OSSymbol::withCString("1.0");
		dc->setObject(kIOPropertyProductRevisionLevelKey, s);
		s->release();
		setProperty(kIOPropertyDeviceCharacteristicsKey, dc);
		dc->release();
	}
	if (pc != NULL) {
		s = OSSymbol::withCString(kIOPropertyPhysicalInterconnectTypeVirtual);
		pc->setObject(kIOPropertyPhysicalInterconnectTypeKey, s);
		s->release();
		s = OSSymbol::withCString(kIOPropertyInterconnectFileKey);
		pc->setObject(kIOPropertyPhysicalInterconnectLocationKey, s);
		s->release();
		setProperty(kIOPropertyProtocolCharacteristicsKey, pc);
		pc->release();
	}
	return true;
}

bool
MinkarDiskImageDevice::start(IOService *provider)
{
	if (!super::start(provider))
		return false;
	if (kernel_thread_start(worker, this, &fThread) != KERN_SUCCESS)
		return false;
	registerService();
	return true;
}

/* Ends the worker once its queue is empty; the file is closed after. */
void
MinkarDiskImageDevice::stop(IOService *provider)
{
	IOLockLock(fLock);
	fStopping = true;
	IOLockWakeup(fLock, &fHead, false);
	while (!fStopped)
		IOLockSleep(fLock, &fStopped, THREAD_UNINT);
	IOLockUnlock(fLock);
	if (fThread != NULL) {
		thread_deallocate(fThread);
		fThread = NULL;
	}
	if (fVnode != NULL) {
		vfs_context_t ctx = vfs_context_create(NULL);

		vnode_close(fVnode, fOpenMode, ctx);
		vfs_context_rele(ctx);
		fVnode = NULL;
	}
	super::stop(provider);
}

void
MinkarDiskImageDevice::free()
{
	if (fVnode != NULL) {		/* never started */
		vfs_context_t ctx = vfs_context_create(NULL);

		vnode_close(fVnode, fOpenMode, ctx);
		vfs_context_rele(ctx);
		fVnode = NULL;
	}
	if (fLock != NULL)
		IOLockFree(fLock);
	super::free();
}

void
MinkarDiskImageDevice::worker(void *arg, wait_result_t)
{
	((MinkarDiskImageDevice *)arg)->serve();
	thread_terminate(current_thread());
}

void
MinkarDiskImageDevice::serve()
{
	vfs_context_t ctx = vfs_context_create(NULL);	/* the kernel's */

	IOLockLock(fLock);
	for (;;) {
		Request *req;
		UInt64 done = 0;
		IOReturn status;

		while (fHead == NULL && !fStopping)
			IOLockSleep(fLock, &fHead, THREAD_UNINT);
		if (fHead == NULL)
			break;
		req = fHead;
		fHead = req->next;
		if (fHead == NULL)
			fTail = NULL;
		IOLockUnlock(fLock);

		status = transfer(req, &done, ctx);
		req->buffer->complete();
		req->buffer->release();
		IOStorage::complete(&req->completion, status, done);
		IODelete(req, Request, 1);

		IOLockLock(fLock);
	}
	fStopped = true;
	IOLockWakeup(fLock, &fStopped, false);
	IOLockUnlock(fLock);
	vfs_context_rele(ctx);
}

/* One request, through a bounce buffer, a chunk at a time. */
IOReturn
MinkarDiskImageDevice::transfer(Request *req, UInt64 *done, vfs_context_t ctx)
{
	bool write = req->buffer->getDirection() == kIODirectionOut;
	UInt64 bytes = req->nblks * kBlock, pos = 0;
	IOReturn status = kIOReturnSuccess;
	char *bounce;

	if (write && fReadOnly)
		return kIOReturnNotWritable;
	bounce = (char *)IOMalloc(kChunk);
	if (bounce == NULL)
		return kIOReturnNoMemory;
	if (vnode_getwithref(fVnode) != 0) {
		IOFree(bounce, kChunk);
		return kIOReturnNoDevice;
	}
	while (pos < bytes && status == kIOReturnSuccess) {
		size_t n = (size_t)min(bytes - pos, (UInt64)kChunk);
		off_t off = (off_t)(fOffset + req->block * kBlock + pos);
		uio_t uio = uio_create(1, off, UIO_SYSSPACE,
		    write ? UIO_WRITE : UIO_READ);
		int error;

		if (uio == NULL) {
			status = kIOReturnNoMemory;
			break;
		}
		if (write && req->buffer->readBytes(pos, bounce, n) != n) {
			uio_free(uio);
			status = kIOReturnIOError;
			break;
		}
		uio_addiov(uio, CAST_USER_ADDR_T(bounce), n);
		error = write ? VNOP_WRITE(fVnode, uio, 0, ctx) :
		    VNOP_READ(fVnode, uio, 0, ctx);
		if (error != 0) {
			status = kIOReturnIOError;
		} else if (!write) {
			size_t got = n - (size_t)uio_resid(uio);

			/* Past the file's end (a sparse tail) reads as zeros. */
			if (got < n)
				bzero(bounce + got, n - got);
			req->buffer->writeBytes(pos, bounce, n);
		} else if (uio_resid(uio) != 0) {
			status = kIOReturnIOError;
		}
		uio_free(uio);
		if (status == kIOReturnSuccess)
			pos += n;
	}
	vnode_put(fVnode);
	IOFree(bounce, kChunk);
	*done = pos;
	return status;
}

IOReturn
MinkarDiskImageDevice::doAsyncReadWrite(IOMemoryDescriptor *buffer,
    UInt64 block, UInt64 nblks, IOStorageAttributes *attributes,
    IOStorageCompletion *completion)
{
	Request *req;

	(void)attributes;
	if (block + nblks > fSize / kBlock || nblks == 0)
		return kIOReturnBadArgument;
	req = IONew(Request, 1);
	if (req == NULL)
		return kIOReturnNoMemory;
	bzero(req, sizeof(*req));
	if (buffer->prepare() != kIOReturnSuccess) {
		IODelete(req, Request, 1);
		return kIOReturnNoMemory;
	}
	buffer->retain();
	req->buffer = buffer;
	req->block = block;
	req->nblks = nblks;
	req->completion = *completion;

	IOLockLock(fLock);
	if (fStopping) {
		IOLockUnlock(fLock);
		buffer->complete();
		buffer->release();
		IODelete(req, Request, 1);
		return kIOReturnNoDevice;
	}
	if (fTail != NULL)
		fTail->next = req;
	else
		fHead = req;
	fTail = req;
	IOLockWakeup(fLock, &fHead, true);
	IOLockUnlock(fLock);
	return kIOReturnSuccess;
}

IOReturn
MinkarDiskImageDevice::doSynchronize(UInt64 block, UInt64 nblks,
    IOStorageSynchronizeOptions options)
{
	vfs_context_t ctx;
	int error;

	(void)block; (void)nblks; (void)options;
	if (fReadOnly)
		return kIOReturnSuccess;
	if (vnode_getwithref(fVnode) != 0)
		return kIOReturnNoDevice;
	ctx = vfs_context_create(NULL);
	error = VNOP_FSYNC(fVnode, MNT_WAIT, ctx);
	vfs_context_rele(ctx);
	vnode_put(fVnode);
	return error == 0 ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn MinkarDiskImageDevice::doEjectMedia(void) { return kIOReturnSuccess; }
IOReturn MinkarDiskImageDevice::doFormatMedia(UInt64) { return kIOReturnUnsupported; }

UInt32
MinkarDiskImageDevice::doGetFormatCapacities(UInt64 *capacities,
    UInt32 capacitiesMaxCount) const
{
	if (capacities != NULL && capacitiesMaxCount > 0)
		capacities[0] = fSize;
	return 1;
}

char *
MinkarDiskImageDevice::getVendorString(void)
{
	static char vendor[] = "Minkar";

	return vendor;
}

char *MinkarDiskImageDevice::getProductString(void) { return fProduct; }

char *
MinkarDiskImageDevice::getRevisionString(void)
{
	static char revision[] = "1.0";

	return revision;
}

char *MinkarDiskImageDevice::getAdditionalDeviceInfoString(void) { return NULL; }

IOReturn
MinkarDiskImageDevice::reportBlockSize(UInt64 *blockSize)
{
	*blockSize = kBlock;
	return kIOReturnSuccess;
}

IOReturn
MinkarDiskImageDevice::reportEjectability(bool *isEjectable)
{
	*isEjectable = true;
	return kIOReturnSuccess;
}

IOReturn
MinkarDiskImageDevice::reportMaxValidBlock(UInt64 *maxBlock)
{
	*maxBlock = fSize / kBlock - 1;
	return kIOReturnSuccess;
}

IOReturn
MinkarDiskImageDevice::reportMediaState(bool *mediaPresent, bool *changedState)
{
	*mediaPresent = true;
	if (changedState != NULL)
		*changedState = false;
	return kIOReturnSuccess;
}

IOReturn
MinkarDiskImageDevice::reportRemovability(bool *isRemovable)
{
	*isRemovable = true;
	return kIOReturnSuccess;
}

IOReturn
MinkarDiskImageDevice::reportWriteProtection(bool *isWriteProtected)
{
	*isWriteProtected = fReadOnly;
	return kIOReturnSuccess;
}

/* ---- The user client ------------------------------------------------------ */

#undef super
#define super IOUserClient
OSDefineMetaClassAndStructors(MinkarDiskImageUserClient, IOUserClient);

bool
MinkarDiskImageUserClient::initWithTask(task_t owningTask, void *securityID,
    UInt32 type)
{
	if (!super::initWithTask(owningTask, securityID, type))
		return false;
	/* Attaching opens files as the kernel serves them: administrators only. */
	return clientHasPrivilege(securityID, kIOClientPrivilegeAdministrator) ==
	    kIOReturnSuccess;
}

bool
MinkarDiskImageUserClient::start(IOService *provider)
{
	fController = OSDynamicCast(MinkarDiskImageController, provider);
	return fController != NULL && super::start(provider);
}

IOReturn
MinkarDiskImageUserClient::clientClose(void)
{
	terminate();
	return kIOReturnSuccess;
}

IOReturn
MinkarDiskImageUserClient::externalMethod(uint32_t selector,
    IOExternalMethodArguments *args, IOExternalMethodDispatch *dispatch,
    OSObject *target, void *reference)
{
	static IOExternalMethodDispatch methods[kMinkarDiskImageMethodCount] = {
		{ (IOExternalMethodAction)&sAttach, 3, kIOUCVariableStructureSize,
		    1, 0 },
		{ (IOExternalMethodAction)&sDetach, 1, 0, 0, 0 },
	};

	(void)dispatch; (void)target;
	if (selector >= kMinkarDiskImageMethodCount)
		return kIOReturnUnsupported;
	return super::externalMethod(selector, args, &methods[selector], this,
	    reference);
}

IOReturn
MinkarDiskImageUserClient::sAttach(OSObject *target, void *,
    IOExternalMethodArguments *args)
{
	MinkarDiskImageUserClient *me = (MinkarDiskImageUserClient *)target;
	char path[MAXPATHLEN];
	size_t n = args->structureInputSize;

	if (args->structureInput == NULL || n == 0 || n > sizeof(path))
		return kIOReturnBadArgument;
	memcpy(path, args->structureInput, n);
	path[n - 1] = '\0';
	return me->attachImage(path, args->scalarInput[0], args->scalarInput[1],
	    args->scalarInput[2], &args->scalarOutput[0]);
}

IOReturn
MinkarDiskImageUserClient::sDetach(OSObject *target, void *,
    IOExternalMethodArguments *args)
{
	return ((MinkarDiskImageUserClient *)target)->detachImage(
	    args->scalarInput[0]);
}

/* Opened as the caller (its rights on the file), served by the kernel. */
IOReturn
MinkarDiskImageUserClient::attachImage(const char *path, UInt64 offset,
    UInt64 length, UInt64 flags, UInt64 *id)
{
	bool readOnly = (flags & kMinkarDiskImageReadOnly) != 0;
	int mode = readOnly ? FREAD : (FREAD | FWRITE);
	vfs_context_t ctx = vfs_context_create(NULL);
	struct vnode_attr va;
	MinkarDiskImageDevice *dev;
	vnode_t vp = NULL;
	UInt64 size;
	int error;

	if (path[0] != '/') {
		vfs_context_rele(ctx);
		return kIOReturnBadArgument;
	}
	error = vnode_open(path, mode, 0, 0, &vp, ctx);
	if (error != 0) {
		vfs_context_rele(ctx);
		return error == EACCES || error == EPERM ? kIOReturnNotPrivileged :
		    error == EROFS ? kIOReturnNotWritable : kIOReturnNotFound;
	}
	VATTR_INIT(&va);
	VATTR_WANTED(&va, va_data_size);
	if (!vnode_isreg(vp) || vnode_getattr(vp, &va, ctx) != 0 ||
	    !VATTR_IS_SUPPORTED(&va, va_data_size)) {
		vnode_close(vp, mode, ctx);
		vfs_context_rele(ctx);
		return kIOReturnBadArgument;
	}
	size = va.va_data_size;
	if (offset > size) {
		vnode_close(vp, mode, ctx);
		vfs_context_rele(ctx);
		return kIOReturnBadArgument;
	}
	if (length == 0 || offset + length > size)
		length = size - offset;
	length -= length % kBlock;
	if (length == 0) {
		vnode_close(vp, mode, ctx);
		vfs_context_rele(ctx);
		return kIOReturnBadArgument;
	}
	vfs_context_rele(ctx);

	dev = OSTypeAlloc(MinkarDiskImageDevice);
	if (dev == NULL || !dev->initWithVnode(vp, mode, offset, length,
	    readOnly, path)) {
		if (dev != NULL)
			dev->release();		/* closes the file */
		else {
			ctx = vfs_context_create(NULL);
			vnode_close(vp, mode, ctx);
			vfs_context_rele(ctx);
		}
		return kIOReturnNoMemory;
	}
	if (!dev->attach(fController)) {
		dev->release();
		return kIOReturnError;
	}
	if (!dev->start(fController)) {
		dev->detach(fController);
		dev->release();
		return kIOReturnError;
	}
	*id = dev->getRegistryEntryID();
	LOG("%s attached (%llu bytes%s)", path, length, readOnly ?
	    ", read-only" : "");
	dev->release();
	return kIOReturnSuccess;
}

/* Refused while something has one of its media open (a mounted volume). */
IOReturn
MinkarDiskImageUserClient::detachImage(UInt64 id)
{
	OSDictionary *match = IOService::registryEntryIDMatching(id);
	IOService *svc = match ? IOService::copyMatchingService(match) : NULL;
	MinkarDiskImageDevice *dev = OSDynamicCast(MinkarDiskImageDevice, svc);
	IORegistryIterator *it;
	OSObject *o;
	bool busy = false;

	if (match != NULL)
		match->release();
	if (dev == NULL) {
		if (svc != NULL)
			svc->release();
		return kIOReturnNotFound;
	}
	it = IORegistryIterator::iterateOver(dev, gIOServicePlane,
	    kIORegistryIterateRecursively);
	while (it != NULL && (o = it->getNextObject()) != NULL) {
		IOMedia *m = OSDynamicCast(IOMedia, o);
		OSIterator *kids;
		OSObject *k;
		bool scheme = false;

		if (m == NULL || !m->isOpen())
			continue;
		/* A partition scheme keeps its whole medium open: not a user. */
		kids = m->getClientIterator();
		while (kids != NULL && (k = kids->getNextObject()) != NULL)
			if (OSDynamicCast(IOStorage, k) != NULL)
				scheme = true;
		if (kids != NULL)
			kids->release();
		if (!scheme)
			busy = true;
	}
	if (it != NULL)
		it->release();
	if (busy) {
		dev->release();
		return kIOReturnBusy;
	}
	dev->terminate(kIOServiceSynchronous);
	dev->release();
	return kIOReturnSuccess;
}
