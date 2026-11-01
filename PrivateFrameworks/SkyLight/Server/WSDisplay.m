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

#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <math.h>
#include <stdio.h>
#import "WSDisplay.h"

/* Method indices of the IOFramebuffer server connection. */
enum {
	kFBMethodGetPixelInformation = 1,	/* in: mode, depth, aperture */
	kFBMethodGetCurrentDisplayMode = 2,	/* out: mode, depth */
	kFBMethodSetDisplayMode = 4,		/* in: mode, depth */
	kFBMethodGetInformationForDisplayMode = 5, /* in: mode */
	kFBMethodGetDisplayModeCount = 6,	/* out: count */
	kFBMethodGetDisplayModes = 7,		/* out: mode IDs */
};

/* Without an EDID size: 72 dots per inch. */
static const double kPixelsPerMM = 72.0 / 25.4;

@implementation WSDisplay {
	io_service_t _service;
	io_connect_t _connect;
	SLSDisplayMode _modes[kSLSMaxDisplayModes];
	uint32_t _currentModeID;
	float _transfer[3 * kSLSGammaTableCapacity];
}

+ (NSArray<WSDisplay *> *)availableDisplays
{
	NSMutableArray *displays = [NSMutableArray array];
	io_iterator_t iter = IO_OBJECT_NULL;
	io_service_t service;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOFramebuffer"),
	    &iter) != KERN_SUCCESS)
		return displays;
	while ((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
		WSDisplay *d = [[WSDisplay alloc] initWithService:service];
		IOObjectRelease(service);	/* the display holds its own reference */
		if (d != nil) {
			d.unit = (uint32_t)displays.count;
			[displays addObject:d];
		}
	}
	IOObjectRelease(iter);
	/* The first is the main display, at the origin. */
	[[displays firstObject] markAsMain];
	return displays;
}

/* A non-zero ID from the registry entry: stable for one boot only.
 * TODO(WS1): derive it from the EDID (vendor, model, serial) when there is
 * one, so it survives a restart as on the reference platform. */
static uint32_t
displayIDForService(io_service_t service)
{
	uint64_t entryID = 0;

	if (IORegistryEntryGetRegistryEntryID(service, &entryID) != KERN_SUCCESS)
		return 1;
	uint32_t ID = (uint32_t)(entryID ^ (entryID >> 32));
	return ID ? ID : 1;
}

/* A number from the attached display's EDID-derived properties. */
static uint32_t
displayInfoNumber(io_service_t framebuffer, CFStringRef key)
{
	uint32_t value = 0;
	io_service_t display = IODisplayForFramebuffer(framebuffer, 0);

	if (display == IO_OBJECT_NULL)
		return 0;
	CFDictionaryRef info = IODisplayCreateInfoDictionary(display, kIODisplayOnlyPreferredName);
	IOObjectRelease(display);
	if (info == NULL)
		return 0;
	CFNumberRef number = CFDictionaryGetValue(info, key);
	if (number != NULL && CFGetTypeID(number) == CFNumberGetTypeID())
		CFNumberGetValue(number, kCFNumberSInt32Type, &value);
	CFRelease(info);
	return value;
}

- (instancetype)initWithService:(io_service_t)service
{
	kern_return_t kr;

	if ((self = [super init]) == nil)
		return nil;
	IOObjectRetain(service);
	_service = service;
	_displayID = displayIDForService(service);

	kr = IOServiceOpen(service, mach_task_self(), kIOFBServerConnectType, &_connect);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "WindowServer: framebuffer 0x%x: no server connection (0x%x)\n",
		    _displayID, kr);
		return nil;
	}
	uint64_t current[2] = { 0, 0 };
	uint32_t count = 2;
	kr = IOConnectCallScalarMethod(_connect, kFBMethodGetCurrentDisplayMode, NULL, 0,
	    current, &count);
	if (kr != KERN_SUCCESS)
		return nil;
	_currentModeID = (uint32_t)current[0];
	[self loadModes];
	const SLSDisplayMode *mode = [self currentMode];
	if (mode == NULL || mode->pixelWidth == 0 || mode->pixelHeight == 0) {
		fprintf(stderr, "WindowServer: framebuffer 0x%x: no usable current mode\n",
		    _displayID);
		return nil;
	}
	_width = mode->pixelWidth;
	_height = mode->pixelHeight;
	_flags = kSLSDisplayActive | kSLSDisplayOnline;
	[self restoreTransfer];

	_vendor = displayInfoNumber(service, CFSTR(kDisplayVendorID));
	_model = displayInfoNumber(service, CFSTR(kDisplayProductID));
	_serial = displayInfoNumber(service, CFSTR(kDisplaySerialNumber));
	uint32_t hmm = displayInfoNumber(service, CFSTR(kDisplayHorizontalImageSize));
	uint32_t vmm = displayInfoNumber(service, CFSTR(kDisplayVerticalImageSize));
	_widthMM = hmm ? hmm : _width / kPixelsPerMM;
	_heightMM = vmm ? vmm : _height / kPixelsPerMM;
	return self;
}

/* One mode's description, or NO when the framebuffer has none for it. */
- (BOOL)describeMode:(uint32_t)modeID into:(SLSDisplayMode *)mode
{
	IODisplayModeInformation info;
	IOPixelInformation pixels;
	uint64_t in[3] = { modeID, 0, kIOFBSystemAperture };
	size_t size = sizeof(info);

	memset(mode, 0, sizeof(*mode));
	memset(&info, 0, sizeof(info));
	if (IOConnectCallMethod(_connect, kFBMethodGetInformationForDisplayMode, in, 1,
	    NULL, 0, NULL, NULL, &info, &size) != KERN_SUCCESS)
		return NO;
	/* The deepest depth of the mode; ours all draw 32 bits per pixel. */
	in[1] = info.maxDepthIndex;
	size = sizeof(pixels);
	memset(&pixels, 0, sizeof(pixels));
	if (IOConnectCallMethod(_connect, kFBMethodGetPixelInformation, in, 3, NULL, 0,
	    NULL, NULL, &pixels, &size) != KERN_SUCCESS)
		pixels.bitsPerPixel = 0;
	mode->modeID = modeID;
	mode->flags = info.flags;
	mode->pixelWidth = pixels.activeWidth ? pixels.activeWidth : info.nominalWidth;
	mode->pixelHeight = pixels.activeHeight ? pixels.activeHeight : info.nominalHeight;
	mode->width = mode->pixelWidth;
	mode->height = mode->pixelHeight;
	mode->depth = pixels.bitsPerPixel;
	mode->refreshRate = info.refreshRate / 65536.0;
	return mode->pixelWidth != 0 && mode->pixelHeight != 0;
}

- (void)loadModes
{
	IODisplayModeID ids[kSLSMaxDisplayModes];
	uint64_t total = 0;
	uint32_t one = 1;
	size_t size = sizeof(ids);

	_modeCount = 0;
	if (IOConnectCallScalarMethod(_connect, kFBMethodGetDisplayModeCount, NULL, 0,
	    &total, &one) != KERN_SUCCESS || total == 0)
		total = 0;
	if (total > kSLSMaxDisplayModes)
		total = kSLSMaxDisplayModes;
	size = (size_t)total * sizeof(IODisplayModeID);
	if (total != 0 && IOConnectCallStructMethod(_connect, kFBMethodGetDisplayModes,
	    NULL, 0, ids, &size) != KERN_SUCCESS)
		total = 0;
	for (uint64_t i = 0; i < total && i < size / sizeof(IODisplayModeID); i++)
		if ([self describeMode:(uint32_t)ids[i] into:&_modes[_modeCount]])
			_modeCount++;
	/* A framebuffer that lists no modes still has the one it is in. */
	if ([self modeWithID:_currentModeID] == NULL && _modeCount < kSLSMaxDisplayModes &&
	    [self describeMode:_currentModeID into:&_modes[_modeCount]])
		_modeCount++;
}

- (const SLSDisplayMode *)modeAtIndex:(uint32_t)index
{
	return index < _modeCount ? &_modes[index] : NULL;
}

- (const SLSDisplayMode *)modeWithID:(uint32_t)modeID
{
	for (uint32_t i = 0; i < _modeCount; i++)
		if (_modes[i].modeID == modeID)
			return &_modes[i];
	return NULL;
}

- (const SLSDisplayMode *)currentMode
{
	return [self modeWithID:_currentModeID];
}

- (int)setModeID:(uint32_t)modeID
{
	const SLSDisplayMode *mode = [self modeWithID:modeID];

	if (mode == NULL)
		return kSLSErrorIllegalArgument;
	if (modeID == _currentModeID)
		return kSLSErrorSuccess;
	/* Depth index 0: every mode is drawn at 32 bits per pixel. */
	uint64_t in[2] = { modeID, 0 };
	if (IOConnectCallScalarMethod(_connect, kFBMethodSetDisplayMode, in, 2, NULL,
	    NULL) != KERN_SUCCESS)
		return kSLSErrorFailure;
	_currentModeID = modeID;
	_width = mode->pixelWidth;
	_height = mode->pixelHeight;
	return kSLSErrorSuccess;
}

/* ---- Transfer tables ---- */

static const SLSGammaFormula identityFormula = { 0, 1, 1, 0, 1, 1, 0, 1, 1 };

- (void)restoreTransfer
{
	[self setTransferFormula:&identityFormula];
}

- (int)setTransferTable:(const float *)table count:(uint32_t)count
{
	const uint32_t cap = kSLSGammaTableCapacity;
	float resampled[3 * kSLSGammaTableCapacity];

	if (count < 2 || count > cap)
		return kSLSErrorIllegalArgument;
	for (uint32_t i = 0; i < 3 * count; i++)
		if (!isfinite(table[i]))
			return kSLSErrorIllegalArgument;
	/* Linear interpolation to the capacity, each value clamped to 0..1. */
	for (uint32_t c = 0; c < 3; c++) {
		const float *in = table + c * count;

		for (uint32_t i = 0; i < cap; i++) {
			double pos = (double)i * (count - 1) / (cap - 1);
			uint32_t lo = (uint32_t)pos;
			uint32_t hi = lo + 1 < count ? lo + 1 : lo;
			double v = in[lo] + (in[hi] - in[lo]) * (pos - lo);

			resampled[c * cap + i] = (float)fmin(1.0, fmax(0.0, v));
		}
	}
	memcpy(_transfer, resampled, sizeof(_transfer));
	_transferFormula = identityFormula;
	return kSLSErrorSuccess;
}

- (void)getTransferTable:(float *)table
{
	memcpy(table, _transfer, sizeof(_transfer));
}

- (int)setTransferFormula:(const SLSGammaFormula *)f
{
	const float *v = (const float *)f;
	const uint32_t cap = kSLSGammaTableCapacity;

	for (int i = 0; i < kSLSGammaFormulaWords; i++)
		if (!isfinite(v[i]))
			return kSLSErrorIllegalArgument;
	for (uint32_t c = 0; c < 3; c++) {
		double lo = v[3 * c], hi = v[3 * c + 1], gamma = v[3 * c + 2];

		if (gamma <= 0 || lo < 0 || hi > 1 || lo > hi)
			return kSLSErrorIllegalArgument;
	}
	for (uint32_t c = 0; c < 3; c++) {
		double lo = v[3 * c], hi = v[3 * c + 1], gamma = v[3 * c + 2];

		for (uint32_t i = 0; i < cap; i++)
			_transfer[c * cap + i] = (float)(lo + (hi - lo) * pow((double)i / (cap - 1), gamma));
	}
	_transferFormula = *f;
	return kSLSErrorSuccess;
}

- (void)dealloc
{
	if (_connect != IO_OBJECT_NULL)
		IOServiceClose(_connect);
	IOObjectRelease(_service);
}

- (void)markAsMain
{
	_flags |= kSLSDisplayPrimary | kSLSDisplayMain;
}

- (void)getInfo:(SLSDisplayInfo *)info
{
	memset(info, 0, sizeof(*info));
	info->display = _displayID;
	info->flags = _flags;
	info->x = _x;
	info->y = _y;
	info->width = _width;
	info->height = _height;
	info->vendor = _vendor;
	info->model = _model;
	info->serial = _serial;
	info->unit = _unit;
	info->rotation = 0;
	info->widthMM = _widthMM;
	info->heightMM = _heightMM;
	info->mirrorOf = 0;
	info->primary = _displayID;	/* no hardware mirroring yet */
	info->openGLMask = 1U << _unit;
}

@end
