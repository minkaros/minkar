/* Copyright(c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGContext.h>
#import <CoreGraphics/CGImage.h>

/* CF's naming rules for what the functions return (Create, Copy). */
CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

typedef void (*CGBitmapContextReleaseDataCallback)(void * _Nullable userInfo, void * _Nullable data);

COREGRAPHICS_EXPORT CGContextRef _Nullable CGBitmapContextCreateWithData(void * _Nullable data, size_t width, size_t height, size_t bitsPerComponent, size_t bytesPerRow, CGColorSpaceRef _Nullable space, uint32_t bitmapInfo, CGBitmapContextReleaseDataCallback _Nullable releaseCallback, void * _Nullable releaseInfo);
COREGRAPHICS_EXPORT CGContextRef _Nullable CGBitmapContextCreate(void * _Nullable data, size_t width, size_t height, size_t bitsPerComponent, size_t bytesPerRow, CGColorSpaceRef _Nullable colorSpace, uint32_t bitmapInfo);

COREGRAPHICS_EXPORT void * _Nullable CGBitmapContextGetData(CGContextRef self);
COREGRAPHICS_EXPORT size_t CGBitmapContextGetWidth(CGContextRef self);
COREGRAPHICS_EXPORT size_t CGBitmapContextGetHeight(CGContextRef self);
COREGRAPHICS_EXPORT size_t CGBitmapContextGetBitsPerComponent(CGContextRef self);
COREGRAPHICS_EXPORT size_t CGBitmapContextGetBytesPerRow(CGContextRef self);
COREGRAPHICS_EXPORT CGColorSpaceRef _Nullable CGBitmapContextGetColorSpace(CGContextRef self);
COREGRAPHICS_EXPORT CGBitmapInfo CGBitmapContextGetBitmapInfo(CGContextRef self);

COREGRAPHICS_EXPORT size_t CGBitmapContextGetBitsPerPixel(CGContextRef self);
COREGRAPHICS_EXPORT CGImageAlphaInfo CGBitmapContextGetAlphaInfo(CGContextRef self);
COREGRAPHICS_EXPORT CGImageRef _Nullable CGBitmapContextCreateImage(CGContextRef self);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED