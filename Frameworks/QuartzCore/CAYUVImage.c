/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A YCbCr IOSurface as a BGRA image, for a layer whose contents it is: a
 * video decoder's frames (NV12 from VideoToolbox, as Firefox shows them
 * through Core Animation), I420, 4:2:2 and 10-bit 4:2:0. The surface's
 * matrix (its YCbCr matrix value, BT.601 or BT.709) and range (video or
 * full, from the pixel format) are honoured; without a matrix, HD sizes
 * are BT.709 and others BT.601, as players do. Built at -O2: it runs for
 * every pixel of every frame shown.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOSurface/IOSurface.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { kNV12, kI420, kP010, kUYVY, kYUYV };

static int layoutOf(OSType f, int *full) {
    *full = 0;
    switch (f) {
    case '420f': *full = 1; /* fall through */
    case '420v': return kNV12;
    case 'f420': *full = 1; /* fall through */
    case 'y420': return kI420;
    case 'xf20': *full = 1; /* fall through */
    case 'x420': return kP010;
    case '2vuy': return kUYVY;
    case 'yuvs': return kYUYV;
    default: return -1;
    }
}

int _CAIsYCbCrSurfaceFormat(OSType format) {
    int full;
    return layoutOf(format, &full) >= 0;
}

/* Coefficients in 16.16 fixed point, from the range's scaled Y and C. */
typedef struct {
    int32_t y, rv, gu, gv, bu;  /* R = Y + rv V; G = Y - gu U - gv V; B = Y + bu U */
    int32_t yOffset;
} Matrix;

static Matrix matrixFor(int bt709, int full) {
    double kr = bt709 ? 0.2126 : 0.299, kb = bt709 ? 0.0722 : 0.114, kg = 1 - kr - kb;
    double ys = full ? 1.0 : 255.0 / 219.0, cs = full ? 1.0 : 255.0 / 224.0;
    Matrix m;

    m.y = (int32_t)(ys * 65536 + 0.5);
    m.rv = (int32_t)(cs * 2 * (1 - kr) * 65536 + 0.5);
    m.bu = (int32_t)(cs * 2 * (1 - kb) * 65536 + 0.5);
    m.gu = (int32_t)(cs * 2 * (1 - kb) * kb / kg * 65536 + 0.5);
    m.gv = (int32_t)(cs * 2 * (1 - kr) * kr / kg * 65536 + 0.5);
    m.yOffset = full ? 0 : 16;
    return m;
}

static inline uint8_t clamp8(int32_t v) {
    return v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v;
}

static inline uint32_t pixel(const Matrix *m, int32_t y, int32_t u, int32_t v) {
    int32_t yy = (y - m->yOffset) * m->y + 32768;
    u -= 128;
    v -= 128;
    uint8_t r = clamp8((yy + m->rv * v) >> 16);
    uint8_t g = clamp8((yy - m->gu * u - m->gv * v) >> 16);
    uint8_t b = clamp8((yy + m->bu * u) >> 16);
    return (uint32_t)b | (uint32_t)g << 8 | (uint32_t)r << 16 | 0xFF000000u;   /* BGRA in memory */
}

static int isBT709(IOSurfaceRef surface, size_t height) {
    static const CFStringRef keys[] = { CFSTR("IOSurfaceYCbCrMatrix"), CFSTR("CVImageBufferYCbCrMatrix") };
    for (int i = 0; i < 2; i++) {
        CFTypeRef v = IOSurfaceCopyValue(surface, keys[i]);
        if (v == NULL)
            continue;
        int bt709 = -1;
        if (CFGetTypeID(v) == CFStringGetTypeID()) {
            if (CFStringFind(v, CFSTR("709"), 0).location != kCFNotFound)
                bt709 = 1;
            else if (CFStringFind(v, CFSTR("601"), 0).location != kCFNotFound)
                bt709 = 0;
        }
        CFRelease(v);
        if (bt709 >= 0)
            return bt709;
    }
    return height >= 720;
}

static void releasePixels(void *info, const void *data, size_t size) {
    (void)info;
    (void)size;
    free((void *)data);
}

/* The conversion, from the planes' bytes: one plane for 4:2:2, two for
 * NV12 and P010, three for I420. */
static uint32_t *convert(int layout, int full, int bt709, size_t width, size_t height,
                         const void *const planes[3], const size_t strides[3]) {
    uint32_t *out = malloc(width * height * 4);
    if (out == NULL)
        return NULL;
    Matrix m = matrixFor(bt709, full);

    if (layout == kNV12 || layout == kI420 || layout == kP010) {
        const uint8_t *yb = planes[0], *cb = planes[1], *crb = planes[2];
        size_t ys = strides[0], cs = strides[1], crs = strides[2];
        for (size_t row = 0; row < height; row++) {
            uint32_t *o = out + row * width;
            const uint8_t *yr = yb + row * ys, *cr = cb + (row / 2) * cs;
            if (layout == kNV12) {
                for (size_t x = 0; x < width; x++)
                    o[x] = pixel(&m, yr[x], cr[(x & ~(size_t)1)], cr[(x & ~(size_t)1) + 1]);
            } else if (layout == kI420) {
                const uint8_t *vr = crb + (row / 2) * crs;
                for (size_t x = 0; x < width; x++)
                    o[x] = pixel(&m, yr[x], cr[x / 2], vr[x / 2]);
            } else {    /* P010: 16-bit samples, the top 10 bits */
                const uint16_t *y16 = (const uint16_t *)yr, *c16 = (const uint16_t *)cr;
                for (size_t x = 0; x < width; x++)
                    o[x] = pixel(&m, y16[x] >> 8, c16[(x & ~(size_t)1)] >> 8, c16[(x & ~(size_t)1) + 1] >> 8);
            }
        }
    } else {
        const uint8_t *base = planes[0];
        for (size_t row = 0; row < height; row++) {
            const uint8_t *s = base + row * strides[0];
            uint32_t *o = out + row * width;
            for (size_t x = 0; x < width; x++) {
                const uint8_t *pair = s + (x / 2) * 4;
                if (layout == kUYVY)    /* U Y0 V Y1 */
                    o[x] = pixel(&m, pair[1 + (x & 1) * 2], pair[0], pair[2]);
                else                    /* Y0 U Y1 V */
                    o[x] = pixel(&m, pair[(x & 1) * 2], pair[1], pair[3]);
            }
        }
    }
    return out;
}

static CGImageRef imageOf(uint32_t *pixels, size_t width, size_t height) {
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, pixels, width * height * 4, releasePixels);
    CGImageRef image = CGImageCreate(width, height, 8, 32, width * 4, space,
                                     kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little, provider, NULL, false,
                                     kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(space);
    return image;
}

static int planesNeeded(int layout) {
    return layout == kI420 ? 3 : layout == kNV12 || layout == kP010 ? 2 : 1;
}

/* For buffers in plain memory (a CVPixelBuffer without a surface):
 * bt709 is 1 or 0, or -1 to choose by the height. */
CGImageRef _CACreateImageFromYCbCrPlanes(OSType format, size_t width, size_t height,
                                         const void *const planes[3], const size_t strides[3], int bt709) {
    int full, layout = layoutOf(format, &full);

    if (layout < 0 || width == 0 || height == 0 || planes == NULL || strides == NULL)
        return NULL;
    for (int i = 0; i < planesNeeded(layout); i++)
        if (planes[i] == NULL)
            return NULL;
    uint32_t *pixels = convert(layout, full, bt709 >= 0 ? bt709 : height >= 720, width, height, planes, strides);
    return pixels != NULL ? imageOf(pixels, width, height) : NULL;
}

CGImageRef _CACreateImageFromYCbCrSurface(IOSurfaceRef surface) {
    OSType format = IOSurfaceGetPixelFormat(surface);
    size_t width = IOSurfaceGetWidth(surface), height = IOSurfaceGetHeight(surface);
    int full, layout = layoutOf(format, &full);

    if (layout < 0 || width == 0 || height == 0)
        return NULL;
    int count = planesNeeded(layout);
    if (count > 1 && (int)IOSurfaceGetPlaneCount(surface) < count)
        return NULL;
    const void *planes[3] = { NULL, NULL, NULL };
    size_t strides[3] = { 0, 0, 0 };
    IOSurfaceLock(surface, kIOSurfaceLockReadOnly, NULL);
    for (int i = 0; i < count; i++) {
        planes[i] = count > 1 ? IOSurfaceGetBaseAddressOfPlane(surface, i) : IOSurfaceGetBaseAddress(surface);
        strides[i] = count > 1 ? IOSurfaceGetBytesPerRowOfPlane(surface, i) : IOSurfaceGetBytesPerRow(surface);
    }
    uint32_t *pixels = NULL;
    if (planes[0] != NULL && (count < 2 || planes[1] != NULL) && (count < 3 || planes[2] != NULL))
        pixels = convert(layout, full, isBT709(surface, height), width, height, planes, strides);
    IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, NULL);
    return pixels != NULL ? imageOf(pixels, width, height) : NULL;
}
