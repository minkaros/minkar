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
 * CAMetalLayer: a layer whose contents are drawables a GPU API renders and
 * presents (the Cocoa API's interface). On Minkar each drawable is an
 * IOSurface in system memory; presenting one shows it in the rectangle of
 * the layer's view in its window. Vulkan presents here through
 * VK_EXT_metal_surface (Mesa's WSI). Until Metal exists, a drawable's
 * texture and the layer's device are nil.
 */

#import <QuartzCore/CALayer.h>
#import <IOSurface/IOSurfaceRef.h>

#if __has_include(<Metal/MTLPixelFormat.h>)
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLDrawable.h>
#else
/* Metal's names this layer's interface uses, until Metal is there. */
typedef NS_ENUM(NSUInteger, MTLPixelFormat) {
	MTLPixelFormatInvalid = 0,
	MTLPixelFormatRGBA8Unorm = 70,
	MTLPixelFormatRGBA8Unorm_sRGB = 71,
	MTLPixelFormatBGRA8Unorm = 80,
	MTLPixelFormatBGRA8Unorm_sRGB = 81,
	MTLPixelFormatRGB10A2Unorm = 90,
	MTLPixelFormatBGR10A2Unorm = 94,
	MTLPixelFormatRGBA16Float = 115,
};
@protocol MTLDevice;
@protocol MTLTexture;
@protocol MTLDrawable <NSObject>
- (void)present;
@property(nonatomic, readonly) NSUInteger drawableID;
@end
#endif

@class CAMetalLayer;

@protocol CAMetalDrawable <MTLDrawable>
@property(readonly) id<MTLTexture> texture;
@property(readonly) CAMetalLayer *layer;
@end

@interface CAMetalLayer : CALayer {
	id<MTLDevice> _device;
	MTLPixelFormat _pixelFormat;
	BOOL _framebufferOnly;
	CGSize _drawableSize;
	BOOL _drawableSizeSet;
	NSUInteger _maximumDrawableCount;
	BOOL _presentsWithTransaction;
	BOOL _displaySyncEnabled;
	BOOL _allowsNextDrawableTimeout;
	BOOL _wantsExtendedDynamicRangeContent;
	CGColorSpaceRef _colorspace;
	NSMutableArray *_drawables;	/* IOSurface-backed, reused in turn */
	NSUInteger _next;
	id _lastPresented;		/* shown again when the window is redrawn */
	uint32_t _window;		/* the SkyLight window of the layer's view */
	CGRect _windowRect;		/* the view's rectangle there, from its top left */
}

@property(retain) id<MTLDevice> device;
@property(readonly) id<MTLDevice> preferredDevice;
@property MTLPixelFormat pixelFormat;
@property BOOL framebufferOnly;
/* In pixels; by default the layer's bounds. */
@property CGSize drawableSize;
/* 2 or 3. */
@property NSUInteger maximumDrawableCount;
@property BOOL presentsWithTransaction;
@property BOOL displaySyncEnabled;
@property BOOL allowsNextDrawableTimeout;
@property BOOL wantsExtendedDynamicRangeContent;
@property CGColorSpaceRef colorspace;

/* The next drawable of drawableSize and pixelFormat, or nil. */
- (id<CAMetalDrawable>)nextDrawable;

@end
