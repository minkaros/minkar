/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The ravynOS contributors
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
 * IONetworkData: a named buffer an interface publishes (statistics, for
 * instance). Drivers update the buffer; user space reads it through the
 * interface's IONetworkUserClient (IONetworkLib's IONetworkReadData) or as
 * the interface's "IONetworkData" property.
 */

#ifndef _IONETWORKDATA_H
#define _IONETWORKDATA_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOReturn.h>
#ifdef KERNEL
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSSymbol.h>
#endif

/* Access types. */
enum {
	kIONetworkDataAccessTypeRead		= 0x01,
	kIONetworkDataAccessTypeWrite		= 0x02,
	kIONetworkDataAccessTypeReset		= 0x04,
	kIONetworkDataAccessTypeSerialize	= 0x08,
	kIONetworkDataAccessTypeMask		= 0xff,
};
#define kIONetworkDataBasicAccessTypes \
	(kIONetworkDataAccessTypeRead | kIONetworkDataAccessTypeSerialize)

/* Buffer types. */
enum {
	kIONetworkDataBufferTypeInternal = 0,
	kIONetworkDataBufferTypeExternal,
	kIONetworkDataBufferTypeNone,
};

/* Keys of a serialized IONetworkData. */
#define kIONetworkDataBytes		"Data"
#define kIONetworkDataAccessTypes	"Access Types"
#define kIONetworkDataSize		"Size"

/* The classes are the kernel's; user space sees the constants. */
#ifdef KERNEL
class IONetworkData : public OSObject
{
	OSDeclareDefaultStructors(IONetworkData);

public:
	/* Called before an access; a nonzero return refuses it. */
	typedef IOReturn (*Action)(void *target, void *param, IONetworkData *data,
	    UInt32 accessType, void *buffer, UInt32 *bufferSize, UInt32 offset);

protected:
	const OSSymbol *	_key;
	UInt32			_access;
	void *			_buffer;
	UInt32			_size;
	UInt32			_bufferType;
	OSObject *		_target;
	Action			_action;
	void *			_param;

	virtual void free() APPLE_KEXT_OVERRIDE;

public:
	static IONetworkData *withInternalBuffer(const char *name, UInt32 bufferSize,
	    UInt32 accessTypes = kIONetworkDataBasicAccessTypes, void *target = NULL,
	    Action action = NULL, void *param = NULL);
	static IONetworkData *withExternalBuffer(const char *name, UInt32 bufferSize,
	    void *externalBuffer, UInt32 accessTypes = kIONetworkDataBasicAccessTypes,
	    void *target = NULL, Action action = NULL, void *param = NULL);
	static IONetworkData *withNoBuffer(const char *name, UInt32 bufferSize,
	    UInt32 accessTypes, void *target, Action action, void *param = NULL);

	virtual bool init(const char *name, UInt32 bufferType, UInt32 bufferSize,
	    void *externalBuffer = NULL, UInt32 accessTypes = kIONetworkDataBasicAccessTypes,
	    void *target = NULL, Action action = NULL, void *param = NULL);

	const OSSymbol *getKey() const;
	UInt32 getBufferType() const;
	UInt32 getAccessTypes() const;
	void setAccessTypes(UInt32 types);
	void *getBuffer() const;
	UInt32 getSize() const;
	void setNotificationTarget(void *target, Action action, void *param = NULL);
	void *getNotificationTarget() const;
	Action getNotificationAction() const;
	void *getNotificationParameter() const;

	virtual bool setBuffer(const void *srcBuffer, UInt32 srcBufferSize, bool clear = true);
	virtual bool copyBuffer(void *dstBuffer, UInt32 *dstBufferSize) const;
	virtual bool clearBuffer();

	/* Accesses (they call the notification action first). */
	virtual IOReturn reset();
	virtual IOReturn read(void *dstBuffer, UInt32 *dstBufferSize, UInt32 dstBufferOffset = 0);
	virtual IOReturn write(void *srcBuffer, UInt32 srcBufferSize, UInt32 srcBufferOffset = 0);

	virtual bool serialize(OSSerialize *s) const APPLE_KEXT_OVERRIDE;

	OSMetaClassDeclareReservedUnused(IONetworkData, 0);
	OSMetaClassDeclareReservedUnused(IONetworkData, 1);
	OSMetaClassDeclareReservedUnused(IONetworkData, 2);
	OSMetaClassDeclareReservedUnused(IONetworkData, 3);
};

#endif /* KERNEL */

#endif /* !_IONETWORKDATA_H */
