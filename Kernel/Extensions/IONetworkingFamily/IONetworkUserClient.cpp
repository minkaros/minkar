/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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

#include <IOKit/network/IONetworkUserClient.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IONetworkData.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(IONetworkUserClient, IOUserClient);

IONetworkUserClient *
IONetworkUserClient::withTask(task_t owningTask, void *securityID)
{
	IONetworkUserClient *uc = new IONetworkUserClient;

	if (uc != NULL && !uc->initWithTask(owningTask, securityID, kIONUCType)) {
		uc->release();
		return NULL;
	}
	uc->_task = owningTask;
	uc->_privileged = IOUserClient::clientHasPrivilege(securityID,
	    kIOClientPrivilegeAdministrator) == kIOReturnSuccess;
	return uc;
}

bool
IONetworkUserClient::start(IOService *provider)
{
	_owner = OSDynamicCast(IONetworkInterface, provider);
	return _owner != NULL && super::start(provider);
}

IOReturn
IONetworkUserClient::clientClose()
{
	if (_owner != NULL)
		detach(_owner);
	_owner = NULL;
	return kIOReturnSuccess;
}

IOReturn
IONetworkUserClient::clientDied()
{
	return clientClose();
}

IOReturn
IONetworkUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *, OSObject *, void *)
{
	IONetworkData *data;
	UInt32 size;
	IOReturn ret;

	if (_owner == NULL)
		return kIOReturnNotAttached;

	switch (selector) {
	case kIONUCGetNetworkDataHandleIndex: {
		/* struct in: the data's name; struct out: UInt32 handle */
		char name[128];
		UInt32 handle;

		if (args->structureInputSize == 0 || args->structureInputSize > sizeof(name) ||
		    args->structureOutputSize < sizeof(handle))
			return kIOReturnBadArgument;
		bcopy(args->structureInput, name, args->structureInputSize);
		name[args->structureInputSize - 1] = '\0';
		if (!_owner->getNetworkDataHandle(name, &handle))
			return kIOReturnNotFound;
		bcopy(&handle, args->structureOutput, sizeof(handle));
		args->structureOutputSize = sizeof(handle);
		return kIOReturnSuccess;
	}
	case kIONUCGetNetworkDataCapacityIndex:
		/* scalar in: handle; scalar out: capacity */
		if (args->scalarInputCount < 1 || args->scalarOutputCount < 1)
			return kIOReturnBadArgument;
		if ((data = _owner->getNetworkDataForHandle((UInt32)args->scalarInput[0])) == NULL)
			return kIOReturnNotFound;
		args->scalarOutput[0] = data->getSize();
		args->scalarOutputCount = 1;
		return kIOReturnSuccess;
	case kIONUCReadNetworkDataIndex: {
		/* scalar in: handle; struct out: the bytes */
		void *buf;

		if (args->scalarInputCount < 1)
			return kIOReturnBadArgument;
		if ((data = _owner->getNetworkDataForHandle((UInt32)args->scalarInput[0])) == NULL)
			return kIOReturnNotFound;
		size = args->structureOutputDescriptor != NULL ?
		    (UInt32)args->structureOutputDescriptorSize : args->structureOutputSize;
		if (size > data->getSize())
			size = data->getSize();
		if ((buf = IOMalloc(data->getSize())) == NULL)
			return kIOReturnNoMemory;
		if ((ret = data->read(buf, &size)) == kIOReturnSuccess) {
			if (args->structureOutputDescriptor != NULL) {
				args->structureOutputDescriptor->prepare();
				args->structureOutputDescriptor->writeBytes(0, buf, size);
				args->structureOutputDescriptor->complete();
				args->structureOutputDescriptorSize = size;
			} else {
				bcopy(buf, args->structureOutput, size);
				args->structureOutputSize = size;
			}
		}
		IOFree(buf, data->getSize());
		return ret;
	}
	case kIONUCWriteNetworkDataIndex:
		/* scalar in: handle; struct in: the bytes */
		if (!_privileged)
			return kIOReturnNotPrivileged;
		if (args->scalarInputCount < 1 || args->structureInput == NULL)
			return kIOReturnBadArgument;
		if ((data = _owner->getNetworkDataForHandle((UInt32)args->scalarInput[0])) == NULL)
			return kIOReturnNotFound;
		return data->write((void *)args->structureInput, args->structureInputSize);
	case kIONUCResetNetworkDataIndex:
		if (!_privileged)
			return kIOReturnNotPrivileged;
		if (args->scalarInputCount < 1)
			return kIOReturnBadArgument;
		if ((data = _owner->getNetworkDataForHandle((UInt32)args->scalarInput[0])) == NULL)
			return kIOReturnNotFound;
		return data->reset();
	default:
		return kIOReturnUnsupported;
	}
}

/* { IONetworkInterfaceProperties: { IORequiredPacketFilters: { group: mask } } } */
IOReturn
IONetworkUserClient::setProperties(OSObject *properties)
{
	OSDictionary *dict = OSDynamicCast(OSDictionary, properties);
	OSDictionary *ifProps;

	if (!_privileged)
		return kIOReturnNotPrivileged;
	if (dict == NULL || _owner == NULL)
		return kIOReturnBadArgument;
	ifProps = OSDynamicCast(OSDictionary, dict->getObject("IONetworkInterfaceProperties"));
	if (ifProps == NULL)
		return kIOReturnBadArgument;
	return _owner->setProperties(ifProps);
}

IOService *
IONetworkUserClient::getService()
{
	return _owner;
}
