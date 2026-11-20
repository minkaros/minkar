/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <MetalKit/MTKView.h>

@implementation MTKView {
    CAMetalLayer *_metalLayer;
    id<CAMetalDrawable> _currentDrawable;
    MTLRenderPassDescriptor *_currentRenderPassDescriptor;
    NSTimer *_frameTimer;
    CGSize _lastDrawableSize;
    BOOL _drawing;
}

#pragma mark Making

/* As the reference's defaults: BGRA8, no depth, one sample, opaque black,
   60 frames a second, the drawable following the view's size. */
- (void)_commonInitWithDevice:(id<MTLDevice>)device {
    _device = device;
    _colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    _depthStencilPixelFormat = MTLPixelFormatInvalid;
    _depthStencilAttachmentTextureUsage = MTLTextureUsageRenderTarget;
    _multisampleColorAttachmentTextureUsage = MTLTextureUsageRenderTarget;
    _sampleCount = 1;
    _clearColor = (MTLClearColor){ 0, 0, 0, 1 };
    _clearDepth = 1;
    _clearStencil = 0;
    _framebufferOnly = YES;
    _preferredFramesPerSecond = 60;
    _autoResizeDrawable = YES;
    self.wantsLayer = YES;
}

- (instancetype)initWithFrame:(CGRect)frameRect device:(id<MTLDevice>)device {
    if ((self = [super initWithFrame:NSRectFromCGRect(frameRect)]) != nil)
        [self _commonInitWithDevice:device];
    return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    return [self initWithFrame:NSRectToCGRect(frameRect) device:nil];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil)
        [self _commonInitWithDevice:nil];
    return self;
}

- (void)dealloc {
    [_frameTimer invalidate];
}

#pragma mark The layer

- (CALayer *)makeBackingLayer {
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = _device;
    layer.pixelFormat = _colorPixelFormat;
    layer.framebufferOnly = _framebufferOnly;
    layer.presentsWithTransaction = _presentsWithTransaction;
    layer.colorspace = _colorspace;
    _metalLayer = layer;
    [self _updateDrawableSize];
    return layer;
}

- (CAMetalLayer *)_layer {
    if (_metalLayer == nil)
        (void)self.layer;
    return _metalLayer;
}

- (void)setDevice:(id<MTLDevice>)device {
    _device = device;
    _metalLayer.device = device;
    [self releaseDrawables];
}

- (id<MTLDevice>)preferredDevice {
    return MTLCreateSystemDefaultDevice();
}

- (void)setColorPixelFormat:(MTLPixelFormat)format {
    _colorPixelFormat = format;
    _metalLayer.pixelFormat = format;
}

- (void)setFramebufferOnly:(BOOL)framebufferOnly {
    _framebufferOnly = framebufferOnly;
    _metalLayer.framebufferOnly = framebufferOnly;
}

- (void)setPresentsWithTransaction:(BOOL)presentsWithTransaction {
    _presentsWithTransaction = presentsWithTransaction;
    _metalLayer.presentsWithTransaction = presentsWithTransaction;
}

- (void)setColorspace:(CGColorSpaceRef)colorspace {
    _colorspace = colorspace;
    _metalLayer.colorspace = colorspace;
}

#pragma mark The drawable's size

/* The view's size in pixels. */
- (CGSize)preferredDrawableSize {
    NSSize size = [self convertSizeToBacking:self.bounds.size];
    return CGSizeMake(floor(size.width), floor(size.height));
}

- (CGSize)drawableSize {
    return _metalLayer ? _metalLayer.drawableSize : self.preferredDrawableSize;
}

- (void)setDrawableSize:(CGSize)size {
    [self _layer].drawableSize = size;
    _lastDrawableSize = size;
}

/* Following the view's size: the delegate is told of a new one first. */
- (void)_updateDrawableSize {
    if (!_autoResizeDrawable || _metalLayer == nil)
        return;
    CGSize size = self.preferredDrawableSize;
    if (CGSizeEqualToSize(size, _lastDrawableSize))
        return;
    if (!CGSizeEqualToSize(_lastDrawableSize, CGSizeZero) && [_delegate respondsToSelector:@selector(mtkView:drawableSizeWillChange:)])
        [_delegate mtkView:self drawableSizeWillChange:size];
    _metalLayer.drawableSize = size;
    _lastDrawableSize = size;
}

- (void)setFrameSize:(NSSize)size {
    [super setFrameSize:size];
    [self _updateDrawableSize];
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self _updateDrawableSize];
}

#pragma mark Drawables

/* The layer's next drawable, the same one through a frame; none without
   a device. */
- (id<CAMetalDrawable>)currentDrawable {
    if (_device == nil)
        return nil;
    if (_currentDrawable == nil)
        _currentDrawable = [[self _layer] nextDrawable];
    return _currentDrawable;
}

/* A pass clearing the current drawable to the clear color. */
- (MTLRenderPassDescriptor *)currentRenderPassDescriptor {
    id<CAMetalDrawable> drawable = self.currentDrawable;
    if (drawable == nil)
        return nil;
    if (_currentRenderPassDescriptor == nil) {
        MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = _clearColor;
        _currentRenderPassDescriptor = pass;
    }
    return _currentRenderPassDescriptor;
}

/* Depth, stencil and multisample textures need a device. */
- (id<MTLTexture>)depthStencilTexture {
    return nil;
}

- (id<MTLTexture>)multisampleColorTexture {
    return nil;
}

- (void)releaseDrawables {
    _currentDrawable = nil;
    _currentRenderPassDescriptor = nil;
}

#pragma mark Drawing

/* One frame: the delegate draws (or drawRect:), then the frame's drawable
   is let go. */
- (void)draw {
    if (_drawing)
        return;
    _drawing = YES;
    [self _updateDrawableSize];
    if ([_delegate respondsToSelector:@selector(drawInMTKView:)])
        [_delegate drawInMTKView:self];
    else
        [self drawRect:self.bounds];
    [self releaseDrawables];
    _drawing = NO;
}

/* Marked as needing display when it draws on demand: a frame. */
- (void)displayLayer:(CALayer *)layer {
    if (_enableSetNeedsDisplay)
        [self draw];
}

- (void)setNeedsDisplay:(BOOL)flag {
    [super setNeedsDisplay:flag];
    if (flag && _enableSetNeedsDisplay)
        [self performSelector:@selector(draw) withObject:nil afterDelay:0 inModes:@[ NSRunLoopCommonModes ]];
}

- (BOOL)wantsUpdateLayer {
    return YES;
}

- (void)updateLayer {
}

#pragma mark The frame clock

/* Frames at the preferred rate while in a window, not paused and not
   drawing on demand. */
- (void)_updateFrameTimer {
    BOOL runs = self.window != nil && !_paused && !_enableSetNeedsDisplay && _preferredFramesPerSecond > 0;
    if (!runs) {
        [_frameTimer invalidate];
        _frameTimer = nil;
        return;
    }
    NSTimeInterval interval = 1.0 / (NSTimeInterval)_preferredFramesPerSecond;
    if (_frameTimer != nil && fabs(_frameTimer.timeInterval - interval) < 1e-6)
        return;
    [_frameTimer invalidate];
    __weak MTKView *weakSelf = self;
    _frameTimer = [NSTimer timerWithTimeInterval:interval repeats:YES block:^(NSTimer *timer) {
        MTKView *view = weakSelf;
        if (view == nil) {
            [timer invalidate];
            return;
        }
        if (!view.isHiddenOrHasHiddenAncestor)
            [view draw];
    }];
    [[NSRunLoop currentRunLoop] addTimer:_frameTimer forMode:NSRunLoopCommonModes];
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    [self _updateFrameTimer];
}

- (void)setPaused:(BOOL)paused {
    _paused = paused;
    [self _updateFrameTimer];
}

- (void)setEnableSetNeedsDisplay:(BOOL)enableSetNeedsDisplay {
    _enableSetNeedsDisplay = enableSetNeedsDisplay;
    [self _updateFrameTimer];
}

- (void)setPreferredFramesPerSecond:(NSInteger)preferredFramesPerSecond {
    _preferredFramesPerSecond = preferredFramesPerSecond;
    [self _updateFrameTimer];
}

- (void)setAutoResizeDrawable:(BOOL)autoResizeDrawable {
    _autoResizeDrawable = autoResizeDrawable;
    [self _updateDrawableSize];
}

@end
