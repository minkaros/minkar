/* Copyright(c) 2006 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreGraphics/CGColorSpace.h>
#import <CoreGraphics/CGDataProvider.h>
#import <CoreGraphics/CGGeometry.h>

/* CF's naming rules for what the functions return (Create, Copy). */
CF_IMPLICIT_BRIDGING_ENABLED

typedef CF_ENUM(uint32_t, CGImageAlphaInfo) {
    kCGImageAlphaNone,
    kCGImageAlphaPremultipliedLast,
    kCGImageAlphaPremultipliedFirst,
    kCGImageAlphaLast,
    kCGImageAlphaFirst,
    kCGImageAlphaNoneSkipLast,
    kCGImageAlphaNoneSkipFirst,
    kCGImageAlphaOnly,
};

/* As on the reference platform: options, the alpha info in the low bits. */
typedef CF_OPTIONS(uint32_t, CGBitmapInfo) {
    kCGBitmapAlphaInfoMask = 0x1F,
    kCGBitmapFloatInfoMask = 0xF00,
    kCGBitmapFloatComponents = 0x100,
    kCGBitmapByteOrderMask = 0x7000,
    kCGBitmapByteOrderDefault = 0,
    kCGBitmapByteOrder16Little = 0x1000,
    kCGBitmapByteOrder32Little = 0x2000,
    kCGBitmapByteOrder16Big = 0x3000,
    kCGBitmapByteOrder32Big = 0x4000,
};

#ifdef __BIG_ENDIAN__
#define kCGBitmapByteOrder16Host kCGBitmapByteOrder16Big
#define kCGBitmapByteOrder32Host kCGBitmapByteOrder32Big
#else
#define kCGBitmapByteOrder16Host kCGBitmapByteOrder16Little
#define kCGBitmapByteOrder32Host kCGBitmapByteOrder32Little
#endif

typedef struct O2Image *CGImageRef;

COREGRAPHICS_EXPORT CGImageRef CGImageRetain(CGImageRef image);
COREGRAPHICS_EXPORT void CGImageRelease(CGImageRef image);

COREGRAPHICS_EXPORT CGImageRef CGImageCreate(size_t width, size_t height, size_t bitsPerComponent, size_t bitsPerPixel, size_t bytesPerRow, CGColorSpaceRef colorSpace, CGBitmapInfo bitmapInfo, CGDataProviderRef dataProvider, const CGFloat *decode, bool interpolate, CGColorRenderingIntent renderingIntent);
COREGRAPHICS_EXPORT CGImageRef CGImageMaskCreate(size_t width, size_t height, size_t bitsPerComponent, size_t bitsPerPixel, size_t bytesPerRow, CGDataProviderRef dataProvider, const CGFloat *decode, bool interpolate);

COREGRAPHICS_EXPORT CGImageRef CGImageCreateCopy(CGImageRef self);
COREGRAPHICS_EXPORT CGImageRef CGImageCreateCopyWithColorSpace(CGImageRef self, CGColorSpaceRef colorSpace);

COREGRAPHICS_EXPORT CGImageRef CGImageCreateWithImageInRect(CGImageRef self, CGRect rect);

COREGRAPHICS_EXPORT CGImageRef CGImageCreateWithJPEGDataProvider(CGDataProviderRef jpegProvider, const CGFloat *decode, bool interpolate, CGColorRenderingIntent renderingIntent);
COREGRAPHICS_EXPORT CGImageRef CGImageCreateWithPNGDataProvider(CGDataProviderRef pngProvider, const CGFloat *decode, bool interpolate, CGColorRenderingIntent renderingIntent);

COREGRAPHICS_EXPORT CGImageRef CGImageCreateWithMask(CGImageRef self, CGImageRef mask);
COREGRAPHICS_EXPORT CGImageRef CGImageCreateWithMaskingColors(CGImageRef self, const CGFloat *components);

COREGRAPHICS_EXPORT size_t CGImageGetWidth(CGImageRef self);
COREGRAPHICS_EXPORT size_t CGImageGetHeight(CGImageRef self);
COREGRAPHICS_EXPORT size_t CGImageGetBitsPerComponent(CGImageRef self);
COREGRAPHICS_EXPORT size_t CGImageGetBitsPerPixel(CGImageRef self);
COREGRAPHICS_EXPORT size_t CGImageGetBytesPerRow(CGImageRef self);
COREGRAPHICS_EXPORT CGColorSpaceRef CGImageGetColorSpace(CGImageRef self);
COREGRAPHICS_EXPORT CGBitmapInfo CGImageGetBitmapInfo(CGImageRef self);
COREGRAPHICS_EXPORT CGDataProviderRef CGImageGetDataProvider(CGImageRef self);
COREGRAPHICS_EXPORT const CGFloat *CGImageGetDecode(CGImageRef self);
COREGRAPHICS_EXPORT bool CGImageGetShouldInterpolate(CGImageRef self);
COREGRAPHICS_EXPORT CGColorRenderingIntent CGImageGetRenderingIntent(CGImageRef self);

COREGRAPHICS_EXPORT bool CGImageIsMask(CGImageRef self);
COREGRAPHICS_EXPORT CGImageAlphaInfo CGImageGetAlphaInfo(CGImageRef self);


CF_IMPLICIT_BRIDGING_DISABLED