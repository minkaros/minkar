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

#include <IOKit/network/IONetworkData.h>
#include <IOKit/IOLib.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSSerialize.h>

#define super OSObject
OSDefineMetaClassAndStructors(IONetworkData, OSObject);
OSMetaClassDefineReservedUnused(IONetworkData, 0);
OSMetaClassDefineReservedUnused(IONetworkData, 1);
OSMetaClassDefineReservedUnused(IONetworkData, 2);
OSMetaClassDefineReservedUnused(IONetworkData, 3);

static IONetworkData *
make(const char *name, UInt32 type, UInt32 size, void *ext, UInt32 access,
    void *target, IONetworkData::Action action, void *param)
{
	IONetworkData *d = new IONetworkData;

	if (d != NULL && !d->init(name, type, size, ext, access, target, action, param)) {
		d->release();
		d = NULL;
	}
	return d;
}

IONetworkData *
IONetworkData::withInternalBuffer(const char *name, UInt32 size, UInt32 access,
    void *target, Action action, void *param)
{
	return make(name, kIONetworkDataBufferTypeInternal, size, NULL, access,
	    target, action, param);
}

IONetworkData *
IONetworkData::withExternalBuffer(const char *name, UInt32 size, void *buffer,
    UInt32 access, void *target, Action action, void *param)
{
	return make(name, kIONetworkDataBufferTypeExternal, size, buffer, access,
	    target, action, param);
}

IONetworkData *
IONetworkData::withNoBuffer(const char *name, UInt32 size, UInt32 access,
    void *target, Action action, void *param)
{
	return make(name, kIONetworkDataBufferTypeNone, size, NULL, access,
	    target, action, param);
}

bool
IONetworkData::init(const char *name, UInt32 bufferType, UInt32 bufferSize,
    void *externalBuffer, UInt32 accessTypes, void *target, Action action, void *param)
{
	if (name == NULL || !super::init())
		return false;
	_key = OSSymbol::withCString(name);
	_bufferType = bufferType;
	_size = bufferSize;
	_access = accessTypes;
	_target = (OSObject *)target;
	_action = action;
	_param = param;
	switch (bufferType) {
	case kIONetworkDataBufferTypeInternal:
		if (bufferSize > 0 && (_buffer = IOMallocZero(bufferSize)) == NULL)
			return false;
		break;
	case kIONetworkDataBufferTypeExternal:
		if (externalBuffer == NULL)
			return false;
		_buffer = externalBuffer;
		break;
	case kIONetworkDataBufferTypeNone:
		_buffer = NULL;
		break;
	default:
		return false;
	}
	return _key != NULL;
}

void
IONetworkData::free()
{
	if (_bufferType == kIONetworkDataBufferTypeInternal && _buffer != NULL)
		IOFree(_buffer, _size);
	_buffer = NULL;
	OSSafeReleaseNULL(_key);
	super::free();
}

const OSSymbol *IONetworkData::getKey() const { return _key; }
UInt32 IONetworkData::getBufferType() const { return _bufferType; }
UInt32 IONetworkData::getAccessTypes() const { return _access; }
void IONetworkData::setAccessTypes(UInt32 types) { _access = types & kIONetworkDataAccessTypeMask; }
void *IONetworkData::getBuffer() const { return _buffer; }
UInt32 IONetworkData::getSize() const { return _size; }
void *IONetworkData::getNotificationTarget() const { return _target; }
IONetworkData::Action IONetworkData::getNotificationAction() const { return _action; }
void *IONetworkData::getNotificationParameter() const { return _param; }

void
IONetworkData::setNotificationTarget(void *target, Action action, void *param)
{
	_target = (OSObject *)target;
	_action = action;
	_param = param;
}

bool
IONetworkData::setBuffer(const void *src, UInt32 size, bool clear)
{
	if (_buffer == NULL || src == NULL || size > _size)
		return false;
	bcopy(src, _buffer, size);
	if (clear && size < _size)
		bzero((uint8_t *)_buffer + size, _size - size);
	return true;
}

bool
IONetworkData::copyBuffer(void *dst, UInt32 *dstSize) const
{
	if (_buffer == NULL || dst == NULL || dstSize == NULL)
		return false;
	if (*dstSize > _size)
		*dstSize = _size;
	bcopy(_buffer, dst, *dstSize);
	return true;
}

bool
IONetworkData::clearBuffer()
{
	if (_buffer == NULL)
		return false;
	bzero(_buffer, _size);
	return true;
}

/* Asks the notification action, if any, before an access. */
#define NOTIFY(type, buf, size, off) \
	(_action != NULL ? _action(_target, _param, this, (type), (buf), (size), (off)) \
	    : kIOReturnSuccess)

IOReturn
IONetworkData::reset()
{
	IOReturn ret;

	if (!(_access & kIONetworkDataAccessTypeReset))
		return kIOReturnNotWritable;
	if ((ret = NOTIFY(kIONetworkDataAccessTypeReset, NULL, NULL, 0)) != kIOReturnSuccess)
		return ret;
	if (_buffer != NULL)
		clearBuffer();
	return kIOReturnSuccess;
}

IOReturn
IONetworkData::read(void *dst, UInt32 *dstSize, UInt32 offset)
{
	IOReturn ret;

	if (!(_access & kIONetworkDataAccessTypeRead))
		return kIOReturnNotReadable;
	if (dst == NULL || dstSize == NULL)
		return kIOReturnBadArgument;
	if ((ret = NOTIFY(kIONetworkDataAccessTypeRead, dst, dstSize, offset)) != kIOReturnSuccess)
		return ret;
	if (_buffer == NULL)
		return kIOReturnSuccess;
	if (offset >= _size) {
		*dstSize = 0;
		return kIOReturnBadArgument;
	}
	if (*dstSize > _size - offset)
		*dstSize = _size - offset;
	bcopy((uint8_t *)_buffer + offset, dst, *dstSize);
	return kIOReturnSuccess;
}

IOReturn
IONetworkData::write(void *src, UInt32 srcSize, UInt32 offset)
{
	IOReturn ret;

	if (!(_access & kIONetworkDataAccessTypeWrite))
		return kIOReturnNotWritable;
	if (src == NULL)
		return kIOReturnBadArgument;
	if ((ret = NOTIFY(kIONetworkDataAccessTypeWrite, src, &srcSize, offset)) != kIOReturnSuccess)
		return ret;
	if (_buffer == NULL)
		return kIOReturnSuccess;
	if (offset >= _size || srcSize > _size - offset)
		return kIOReturnBadArgument;
	bcopy(src, (uint8_t *)_buffer + offset, srcSize);
	return kIOReturnSuccess;
}

/* As a dictionary: the access types, the size and (if allowed) the bytes. */
bool
IONetworkData::serialize(OSSerialize *s) const
{
	OSDictionary *dict = OSDictionary::withCapacity(3);
	OSNumber *num;
	bool ok;

	if (dict == NULL)
		return false;
	if ((num = OSNumber::withNumber(_access, 32)) != NULL) {
		dict->setObject(kIONetworkDataAccessTypes, num);
		num->release();
	}
	if ((num = OSNumber::withNumber(_size, 32)) != NULL) {
		dict->setObject(kIONetworkDataSize, num);
		num->release();
	}
	if ((_access & kIONetworkDataAccessTypeSerialize) && _buffer != NULL) {
		IONetworkData *self = const_cast<IONetworkData *>(this);
		UInt32 size = _size;
		void *copy = IOMalloc(_size);

		/* The notification action may refresh the buffer first. */
		if (copy != NULL) {
			if (self->_action == NULL ||
			    self->_action(_target, _param, self, kIONetworkDataAccessTypeSerialize,
			    copy, &size, 0) == kIOReturnSuccess) {
				OSData *data = OSData::withBytes(_buffer, _size);

				if (data != NULL) {
					dict->setObject(kIONetworkDataBytes, data);
					data->release();
				}
			}
			IOFree(copy, _size);
		}
	}
	ok = dict->serialize(s);
	dict->release();
	return ok;
}
