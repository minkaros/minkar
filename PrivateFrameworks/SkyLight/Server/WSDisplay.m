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
#include <unistd.h>
#import "WSDisplay.h"
#include "MinkarAMDGPUInterface.h"

/* Method indices of the IOFramebuffer server connection. */
enum {
	kFBMethodGetPixelInformation = 1,	/* in: mode, depth, aperture */
	kFBMethodGetCurrentDisplayMode = 2,	/* out: mode, depth */
	kFBMethodSetDisplayMode = 4,		/* in: mode, depth */
	kFBMethodGetInformationForDisplayMode = 5, /* in: mode */
	kFBMethodGetDisplayModeCount = 6,	/* out: count */
	kFBMethodGetDisplayModes = 7,		/* out: mode IDs */
	kFBMethodSetAttribute = 19,		/* in: attribute, value */
};

/* Minkar's flush attributes: a rect's origin, then its size (each as
 * a << 16 | b); the size flushes. Drivers that do not know them fail. */
enum {
	kIOFBMinkarFlushOrigin = 'mfbo',
	kIOFBMinkarFlushSize = 'mfbs',
};

/* Without an EDID size: 72 dots per inch. */
static const double kPixelsPerMM = 72.0 / 25.4;

@implementation WSDisplay {
	io_service_t _service;
	io_connect_t _connect;
	mach_vm_address_t _vram;
	mach_vm_size_t _vramSize;
	SLSDisplayMode _modes[kSLSMaxDisplayModes];
	uint32_t _currentModeID;
	BOOL _noFlush;		/* the driver does not take flushes */
	io_connect_t _gpu;	/* the companion GPU's user client */
	uint64_t _gpuShadow;	/* the shadow, imported: its buffer */
	/* A GPU compositor's frames: shared VRAM buffers, their CPU mappings;
	 * which one the display shows (-1: its framebuffer). */
	uint64_t _gpuFrames[2];
	mach_vm_address_t _gpuFrameCPU[2];
	unsigned _gpuFrameCount;
	int _gpuShown;
	uint32_t _gpuRenderMinor;
	uint64_t _gpuEntryID;
	float _transfer[3 * kSLSGammaTableCapacity];
}

/* Sorts a display whose device is marked AAPL,boot-display (the property
 * IOGraphics knows the boot display by) before the others, and those by
 * their registry path: their PCI places, the same from boot to boot.
 * TODO(Phase 4): mark the device that holds the firmware's framebuffer. */
static NSString *
displayOrderKey(io_service_t framebuffer)
{
	CFTypeRef boot = IORegistryEntrySearchCFProperty(framebuffer, kIOServicePlane,
	    CFSTR("AAPL,boot-display"), kCFAllocatorDefault,
	    kIORegistryIterateRecursively | kIORegistryIterateParents);
	io_string_t path = "";

	if (boot != NULL)
		CFRelease(boot);
	IORegistryEntryGetPath(framebuffer, kIOServicePlane, path);
	return [NSString stringWithFormat:@"%d %s", boot != NULL ? 0 : 1, path];
}

+ (NSArray<WSDisplay *> *)availableDisplays
{
	NSMutableArray *displays = [NSMutableArray array];
	io_iterator_t iter = IO_OBJECT_NULL;
	io_service_t service;

	if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOFramebuffer"),
	    &iter) != KERN_SUCCESS)
		return displays;
	NSMutableDictionary<NSValue *, NSString *> *order = [NSMutableDictionary dictionary];
	while ((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
		WSDisplay *d = [[WSDisplay alloc] initWithService:service];
		if (d != nil) {
			order[[NSValue valueWithNonretainedObject:d]] = displayOrderKey(service);
			[displays addObject:d];
		}
		IOObjectRelease(service);	/* the display holds its own reference */
	}
	IOObjectRelease(iter);
	/* The registry lists the framebuffers in the order their drivers
	 * started, which changes from boot to boot: the boot display first,
	 * then the others by their place in the registry. */
	[displays sortUsingComparator:^NSComparisonResult(WSDisplay *a, WSDisplay *b) {
		return [order[[NSValue valueWithNonretainedObject:a]]
		    compare:order[[NSValue valueWithNonretainedObject:b]]];
	}];
	for (NSUInteger i = 0; i < displays.count; i++)
		((WSDisplay *)displays[i]).unit = (uint32_t)i;
	/* The first is the main display, at the origin; each other one to the
	 * right of the one before, their tops aligned: one desktop across
	 * them, as a display joins it on the reference platform. A saved
	 * configuration (WSDisplayServices) places them otherwise. */
	[[displays firstObject] markAsMain];
	double x = 0;
	for (WSDisplay *d in displays) {
		d.x = x;
		d.y = 0;
		x += d.width;
	}
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

- (NSString *)persistentKey
{
	/* 'unkn': IODisplay's vendor for a display with no EDID. */
	if ((self.vendor != 0 || self.model != 0) && self.vendor != 0x756e6b6e)
		return [NSString stringWithFormat:@"edid:%x:%x:%x", self.vendor, self.model,
		    self.serial];
	io_string_t path;
	if (IORegistryEntryGetPath(_service, kIOServicePlane, path) == KERN_SUCCESS)
		return [NSString stringWithFormat:@"path:%s", path];
	return [NSString stringWithFormat:@"unit:%u", self.unit];
}

- (instancetype)initWithService:(io_service_t)service
{
	kern_return_t kr;

	if ((self = [super init]) == nil)
		return nil;
	IOObjectRetain(service);
	_service = service;
	_displayID = displayIDForService(service);
	_menuBarHeight = kSLSMenuBarHeight;

	/* The framebuffer takes one server at a time: a window server that
	 * just quit may still hold it for a moment (launchd starts the next
	 * one at once), so it is asked again for a few seconds. */
	kr = IOServiceOpen(service, mach_task_self(), kIOFBServerConnectType, &_connect);
	for (int tries = 0; kr == kIOReturnUnsupported && tries < 50; tries++) {
		usleep(100000);
		kr = IOServiceOpen(service, mach_task_self(), kIOFBServerConnectType, &_connect);
	}
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
	/* Again: a scaled mode runs at the refresh rate of the timing in use,
	 * which may have changed with this one. */
	[self loadModes];
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

- (BOOL)mapFramebuffer:(uint8_t **)base rowBytes:(uint32_t *)rowBytes
{
	IOPixelInformation pixels;
	uint64_t query[3] = { _currentModeID, 0, kIOFBSystemAperture };
	size_t size = sizeof(pixels);
	IOOptionBits options = kIOMapAnywhere | kIOMapWriteCombineCache;

	memset(&pixels, 0, sizeof(pixels));
	if (IOConnectCallMethod(_connect, kFBMethodGetPixelInformation, query, 3, NULL, 0,
	    NULL, NULL, &pixels, &size) != KERN_SUCCESS || pixels.bitsPerPixel != 32)
		return NO;
	if (_vram != 0) {
		IOConnectUnmapMemory64(_connect, kIOFBSystemAperture, mach_task_self(), _vram);
		_vram = 0;
	}
	if (IOConnectMapMemory64(_connect, kIOFBSystemAperture, mach_task_self(), &_vram,
	    &_vramSize, options) != KERN_SUCCESS) {
		_vram = 0;
		return NO;
	}
	if (_vramSize < (mach_vm_size_t)pixels.bytesPerRow * _height)
		return NO;
	*base = (uint8_t *)(uintptr_t)_vram;
	*rowBytes = pixels.bytesPerRow;
	return YES;
}

/* The framebuffer's sibling on the same device, of the class it names;
 * its render node in *minor (0 if none) and its registry entry ID in
 * *entryID. */
static io_connect_t
openAccelerator(io_service_t framebuffer, uint32_t *minor, uint64_t *entryID)
{
	CFTypeRef name = IORegistryEntryCreateCFProperty(framebuffer,
	    CFSTR("MinkarAcceleratorClass"), kCFAllocatorDefault, 0);
	char class[64];
	io_registry_entry_t parent = IO_OBJECT_NULL;
	io_iterator_t it = IO_OBJECT_NULL;
	io_service_t s;
	io_connect_t connect = IO_OBJECT_NULL;

	if (name == NULL || CFGetTypeID(name) != CFStringGetTypeID() ||
	    !CFStringGetCString(name, class, sizeof(class), kCFStringEncodingUTF8)) {
		if (name != NULL)
			CFRelease(name);
		return IO_OBJECT_NULL;
	}
	CFRelease(name);
	if (IORegistryEntryGetParentEntry(framebuffer, kIOServicePlane, &parent) != KERN_SUCCESS)
		return IO_OBJECT_NULL;
	if (IORegistryEntryGetChildIterator(parent, kIOServicePlane, &it) == KERN_SUCCESS) {
		while (connect == IO_OBJECT_NULL && (s = IOIteratorNext(it)) != IO_OBJECT_NULL) {
			if (IOObjectConformsTo(s, class) &&
			    IOServiceOpen(s, mach_task_self(), 0, &connect) != KERN_SUCCESS)
				connect = IO_OBJECT_NULL;
			if (connect != IO_OBJECT_NULL && entryID != NULL)
				IORegistryEntryGetRegistryEntryID(s, entryID);
			if (connect != IO_OBJECT_NULL && minor != NULL) {
				CFTypeRef m = IORegistryEntryCreateCFProperty(s, CFSTR("MinkarDRMMinor"),
				    kCFAllocatorDefault, 0);
				*minor = 0;
				if (m != NULL && CFGetTypeID(m) == CFNumberGetTypeID())
					CFNumberGetValue(m, kCFNumberSInt32Type, minor);
				if (m != NULL)
					CFRelease(m);
			}
			IOObjectRelease(s);
		}
		IOObjectRelease(it);
	}
	IOObjectRelease(parent);
	return connect;
}

- (BOOL)setUpGPUPresentFrom:(void *)shadow bytes:(size_t)bytes
{
	[self tearDownGPUPresent];
	if (getenv("WSNoGPUPresent") != NULL)
		return NO;
	if (_gpu == IO_OBJECT_NULL)
		_gpu = openAccelerator(_service, &_gpuRenderMinor, &_gpuEntryID);
	if (_gpu == IO_OBJECT_NULL)
		return NO;
	uint64_t in[2] = { (uint64_t)(uintptr_t)shadow, bytes }, out[2];
	uint32_t n = 2;
	if (IOConnectCallScalarMethod(_gpu, kMinkarAMDGPUImportMemory, in, 2, out, &n) !=
	    KERN_SUCCESS)
		return NO;
	_gpuShadow = out[0];
	return YES;
}

- (BOOL)setUpGPUFrames:(unsigned)count bytes:(size_t)bytes names:(uint64_t *)names
    cpu:(uint8_t **)cpu
{
	[self tearDownGPUPresent];
	if (_gpu == IO_OBJECT_NULL)
		_gpu = openAccelerator(_service, &_gpuRenderMinor, &_gpuEntryID);
	if (_gpu == IO_OBJECT_NULL || count == 0 || count > 2)
		return NO;
	for (unsigned i = 0; i < count; i++) {
		uint64_t in = bytes, out[3] = { 0, 0, 0 };
		uint32_t n = 3;
		mach_vm_size_t size = 0;
		if (IOConnectCallScalarMethod(_gpu, kMinkarAMDGPUCreateSharedBuffer, &in, 1, out, &n) !=
		    KERN_SUCCESS) {
			[self tearDownGPUPresent];
			return NO;
		}
		_gpuFrames[i] = out[0];
		_gpuFrameCount = i + 1;
		if (IOConnectMapMemory64(_gpu, (uint32_t)out[0], mach_task_self(), &_gpuFrameCPU[i],
		    &size, kIOMapAnywhere) != KERN_SUCCESS) {
			[self tearDownGPUPresent];
			return NO;
		}
		names[i] = out[2];
		cpu[i] = (uint8_t *)(uintptr_t)_gpuFrameCPU[i];
	}
	_gpuShown = -1;
	return YES;
}

- (BOOL)gpuPresentFrame:(unsigned)index offset:(size_t)offset bytes:(size_t)bytes
{
	uint64_t in[4] = { index < _gpuFrameCount ? _gpuFrames[index] : 0, offset, offset, bytes };
	uint64_t ns;
	uint32_t n = 1;

	return in[0] != 0 && IOConnectCallScalarMethod(_gpu, kMinkarAMDGPUCopyToScanout, in, 4,
	    &ns, &n) == KERN_SUCCESS;
}

- (BOOL)flipToGPUFrame:(int)index
{
	uint64_t in = index >= 0 && (unsigned)index < _gpuFrameCount ? _gpuFrames[index] : 0, ns;
	uint32_t n = 1;

	if (index >= 0 && in == 0)
		return NO;
	if (IOConnectCallScalarMethod(_gpu, kMinkarAMDGPUFlip, &in, 1, &ns, &n) != KERN_SUCCESS)
		return NO;
	_gpuShown = index;
	return YES;
}

- (uint32_t)gpuRenderMinor
{
	if (_gpu == IO_OBJECT_NULL)
		_gpu = openAccelerator(_service, &_gpuRenderMinor, &_gpuEntryID);
	return _gpuRenderMinor;
}

- (uint64_t)gpuEntryID
{
	if (_gpu == IO_OBJECT_NULL)
		_gpu = openAccelerator(_service, &_gpuRenderMinor, &_gpuEntryID);
	return _gpuEntryID;
}

- (void)tearDownGPUPresent
{
	/* The display shows its framebuffer again before the frames go. */
	if (_gpuFrameCount > 0 && _gpuShown >= 0)
		[self flipToGPUFrame:-1];
	for (unsigned i = 0; i < _gpuFrameCount; i++) {
		uint64_t h = _gpuFrames[i];
		if (_gpuFrameCPU[i] != 0)
			IOConnectUnmapMemory64(_gpu, (uint32_t)h, mach_task_self(), _gpuFrameCPU[i]);
		IOConnectCallScalarMethod(_gpu, kMinkarAMDGPUDestroyBuffer, &h, 1, NULL, NULL);
		_gpuFrames[i] = 0;
		_gpuFrameCPU[i] = 0;
	}
	_gpuFrameCount = 0;
	_gpuShown = -1;
	if (_gpuShadow != 0) {
		uint64_t h = _gpuShadow;
		IOConnectCallScalarMethod(_gpu, kMinkarAMDGPUDestroyBuffer, &h, 1, NULL, NULL);
		_gpuShadow = 0;
	}
}

- (BOOL)gpuPresentOffset:(size_t)offset bytes:(size_t)bytes
{
	uint64_t in[4] = { _gpuShadow, offset, offset, bytes }, ns;
	uint32_t n = 1;

	return _gpuShadow != 0 && IOConnectCallScalarMethod(_gpu, kMinkarAMDGPUCopyToScanout, in, 4,
	    &ns, &n) == KERN_SUCCESS;
}

- (void)flushX:(uint32_t)x y:(uint32_t)y width:(uint32_t)width height:(uint32_t)height
{
	if (_noFlush || width == 0 || height == 0)
		return;
	uint64_t origin[2] = { kIOFBMinkarFlushOrigin, (uint64_t)(x << 16 | y) };
	uint64_t size[2] = { kIOFBMinkarFlushSize, (uint64_t)(width << 16 | height) };
	if (IOConnectCallScalarMethod(_connect, kFBMethodSetAttribute, origin, 2, NULL, NULL) !=
	        KERN_SUCCESS ||
	    IOConnectCallScalarMethod(_connect, kFBMethodSetAttribute, size, 2, NULL, NULL) !=
	        KERN_SUCCESS)
		_noFlush = YES;
}

- (void)dealloc
{
	[self tearDownGPUPresent];
	if (_gpu != IO_OBJECT_NULL)
		IOServiceClose(_gpu);
	if (_vram != 0)
		IOConnectUnmapMemory64(_connect, kIOFBSystemAperture, mach_task_self(), _vram);
	if (_connect != IO_OBJECT_NULL)
		IOServiceClose(_connect);
	IOObjectRelease(_service);
}

- (void)setMirrorOf:(SLSDisplayID)mirrorOf
{
	_mirrorOf = mirrorOf;
	if (mirrorOf)
		_flags |= kSLSDisplayMirrored;
	else
		_flags &= ~kSLSDisplayMirrored;
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
	info->mirrorOf = _mirrorOf;
	/* Software mirroring: the primary of a mirror set is what it mirrors. */
	info->primary = _mirrorOf ? _mirrorOf : _displayID;
	info->openGLMask = 1U << _unit;
}

@end
