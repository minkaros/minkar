/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSOpenGLLayer: CAOpenGLLayer's CGL hooks answered with the layer's
 * NSOpenGLPixelFormat and NSOpenGLContext, and its drawing calls passed to
 * the NSOpenGL ones, with the context made current.
 */

#import <AppKit/NSOpenGLLayer.h>
#import <AppKit/NSOpenGLContext.h>
#import <AppKit/NSOpenGLPixelFormat.h>
#import <OpenGL/gl.h>

@implementation NSOpenGLLayer

-(void)dealloc {
   [_openGLContext release];
   [_openGLPixelFormat release];
   [super dealloc];
}

-(NSView *)view {
   return _view;
}

-(void)setView:(NSView *)view {
   _view=view;
}

-(NSOpenGLPixelFormat *)openGLPixelFormat {
   return _openGLPixelFormat;
}

-(void)setOpenGLPixelFormat:(NSOpenGLPixelFormat *)format {
   [format retain];
   [_openGLPixelFormat release];
   _openGLPixelFormat=format;
}

-(NSOpenGLContext *)openGLContext {
   return _openGLContext;
}

-(void)setOpenGLContext:(NSOpenGLContext *)context {
   [context retain];
   [_openGLContext release];
   _openGLContext=context;
}

-(NSOpenGLPixelFormat *)openGLPixelFormatForDisplayMask:(uint32_t)mask {
   NSOpenGLPixelFormatAttribute attributes[]={ NSOpenGLPFAColorSize,24,NSOpenGLPFAAlphaSize,8,0 };

   return [[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes] autorelease];
}

-(NSOpenGLContext *)openGLContextForPixelFormat:(NSOpenGLPixelFormat *)pixelFormat {
   return [[[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil] autorelease];
}

-(BOOL)canDrawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat
                 forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
   return YES;
}

-(void)drawInOpenGLContext:(NSOpenGLContext *)context pixelFormat:(NSOpenGLPixelFormat *)pixelFormat
              forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
   glFlush();
}

/* ---- CAOpenGLLayer's hooks ---- */

-(CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
   if(_openGLPixelFormat==nil)
    [self setOpenGLPixelFormat:[self openGLPixelFormatForDisplayMask:mask]];
   return _openGLPixelFormat!=nil?CGLRetainPixelFormat([_openGLPixelFormat CGLPixelFormatObj]):NULL;
}

-(CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pf {
   if(_openGLContext==nil)
    [self setOpenGLContext:[self openGLContextForPixelFormat:_openGLPixelFormat]];
   return _openGLContext!=nil?CGLRetainContext([_openGLContext CGLContextObj]):NULL;
}

-(BOOL)canDrawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf
              forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
   return [self canDrawInOpenGLContext:_openGLContext pixelFormat:_openGLPixelFormat forLayerTime:t displayTime:ts];
}

-(void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf
           forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
   [_openGLContext makeCurrentContext];
   [self drawInOpenGLContext:_openGLContext pixelFormat:_openGLPixelFormat forLayerTime:t displayTime:ts];
}

@end
