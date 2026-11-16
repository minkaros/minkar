/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSLock.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>

@class NSOpenGLPixelFormat, NSOpenGLPixelBuffer, NSView, NSWindow;

typedef NS_ENUM(NSInteger, NSOpenGLContextParameter) {
    NSOpenGLContextParameterSwapInterval = 222,
    NSOpenGLContextParameterSurfaceOrder = 235,
    NSOpenGLContextParameterSurfaceOpacity = 236,
    NSOpenGLContextParameterSurfaceBackingSize = 304,
    NSOpenGLContextParameterReclaimResources = 308,
    NSOpenGLContextParameterCurrentRendererID = 309,
    NSOpenGLContextParameterGPUVertexProcessing = 310,
    NSOpenGLContextParameterGPUFragmentProcessing = 311,
    NSOpenGLContextParameterHasDrawable = 314,
    NSOpenGLContextParameterMPSwapsInFlight = 315,
    NSOpenGLContextParameterSwapRectangle = 200,
    NSOpenGLContextParameterSwapRectangleEnable = 201,
    NSOpenGLContextParameterRasterizationEnable = 221,
    NSOpenGLContextParameterStateValidation = 301,
    NSOpenGLContextParameterSurfaceSurfaceVolatile = 306,
};

/* The names before NSOpenGLContextParameter. */
static const NSOpenGLContextParameter NSOpenGLCPSwapInterval = NSOpenGLContextParameterSwapInterval;
static const NSOpenGLContextParameter NSOpenGLCPSurfaceOrder = NSOpenGLContextParameterSurfaceOrder;
static const NSOpenGLContextParameter NSOpenGLCPSurfaceOpacity = NSOpenGLContextParameterSurfaceOpacity;
static const NSOpenGLContextParameter NSOpenGLCPSurfaceBackingSize = NSOpenGLContextParameterSurfaceBackingSize;
static const NSOpenGLContextParameter NSOpenGLCPReclaimResources = NSOpenGLContextParameterReclaimResources;
static const NSOpenGLContextParameter NSOpenGLCPCurrentRendererID = NSOpenGLContextParameterCurrentRendererID;
static const NSOpenGLContextParameter NSOpenGLCPGPUVertexProcessing = NSOpenGLContextParameterGPUVertexProcessing;
static const NSOpenGLContextParameter NSOpenGLCPGPUFragmentProcessing = NSOpenGLContextParameterGPUFragmentProcessing;
static const NSOpenGLContextParameter NSOpenGLCPHasDrawable = NSOpenGLContextParameterHasDrawable;
static const NSOpenGLContextParameter NSOpenGLCPMPSwapsInFlight = NSOpenGLContextParameterMPSwapsInFlight;
static const NSOpenGLContextParameter NSOpenGLCPSwapRectangle = NSOpenGLContextParameterSwapRectangle;
static const NSOpenGLContextParameter NSOpenGLCPSwapRectangleEnable = NSOpenGLContextParameterSwapRectangleEnable;
static const NSOpenGLContextParameter NSOpenGLCPRasterizationEnable = NSOpenGLContextParameterRasterizationEnable;
static const NSOpenGLContextParameter NSOpenGLCPStateValidation = NSOpenGLContextParameterStateValidation;
static const NSOpenGLContextParameter NSOpenGLCPSurfaceSurfaceVolatile = NSOpenGLContextParameterSurfaceSurfaceVolatile;

@interface NSOpenGLContext : NSObject <NSLocking> {
    NSOpenGLPixelFormat *_pixelFormat;
    NSView *_view;
    void *_glContext;
    BOOL _hasPrepared;
    NSOpenGLPixelBuffer *_pixelBuffer;
    unsigned long _pixelBufferFace;
    long _pixelBufferLevel;
    /* Where the surface shows (read by the presenter on any thread). */
    NSWindow *_surfaceWindow;
    int32_t _surfaceX, _surfaceY;
    int32_t _clipX, _clipY, _clipWidth, _clipHeight;
}

@property (class, readonly, retain) NSOpenGLContext *currentContext;
+ (void)clearCurrentContext;

- (instancetype)initWithFormat:(NSOpenGLPixelFormat *)format shareContext:(NSOpenGLContext *)share;
- (instancetype)initWithCGLContextObj:(void *)context;

@property (readonly, retain) NSOpenGLPixelFormat *pixelFormat;
@property (assign) NSView *view;
@property (readonly) void *CGLContextObj;

- (void)setFullScreen;
- (void)setOffScreen:(void *)baseaddr width:(GLsizei)width height:(GLsizei)height rowbytes:(GLint)rowbytes;
- (void)clearDrawable;
- (void)update;

- (void)flushBuffer;
- (void)makeCurrentContext;

- (void)copyAttributesFromContext:(NSOpenGLContext *)context withMask:(GLbitfield)mask;
- (void)setValues:(const GLint *)vals forParameter:(NSOpenGLContextParameter)param;
- (void)getValues:(GLint *)vals forParameter:(NSOpenGLContextParameter)param;

@property GLint currentVirtualScreen;

- (void)createTexture:(GLenum)target fromView:(NSView *)view internalFormat:(GLenum)format;

- (void)setPixelBuffer:(NSOpenGLPixelBuffer *)pixelBuffer cubeMapFace:(GLenum)face mipMapLevel:(GLint)level
  currentVirtualScreen:(GLint)screen;
@property (readonly, retain) NSOpenGLPixelBuffer *pixelBuffer;
@property (readonly) GLenum pixelBufferCubeMapFace;
@property (readonly) GLint pixelBufferMipMapLevel;
- (void)setTextureImageToPixelBuffer:(NSOpenGLPixelBuffer *)pixelBuffer colorBuffer:(GLenum)source;

- (void)lock;
- (void)unlock;

@end
