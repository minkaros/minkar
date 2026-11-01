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
#include <stdio.h>
#import "WSDisplay.h"

/* Method indices of the IOFramebuffer server connection. */
enum {
	kFBMethodGetPixelInformation = 1,	/* in: mode, depth, aperture */
	kFBMethodGetCurrentDisplayMode = 2,	/* out: mode, depth */
};

/* Without an EDID size: 72 dots per inch. */
static const double kPixelsPerMM = 72.0 / 25.4;

@implementation WSDisplay {
	io_service_t _service;
	io_connect_t _connect;
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

/* A stable, non-zero ID derived from the registry entry. */
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
	uint64_t query[3] = { current[0], current[1], kIOFBSystemAperture };
	IOPixelInformation pixels;
	size_t size = sizeof(pixels);
	memset(&pixels, 0, sizeof(pixels));
	kr = IOConnectCallMethod(_connect, kFBMethodGetPixelInformation, query, 3, NULL, 0,
	    NULL, NULL, &pixels, &size);
	if (kr != KERN_SUCCESS || pixels.activeWidth == 0 || pixels.activeHeight == 0)
		return nil;
	_width = pixels.activeWidth;
	_height = pixels.activeHeight;
	_flags = kSLSDisplayActive | kSLSDisplayOnline;

	_vendor = displayInfoNumber(service, CFSTR(kDisplayVendorID));
	_model = displayInfoNumber(service, CFSTR(kDisplayProductID));
	_serial = displayInfoNumber(service, CFSTR(kDisplaySerialNumber));
	uint32_t hmm = displayInfoNumber(service, CFSTR(kDisplayHorizontalImageSize));
	uint32_t vmm = displayInfoNumber(service, CFSTR(kDisplayVerticalImageSize));
	_widthMM = hmm ? hmm : _width / kPixelsPerMM;
	_heightMM = vmm ? vmm : _height / kPixelsPerMM;
	return self;
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
