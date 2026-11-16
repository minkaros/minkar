/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A layer whose contents OpenGL draws: each display makes a CGL context
 * current on an offscreen drawable of the layer's size, asks the layer to
 * draw, and shows the frame as the layer's contents.
 */

#import <QuartzCore/CALayer.h>
#import <CoreVideo/CVBase.h>
#import <OpenGL/OpenGL.h>

@interface CAOpenGLLayer : CALayer {
    BOOL _asynchronous;
    CGColorSpaceRef _colorspace;
    BOOL _wantsExtendedDynamicRangeContent;
    CGLPixelFormatObj _glPixelFormat;
    CGLContextObj _glContext;
    void *_pixels;
    size_t _pixelsWidth, _pixelsHeight;
    void *_frames[3];
    int _nextFrame;
    void *_timer;               /* dispatch_source_t */
}

/* Drawn again at every display refresh, when canDrawInCGLContext:... says so. */
@property (getter=isAsynchronous) BOOL asynchronous;
@property CGColorSpaceRef colorspace;
@property BOOL wantsExtendedDynamicRangeContent;

- (BOOL)canDrawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf
               forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts;
- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf
            forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts;

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask;
- (void)releaseCGLPixelFormat:(CGLPixelFormatObj)pf;
- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pf;
- (void)releaseCGLContext:(CGLContextObj)ctx;

@end
