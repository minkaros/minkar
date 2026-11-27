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

#include <pthread.h>
#import "WSSurface.h"

static uint32_t nextSurfaceID = 1;

@implementation WSSurface {
	pthread_mutex_t _lock;
	mach_vm_address_t _base;
	mach_vm_size_t _size;		/* one buffer */
	mach_vm_size_t _allocated;	/* both, in whole pages */
	uint32_t _front;
}

- (instancetype)initWithWidth:(uint32_t)width height:(uint32_t)height
{
	memory_object_size_t entrySize;

	if ((self = [super init]) == nil)
		return nil;
	pthread_mutex_init(&_lock, NULL);
	_surfaceID = nextSurfaceID++;
	_width = width;
	_height = height;
	_rowBytes = width * 4;
	/* The second buffer starts right after the first (rowBytes * height),
	 * as the client computes it; the whole is rounded to pages. */
	_size = (mach_vm_size_t)_rowBytes * height;
	_allocated = round_page(2 * _size);
	/* Zero-filled: transparent until drawn. */
	if (mach_vm_allocate(mach_task_self(), &_base, _allocated, VM_FLAGS_ANYWHERE) !=
	    KERN_SUCCESS)
		return nil;
	entrySize = _allocated;
	if (mach_make_memory_entry_64(mach_task_self(), &entrySize, _base,
	    VM_PROT_READ | VM_PROT_WRITE, &_memoryEntry, MACH_PORT_NULL) != KERN_SUCCESS ||
	    entrySize < _allocated) {
		mach_vm_deallocate(mach_task_self(), _base, _allocated);
		_base = 0;
		return nil;
	}
	_front = 1;	/* the client starts drawing into buffer 0 */
	return self;
}

- (void)dealloc
{
	if (_memoryEntry != MACH_PORT_NULL)
		mach_port_deallocate(mach_task_self(), _memoryEntry);
	if (_base != 0)
		mach_vm_deallocate(mach_task_self(), _base, _allocated);
	pthread_mutex_destroy(&_lock);
}

- (uint32_t)backBuffer
{
	pthread_mutex_lock(&_lock);
	uint32_t back = 1 - _front;
	pthread_mutex_unlock(&_lock);
	return back;
}

- (BOOL)commitBuffer:(uint32_t)buffer
{
	BOOL good;

	pthread_mutex_lock(&_lock);
	good = buffer == 1 - _front;
	if (good) {
		_front = buffer;
		_committed = YES;
	}
	pthread_mutex_unlock(&_lock);
	return good;
}

- (BOOL)live
{
	return NO;
}

- (mach_vm_address_t)memoryBase
{
	return _base;
}

- (mach_vm_size_t)memorySize
{
	return _allocated;
}

- (const uint8_t *)lockFront
{
	pthread_mutex_lock(&_lock);
	return (const uint8_t *)(uintptr_t)(_base + _front * _size);
}

- (void)unlock
{
	pthread_mutex_unlock(&_lock);
}

@end

/* IOSurface's own, for surfaces in GPU memory (Frameworks/IOSurface). */
extern uint64_t IOSurfaceGetMinkarGPUBuffer(IOSurfaceRef buffer);
extern uint64_t IOSurfaceGetMinkarGPU(IOSurfaceRef buffer);

/* 4:2:0 YCbCr of two planes (a video's frame): Y, then Cb and Cr
 * interleaved at half the size. */
static BOOL
isYCbCr(OSType format)
{
	return format == '420v' || format == '420f';
}

/* Fixed-point (16 bits) coefficients of a YCbCr matrix: R = Y + rv*Cr,
 * G = Y - gu*Cb - gv*Cr, B = Y + bu*Cb, Y and C first brought to full
 * range. */
typedef struct {
	int32_t rv, gu, gv, bu;
} WSYCbCrMatrix;

static const WSYCbCrMatrix kMatrix601 = { 91881, 22553, 46802, 116130 };
static const WSYCbCrMatrix kMatrix709 = { 103206, 12276, 30679, 121609 };
static const WSYCbCrMatrix kMatrix2020 = { 96639, 10784, 37444, 123299 };

static inline uint8_t
clamp8(int32_t v)
{
	return v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v;
}

@implementation WSIOSurfaceBacking {
	IOSurfaceRef _surface;
	/* A YCbCr surface: its pixels as BGRA, converted for a seed. */
	BOOL _ycbcr, _fullRange;
	WSYCbCrMatrix _matrix;
	uint8_t *_converted;
	uint32_t _convertedSeed;
	BOOL _convertedOnce;
}

- (instancetype)initWithSurfaceID:(IOSurfaceID)surfaceID width:(uint32_t)width
    height:(uint32_t)height
{
	if ((self = [super init]) == nil)
		return nil;
	_surface = IOSurfaceLookup(surfaceID);
	if (_surface == NULL || IOSurfaceGetWidth(_surface) != width ||
	    IOSurfaceGetHeight(_surface) != height)
		return nil;
	OSType format = IOSurfaceGetPixelFormat(_surface);
	if (isYCbCr(format)) {
		if (IOSurfaceGetPlaneCount(_surface) != 2 || width < 2 || height < 2)
			return nil;
		_ycbcr = YES;
		_fullRange = format == '420f';
		/* The matrix the app gave the surface (read once). */
		_matrix = kMatrix601;
		CFTypeRef m = IOSurfaceCopyValue(_surface, CFSTR("IOSurfaceYCbCrMatrix"));
		if (m != NULL && CFGetTypeID(m) == CFStringGetTypeID()) {
			if (CFStringFind(m, CFSTR("709"), 0).location != kCFNotFound)
				_matrix = kMatrix709;
			else if (CFStringFind(m, CFSTR("2020"), 0).location != kCFNotFound)
				_matrix = kMatrix2020;
		}
		if (m != NULL)
			CFRelease(m);
		_converted = malloc((size_t)width * height * 4);
		if (_converted == NULL)
			return nil;
		_rowBytes = width * 4;
	} else {
		if (IOSurfaceGetBytesPerElement(_surface) != 4 || (format != 'BGRA' && format != 0))
			return nil;
		_rowBytes = (uint32_t)IOSurfaceGetBytesPerRow(_surface);
		_gpuBuffer = IOSurfaceGetMinkarGPUBuffer(_surface);
		_gpuEntryID = _gpuBuffer != 0 ? IOSurfaceGetMinkarGPU(_surface) : 0;
	}
	_surfaceID = surfaceID;
	_width = width;
	_height = height;
	_allocSize = IOSurfaceGetAllocSize(_surface);
	return self;
}

- (void)dealloc
{
	free(_converted);
	if (_surface != NULL)
		CFRelease(_surface);
}

/* The YCbCr planes into _converted, once for each seed (the app's
 * writes change it): two rows at a time, each Cb, Cr pair for a 2 x 2
 * block. */
- (void)convert
{
	IOSurfaceLock(_surface, kIOSurfaceLockReadOnly, NULL);
	uint32_t seed = IOSurfaceGetSeed(_surface);
	if (_convertedOnce && seed == _convertedSeed) {
		IOSurfaceUnlock(_surface, kIOSurfaceLockReadOnly, NULL);
		return;
	}
	const uint8_t *yPlane = IOSurfaceGetBaseAddressOfPlane(_surface, 0);
	const uint8_t *cPlane = IOSurfaceGetBaseAddressOfPlane(_surface, 1);
	size_t yRow = IOSurfaceGetBytesPerRowOfPlane(_surface, 0);
	size_t cRow = IOSurfaceGetBytesPerRowOfPlane(_surface, 1);
	/* Video range: Y 16-235, C 16-240, scaled to full (16 bits). */
	int32_t yScale = _fullRange ? 65536 : 76309, yBase = _fullRange ? 0 : 16;
	int32_t cScale = _fullRange ? 65536 : 74606;
	const WSYCbCrMatrix m = _matrix;

	for (uint32_t row = 0; row < _height; row++) {
		const uint8_t *ys = yPlane + row * yRow;
		const uint8_t *cs = cPlane + (row / 2) * cRow;
		uint8_t *out = _converted + (size_t)row * _rowBytes;
		for (uint32_t col = 0; col < _width; col++) {
			int32_t y = ((int32_t)ys[col] - yBase) * yScale;
			int32_t cb = (((int32_t)cs[(col & ~1u)] - 128) * cScale) >> 16;
			int32_t cr = (((int32_t)cs[(col & ~1u) + 1] - 128) * cScale) >> 16;
			out[col * 4 + 0] = clamp8((y + m.bu * cb + 32768) >> 16);
			out[col * 4 + 1] = clamp8((y - m.gu * cb - m.gv * cr + 32768) >> 16);
			out[col * 4 + 2] = clamp8((y + m.rv * cr + 32768) >> 16);
			out[col * 4 + 3] = 255;
		}
	}
	IOSurfaceUnlock(_surface, kIOSurfaceLockReadOnly, NULL);
	_convertedSeed = seed;
	_convertedOnce = YES;
}

- (uint32_t)backBuffer
{
	return 0;
}

- (BOOL)committed
{
	return YES;
}

- (BOOL)live
{
	return YES;
}

- (BOOL)commitBuffer:(uint32_t)buffer
{
	return buffer == 0;
}

- (const uint8_t *)lockFront
{
	if (_ycbcr) {
		[self convert];
		return _converted;
	}
	IOSurfaceLock(_surface, kIOSurfaceLockReadOnly, NULL);
	return IOSurfaceGetBaseAddress(_surface);
}

- (void)unlock
{
	if (!_ycbcr)
		IOSurfaceUnlock(_surface, kIOSurfaceLockReadOnly, NULL);
}

@end
