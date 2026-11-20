/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <MetalKit/MTKDefines.h>

@protocol MTKViewDelegate;

NS_ASSUME_NONNULL_BEGIN

/* A view backed by a CAMetalLayer that draws at a frame rate (or when
   marked as needing display), giving its delegate a drawable and a render
   pass to draw it with. Without a Metal device there is no drawable: the
   delegate is still told to draw, as on the reference. */
MTK_EXPORT
@interface MTKView : NSView <NSCoding>

- (instancetype)initWithFrame:(CGRect)frameRect device:(nullable id<MTLDevice>)device NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;

@property (nonatomic, weak, nullable) id<MTKViewDelegate> delegate;
@property (nonatomic, retain, nullable) id<MTLDevice> device;
@property (nonatomic, readonly, nullable) id<MTLDevice> preferredDevice;

@property (nonatomic, readonly, nullable) id<CAMetalDrawable> currentDrawable;
@property (nonatomic, readonly, nullable) MTLRenderPassDescriptor *currentRenderPassDescriptor;
@property (nonatomic, readonly, nullable) id<MTLTexture> depthStencilTexture;
@property (nonatomic, readonly, nullable) id<MTLTexture> multisampleColorTexture;

@property (nonatomic) BOOL framebufferOnly;
@property (nonatomic) BOOL presentsWithTransaction;
@property (nonatomic) MTLPixelFormat colorPixelFormat;
@property (nonatomic) MTLPixelFormat depthStencilPixelFormat;
@property (nonatomic) MTLTextureUsage depthStencilAttachmentTextureUsage;
@property (nonatomic) MTLTextureUsage multisampleColorAttachmentTextureUsage;
@property (nonatomic) NSUInteger sampleCount;
@property (nonatomic) MTLClearColor clearColor;
@property (nonatomic) double clearDepth;
@property (nonatomic) uint32_t clearStencil;
@property (nonatomic, nullable) CGColorSpaceRef colorspace;

@property (nonatomic) NSInteger preferredFramesPerSecond;
@property (nonatomic) BOOL enableSetNeedsDisplay;
@property (nonatomic) BOOL autoResizeDrawable;
@property (nonatomic) CGSize drawableSize;
@property (nonatomic, readonly) CGSize preferredDrawableSize;
@property (nonatomic, getter=isPaused) BOOL paused;

- (void)releaseDrawables;
- (void)draw;

@end

@protocol MTKViewDelegate <NSObject>
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size;
- (void)drawInMTKView:(MTKView *)view;
@end

NS_ASSUME_NONNULL_END
