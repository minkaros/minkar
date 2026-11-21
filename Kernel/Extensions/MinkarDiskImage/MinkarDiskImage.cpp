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
	for (unsigned i = 0; i < kCacheSlots; i++)
		fCache[i].chunk = ~0U;
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
	if (fChunks != NULL)
		IOFree(fChunks, sizeof(*fChunks) * fChunkCount);
	for (unsigned i = 0; i < kCacheSlots; i++)
		if (fCache[i].data != NULL)
			IOFree(fCache[i].data, fCache[i].size);
	super::free();
}

bool
MinkarDiskImageDevice::setChunks(const MinkarDiskImageChunk *chunks, UInt32 count)
{
	fChunks = (MinkarDiskImageChunk *)IOMalloc(sizeof(*chunks) * count);
	if (fChunks == NULL)
		return false;
	memcpy(fChunks, chunks, sizeof(*chunks) * count);
	fChunkCount = count;
	strlcpy(fProduct, "Compressed Image", sizeof(fProduct));
	return true;
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
	if (fChunks != NULL)
		return transferChunks(req, done, ctx);
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

/* ---- A compressed image: its chunks ------------------------------------ */

/* The biggest chunk taken, compressed or not: UDIF's are 1 MB at most in
 * practice; what is past this is refused, not allocated. */
static const UInt64 kMaxChunkBytes = 64ULL * 1024 * 1024;
static const UInt8 kZeros[kBlock] = { 0 };

/* n bytes of the image file at offset (all of them, or an error). */
IOReturn
MinkarDiskImageDevice::readFile(UInt64 offset, void *buf, size_t n, vfs_context_t ctx)
{
	uio_t uio = uio_create(1, (off_t)offset, UIO_SYSSPACE, UIO_READ);
	int error;

	if (uio == NULL)
		return kIOReturnNoMemory;
	uio_addiov(uio, CAST_USER_ADDR_T(buf), n);
	error = VNOP_READ(fVnode, uio, 0, ctx);
	bool whole = error == 0 && uio_resid(uio) == 0;
	uio_free(uio);
	return whole ? kIOReturnSuccess : kIOReturnIOError;
}

/* A compressed chunk's sectors: from the cache, or read and decompressed
 * into the least recently used slot. */
const UInt8 *
MinkarDiskImageDevice::chunkData(UInt32 index, vfs_context_t ctx, IOReturn *status)
{
	const MinkarDiskImageChunk *c = &fChunks[index];
	unsigned slot = 0;

	for (unsigned i = 0; i < kCacheSlots; i++) {
		if (fCache[i].chunk == index) {
			fCache[i].used = ++fUseClock;
			return fCache[i].data;
		}
		if (fCache[i].used < fCache[slot].used)
			slot = i;
	}
	size_t outLen = (size_t)(c->sectors * kBlock), inLen = (size_t)c->length;
	UInt8 *in = (UInt8 *)IOMalloc(inLen);
	UInt8 *out = (UInt8 *)IOMalloc(outLen);
	*status = kIOReturnNoMemory;
	if (in != NULL && out != NULL) {
		*status = readFile(c->offset, in, inLen, ctx);
		if (*status == kIOReturnSuccess &&
		    MinkarDiskImageDecompress(c->type, in, inLen, out, outLen) != 0) {
			LOG("chunk %u (type 0x%x, sectors %llu to %llu) does not decompress", index, c->type,
			    c->sector, c->sector + c->sectors);
			*status = kIOReturnIOError;
		}
	}
	if (in != NULL)
		IOFree(in, inLen);
	if (*status != kIOReturnSuccess) {
		if (out != NULL)
			IOFree(out, outLen);
		return NULL;
	}
	if (fCache[slot].data != NULL)
		IOFree(fCache[slot].data, fCache[slot].size);
	fCache[slot].chunk = index;
	fCache[slot].data = out;
	fCache[slot].size = outLen;
	fCache[slot].used = ++fUseClock;
	return out;
}

/* A read of a compressed image, run by run of the chunks it covers. */
IOReturn
MinkarDiskImageDevice::transferChunks(Request *req, UInt64 *done, vfs_context_t ctx)
{
	UInt64 sector = req->block, end = req->block + req->nblks, pos = 0;
	IOReturn status = kIOReturnSuccess;
	char *bounce = NULL;

	if (req->buffer->getDirection() == kIODirectionOut)
		return kIOReturnNotWritable;
	if (vnode_getwithref(fVnode) != 0)
		return kIOReturnNoDevice;
	while (sector < end && status == kIOReturnSuccess) {
		/* The chunk holding sector: the last one starting at or before. */
		UInt32 lo = 0, hi = fChunkCount;
		while (hi - lo > 1) {
			UInt32 mid = (lo + hi) / 2;
			if (fChunks[mid].sector <= sector)
				lo = mid;
			else
				hi = mid;
		}
		const MinkarDiskImageChunk *c = &fChunks[lo];
		if (sector < c->sector || sector >= c->sector + c->sectors) {
			status = kIOReturnIOError;
			break;
		}
		UInt64 n = min(end, c->sector + c->sectors) - sector;
		UInt64 skip = (sector - c->sector) * kBlock;
		switch (c->type) {
		case kMinkarDIChunkZero:
		case kMinkarDIChunkIgnore:
			for (UInt64 i = 0; i < n; i++)
				req->buffer->writeBytes(pos + i * kBlock, kZeros, kBlock);
			break;
		case kMinkarDIChunkRaw:
			if (bounce == NULL && (bounce = (char *)IOMalloc(kChunk)) == NULL) {
				status = kIOReturnNoMemory;
				break;
			}
			for (UInt64 off = 0; off < n * kBlock && status == kIOReturnSuccess; off += kChunk) {
				size_t len = (size_t)min(n * kBlock - off, (UInt64)kChunk);
				status = readFile(c->offset + skip + off, bounce, len, ctx);
				if (status == kIOReturnSuccess)
					req->buffer->writeBytes(pos + off, bounce, len);
			}
			break;
		default: {
			const UInt8 *data = chunkData(lo, ctx, &status);
			if (data != NULL)
				req->buffer->writeBytes(pos, data + skip, (IOByteCount)(n * kBlock));
			break;
		}
		}
		if (status == kIOReturnSuccess) {
			sector += n;
			pos += n * kBlock;
		}
	}
	vnode_put(fVnode);
	if (bounce != NULL)
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
		{ (IOExternalMethodAction)&sAttachChunks, 1, kIOUCVariableStructureSize,
		    1, 0 },
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

/* A compressed image: the chunk table hdiutil read from its "blkx" blocks
 * (MinkarDiskImageChunkTable), inline or, when long, in a memory
 * descriptor; checked whole before the image is attached, read-only. */
IOReturn
MinkarDiskImageUserClient::sAttachChunks(OSObject *target, void *,
    IOExternalMethodArguments *args)
{
	MinkarDiskImageUserClient *me = (MinkarDiskImageUserClient *)target;
	IOMemoryDescriptor *desc = args->structureInputDescriptor;
	size_t size = desc != NULL ? (size_t)desc->getLength() : args->structureInputSize;
	const MinkarDiskImageChunkTable *table;
	UInt8 *bytes;
	IOReturn status = kIOReturnBadArgument;

	if (size < sizeof(*table) || size > 64 * 1024 * 1024)
		return kIOReturnBadArgument;
	bytes = (UInt8 *)IOMalloc(size);
	if (bytes == NULL)
		return kIOReturnNoMemory;
	if (desc != NULL) {
		bool read = false;

		if (desc->prepare() == kIOReturnSuccess) {
			read = desc->readBytes(0, bytes, size) == size;
			desc->complete();
		}
		if (!read) {
			IOFree(bytes, size);
			return kIOReturnBadArgument;
		}
	} else
		memcpy(bytes, args->structureInput, size);

	table = (const MinkarDiskImageChunkTable *)bytes;
	size_t pathSpace = ((size_t)table->pathLength + 7) & ~(size_t)7;
	size_t chunkBytes = (size_t)table->chunkCount * sizeof(MinkarDiskImageChunk);
	if (table->pathLength < 2 || table->pathLength > MAXPATHLEN || table->chunkCount == 0 ||
	    table->chunkCount > 1024 * 1024 || sizeof(*table) + pathSpace + chunkBytes != size)
		goto out;
	{
		const char *path = (const char *)(bytes + sizeof(*table));
		const MinkarDiskImageChunk *in = (const MinkarDiskImageChunk *)(bytes + sizeof(*table) + pathSpace);
		UInt64 next = 0;

		if (path[table->pathLength - 1] != '\0')
			goto out;
		/* Sorted, back to back, covering the disk, each one within reason. */
		for (UInt32 i = 0; i < table->chunkCount; i++) {
			const MinkarDiskImageChunk *c = &in[i];
			bool packed = (c->type & 0x80000000) != 0;
			/* A compressed chunk is decompressed whole: within reason. A
			 * raw one holds its sectors. */
			if (c->sector != next || c->sectors == 0 || c->sectors > (1ULL << 50) ||
			    c->offset + c->length < c->offset ||
			    (packed && (c->sectors * kBlock > kMaxChunkBytes || c->length > kMaxChunkBytes)) ||
			    (c->type == kMinkarDIChunkRaw && c->length < c->sectors * kBlock))
				goto out;
			if (c->type != kMinkarDIChunkZero && c->type != kMinkarDIChunkRaw &&
			    c->type != kMinkarDIChunkIgnore && c->type != kMinkarDIChunkADC &&
			    c->type != kMinkarDIChunkZlib && c->type != kMinkarDIChunkBzip2 &&
			    c->type != kMinkarDIChunkLZFSE && c->type != kMinkarDIChunkLZMA)
				goto out;
			next = c->sector + c->sectors;
		}
		if (next != table->sectorCount)
			goto out;
		/* The table is in bytes, aligned as sent; the device copies it. */
		status = me->attachImage(path, 0, table->sectorCount * kBlock,
		    args->scalarInput[0] | kMinkarDiskImageReadOnly, &args->scalarOutput[0],
		    in, table->chunkCount);
	}
out:
	IOFree(bytes, size);
	return status;
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
    UInt64 length, UInt64 flags, UInt64 *id, const MinkarDiskImageChunk *chunks,
    UInt32 chunkCount)
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
	/* A compressed image: as big as its chunks say; each chunk within the
	 * file. */
	if (chunks != NULL) {
		for (UInt32 i = 0; i < chunkCount; i++)
			if ((chunks[i].type == kMinkarDIChunkRaw || (chunks[i].type & 0x80000000)) &&
			    chunks[i].offset + chunks[i].length > size) {
				vnode_close(vp, mode, ctx);
				vfs_context_rele(ctx);
				return kIOReturnBadArgument;
			}
		size = offset + length;
	}
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
	/* A compressed image: the device keeps a copy of its chunks. */
	if (chunks != NULL && !dev->setChunks(chunks, chunkCount)) {
		dev->release();
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
