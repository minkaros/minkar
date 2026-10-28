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
 * MinkarUSB: the USB class drivers Minkar needs on top of IOUSBFamily.
 *
 *  - MinkarUSBComposite configures a device whose class is defined per
 *    interface, so its IOUSBInterface nubs appear and class drivers match.
 *  - MinkarUSBKeyboard: a HID boot-protocol keyboard as an IOHIKeyboard
 *    (HID usage IDs to ADB key codes, as ApplePS2Keyboard sends).
 *  - MinkarUSBPointer: a HID mouse or tablet as an IOHIPointing; the
 *    report layout comes from the report descriptor (HIDReport.c), with the
 *    boot mouse format as the fallback.
 *  - MinkarUSBHIDInterface: a HID interface without the boot subclass.
 *    From its report descriptor it makes a MinkarUSBKeyboard, a
 *    MinkarUSBPointer or both (a KVM switch's keyboard and mouse share
 *    one interface), and feeds them its reports.
 *
 * The UIM below (MinkarXHCIPort) is synchronous: each driver reads its
 * interrupt-IN pipe on a kernel thread of its own (MinkarUSBHIDReader).
 * The pointer publishes what it last dispatched (MinkarEvents, MinkarX,
 * MinkarY, MinkarButtons; MinkarAbsolutePointer on a tablet) and the
 * keyboard its count and last key code,
 * for tests and diagnosis.
 */

#ifndef _MINKARUSB_H
#define _MINKARUSB_H

#include <IOKit/IOService.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <kern/thread.h>

extern "C" {
#include "HIDReport.h"
}

class MinkarUSBComposite : public IOService
{
	OSDeclareDefaultStructors(MinkarUSBComposite);

	IOUSBDevice *	fDevice;

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

/* Reads an interrupt-IN pipe on its own thread and hands each report on. */
class MinkarUSBHIDReader : public OSObject
{
	OSDeclareDefaultStructors(MinkarUSBHIDReader);

public:
	typedef void (*Handler)(OSObject *owner, const uint8_t *report, uint32_t len);

	static MinkarUSBHIDReader *withInterface(IOUSBInterface *intf, OSObject *owner,
	    Handler handler);
	bool startReading();
	void stopReading();		/* returns once the thread has ended */

	virtual void free() APPLE_KEXT_OVERRIDE;

private:
	static void threadMain(void *arg, wait_result_t wr);

	IOUSBInterface *		fInterface;
	IOUSBPipe *			fPipe;
	IOBufferMemoryDescriptor *	fBuffer;
	uint32_t			fMaxPacket;
	OSObject *			fOwner;
	Handler				fHandler;
	volatile bool			fStop;
	volatile bool			fRunning;
};

/* HID class requests to the interface. */
IOReturn MinkarUSBHIDRequest(IOUSBInterface *intf, bool in, UInt8 request,
    UInt16 value, void *data, UInt16 length);

/* The interface's report descriptor; free it with IOFree(desc, len). */
bool MinkarUSBReadReportDescriptor(IOUSBInterface *intf, uint8_t **desc, uint16_t *len);

class MinkarUSBKeyboard;
class MinkarUSBPointer;

/* A HID interface in report protocol: a keyboard, a pointer, or both. */
class MinkarUSBHIDInterface : public IOService
{
	OSDeclareDefaultStructors(MinkarUSBHIDInterface);

	IOUSBInterface *		fInterface;
	MinkarUSBHIDReader *		fReader;
	MinkarUSBKeyboard *		fKeyboard;
	MinkarUSBPointer *		fPointer;
	struct HIDKeyboardLayout	fKeyboardLayout;
	struct HIDPointerLayout		fPointerLayout;

	static void handleReport(OSObject *owner, const uint8_t *report, uint32_t len);

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

	IOUSBInterface *interface() const { return fInterface; }
	const struct HIDKeyboardLayout &keyboardLayout() const { return fKeyboardLayout; }
	const struct HIDPointerLayout &pointerLayout() const { return fPointerLayout; }
};

class MinkarUSBKeyboard : public IOHIKeyboard
{
	OSDeclareDefaultStructors(MinkarUSBKeyboard);

	IOUSBInterface *	fInterface;
	MinkarUSBHIDReader *	fReader;
	uint8_t			fLastReport[8];
	uint32_t		fEvents;
	bool			fReportMode;	/* fed by MinkarUSBHIDInterface */
	struct HIDKeyboardLayout fLayout;

	static void handleReport(OSObject *owner, const uint8_t *report, uint32_t len);
	void processReport(const uint8_t *report, uint32_t len);
	void dispatchUsage(uint8_t usage, bool down);

protected:
	virtual const unsigned char *defaultKeymapOfLength(UInt32 *length) APPLE_KEXT_OVERRIDE;
	virtual void setAlphaLockFeedback(bool locked) APPLE_KEXT_OVERRIDE;
	virtual UInt32 maxKeyCodes() APPLE_KEXT_OVERRIDE;

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual UInt32 deviceType() APPLE_KEXT_OVERRIDE;
	virtual UInt32 interfaceID() APPLE_KEXT_OVERRIDE;
	void feedReport(const uint8_t *report, uint32_t len);
};

class MinkarUSBPointer : public IOHIPointing
{
	OSDeclareDefaultStructors(MinkarUSBPointer);

	IOUSBInterface *	fInterface;
	MinkarUSBHIDReader *	fReader;
	struct HIDPointerLayout	fLayout;
	bool			fBootMouse;
	uint32_t		fEvents;

	static void handleReport(OSObject *owner, const uint8_t *report, uint32_t len);
	void processReport(const uint8_t *report, uint32_t len);
	bool readLayout();
	void publish(int32_t x, int32_t y, uint32_t buttons, bool absolute);

public:
	void feedReport(const uint8_t *report, uint32_t len);

protected:
	virtual IOItemCount buttonCount() APPLE_KEXT_OVERRIDE;
	virtual IOFixed resolution() APPLE_KEXT_OVERRIDE;

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#endif /* !_MINKARUSB_H */
