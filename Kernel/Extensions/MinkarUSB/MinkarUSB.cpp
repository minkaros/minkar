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

#include "MinkarUSB.h"
#include <IOKit/IOLib.h>
#include <IOKit/usb/USBSpec.h>

#define LOG(fmt, ...) IOLog("MinkarUSB: " fmt "\n", ##__VA_ARGS__)

/* ---- Composite device -------------------------------------------------- */

OSDefineMetaClassAndStructors(MinkarUSBComposite, IOService);

/*
 * The first configuration: SetConfiguration() publishes an IOUSBInterface
 * nub for each interface, which the class drivers then match.
 */
bool
MinkarUSBComposite::start(IOService *provider)
{
	fDevice = OSDynamicCast(IOUSBDevice, provider);
	if (fDevice == NULL || !IOService::start(provider))
		return false;
	if (!fDevice->open(this)) {
		LOG("cannot open device %04x:%04x", fDevice->GetVendorID(), fDevice->GetProductID());
		return false;
	}
	const IOUSBConfigurationDescriptor *cd = fDevice->GetFullConfigurationDescriptor(0);
	if (cd == NULL) {
		fDevice->close(this);
		return false;
	}
	IOReturn err = fDevice->SetConfiguration(this, cd->bConfigurationValue, true);
	if (err != kIOReturnSuccess) {
		LOG("device %04x:%04x: SetConfiguration(%u) failed: 0x%x", fDevice->GetVendorID(),
		    fDevice->GetProductID(), cd->bConfigurationValue, err);
		fDevice->close(this);
		return false;
	}
	LOG("device %04x:%04x configured (%u interfaces)", fDevice->GetVendorID(),
	    fDevice->GetProductID(), cd->bNumInterfaces);
	return true;
}

void
MinkarUSBComposite::stop(IOService *provider)
{
	if (fDevice != NULL && fDevice->isOpen(this))
		fDevice->close(this);
	IOService::stop(provider);
}

/* ---- HID requests ------------------------------------------------------ */

IOReturn
MinkarUSBHIDRequest(IOUSBInterface *intf, bool in, UInt8 request, UInt16 value,
    void *data, UInt16 length)
{
	IOUSBDevRequest req;

	req.bmRequestType = USBmakebmRequestType(in ? kUSBIn : kUSBOut, kUSBClass, kUSBInterface);
	req.bRequest = request;
	req.wValue = value;
	req.wIndex = intf->GetInterfaceNumber();
	req.wLength = length;
	req.pData = data;
	req.wLenDone = 0;
	return intf->DeviceRequest(&req, NULL);
}

/*
 * The interface's HID report descriptor, read with GET_DESCRIPTOR; its
 * length is in the HID descriptor that follows the interface descriptor.
 * The caller frees it with IOFree(desc, len).
 */
#define MAX_REPORT_DESC		1024

bool
MinkarUSBReadReportDescriptor(IOUSBInterface *intf, uint8_t **outDesc, uint16_t *outLen)
{
	const IOUSBDescriptorHeader *h = intf->FindNextAssociatedDescriptor(NULL, kUSBHIDDesc);
	const uint8_t *hid = (const uint8_t *)h;
	uint16_t len = 0;

	*outDesc = NULL;
	*outLen = 0;
	if (hid == NULL || hid[0] < 9)
		return false;
	/* bNumDescriptors entries of (bDescriptorType, wDescriptorLength). */
	for (unsigned i = 0; i < hid[5] && 6 + 3 * i + 2 < hid[0]; i++) {
		if (hid[6 + 3 * i] == kUSBReportDesc) {
			len = (uint16_t)(hid[7 + 3 * i] | (hid[8 + 3 * i] << 8));
			break;
		}
	}
	if (len == 0 || len > MAX_REPORT_DESC)
		return false;

	uint8_t *desc = (uint8_t *)IOMalloc(len);
	if (desc == NULL)
		return false;
	IOUSBDevRequest req;
	req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBInterface);
	req.bRequest = kUSBRqGetDescriptor;
	req.wValue = (UInt16)(kUSBReportDesc << 8);
	req.wIndex = intf->GetInterfaceNumber();
	req.wLength = len;
	req.pData = desc;
	req.wLenDone = 0;
	if (intf->DeviceRequest(&req, NULL) != kIOReturnSuccess) {
		IOFree(desc, len);
		return false;
	}
	*outDesc = desc;
	*outLen = len;
	return true;
}

/* ---- HID interface in report protocol ---------------------------------- */

OSDefineMetaClassAndStructors(MinkarUSBHIDInterface, IOService);

/*
 * A HID interface without the boot subclass: its report descriptor says
 * what it is. A keyboard and a pointer may share it (KVM switches send
 * both through one interface, with report IDs), so one reader feeds a
 * MinkarUSBKeyboard and a MinkarUSBPointer created here as its children.
 */
bool
MinkarUSBHIDInterface::start(IOService *provider)
{
	uint8_t *desc;
	uint16_t len;

	fInterface = OSDynamicCast(IOUSBInterface, provider);
	if (fInterface == NULL || !fInterface->open(this))
		return false;
	unsigned number = fInterface->GetInterfaceNumber();
	if (!MinkarUSBReadReportDescriptor(fInterface, &desc, &len)) {
		LOG("interface %u: no report descriptor", number);
		fInterface->close(this);
		return false;
	}
	bool keyboard = HIDParseKeyboard(desc, len, &fKeyboardLayout);
	bool pointer = HIDParsePointer(desc, len, &fPointerLayout);
	IOFree(desc, len);
	if (!keyboard && !pointer) {
		LOG("interface %u: no keyboard or pointer in its %u-byte report descriptor",
		    number, (unsigned)len);
		fInterface->close(this);
		return false;
	}
	if (!IOService::start(provider)) {
		fInterface->close(this);
		return false;
	}
	(void)MinkarUSBHIDRequest(fInterface, false, kHIDRqSetIdle, 0, NULL, 0);

	if (keyboard) {
		fKeyboard = new MinkarUSBKeyboard;
		if (fKeyboard == NULL || !fKeyboard->init() || !fKeyboard->attach(this)) {
			OSSafeReleaseNULL(fKeyboard);
		} else if (!fKeyboard->start(this)) {
			fKeyboard->detach(this);
			OSSafeReleaseNULL(fKeyboard);
		}
	}
	if (pointer) {
		fPointer = new MinkarUSBPointer;
		if (fPointer == NULL || !fPointer->init() || !fPointer->attach(this)) {
			OSSafeReleaseNULL(fPointer);
		} else if (!fPointer->start(this)) {
			fPointer->detach(this);
			OSSafeReleaseNULL(fPointer);
		}
	}
	fReader = MinkarUSBHIDReader::withInterface(fInterface, this, &MinkarUSBHIDInterface::handleReport);
	if (fReader == NULL || !fReader->startReading()) {
		LOG("interface %u: cannot start the report thread", number);
		stop(provider);
		return false;
	}
	LOG("interface %u: report protocol,%s%s", number,
	    fKeyboard ? " keyboard" : "", fPointer ? " pointer" : "");
	registerService();
	return true;
}

void
MinkarUSBHIDInterface::stop(IOService *provider)
{
	if (fReader != NULL) {
		fReader->stopReading();
		OSSafeReleaseNULL(fReader);
	}
	if (fKeyboard != NULL) {
		fKeyboard->stop(this);
		fKeyboard->detach(this);
		OSSafeReleaseNULL(fKeyboard);
	}
	if (fPointer != NULL) {
		fPointer->stop(this);
		fPointer->detach(this);
		OSSafeReleaseNULL(fPointer);
	}
	if (fInterface != NULL && fInterface->isOpen(this))
		fInterface->close(this);
	IOService::stop(provider);
}

/* Each report goes to both; each takes only its own report ID. */
void
MinkarUSBHIDInterface::handleReport(OSObject *owner, const uint8_t *report, uint32_t len)
{
	MinkarUSBHIDInterface *self = (MinkarUSBHIDInterface *)owner;

	if (self->fKeyboard != NULL)
		self->fKeyboard->feedReport(report, len);
	if (self->fPointer != NULL)
		self->fPointer->feedReport(report, len);
}

/* ---- Report reader ----------------------------------------------------- */

OSDefineMetaClassAndStructors(MinkarUSBHIDReader, OSObject);

MinkarUSBHIDReader *
MinkarUSBHIDReader::withInterface(IOUSBInterface *intf, OSObject *owner, Handler handler)
{
	MinkarUSBHIDReader *r = new MinkarUSBHIDReader;
	IOUSBFindEndpointRequest find;

	if (r == NULL || !r->init()) {
		OSSafeReleaseNULL(r);
		return NULL;
	}
	find.type = kUSBInterrupt;
	find.direction = kUSBIn;
	find.maxPacketSize = 0;
	find.interval = 0;
	r->fPipe = intf->FindNextPipe(NULL, &find);
	if (r->fPipe == NULL) {
		LOG("interface %u: no interrupt-IN pipe", intf->GetInterfaceNumber());
		r->release();
		return NULL;
	}
	r->fPipe->retain();
	r->fMaxPacket = find.maxPacketSize;
	if (r->fMaxPacket < 1 || r->fMaxPacket > 64)
		r->fMaxPacket = 8;
	r->fBuffer = IOBufferMemoryDescriptor::withCapacity(r->fMaxPacket, kIODirectionIn);
	if (r->fBuffer == NULL) {
		r->release();
		return NULL;
	}
	r->fBuffer->setLength(r->fMaxPacket);
	r->fInterface = intf;
	r->fOwner = owner;
	r->fHandler = handler;
	return r;
}

bool
MinkarUSBHIDReader::startReading()
{
	thread_t thread;

	fStop = false;
	fRunning = true;
	if (kernel_thread_start(&MinkarUSBHIDReader::threadMain, this, &thread) != KERN_SUCCESS) {
		fRunning = false;
		return false;
	}
	thread_deallocate(thread);
	return true;
}

void
MinkarUSBHIDReader::stopReading()
{
	fStop = true;
	/* The UIM returns within its 20 ms poll; give it a second at most. */
	for (int i = 0; i < 100 && fRunning; i++)
		IOSleep(10);
}

/*
 * One synchronous read at a time: each returns a report, or a timeout
 * after the UIM's short poll, so the loop also sees fStop promptly.
 */
void
MinkarUSBHIDReader::threadMain(void *arg, wait_result_t)
{
	MinkarUSBHIDReader *self = (MinkarUSBHIDReader *)arg;
	const uint8_t *bytes = (const uint8_t *)self->fBuffer->getBytesNoCopy();
	int errors = 0;

	self->retain();
	while (!self->fStop) {
		IOByteCount n = 0;
		IOReturn err = self->fPipe->Read(self->fBuffer, 0, 0, self->fMaxPacket,
		    (IOUSBCompletion *)NULL, &n);

		if (err == kIOReturnSuccess) {
			errors = 0;
			self->fHandler(self->fOwner, bytes, self->fMaxPacket);
		} else if (err == kIOReturnNotReady) {
			/*
			 * The host controller never set this endpoint up: no read
			 * will ever succeed, and retrying only floods the console.
			 */
			LOG("interface %u: endpoint not configured; not reading it",
			    self->fInterface->GetInterfaceNumber());
			break;
		} else if (err != (IOReturn)kIOUSBTransactionTimeout) {
			/* A stalled or failed transfer: clear it and keep going, slower. */
			if (++errors == 1)
				LOG("interface %u: read failed (0x%x)",
				    self->fInterface->GetInterfaceNumber(), err);
			self->fPipe->ClearStall();
			IOSleep(errors < 10 ? 50 : 500);
		}
	}
	self->fRunning = false;
	self->release();
	thread_terminate(current_thread());
}

void
MinkarUSBHIDReader::free()
{
	OSSafeReleaseNULL(fBuffer);
	OSSafeReleaseNULL(fPipe);
	OSObject::free();
}
