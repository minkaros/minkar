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
 * MinkarDiskImage: disk images as disks. A file (or a range of one, the
 * data of an image format with a header or trailer) becomes a block
 * storage device; IOStorageFamily puts its media, partitions and
 * /dev/diskN on it as on any disk. hdiutil attaches and detaches images
 * through the controller's user client, an administrator's only.
 */

#ifndef _MINKAR_DISK_IMAGE_H_
#define _MINKAR_DISK_IMAGE_H_

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLocks.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <sys/vnode.h>
#include <kern/thread.h>

/* The user client's methods. */
enum {
	kMinkarDiskImageAttach = 0,	/* path; offset, length, flags -> ID */
	kMinkarDiskImageDetach = 1,	/* ID */
	kMinkarDiskImageMethodCount
};
#define kMinkarDiskImageReadOnly	0x1

class MinkarDiskImageController : public IOService {
	OSDeclareDefaultStructors(MinkarDiskImageController);
public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
};

class MinkarDiskImageDevice : public IOBlockStorageDevice {
	OSDeclareDefaultStructors(MinkarDiskImageDevice);

	struct Request {
		Request			*next;
		IOMemoryDescriptor	*buffer;
		UInt64			 block, nblks;
		IOStorageCompletion	 completion;
	};

	vnode_t			fVnode;
	int			fOpenMode;
	UInt64			fOffset;	/* where the disk starts in the file */
	UInt64			fSize;		/* bytes, a multiple of 512 */
	bool			fReadOnly;
	IOLock			*fLock;
	Request			*fHead, *fTail;
	bool			fStopping, fStopped;
	thread_t		fThread;
	char			fProduct[32];

	static void worker(void *arg, wait_result_t);
	void serve();
	IOReturn transfer(Request *req, UInt64 *done, vfs_context_t ctx);

public:
	bool initWithVnode(vnode_t vp, int mode, UInt64 offset, UInt64 size,
	    bool readOnly, const char *path);
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void free() APPLE_KEXT_OVERRIDE;

	virtual IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
	    UInt64 nblks, IOStorageAttributes *attributes,
	    IOStorageCompletion *completion) APPLE_KEXT_OVERRIDE;
	virtual IOReturn doSynchronize(UInt64 block, UInt64 nblks,
	    IOStorageSynchronizeOptions options = 0) APPLE_KEXT_OVERRIDE;
	virtual IOReturn doEjectMedia(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn doFormatMedia(UInt64 byteCapacity) APPLE_KEXT_OVERRIDE;
	virtual UInt32 doGetFormatCapacities(UInt64 *capacities,
	    UInt32 capacitiesMaxCount) const APPLE_KEXT_OVERRIDE;
	virtual char *getVendorString(void) APPLE_KEXT_OVERRIDE;
	virtual char *getProductString(void) APPLE_KEXT_OVERRIDE;
	virtual char *getRevisionString(void) APPLE_KEXT_OVERRIDE;
	virtual char *getAdditionalDeviceInfoString(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportBlockSize(UInt64 *blockSize) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportEjectability(bool *isEjectable) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportMaxValidBlock(UInt64 *maxBlock) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportMediaState(bool *mediaPresent,
	    bool *changedState = 0) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportRemovability(bool *isRemovable) APPLE_KEXT_OVERRIDE;
	virtual IOReturn reportWriteProtection(bool *isWriteProtected)
	    APPLE_KEXT_OVERRIDE;
};

class MinkarDiskImageUserClient : public IOUserClient {
	OSDeclareDefaultStructors(MinkarDiskImageUserClient);

	MinkarDiskImageController	*fController;

	static IOReturn sAttach(OSObject *target, void *ref,
	    IOExternalMethodArguments *args);
	static IOReturn sDetach(OSObject *target, void *ref,
	    IOExternalMethodArguments *args);
	IOReturn attachImage(const char *path, UInt64 offset, UInt64 length,
	    UInt64 flags, UInt64 *id);
	IOReturn detachImage(UInt64 id);

public:
	virtual bool initWithTask(task_t owningTask, void *securityID, UInt32 type)
	    APPLE_KEXT_OVERRIDE;
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn externalMethod(uint32_t selector,
	    IOExternalMethodArguments *args, IOExternalMethodDispatch *dispatch,
	    OSObject *target, void *reference) APPLE_KEXT_OVERRIDE;
};

#endif /* _MINKAR_DISK_IMAGE_H_ */
