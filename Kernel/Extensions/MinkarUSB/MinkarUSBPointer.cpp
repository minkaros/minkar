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

#define super IOHIPointing
#define LOG(fmt, ...) IOLog("MinkarUSBPointer: " fmt "\n", ##__VA_ARGS__)

#define BOOT_MOUSE_PROTOCOL	2

OSDefineMetaClassAndStructors(MinkarUSBPointer, IOHIPointing);

/* The report layout from the HID report descriptor. */
bool
MinkarUSBPointer::readLayout()
{
	uint8_t *desc;
	uint16_t len;

	if (!MinkarUSBReadReportDescriptor(fInterface, &desc, &len))
		return false;
	bool ok = HIDParsePointer(desc, len, &fLayout);
	IOFree(desc, len);
	return ok;
}

bool
MinkarUSBPointer::start(IOService *provider)
{
	/* Under MinkarUSBHIDInterface, which reads the pipe and passes reports on. */
	MinkarUSBHIDInterface *hid = OSDynamicCast(MinkarUSBHIDInterface, provider);
	if (hid != NULL) {
		fInterface = NULL;
		fReader = NULL;
		fLayout = hid->pointerLayout();
		if (!super::start(provider))
			return false;
		setProperty("MinkarEvents", 0ULL, 32);
		if (!fLayout.x.relative)
			setProperty("MinkarAbsolutePointer", 1ULL, 32);
		LOG("started (report %u, %s, %u buttons%s)", fLayout.reportID,
		    fLayout.x.relative ? "relative" : "absolute", (unsigned)buttonCount(),
		    fLayout.wheel.present ? ", wheel" : "");
		return true;
	}

	fInterface = OSDynamicCast(IOUSBInterface, provider);
	if (fInterface == NULL || !fInterface->open(this))
		return false;
	if (!readLayout()) {
		if (fInterface->GetInterfaceProtocol() != BOOT_MOUSE_PROTOCOL) {
			LOG("interface %u: no pointer in the report descriptor",
			    fInterface->GetInterfaceNumber());
			fInterface->close(this);
			return false;
		}
		/* A boot mouse: buttons, dx, dy and an optional wheel. */
		fBootMouse = true;
		(void)MinkarUSBHIDRequest(fInterface, false, kHIDRqSetProtocol,
		    kHIDBootProtocolValue, NULL, 0);
	}
	(void)MinkarUSBHIDRequest(fInterface, false, kHIDRqSetIdle, 0, NULL, 0);
	fReader = MinkarUSBHIDReader::withInterface(fInterface, this, &MinkarUSBPointer::handleReport);
	if (fReader == NULL || !super::start(provider)) {
		OSSafeReleaseNULL(fReader);
		fInterface->close(this);
		return false;
	}
	setProperty("MinkarEvents", 0ULL, 32);
	if (!fBootMouse && !fLayout.x.relative)
		setProperty("MinkarAbsolutePointer", 1ULL, 32);
	if (!fReader->startReading()) {
		LOG("cannot start the report thread");
		return false;
	}
	LOG("started (%s, %u buttons%s)", fBootMouse ? "boot mouse" :
	    (fLayout.x.relative ? "relative" : "absolute"), (unsigned)buttonCount(),
	    (fBootMouse || fLayout.wheel.present) ? ", wheel" : "");
	return true;
}

void
MinkarUSBPointer::stop(IOService *provider)
{
	if (fReader != NULL) {
		fReader->stopReading();
		OSSafeReleaseNULL(fReader);
	}
	if (fInterface != NULL && fInterface->isOpen(this))
		fInterface->close(this);
	super::stop(provider);
}

void
MinkarUSBPointer::handleReport(OSObject *owner, const uint8_t *report, uint32_t len)
{
	((MinkarUSBPointer *)owner)->processReport(report, len);
}

void
MinkarUSBPointer::feedReport(const uint8_t *report, uint32_t len)
{
	processReport(report, len);
}

void
MinkarUSBPointer::publish(int32_t x, int32_t y, uint32_t buttons, bool absolute)
{
	fEvents++;
	setProperty("MinkarEvents", (unsigned long long)fEvents, 32);
	setProperty("MinkarX", (unsigned long long)(uint32_t)x, 32);
	setProperty("MinkarY", (unsigned long long)(uint32_t)y, 32);
	setProperty("MinkarButtons", (unsigned long long)buttons, 32);
	setProperty("MinkarAbsolute", absolute);
}

void
MinkarUSBPointer::processReport(const uint8_t *r, uint32_t len)
{
	AbsoluteTime now;
	uint32_t buttons = 0;
	int32_t wheel = 0;

	clock_get_uptime((uint64_t *)&now);
	if (fBootMouse) {
		if (len < 3)
			return;
		buttons = r[0] & 0x07;
		int32_t dx = (int8_t)r[1], dy = (int8_t)r[2];
		wheel = len > 3 ? (int8_t)r[3] : 0;
		dispatchRelativePointerEvent(dx, dy, buttons, now);
		publish(dx, dy, buttons, false);
	} else {
		if (fLayout.reportID != 0 && (len < 1 || r[0] != fLayout.reportID))
			return;
		for (unsigned b = 0; b < fLayout.buttonCount; b++)
			if (HIDFieldValue(&fLayout, &fLayout.buttons[b], r, len))
				buttons |= 1U << b;
		int32_t x = HIDFieldValue(&fLayout, &fLayout.x, r, len);
		int32_t y = HIDFieldValue(&fLayout, &fLayout.y, r, len);
		if (fLayout.wheel.present)
			wheel = HIDFieldValue(&fLayout, &fLayout.wheel, r, len);
		if (fLayout.x.relative) {
			dispatchRelativePointerEvent(x, y, buttons, now);
			publish(x, y, buttons, false);
		} else {
			IOGPoint at = { (SInt16)x, (SInt16)y };
			IOGBounds bounds = { (SInt16)fLayout.x.logicalMin, (SInt16)fLayout.x.logicalMax,
			    (SInt16)fLayout.y.logicalMin, (SInt16)fLayout.y.logicalMax };

			dispatchAbsolutePointerEvent(&at, &bounds, buttons, true,
			    buttons ? 255 : 0, 0, 255, 90, now);
			publish(x, y, buttons, true);
		}
	}
	if (wheel != 0)
		dispatchScrollWheelEvent((short)wheel, 0, 0, now);
}

IOItemCount
MinkarUSBPointer::buttonCount()
{
	if (fBootMouse || fLayout.buttonCount == 0)
		return 3;
	return fLayout.buttonCount;
}

/* 400 dots per inch, as IOHIPointing expects it (16.16 fixed point). */
IOFixed
MinkarUSBPointer::resolution()
{
	return 400 << 16;
}
