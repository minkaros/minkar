/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <AppKit/AppKitExport.h>
#import <OpenGL/OpenGL.h>

@class NSData;

enum {
    NSOpenGLPFAAllRenderers = 1,
    NSOpenGLPFATripleBuffer = 3,
    NSOpenGLPFADoubleBuffer = 5,
    NSOpenGLPFAStereo = 6,
    NSOpenGLPFAAuxBuffers = 7,
    NSOpenGLPFAColorSize = 8,
    NSOpenGLPFAAlphaSize = 11,
    NSOpenGLPFADepthSize = 12,
    NSOpenGLPFAStencilSize = 13,
    NSOpenGLPFAAccumSize = 14,
    NSOpenGLPFAMinimumPolicy = 51,
    NSOpenGLPFAMaximumPolicy = 52,
    NSOpenGLPFAOffScreen = 53,
    NSOpenGLPFAFullScreen = 54,
    NSOpenGLPFASampleBuffers = 55,
    NSOpenGLPFASamples = 56,
    NSOpenGLPFAAuxDepthStencil = 57,
    NSOpenGLPFAColorFloat = 58,
    NSOpenGLPFAMultisample = 59,
    NSOpenGLPFASupersample = 60,
    NSOpenGLPFASampleAlpha = 61,
    NSOpenGLPFARendererID = 70,
    NSOpenGLPFASingleRenderer = 71,
    NSOpenGLPFANoRecovery = 72,
    NSOpenGLPFAAccelerated = 73,
    NSOpenGLPFAClosestPolicy = 74,
    NSOpenGLPFARobust = 75,
    NSOpenGLPFABackingStore = 76,
    NSOpenGLPFAMPSafe = 78,
    NSOpenGLPFAWindow = 80,
    NSOpenGLPFAMultiScreen = 81,
    NSOpenGLPFACompliant = 83,
    NSOpenGLPFAScreenMask = 84,
    NSOpenGLPFAPixelBuffer = 90,
    NSOpenGLPFARemotePixelBuffer = 91,
    NSOpenGLPFAAllowOfflineRenderers = 96,
    NSOpenGLPFAAcceleratedCompute = 97,
    NSOpenGLPFAOpenGLProfile = 99,
    NSOpenGLPFAVirtualScreenCount = 128,
};

/* The values of NSOpenGLPFAOpenGLProfile. */
enum {
    NSOpenGLProfileVersionLegacy = 0x1000,
    NSOpenGLProfileVersion3_2Core = 0x3200,
    NSOpenGLProfileVersion4_1Core = 0x4100,
};

typedef uint32_t NSOpenGLPixelFormatAttribute;

/* Options for every context of the process (CGLSetOption). */
typedef uint32_t NSOpenGLGlobalOption;
enum {
    NSOpenGLGOFormatCacheSize = 501,
    NSOpenGLGOClearFormatCache = 502,
    NSOpenGLGORetainRenderers = 503,
    NSOpenGLGOUseBuildCache = 506,
    NSOpenGLGOResetLibrary = 504,
};

APPKIT_EXPORT void NSOpenGLSetOption(NSOpenGLGlobalOption pname, GLint param);
APPKIT_EXPORT void NSOpenGLGetOption(NSOpenGLGlobalOption pname, GLint *param);
APPKIT_EXPORT void NSOpenGLGetVersion(GLint *major, GLint *minor);

@interface NSOpenGLPixelFormat : NSObject <NSCoding> {
    void *_cglPixelFormat;
    GLint _numberOfVirtualScreens;
    NSData *_attributes;
}

- (instancetype)initWithAttributes:(const NSOpenGLPixelFormatAttribute *)attributes;
- (instancetype)initWithCGLPixelFormatObj:(CGLPixelFormatObj)format;
- (instancetype)initWithData:(NSData *)attributes;

@property (readonly) GLint numberOfVirtualScreens;
@property (readonly) void *CGLPixelFormatObj;

- (void)getValues:(GLint *)values forAttribute:(NSOpenGLPixelFormatAttribute)attribute forVirtualScreen:(GLint)screen;

- (NSData *)attributes;
- (void)setAttributes:(NSData *)attributes;

@end
