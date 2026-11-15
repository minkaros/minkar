/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Pixel format descriptions. Each built-in format is a row: its name, its
 * components (R: RGB, Y: Y'CbCr, G: gray, O: other), whether it has
 * alpha, its range (V: video, F: full, W: wide, -: none) and its planes,
 * each with its bits per block, block size and subsampling. A
 * description is made from a row; a description an app registers is read
 * back into one, so CVPixelBufferCreate makes buffers of it too.
 */

#include "CVPixelFormatDescription.h"
#include "CVInternal.h"
#include <pthread.h>
#include <stdlib.h>

#define KEY(name, value) const CFStringRef name = CFSTR(value)

KEY(kCVPixelFormatName, "Name");
KEY(kCVPixelFormatConstant, "PixelFormat");
KEY(kCVPixelFormatCodecType, "CodecType");
KEY(kCVPixelFormatFourCC, "FourCC");
KEY(kCVPixelFormatContainsAlpha, "ContainsAlpha");
KEY(kCVPixelFormatContainsYCbCr, "ContainsYCbCr");
KEY(kCVPixelFormatContainsRGB, "ContainsRGB");
KEY(kCVPixelFormatComponentRange, "ComponentRange");
KEY(kCVPixelFormatComponentRange_VideoRange, "VideoRange");
KEY(kCVPixelFormatComponentRange_FullRange, "FullRange");
KEY(kCVPixelFormatComponentRange_WideRange, "WideRange");
KEY(kCVPixelFormatPlanes, "Planes");
KEY(kCVPixelFormatBlockWidth, "BlockWidth");
KEY(kCVPixelFormatBlockHeight, "BlockHeight");
KEY(kCVPixelFormatBitsPerBlock, "BitsPerBlock");
KEY(kCVPixelFormatBlockHorizontalAlignment, "BlockHorizontalAlignment");
KEY(kCVPixelFormatBlockVerticalAlignment, "BlockVerticalAlignment");
KEY(kCVPixelFormatBlackBlock, "BlackBlock");
KEY(kCVPixelFormatHorizontalSubsampling, "HorizontalSubsampling");
KEY(kCVPixelFormatVerticalSubsampling, "VerticalSubsampling");
KEY(kCVPixelFormatOpenGLFormat, "OpenGLFormat");
KEY(kCVPixelFormatOpenGLType, "OpenGLType");
KEY(kCVPixelFormatOpenGLInternalFormat, "OpenGLInternalFormat");
KEY(kCVPixelFormatCGBitmapInfo, "CGBitmapInfo");
KEY(kCVPixelFormatQDCompatibility, "QDCompatibility");
KEY(kCVPixelFormatCGBitmapContextCompatibility, "CGBitmapContextCompatibility");
KEY(kCVPixelFormatCGImageCompatibility, "CGImageCompatibility");
KEY(kCVPixelFormatOpenGLCompatibility, "OpenGLCompatibility");

typedef struct {
    OSType format;
    const char *name;
    char components;
    int alpha;
    char range;
    int planes;
    CVFormatPlane plane[CV_MAX_PLANES];
} Row;

static const Row kRows[] = {
    { kCVPixelFormatType_1Monochrome, "1 bit monochrome", 'G', 0, '-', 1, { { 1, 8, 1, 1, 1 } } },
    { kCVPixelFormatType_2Indexed, "2 bit indexed", 'R', 0, '-', 1, { { 2, 4, 1, 1, 1 } } },
    { kCVPixelFormatType_4Indexed, "4 bit indexed", 'R', 0, '-', 1, { { 4, 2, 1, 1, 1 } } },
    { kCVPixelFormatType_8Indexed, "8 bit indexed", 'R', 0, '-', 1, { { 8, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_1IndexedGray_WhiteIsZero, "1 bit indexed gray, white is zero", 'G', 0, '-', 1, { { 1, 8, 1, 1, 1 } } },
    { kCVPixelFormatType_2IndexedGray_WhiteIsZero, "2 bit indexed gray, white is zero", 'G', 0, '-', 1, { { 2, 4, 1, 1, 1 } } },
    { kCVPixelFormatType_4IndexedGray_WhiteIsZero, "4 bit indexed gray, white is zero", 'G', 0, '-', 1, { { 4, 2, 1, 1, 1 } } },
    { kCVPixelFormatType_8IndexedGray_WhiteIsZero, "8 bit indexed gray, white is zero", 'G', 0, '-', 1, { { 8, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_16BE555, "16 bit BE RGB 555", 'R', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_16LE555, "16 bit LE RGB 555", 'R', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_16LE5551, "16 bit LE RGB 5551", 'R', 1, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_16BE565, "16 bit BE RGB 565", 'R', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_16LE565, "16 bit LE RGB 565", 'R', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_24RGB, "24 bit RGB", 'R', 0, '-', 1, { { 24, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_24BGR, "24 bit BGR", 'R', 0, '-', 1, { { 24, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_32ARGB, "32 bit ARGB", 'R', 1, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_32BGRA, "32 bit BGRA", 'R', 1, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_32ABGR, "32 bit ABGR", 'R', 1, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_32RGBA, "32 bit RGBA", 'R', 1, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_64ARGB, "64 bit ARGB, 16 bit BE components", 'R', 1, '-', 1, { { 64, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_64RGBALE, "64 bit RGBA, 16 bit LE components", 'R', 1, '-', 1, { { 64, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_48RGB, "48 bit RGB, 16 bit BE components", 'R', 0, '-', 1, { { 48, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_32AlphaGray, "32 bit alpha and gray, 16 bit BE components", 'G', 1, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_16Gray, "16 bit BE gray", 'G', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_30RGB, "30 bit RGB, 10 bit BE components", 'R', 0, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_422YpCbCr8, "Component Y'CbCr 8 bit 4:2:2, ordered Cb Y'0 Cr Y'1", 'Y', 0, 'V', 1, { { 32, 2, 1, 1, 1 } } },
    { kCVPixelFormatType_4444YpCbCrA8, "Component Y'CbCrA 8 bit 4:4:4:4, ordered Cb Y' Cr A", 'Y', 1, 'V', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_4444YpCbCrA8R, "Component Y'CbCrA 8 bit 4:4:4:4, rendering format", 'Y', 1, 'F', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_4444AYpCbCr8, "Component Y'CbCrA 8 bit 4:4:4:4, ordered A Y' Cb Cr", 'Y', 1, 'V', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_4444AYpCbCr16, "Component Y'CbCrA 16 bit 4:4:4:4, ordered A Y' Cb Cr", 'Y', 1, 'V', 1, { { 64, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_444YpCbCr8, "Component Y'CbCr 8 bit 4:4:4", 'Y', 0, 'V', 1, { { 24, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_422YpCbCr16, "Component Y'CbCr 10, 12, 14 or 16 bit 4:2:2", 'Y', 0, 'V', 1, { { 64, 2, 1, 1, 1 } } },
    { kCVPixelFormatType_422YpCbCr10, "Component Y'CbCr 10 bit 4:2:2", 'Y', 0, 'V', 1, { { 128, 6, 1, 1, 1 } } },
    { kCVPixelFormatType_444YpCbCr10, "Component Y'CbCr 10 bit 4:4:4", 'Y', 0, 'V', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr8Planar, "Planar Component Y'CbCr 8 bit 4:2:0", 'Y', 0, 'V', 3, { { 8, 1, 1, 1, 1 }, { 8, 1, 1, 2, 2 }, { 8, 1, 1, 2, 2 } } },
    { kCVPixelFormatType_420YpCbCr8PlanarFullRange, "Planar Component Y'CbCr 8 bit 4:2:0, full range", 'Y', 0, 'F', 3, { { 8, 1, 1, 1, 1 }, { 8, 1, 1, 2, 2 }, { 8, 1, 1, 2, 2 } } },
    { kCVPixelFormatType_422YpCbCr_4A_8BiPlanar, "First plane Y'CbCr 8 bit 4:2:2, second plane alpha 8 bit", 'Y', 1, 'V', 2, { { 32, 2, 1, 1, 1 }, { 8, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, "Bi-Planar Component Y'CbCr 8 bit 4:2:0, video range", 'Y', 0, 'V', 2, { { 8, 1, 1, 1, 1 }, { 16, 1, 1, 2, 2 } } },
    { kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, "Bi-Planar Component Y'CbCr 8 bit 4:2:0, full range", 'Y', 0, 'F', 2, { { 8, 1, 1, 1, 1 }, { 16, 1, 1, 2, 2 } } },
    { kCVPixelFormatType_422YpCbCr8BiPlanarVideoRange, "Bi-Planar Component Y'CbCr 8 bit 4:2:2, video range", 'Y', 0, 'V', 2, { { 8, 1, 1, 1, 1 }, { 16, 1, 1, 2, 1 } } },
    { kCVPixelFormatType_422YpCbCr8BiPlanarFullRange, "Bi-Planar Component Y'CbCr 8 bit 4:2:2, full range", 'Y', 0, 'F', 2, { { 8, 1, 1, 1, 1 }, { 16, 1, 1, 2, 1 } } },
    { kCVPixelFormatType_444YpCbCr8BiPlanarVideoRange, "Bi-Planar Component Y'CbCr 8 bit 4:4:4, video range", 'Y', 0, 'V', 2, { { 8, 1, 1, 1, 1 }, { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_444YpCbCr8BiPlanarFullRange, "Bi-Planar Component Y'CbCr 8 bit 4:4:4, full range", 'Y', 0, 'F', 2, { { 8, 1, 1, 1, 1 }, { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_422YpCbCr8_yuvs, "Component Y'CbCr 8 bit 4:2:2, ordered Y'0 Cb Y'1 Cr", 'Y', 0, 'V', 1, { { 32, 2, 1, 1, 1 } } },
    { kCVPixelFormatType_422YpCbCr8FullRange, "Component Y'CbCr 8 bit 4:2:2, full range", 'Y', 0, 'F', 1, { { 32, 2, 1, 1, 1 } } },
    { kCVPixelFormatType_OneComponent8, "8 bit one component", 'O', 0, '-', 1, { { 8, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_TwoComponent8, "8 bit two component", 'O', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_30RGBLEPackedWideGamut, "Little-endian RGB101010, 2 MSB are zero, wide-gamut", 'R', 0, 'W', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_ARGB2101010LEPacked, "Little-endian ARGB2101010 full-range ARGB", 'R', 1, 'F', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_OneComponent16Half, "16 bit one component IEEE half-precision float", 'O', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_OneComponent32Float, "32 bit one component IEEE float", 'O', 0, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_TwoComponent16Half, "16 bit two component IEEE half-precision float", 'O', 0, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_TwoComponent32Float, "32 bit two component IEEE float", 'O', 0, '-', 1, { { 64, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_64RGBAHalf, "64 bit RGBA IEEE half-precision float", 'R', 1, '-', 1, { { 64, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_128RGBAFloat, "128 bit RGBA IEEE float", 'R', 1, '-', 1, { { 128, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_14Bayer_GRBG, "Bayer 14 bit, G R / B G", 'O', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_14Bayer_RGGB, "Bayer 14 bit, R G / G B", 'O', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_14Bayer_BGGR, "Bayer 14 bit, B G / G R", 'O', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_14Bayer_GBRG, "Bayer 14 bit, G B / R G", 'O', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_DisparityFloat16, "IEEE754-2008 binary16 (half float), describing the normalized shift", 'O', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_DisparityFloat32, "IEEE754-2008 binary32 float, describing the normalized shift", 'O', 0, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_DepthFloat16, "IEEE754-2008 binary16 (half float), describing the depth in meters", 'O', 0, '-', 1, { { 16, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_DepthFloat32, "IEEE754-2008 binary32 float, describing the depth in meters", 'O', 0, '-', 1, { { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, "2 plane YCbCr10 4:2:0, video range", 'Y', 0, 'V', 2, { { 16, 1, 1, 1, 1 }, { 32, 1, 1, 2, 2 } } },
    { kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange, "2 plane YCbCr10 4:2:2, video range", 'Y', 0, 'V', 2, { { 16, 1, 1, 1, 1 }, { 32, 1, 1, 2, 1 } } },
    { kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange, "2 plane YCbCr10 4:4:4, video range", 'Y', 0, 'V', 2, { { 16, 1, 1, 1, 1 }, { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr10BiPlanarFullRange, "2 plane YCbCr10 4:2:0, full range", 'Y', 0, 'F', 2, { { 16, 1, 1, 1, 1 }, { 32, 1, 1, 2, 2 } } },
    { kCVPixelFormatType_422YpCbCr10BiPlanarFullRange, "2 plane YCbCr10 4:2:2, full range", 'Y', 0, 'F', 2, { { 16, 1, 1, 1, 1 }, { 32, 1, 1, 2, 1 } } },
    { kCVPixelFormatType_444YpCbCr10BiPlanarFullRange, "2 plane YCbCr10 4:4:4, full range", 'Y', 0, 'F', 2, { { 16, 1, 1, 1, 1 }, { 32, 1, 1, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr8VideoRange_8A_TriPlanar, "First and second planes as 4:2:0 video range, third plane alpha 8 bit", 'Y', 1, 'V', 3, { { 8, 1, 1, 1, 1 }, { 16, 1, 1, 2, 2 }, { 8, 1, 1, 1, 1 } } },
};
#define ROW_COUNT (sizeof(kRows) / sizeof(kRows[0]))

/* The ones apps registered: their rows, and their descriptions as given. */
static pthread_mutex_t registeredLock = PTHREAD_MUTEX_INITIALIZER;
static Row *registered;
static CFDictionaryRef *registeredDescriptions;
static size_t registeredCount;

static Boolean findRow(OSType format, Row *out, CFDictionaryRef *description) {
    for (size_t i = 0; i < ROW_COUNT; i++)
        if (kRows[i].format == format) {
            *out = kRows[i];
            if (description != NULL)
                *description = NULL;
            return true;
        }
    pthread_mutex_lock(&registeredLock);
    for (size_t i = 0; i < registeredCount; i++)
        if (registered[i].format == format) {
            *out = registered[i];
            if (description != NULL)
                *description = (CFDictionaryRef)CFRetain(registeredDescriptions[i]);
            pthread_mutex_unlock(&registeredLock);
            return true;
        }
    pthread_mutex_unlock(&registeredLock);
    return false;
}

Boolean _CVPixelFormatGetLayout(OSType format, CVFormatLayout *layout) {
    Row row;
    if (!findRow(format, &row, NULL))
        return false;
    layout->planes = row.planes;
    for (int i = 0; i < row.planes && i < CV_MAX_PLANES; i++)
        layout->plane[i] = row.plane[i];
    return true;
}

static void setInt(CFMutableDictionaryRef d, CFStringRef key, long long v) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongLongType, &v);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

static void setPlane(CFMutableDictionaryRef d, const CVFormatPlane *p) {
    setInt(d, kCVPixelFormatBitsPerBlock, p->bitsPerBlock);
    if (p->blockWidth > 1)
        setInt(d, kCVPixelFormatBlockWidth, p->blockWidth);
    if (p->blockHeight > 1)
        setInt(d, kCVPixelFormatBlockHeight, p->blockHeight);
    if (p->horizontalSubsampling > 1)
        setInt(d, kCVPixelFormatHorizontalSubsampling, p->horizontalSubsampling);
    if (p->verticalSubsampling > 1)
        setInt(d, kCVPixelFormatVerticalSubsampling, p->verticalSubsampling);
}

static Boolean isFourCC(OSType f) {
    for (int i = 0; i < 4; i++) {
        char c = (char)(f >> (24 - 8 * i));
        if (c < 0x20 || c > 0x7e)
            return false;
    }
    return true;
}

CFDictionaryRef CVPixelFormatDescriptionCreateWithPixelFormatType(CFAllocatorRef allocator, OSType format) {
    Row row;
    CFDictionaryRef given = NULL;
    if (!findRow(format, &row, &given))
        return NULL;
    if (given != NULL)
        return given;
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    CFStringRef name = CFStringCreateWithCString(allocator, row.name, kCFStringEncodingUTF8);
    CFDictionarySetValue(d, kCVPixelFormatName, name);
    CFRelease(name);
    setInt(d, kCVPixelFormatConstant, format);
    setInt(d, kCVPixelFormatCodecType, format);
    if (isFourCC(format)) {
        char cc[5] = { (char)(format >> 24), (char)(format >> 16), (char)(format >> 8), (char)format, 0 };
        CFStringRef s = CFStringCreateWithCString(allocator, cc, kCFStringEncodingASCII);
        CFDictionarySetValue(d, kCVPixelFormatFourCC, s);
        CFRelease(s);
    }
    CFDictionarySetValue(d, kCVPixelFormatContainsAlpha, row.alpha ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(d, kCVPixelFormatContainsYCbCr, row.components == 'Y' ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(d, kCVPixelFormatContainsRGB, row.components == 'R' ? kCFBooleanTrue : kCFBooleanFalse);
    if (row.range != '-')
        CFDictionarySetValue(d, kCVPixelFormatComponentRange,
                             row.range == 'V' ? kCVPixelFormatComponentRange_VideoRange
                             : row.range == 'F' ? kCVPixelFormatComponentRange_FullRange
                                                : kCVPixelFormatComponentRange_WideRange);
    if (row.planes > 1) {
        CFMutableArrayRef planes = CFArrayCreateMutable(allocator, row.planes, &kCFTypeArrayCallBacks);
        for (int i = 0; i < row.planes; i++) {
            CFMutableDictionaryRef p = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                 &kCFTypeDictionaryValueCallBacks);
            setPlane(p, &row.plane[i]);
            CFArrayAppendValue(planes, p);
            CFRelease(p);
        }
        CFDictionarySetValue(d, kCVPixelFormatPlanes, planes);
        CFRelease(planes);
    } else
        setPlane(d, &row.plane[0]);
    /* The 32-bit RGB formats with alpha first or last draw in bitmaps. */
    if (format == kCVPixelFormatType_32ARGB || format == kCVPixelFormatType_32BGRA) {
        CFDictionarySetValue(d, kCVPixelFormatCGBitmapContextCompatibility, kCFBooleanTrue);
        CFDictionarySetValue(d, kCVPixelFormatCGImageCompatibility, kCFBooleanTrue);
    }
    return d;
}

CFArrayRef CVPixelFormatDescriptionArrayCreateWithAllPixelFormatTypes(CFAllocatorRef allocator) {
    CFMutableArrayRef all = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < ROW_COUNT; i++) {
        long long f = kRows[i].format;
        CFNumberRef n = CFNumberCreate(allocator, kCFNumberLongLongType, &f);
        CFArrayAppendValue(all, n);
        CFRelease(n);
    }
    pthread_mutex_lock(&registeredLock);
    for (size_t i = 0; i < registeredCount; i++) {
        long long f = registered[i].format;
        CFNumberRef n = CFNumberCreate(allocator, kCFNumberLongLongType, &f);
        CFArrayAppendValue(all, n);
        CFRelease(n);
    }
    pthread_mutex_unlock(&registeredLock);
    return all;
}

static int intValue(CFDictionaryRef d, CFStringRef key, int otherwise) {
    CFTypeRef v = d != NULL ? CFDictionaryGetValue(d, key) : NULL;
    int x = otherwise;
    if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberIntType, &x);
    return x;
}

static CVFormatPlane readPlane(CFDictionaryRef d) {
    CVFormatPlane p = { intValue(d, kCVPixelFormatBitsPerBlock, 0), intValue(d, kCVPixelFormatBlockWidth, 1),
                        intValue(d, kCVPixelFormatBlockHeight, 1), intValue(d, kCVPixelFormatHorizontalSubsampling, 1),
                        intValue(d, kCVPixelFormatVerticalSubsampling, 1) };
    return p;
}

static Boolean truth(CFDictionaryRef d, CFStringRef key) {
    CFTypeRef v = CFDictionaryGetValue(d, key);
    return v != NULL && CFGetTypeID(v) == CFBooleanGetTypeID() && CFBooleanGetValue(v);
}

void CVPixelFormatDescriptionRegisterDescriptionWithPixelFormatType(CFDictionaryRef description, OSType format) {
    if (description == NULL || CFGetTypeID(description) != CFDictionaryGetTypeID())
        return;
    Row row = { format, NULL, truth(description, kCVPixelFormatContainsYCbCr) ? 'Y'
                              : truth(description, kCVPixelFormatContainsRGB) ? 'R' : 'O',
                truth(description, kCVPixelFormatContainsAlpha), '-', 1, { { 0 } } };
    CFArrayRef planes = CFDictionaryGetValue(description, kCVPixelFormatPlanes);
    if (planes != NULL && CFGetTypeID(planes) == CFArrayGetTypeID() && CFArrayGetCount(planes) > 0) {
        row.planes = (int)CFArrayGetCount(planes) < CV_MAX_PLANES ? (int)CFArrayGetCount(planes) : CV_MAX_PLANES;
        for (int i = 0; i < row.planes; i++)
            row.plane[i] = readPlane(CFArrayGetValueAtIndex(planes, i));
    } else
        row.plane[0] = readPlane(description);
    for (int i = 0; i < row.planes; i++)
        if (row.plane[i].bitsPerBlock <= 0)
            return;
    Row existing;
    if (findRow(format, &existing, NULL))
        return;                 /* a format is described once */
    pthread_mutex_lock(&registeredLock);
    registered = realloc(registered, (registeredCount + 1) * sizeof(Row));
    registeredDescriptions = realloc(registeredDescriptions, (registeredCount + 1) * sizeof(CFDictionaryRef));
    registered[registeredCount] = row;
    registeredDescriptions[registeredCount] = CFDictionaryCreateCopy(NULL, description);
    registeredCount++;
    pthread_mutex_unlock(&registeredLock);
}
