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

#import <QuartzCore/CAMetalLayer.h>
#import <Foundation/Foundation.h>
#import <SkyLight/SkyLight.h>
#import <pthread.h>
#import <stdint.h>
#import <string.h>
#import "CAMetalLayerPrivate.h"

/* A drawable: an IOSurface of the layer's size, BGRA. */
@interface CAMetalDrawableMinkar : NSObject <CAMetalDrawable> {
	CAMetalLayer *_layer;		/* not retained: the layer owns its drawables */
	IOSurfaceRef _surface;
	NSUInteger _drawableID;
}
- (instancetype)initWithLayer:(CAMetalLayer *)layer surface:(IOSurfaceRef)surface ID:(NSUInteger)drawableID;
- (void)_detach;
@end

@interface CAMetalLayer (MinkarPresenting)
- (void)_minkarPresentDrawable:(CAMetalDrawableMinkar *)drawable;
- (uint32_t)_minkarWindow;
- (void)_minkarShowLastFrame;
@end

/* The layers that show in windows, to show their last frame again after
 * the window's own drawing covered it (_CAMetalLayersWindowFlushed). */
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static CAMetalLayer *sLayers[64];	/* not retained: a layer leaves at dealloc */
static unsigned sLayerCount;

static IOSurfaceRef
newSurface(size_t width, size_t height)
{
	int w = (int)width, h = (int)height, bpe = 4;
	uint32_t format = 'BGRA';
	CFNumberRef nw = CFNumberCreate(NULL, kCFNumberIntType, &w);
	CFNumberRef nh = CFNumberCreate(NULL, kCFNumberIntType, &h);
	CFNumberRef nb = CFNumberCreate(NULL, kCFNumberIntType, &bpe);
	CFNumberRef nf = CFNumberCreate(NULL, kCFNumberSInt32Type, &format);
	const void *keys[] = { kIOSurfaceWidth, kIOSurfaceHeight, kIOSurfaceBytesPerElement,
	    kIOSurfacePixelFormat };
	const void *values[] = { nw, nh, nb, nf };
	CFDictionaryRef props = CFDictionaryCreate(NULL, keys, values, 4, &kCFTypeDictionaryKeyCallBacks,
	    &kCFTypeDictionaryValueCallBacks);
	IOSurfaceRef s = IOSurfaceCreate(props);
	CFRelease(props);
	CFRelease(nw);
	CFRelease(nh);
	CFRelease(nb);
	CFRelease(nf);
	return s;
}

@implementation CAMetalDrawableMinkar

- (instancetype)initWithLayer:(CAMetalLayer *)layer surface:(IOSurfaceRef)surface ID:(NSUInteger)drawableID
{
	if ((self = [super init]) != nil) {
		_layer = layer;
		_surface = surface;
		_drawableID = drawableID;
	}
	return self;
}

- (void)dealloc
{
	if (_surface != NULL)
		CFRelease(_surface);
	[super dealloc];
}

- (void)_detach
{
	@synchronized(self) {
		_layer = nil;
	}
}

- (id<MTLTexture>)texture
{
	return nil;		/* no Metal yet */
}

- (CAMetalLayer *)layer
{
	return _layer;
}

- (NSUInteger)drawableID
{
	return _drawableID;
}

- (IOSurfaceRef)_minkarIOSurface
{
	return _surface;
}

- (void)present
{
	CAMetalLayer *layer;

	@synchronized(self) {
		layer = [_layer retain];
	}
	[layer _minkarPresentDrawable:self];
	[layer release];
}

@end

@implementation CAMetalLayer

- (instancetype)init
{
	if ((self = [super init]) != nil) {
		_pixelFormat = MTLPixelFormatBGRA8Unorm;
		_framebufferOnly = YES;
		_maximumDrawableCount = 3;
		_displaySyncEnabled = YES;
		_allowsNextDrawableTimeout = YES;
		_drawables = [[NSMutableArray alloc] init];
	}
	return self;
}

- (void)dealloc
{
	pthread_mutex_lock(&sLock);
	for (unsigned i = 0; i < sLayerCount; i++)
		if (sLayers[i] == self) {
			sLayers[i] = sLayers[--sLayerCount];
			break;
		}
	pthread_mutex_unlock(&sLock);
	for (CAMetalDrawableMinkar *d in _drawables)
		[d _detach];
	[_drawables release];
	[_lastPresented release];
	[(id)_device release];
	if (_colorspace != NULL)
		CGColorSpaceRelease(_colorspace);
	[super dealloc];
}

@synthesize device = _device;
@synthesize framebufferOnly = _framebufferOnly;
@synthesize presentsWithTransaction = _presentsWithTransaction;
@synthesize displaySyncEnabled = _displaySyncEnabled;
@synthesize allowsNextDrawableTimeout = _allowsNextDrawableTimeout;
@synthesize wantsExtendedDynamicRangeContent = _wantsExtendedDynamicRangeContent;

- (id<MTLDevice>)preferredDevice
{
	return nil;		/* no Metal yet */
}

- (MTLPixelFormat)pixelFormat
{
	return _pixelFormat;
}

- (void)setPixelFormat:(MTLPixelFormat)format
{
	/* The window takes BGRA, 8 bits a channel. */
	if (format == MTLPixelFormatBGRA8Unorm || format == MTLPixelFormatBGRA8Unorm_sRGB)
		_pixelFormat = format;
	else
		NSLog(@"CAMetalLayer: pixel format %lu is not supported, kept %lu", (unsigned long)format,
		    (unsigned long)_pixelFormat);
}

- (CGSize)drawableSize
{
	@synchronized(self) {
		return _drawableSizeSet ? _drawableSize : [self bounds].size;
	}
}

- (void)setDrawableSize:(CGSize)size
{
	@synchronized(self) {
		_drawableSize = size;
		_drawableSizeSet = YES;
	}
}

- (NSUInteger)maximumDrawableCount
{
	return _maximumDrawableCount;
}

- (void)setMaximumDrawableCount:(NSUInteger)count
{
	if (count == 2 || count == 3)
		_maximumDrawableCount = count;
}

- (CGColorSpaceRef)colorspace
{
	return _colorspace;
}

- (void)setColorspace:(CGColorSpaceRef)space
{
	if (space != NULL)
		CGColorSpaceRetain(space);
	if (_colorspace != NULL)
		CGColorSpaceRelease(_colorspace);
	_colorspace = space;
}

- (id<CAMetalDrawable>)nextDrawable
{
	CGSize size = [self drawableSize];
	size_t w = (size_t)size.width, h = (size_t)size.height;

	if (w == 0 || h == 0 || w > kSLSMaxSurfaceSide || h > kSLSMaxSurfaceSide)
		return nil;
	@synchronized(self) {
		/* A new size: new drawables. */
		CAMetalDrawableMinkar *first = [_drawables count] ? [_drawables objectAtIndex:0] : nil;
		if (first != nil && (IOSurfaceGetWidth([first _minkarIOSurface]) != w ||
		    IOSurfaceGetHeight([first _minkarIOSurface]) != h)) {
			for (CAMetalDrawableMinkar *d in _drawables)
				[d _detach];
			[_drawables removeAllObjects];
			_next = 0;
		}
		if ([_drawables count] < _maximumDrawableCount) {
			IOSurfaceRef s = newSurface(w, h);
			if (s == NULL)
				return nil;
			CAMetalDrawableMinkar *d = [[CAMetalDrawableMinkar alloc] initWithLayer:self surface:s
			    ID:[_drawables count]];
			[_drawables addObject:d];
			[d release];
		}
		/* Presenting copies the drawable at once, so the next in turn
		 * is free. */
		CAMetalDrawableMinkar *d = [_drawables objectAtIndex:_next % [_drawables count]];
		_next++;
		return [[d retain] autorelease];
	}
}

/* Shows the drawable in the view's rectangle of its window. */
- (void)_minkarShow:(CAMetalDrawableMinkar *)drawable
{
	uint32_t window;
	CGRect rect;

	@synchronized(self) {
		window = _window;
		rect = _windowRect;
	}
	IOSurfaceRef s = [drawable _minkarIOSurface];
	if (window == 0 || s == NULL)
		return;
	size_t w = IOSurfaceGetWidth(s), h = IOSurfaceGetHeight(s), rowBytes = IOSurfaceGetBytesPerRow(s);
	int32_t x = (int32_t)rect.origin.x, y = (int32_t)rect.origin.y;
	int32_t cw = (int32_t)rect.size.width, ch = (int32_t)rect.size.height;
	if (cw > (int32_t)w)
		cw = (int32_t)w;
	if (ch > (int32_t)h)
		ch = (int32_t)h;
	if (cw <= 0 || ch <= 0 || x < 0 || y < 0)
		return;
	IOSurfaceLock(s, kIOSurfaceLockReadOnly, NULL);
	/* SLSWindowFlushPixels reads its source at the window's coordinates:
	 * the drawable's first pixel is the rectangle's corner. */
	const uint8_t *origin = (const uint8_t *)((uintptr_t)IOSurfaceGetBaseAddress(s) -
	    (uintptr_t)y * rowBytes - (uintptr_t)x * 4);
	SLSIntRect r = { x, y, cw, ch };
	SLSWindowFlushPixels(window, origin, (uint32_t)rowBytes, &r, 1);
	IOSurfaceUnlock(s, kIOSurfaceLockReadOnly, NULL);
}

- (void)_minkarPresentDrawable:(CAMetalDrawableMinkar *)drawable
{
	@synchronized(self) {
		if (_lastPresented != drawable) {
			[_lastPresented release];
			_lastPresented = [drawable retain];
		}
	}
	[self _minkarShow:drawable];
}

- (void)_minkarSetWindow:(uint32_t)window rect:(CGRect)rect
{
	@synchronized(self) {
		_window = window;
		_windowRect = rect;
	}
	pthread_mutex_lock(&sLock);
	unsigned i;
	for (i = 0; i < sLayerCount && sLayers[i] != self; i++)
		;
	if (window != 0 && i == sLayerCount && sLayerCount < sizeof(sLayers) / sizeof(sLayers[0]))
		sLayers[sLayerCount++] = self;
	else if (window == 0 && i < sLayerCount)
		sLayers[i] = sLayers[--sLayerCount];
	pthread_mutex_unlock(&sLock);
}

- (uint32_t)_minkarWindow
{
	@synchronized(self) {
		return _window;
	}
}

- (void)_minkarShowLastFrame
{
	id last;

	@synchronized(self) {
		last = [_lastPresented retain];
	}
	if (last != nil)
		[self _minkarShow:last];
	[last release];
}

@end

void
_CAMetalLayersWindowFlushed(uint32_t window)
{
	CAMetalLayer *layers[64];
	unsigned n = 0;

	pthread_mutex_lock(&sLock);
	for (unsigned i = 0; i < sLayerCount; i++)
		if ([sLayers[i] _minkarWindow] == window)
			layers[n++] = [sLayers[i] retain];
	pthread_mutex_unlock(&sLock);
	for (unsigned i = 0; i < n; i++) {
		[layers[i] _minkarShowLastFrame];
		[layers[i] release];
	}
}
