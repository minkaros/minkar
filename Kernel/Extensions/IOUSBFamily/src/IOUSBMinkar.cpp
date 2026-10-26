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
 * The IOUSBDevice and IOUSBInterface methods the reconstructed sources
 * left undefined, written from what their headers document. Without them
 * IOUSBFamily did not link into the kernel cache. Minkar has no USB 3
 * streams, no extra bus power to hand out and no location overrides; the
 * rest walks the configuration descriptor or sends the standard request.
 */

#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/USBTracepoints.h>
#include <libkern/OSByteOrder.h>

#ifndef kIOUSBEndpointNotFound
#define kIOUSBEndpointNotFound	iokit_usb_err(0x57)
#endif

/* Tracepoints stay off (USBTracepoints.h tests this). */
UInt32 gUSBStackDebugFlags = 0;

/* ---- IOUSBDevice ------------------------------------------------------- */

void
IOUSBDevice::SetAddress(USBDeviceAddress address)
{
	_address = address;
}

/* No power beyond the bus's: nothing is granted, so nothing comes back. */
UInt32
IOUSBDevice::RequestExtraPower(UInt32 type, UInt32 requestedPower)
{
	(void)type;
	(void)requestedPower;
	return 0;
}

IOReturn
IOUSBDevice::ReturnExtraPower(UInt32 type, UInt32 returnedPower)
{
	(void)type;
	return returnedPower == 0 ? kIOReturnSuccess : kIOReturnBadArgument;
}

UInt32
IOUSBDevice::GetExtraPowerAllocated(UInt32 type)
{
	(void)type;
	return 0;
}

IOReturn
IOUSBDevice::GetDeviceInformation(UInt32 *info)
{
	if (info == NULL)
		return kIOReturnBadArgument;
	*info = 0;
	return kIOReturnSuccess;
}

/* Pipes of every interface of the device: reopened, or unlinked. */
void
IOUSBDevice::OpenOrCloseAllInterfacePipes(bool open)
{
	OSIterator *it = getChildIterator(gIOServicePlane);

	if (it == NULL)
		return;
	while (OSObject *child = it->getNextObject()) {
		IOUSBInterface *intf = OSDynamicCast(IOUSBInterface, child);

		if (intf == NULL)
			continue;
		if (open)
			intf->ReopenPipes();
		else
			intf->UnlinkPipes();
	}
	it->release();
}

bool
IOUSBDevice::DoLocationOverrideAndModelMatch()
{
	return false;
}

/* SuperSpeed endpoints with streams (IOUSBPipeV2) are not supported. */
IOUSBPipeV2 *
IOUSBDevice::MakePipe(const IOUSBEndpointDescriptor *ep,
    IOUSBSuperSpeedEndpointCompanionDescriptor *sscd, IOUSBInterface *interface)
{
	(void)ep;
	(void)sscd;
	(void)interface;
	return NULL;
}

OSMetaClassDefineReservedUnused(IOUSBDevice, 17);
OSMetaClassDefineReservedUnused(IOUSBDevice, 18);
OSMetaClassDefineReservedUnused(IOUSBDevice, 19);

/* ---- IOUSBInterface ---------------------------------------------------- */

UInt8 IOUSBInterface::GetConfigValue() { return _configDesc ? _configDesc->bConfigurationValue : 0; }
IOUSBDevice *IOUSBInterface::GetDevice() { return _device; }
UInt8 IOUSBInterface::GetInterfaceNumber() { return _bInterfaceNumber; }
UInt8 IOUSBInterface::GetAlternateSetting() { return _bAlternateSetting; }
UInt8 IOUSBInterface::GetNumEndpoints() { return _bNumEndpoints; }
UInt8 IOUSBInterface::GetInterfaceClass() { return _bInterfaceClass; }
UInt8 IOUSBInterface::GetInterfaceSubClass() { return _bInterfaceSubClass; }
UInt8 IOUSBInterface::GetInterfaceProtocol() { return _bInterfaceProtocol; }
UInt8 IOUSBInterface::GetInterfaceStringIndex() { return _iInterface; }

/* Pipes are made by CreatePipes() and destroyed by ClosePipes(). */
void
IOUSBInterface::ReopenPipes(void)
{
	for (int i = 0; i < kUSBMaxPipes; i++)
		if (_pipeList[i] != NULL)
			return;
	CreatePipes();
}

void
IOUSBInterface::UnlinkPipes(void)
{
	ClosePipes();
}

/* No streams without IOUSBPipeV2. */
IOReturn IOUSBInterface::RememberStreams(void) { return kIOReturnSuccess; }
IOReturn IOUSBInterface::RecreateStreams(void) { return kIOReturnSuccess; }

/* Function remote wake and function suspend are SuperSpeed features. */
IOReturn
IOUSBInterface::EnableRemoteWake(bool enable)
{
	(void)enable;
	return kIOReturnUnsupported;
}

IOReturn
IOUSBInterface::SetFunctionSuspendFeature(UInt8 options)
{
	(void)options;
	return kIOReturnUnsupported;
}

/* GET_STATUS to the interface. */
IOReturn
IOUSBInterface::GetInterfaceStatus(USBStatus *status)
{
	IOUSBDevRequest req;
	USBStatus value = 0;
	IOReturn err;

	if (status == NULL)
		return kIOReturnBadArgument;
	req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBInterface);
	req.bRequest = kUSBRqGetStatus;
	req.wValue = 0;
	req.wIndex = _bInterfaceNumber;
	req.wLength = sizeof(value);
	req.pData = &value;
	req.wLenDone = 0;
	err = DeviceRequest(&req, NULL);
	if (err == kIOReturnSuccess)
		*status = OSSwapLittleToHostInt16(value);
	return err;
}

/*
 * The descriptor that follows `after` inside the configuration descriptor,
 * or NULL at its end.
 */
static const IOUSBDescriptorHeader *
nextDescriptor(const IOUSBConfigurationDescriptor *config, const void *after)
{
	const UInt8 *base = (const UInt8 *)config;
	UInt16 total = OSSwapLittleToHostInt16(config->wTotalLength);
	const UInt8 *p = (const UInt8 *)after;

	if (p < base || p >= base + total)
		return NULL;
	if (p[0] < 2)
		return NULL;
	p += p[0];
	if (p + 2 > base + total || p[0] < 2 || p + p[0] > base + total)
		return NULL;
	return (const IOUSBDescriptorHeader *)p;
}

/* The interface descriptor of this interface number with that alternate setting. */
static const IOUSBInterfaceDescriptor *
findAltSetting(const IOUSBConfigurationDescriptor *config, UInt8 number, UInt8 alt)
{
	const IOUSBDescriptorHeader *d = (const IOUSBDescriptorHeader *)config;

	while ((d = nextDescriptor(config, d)) != NULL) {
		if (d->bDescriptorType == kUSBInterfaceDesc && d->bLength >= 9) {
			const IOUSBInterfaceDescriptor *i = (const IOUSBInterfaceDescriptor *)d;

			if (i->bInterfaceNumber == number && i->bAlternateSetting == alt)
				return i;
		}
	}
	return NULL;
}

/* The descriptors after the interface descriptor, up to the next interface. */
const IOUSBDescriptorHeader *
IOUSBInterface::FindNextAssociatedDescriptor(const void *current, UInt8 type)
{
	if (_configDesc == NULL || _interfaceDesc == NULL)
		return NULL;
	const void *from = current != NULL ? current : (const void *)_interfaceDesc;
	const IOUSBDescriptorHeader *d = (const IOUSBDescriptorHeader *)from;

	while ((d = nextDescriptor(_configDesc, d)) != NULL) {
		if (d->bDescriptorType == kUSBInterfaceDesc)
			return NULL;
		if (type == kUSBAnyDesc || d->bDescriptorType == type)
			return d;
	}
	return NULL;
}

IOReturn
IOUSBInterface::GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber,
    UInt8 direction, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval)
{
	IOUSBEndpointProperties props;

	bzero(&props, sizeof(props));
	props.bAlternateSetting = alternateSetting;
	props.bEndpointNumber = endpointNumber;
	props.bDirection = direction;
	IOReturn err = GetEndpointPropertiesV3(&props);
	if (err != kIOReturnSuccess)
		return err;
	if (transferType != NULL)
		*transferType = props.bTransferType;
	if (maxPacketSize != NULL)
		*maxPacketSize = props.wMaxPacketSize;
	if (interval != NULL)
		*interval = props.bInterval;
	return kIOReturnSuccess;
}

/*
 * In: bAlternateSetting, bEndpointNumber, bDirection (kUSBIn or kUSBOut).
 * Out: the rest, from the endpoint descriptor.
 */
IOReturn
IOUSBInterface::GetEndpointPropertiesV3(IOUSBEndpointProperties *p)
{
	if (p == NULL)
		return kIOReturnBadArgument;
	if (_configDesc == NULL)
		return kIOReturnNotAttached;
	const IOUSBInterfaceDescriptor *intf = findAltSetting(_configDesc,
	    _bInterfaceNumber, p->bAlternateSetting);
	if (intf == NULL)
		return kIOUSBInterfaceNotFound;

	const IOUSBDescriptorHeader *d = (const IOUSBDescriptorHeader *)intf;
	while ((d = nextDescriptor(_configDesc, d)) != NULL &&
	    d->bDescriptorType != kUSBInterfaceDesc) {
		if (d->bDescriptorType != kUSBEndpointDesc || d->bLength < 7)
			continue;
		const IOUSBEndpointDescriptor *ep = (const IOUSBEndpointDescriptor *)d;
		UInt8 number = ep->bEndpointAddress & 0x0F;
		UInt8 dir = (ep->bEndpointAddress & 0x80) ? kUSBIn : kUSBOut;

		if (number != p->bEndpointNumber || dir != p->bDirection)
			continue;
		UInt16 wMax = OSSwapLittleToHostInt16(ep->wMaxPacketSize);

		p->bTransferType = ep->bmAttributes & 0x03;
		p->bSyncType = (ep->bmAttributes >> 2) & 0x03;
		p->bUsageType = (ep->bmAttributes >> 4) & 0x03;
		p->wMaxPacketSize = wMax & 0x07FF;
		p->bMult = (UInt8)((wMax >> 11) & 0x03);
		p->bInterval = ep->bInterval;
		p->bMaxBurst = 0;
		p->bMaxStreams = 0;
		p->wBytesPerInterval = (UInt16)(p->wMaxPacketSize * (p->bMult + 1));
		return kIOReturnSuccess;
	}
	return kIOUSBEndpointNotFound;
}

/*
 * SET_INTERFACE, then the interface takes the alternate setting's
 * descriptor, properties and pipes. The client must have it open.
 */
IOReturn
IOUSBInterface::SetAlternateInterface(IOService *forClient, UInt16 alternateSetting)
{
	if (forClient == NULL || !isOpen(forClient))
		return kIOReturnExclusiveAccess;
	if (_configDesc == NULL)
		return kIOReturnNotAttached;
	const IOUSBInterfaceDescriptor *alt = findAltSetting(_configDesc,
	    _bInterfaceNumber, (UInt8)alternateSetting);
	if (alt == NULL)
		return kIOUSBInterfaceNotFound;

	ClosePipes();

	IOUSBDevRequest req;
	req.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBInterface);
	req.bRequest = kUSBRqSetInterface;
	req.wValue = alternateSetting;
	req.wIndex = _bInterfaceNumber;
	req.wLength = 0;
	req.pData = NULL;
	req.wLenDone = 0;
	IOReturn err = DeviceRequest(&req, NULL);
	if (err != kIOReturnSuccess) {
		CreatePipes();		/* back to the setting we had */
		return err;
	}

	_interfaceDesc = alt;
	_bAlternateSetting = alt->bAlternateSetting;
	_bNumEndpoints = alt->bNumEndpoints;
	_bInterfaceClass = alt->bInterfaceClass;
	_bInterfaceSubClass = alt->bInterfaceSubClass;
	_bInterfaceProtocol = alt->bInterfaceProtocol;
	_iInterface = alt->iInterface;
	SetProperties();
	return CreatePipes();
}

OSMetaClassDefineReservedUnused(IOUSBInterface, 10);
OSMetaClassDefineReservedUnused(IOUSBInterface, 11);
OSMetaClassDefineReservedUnused(IOUSBInterface, 12);
OSMetaClassDefineReservedUnused(IOUSBInterface, 13);
OSMetaClassDefineReservedUnused(IOUSBInterface, 14);
OSMetaClassDefineReservedUnused(IOUSBInterface, 15);
OSMetaClassDefineReservedUnused(IOUSBInterface, 16);
OSMetaClassDefineReservedUnused(IOUSBInterface, 17);
OSMetaClassDefineReservedUnused(IOUSBInterface, 18);
OSMetaClassDefineReservedUnused(IOUSBInterface, 19);
