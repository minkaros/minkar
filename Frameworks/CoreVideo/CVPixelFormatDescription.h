/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Pixel format descriptions: what each pixel format is (its components,
 * planes, bits per block and subsampling), as dictionaries, for the
 * formats CoreVideo knows and the ones an app registers.
 */

#ifndef __CVPIXELFORMATDESCRIPTION__
#define __CVPIXELFORMATDESCRIPTION__

#include <CoreVideo/CVPixelBuffer.h>

extern const CFStringRef kCVPixelFormatName;
extern const CFStringRef kCVPixelFormatConstant;
extern const CFStringRef kCVPixelFormatCodecType;
extern const CFStringRef kCVPixelFormatFourCC;
extern const CFStringRef kCVPixelFormatContainsAlpha;
extern const CFStringRef kCVPixelFormatContainsYCbCr;
extern const CFStringRef kCVPixelFormatContainsRGB;
extern const CFStringRef kCVPixelFormatComponentRange;
extern const CFStringRef kCVPixelFormatComponentRange_VideoRange;
extern const CFStringRef kCVPixelFormatComponentRange_FullRange;
extern const CFStringRef kCVPixelFormatComponentRange_WideRange;
extern const CFStringRef kCVPixelFormatPlanes;
extern const CFStringRef kCVPixelFormatBlockWidth;
extern const CFStringRef kCVPixelFormatBlockHeight;
extern const CFStringRef kCVPixelFormatBitsPerBlock;
extern const CFStringRef kCVPixelFormatBlockHorizontalAlignment;
extern const CFStringRef kCVPixelFormatBlockVerticalAlignment;
extern const CFStringRef kCVPixelFormatBlackBlock;
extern const CFStringRef kCVPixelFormatHorizontalSubsampling;
extern const CFStringRef kCVPixelFormatVerticalSubsampling;
extern const CFStringRef kCVPixelFormatOpenGLFormat;
extern const CFStringRef kCVPixelFormatOpenGLType;
extern const CFStringRef kCVPixelFormatOpenGLInternalFormat;
extern const CFStringRef kCVPixelFormatCGBitmapInfo;
extern const CFStringRef kCVPixelFormatQDCompatibility;
extern const CFStringRef kCVPixelFormatCGBitmapContextCompatibility;
extern const CFStringRef kCVPixelFormatCGImageCompatibility;
extern const CFStringRef kCVPixelFormatOpenGLCompatibility;

CFDictionaryRef CVPixelFormatDescriptionCreateWithPixelFormatType(CFAllocatorRef allocator, OSType pixelFormat);
CFArrayRef CVPixelFormatDescriptionArrayCreateWithAllPixelFormatTypes(CFAllocatorRef allocator);
void CVPixelFormatDescriptionRegisterDescriptionWithPixelFormatType(CFDictionaryRef description, OSType pixelFormat);

#endif
