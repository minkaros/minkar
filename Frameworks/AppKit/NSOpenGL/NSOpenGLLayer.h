/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CAOpenGLLayer with AppKit's OpenGL objects: an NSOpenGLPixelFormat and
 * an NSOpenGLContext, made by the layer's methods when first needed.
 */

#import <QuartzCore/CAOpenGLLayer.h>

@class NSOpenGLPixelFormat, NSOpenGLContext, NSView;

@interface NSOpenGLLayer : CAOpenGLLayer {
    NSView *_view;
    NSOpenGLPixelFormat *_openGLPixelFormat;
    NSOpenGLContext *_openGLContext;
}

@property (assign) NSView *view;
@property (retain) NSOpenGLPixelFormat *openGLPixelFormat;
@property (retain) NSOpenGLContext *openGLContext;

- (NSOpenGLPixelFormat *)openGLPixelFormatForDisplayMask:(uint32_t)mask;
- (NSOpenGLContext *)openGLContextForPixelFormat:(NSOpenGLPixelFormat *)pixelFormat;
- (BOOL)canDrawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat
                  forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts;
- (void)drawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat
               forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts;

@end
