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
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hidsystem/IOLLEvent.h>
#include <IOKit/usb/USBSpec.h>

/*
 * The US keymap of ADB key codes the PS/2 keyboard uses too: an APSL
 * data file of the tree (ApplePS2Controller), included rather than copied.
 */
#include "../ApplePS2Controller/ApplePS2KeyboardMap.h"

#define super IOHIKeyboard
#define LOG(fmt, ...) IOLog("MinkarUSBKeyboard: " fmt "\n", ##__VA_ARGS__)

OSDefineMetaClassAndStructors(MinkarUSBKeyboard, IOHIKeyboard);

/*
 * HID keyboard usage IDs (page 0x07, HID Usage Tables) to the ADB key
 * codes of IOHIKeyboard's keymap. Usages not listed have no key.
 */
static const struct { uint8_t usage, adb; } kUsageToADB[] = {
	/* letters a..z */
	{ 0x04, 0x00 }, { 0x05, 0x0B }, { 0x06, 0x08 }, { 0x07, 0x02 }, { 0x08, 0x0E },
	{ 0x09, 0x03 }, { 0x0A, 0x05 }, { 0x0B, 0x04 }, { 0x0C, 0x22 }, { 0x0D, 0x26 },
	{ 0x0E, 0x28 }, { 0x0F, 0x25 }, { 0x10, 0x2E }, { 0x11, 0x2D }, { 0x12, 0x1F },
	{ 0x13, 0x23 }, { 0x14, 0x0C }, { 0x15, 0x0F }, { 0x16, 0x01 }, { 0x17, 0x11 },
	{ 0x18, 0x20 }, { 0x19, 0x09 }, { 0x1A, 0x0D }, { 0x1B, 0x07 }, { 0x1C, 0x10 },
	{ 0x1D, 0x06 },
	/* digits 1..9, 0 */
	{ 0x1E, 0x12 }, { 0x1F, 0x13 }, { 0x20, 0x14 }, { 0x21, 0x15 }, { 0x22, 0x17 },
	{ 0x23, 0x16 }, { 0x24, 0x1A }, { 0x25, 0x1C }, { 0x26, 0x19 }, { 0x27, 0x1D },
	/* return, escape, backspace, tab, space, punctuation */
	{ 0x28, 0x24 }, { 0x29, 0x35 }, { 0x2A, 0x33 }, { 0x2B, 0x30 }, { 0x2C, 0x31 },
	{ 0x2D, 0x1B }, { 0x2E, 0x18 }, { 0x2F, 0x21 }, { 0x30, 0x1E }, { 0x31, 0x2A },
	{ 0x32, 0x2A }, { 0x33, 0x29 }, { 0x34, 0x27 }, { 0x35, 0x32 }, { 0x36, 0x2B },
	{ 0x37, 0x2F }, { 0x38, 0x2C }, { 0x39, 0x39 },
	/* F1..F12 */
	{ 0x3A, 0x7A }, { 0x3B, 0x78 }, { 0x3C, 0x63 }, { 0x3D, 0x76 }, { 0x3E, 0x60 },
	{ 0x3F, 0x61 }, { 0x40, 0x62 }, { 0x41, 0x64 }, { 0x42, 0x65 }, { 0x43, 0x6D },
	{ 0x44, 0x67 }, { 0x45, 0x6F },
	/* print screen, scroll lock, pause (as F13..F15), navigation */
	{ 0x46, 0x69 }, { 0x47, 0x6B }, { 0x48, 0x71 }, { 0x49, 0x72 }, { 0x4A, 0x73 },
	{ 0x4B, 0x74 }, { 0x4C, 0x75 }, { 0x4D, 0x77 }, { 0x4E, 0x79 }, { 0x4F, 0x7C },
	{ 0x50, 0x7B }, { 0x51, 0x7D }, { 0x52, 0x7E },
	/* keypad */
	{ 0x53, 0x47 }, { 0x54, 0x4B }, { 0x55, 0x43 }, { 0x56, 0x4E }, { 0x57, 0x45 },
	{ 0x58, 0x4C }, { 0x59, 0x53 }, { 0x5A, 0x54 }, { 0x5B, 0x55 }, { 0x5C, 0x56 },
	{ 0x5D, 0x57 }, { 0x5E, 0x58 }, { 0x5F, 0x59 }, { 0x60, 0x5B }, { 0x61, 0x5C },
	{ 0x62, 0x52 }, { 0x63, 0x41 }, { 0x67, 0x51 },
	/* the ISO key, the menu key, F13..F15 */
	{ 0x64, 0x0A }, { 0x65, 0x6E }, { 0x68, 0x69 }, { 0x69, 0x6B }, { 0x6A, 0x71 },
	/* modifiers: left control, shift, option, command; then the right ones */
	{ 0xE0, 0x3B }, { 0xE1, 0x38 }, { 0xE2, 0x3A }, { 0xE3, 0x37 },
	{ 0xE4, 0x3E }, { 0xE5, 0x3C }, { 0xE6, 0x3D }, { 0xE7, 0x36 },
};

static uint8_t
usageToADB(uint8_t usage)
{
	for (size_t i = 0; i < sizeof(kUsageToADB) / sizeof(kUsageToADB[0]); i++)
		if (kUsageToADB[i].usage == usage)
			return kUsageToADB[i].adb;
	return 0xFF;
}

bool
MinkarUSBKeyboard::start(IOService *provider)
{
	fInterface = OSDynamicCast(IOUSBInterface, provider);
	if (fInterface == NULL || !fInterface->open(this))
		return false;
	/* The boot protocol's fixed 8-byte report; a report only on change. */
	(void)MinkarUSBHIDRequest(fInterface, false, kHIDRqSetProtocol, kHIDBootProtocolValue, NULL, 0);
	(void)MinkarUSBHIDRequest(fInterface, false, kHIDRqSetIdle, 0, NULL, 0);
	fReader = MinkarUSBHIDReader::withInterface(fInterface, this, &MinkarUSBKeyboard::handleReport);
	if (fReader == NULL || !super::start(provider)) {
		OSSafeReleaseNULL(fReader);
		fInterface->close(this);
		return false;
	}
	setProperty("MinkarKeyEvents", 0ULL, 32);
	if (!fReader->startReading()) {
		LOG("cannot start the report thread");
		return false;
	}
	LOG("started (%04x:%04x interface %u, boot protocol)",
	    fInterface->GetDevice() ? fInterface->GetDevice()->GetVendorID() : 0,
	    fInterface->GetDevice() ? fInterface->GetDevice()->GetProductID() : 0,
	    fInterface->GetInterfaceNumber());
	return true;
}

void
MinkarUSBKeyboard::stop(IOService *provider)
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
MinkarUSBKeyboard::handleReport(OSObject *owner, const uint8_t *report, uint32_t len)
{
	((MinkarUSBKeyboard *)owner)->processReport(report, len);
}

void
MinkarUSBKeyboard::dispatchUsage(uint8_t usage, bool down)
{
	uint8_t adb = usageToADB(usage);
	AbsoluteTime now;

	if (adb == 0xFF)
		return;
	clock_get_uptime((uint64_t *)&now);
	dispatchKeyboardEvent(adb, down, now);
	fEvents++;
	setProperty("MinkarKeyEvents", (unsigned long long)fEvents, 32);
	setProperty("MinkarLastKeyCode", (unsigned long long)adb, 8);
}

/*
 * Boot report: modifier bits, a reserved byte, up to six usages of keys
 * held down. A key down is a usage that appeared, a key up one that went.
 */
void
MinkarUSBKeyboard::processReport(const uint8_t *r, uint32_t len)
{
	if (len < 8)
		return;
	/* Phantom state (too many keys): every slot reads ErrorRollOver. */
	if (r[2] == 0x01 && r[3] == 0x01)
		return;

	uint8_t changed = r[0] ^ fLastReport[0];
	for (int bit = 0; bit < 8; bit++)
		if (changed & (1 << bit))
			dispatchUsage((uint8_t)(0xE0 + bit), (r[0] & (1 << bit)) != 0);

	for (int i = 2; i < 8; i++) {
		uint8_t old = fLastReport[i];
		bool still = false;

		if (old < 0x04)
			continue;
		for (int j = 2; j < 8; j++)
			if (r[j] == old)
				still = true;
		if (!still)
			dispatchUsage(old, false);
	}
	for (int i = 2; i < 8; i++) {
		uint8_t now = r[i];
		bool before = false;

		if (now < 0x04)
			continue;
		for (int j = 2; j < 8; j++)
			if (fLastReport[j] == now)
				before = true;
		if (!before)
			dispatchUsage(now, true);
	}
	bcopy(r, fLastReport, 8);
}

/* Caps lock LED: bit 1 of the boot keyboard's output report. */
void
MinkarUSBKeyboard::setAlphaLockFeedback(bool locked)
{
	uint8_t leds = locked ? 0x02 : 0x00;

	if (fInterface != NULL)
		(void)MinkarUSBHIDRequest(fInterface, false, kHIDRqSetReport,
		    (UInt16)((kHIDRtOutputReport << 8) | 0), &leds, 1);
}

const unsigned char *
MinkarUSBKeyboard::defaultKeymapOfLength(UInt32 *length)
{
	*length = sizeof(applePS2USAKeyMap);
	return applePS2USAKeyMap;
}

UInt32
MinkarUSBKeyboard::maxKeyCodes()
{
	return NX_NUMKEYCODES;
}

UInt32
MinkarUSBKeyboard::deviceType()
{
	return NX_EVS_DEVICE_TYPE_KEYBOARD;
}

/* The key codes are ADB's, as the keymap expects. */
UInt32
MinkarUSBKeyboard::interfaceID()
{
	return NX_EVS_DEVICE_INTERFACE_ADB;
}
