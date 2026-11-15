/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An image buffer's sizes, clean rectangle and color space, read from its
 * attachments: the clean aperture (a width and height, centered, moved by
 * its offsets), the pixel aspect ratio, the display dimensions, and the
 * color primaries and transfer function (a named color space for the
 * usual pairs). Also several buffer attribute dictionaries made one.
 */

#include "CVPixelBuffer.h"

static double number(CFDictionaryRef d, CFStringRef key, double otherwise) {
    CFTypeRef v = d != NULL && CFGetTypeID(d) == CFDictionaryGetTypeID() ? CFDictionaryGetValue(d, key) : NULL;
    double x = otherwise;
    if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberDoubleType, &x);
    return x;
}

CGSize CVImageBufferGetEncodedSize(CVImageBufferRef b) {
    if (b == NULL || CFGetTypeID(b) != CVPixelBufferGetTypeID())
        return CGSizeZero;
    return CGSizeMake(CVPixelBufferGetWidth(b), CVPixelBufferGetHeight(b));
}

CGRect CVImageBufferGetCleanRect(CVImageBufferRef b) {
    CGSize size = CVImageBufferGetEncodedSize(b);
    CFDictionaryRef ca = b != NULL ? CVBufferGetAttachment(b, kCVImageBufferCleanApertureKey, NULL) : NULL;
    double w = number(ca, kCVImageBufferCleanApertureWidthKey, size.width);
    double h = number(ca, kCVImageBufferCleanApertureHeightKey, size.height);
    double x = (size.width - w) / 2 + number(ca, kCVImageBufferCleanApertureHorizontalOffsetKey, 0);
    double y = (size.height - h) / 2 + number(ca, kCVImageBufferCleanApertureVerticalOffsetKey, 0);
    return CGRectMake(x, y, w, h);
}

/* The display dimensions when given; else the clean rectangle at the
   pixel aspect ratio. */
CGSize CVImageBufferGetDisplaySize(CVImageBufferRef b) {
    if (b == NULL)
        return CGSizeZero;
    CFDictionaryRef dd = CVBufferGetAttachment(b, kCVImageBufferDisplayDimensionsKey, NULL);
    if (dd != NULL && number(dd, kCVImageBufferDisplayWidthKey, 0) > 0)
        return CGSizeMake(number(dd, kCVImageBufferDisplayWidthKey, 0), number(dd, kCVImageBufferDisplayHeightKey, 0));
    CGSize size = CVImageBufferGetCleanRect(b).size;
    CFDictionaryRef par = CVBufferGetAttachment(b, kCVImageBufferPixelAspectRatioKey, NULL);
    double hs = number(par, kCVImageBufferPixelAspectRatioHorizontalSpacingKey, 1);
    double vs = number(par, kCVImageBufferPixelAspectRatioVerticalSpacingKey, 1);
    if (hs > 0 && vs > 0)
        size.width = size.width * hs / vs;
    return size;
}

/* A pixel buffer's rows go down from its first: not flipped. */
Boolean CVImageBufferIsFlipped(CVImageBufferRef b) {
    return false;
}

static Boolean is(CFDictionaryRef d, CFStringRef key, CFStringRef value) {
    CFTypeRef v = CFDictionaryGetValue(d, key);
    return v != NULL && CFEqual(v, value);
}

CGColorSpaceRef CVImageBufferCreateColorSpaceFromAttachments(CFDictionaryRef a) {
    if (a == NULL || CFGetTypeID(a) != CFDictionaryGetTypeID())
        return NULL;
    CFTypeRef given = CFDictionaryGetValue(a, kCVImageBufferCGColorSpaceKey);
    if (given != NULL)
        return (CGColorSpaceRef)CFRetain(given);
    CFStringRef name = NULL;
    Boolean linear = is(a, kCVImageBufferTransferFunctionKey, kCVImageBufferTransferFunction_Linear);
    Boolean srgb = is(a, kCVImageBufferTransferFunctionKey, kCVImageBufferTransferFunction_sRGB);
    if (is(a, kCVImageBufferColorPrimariesKey, kCVImageBufferColorPrimaries_ITU_R_709_2))
        name = linear ? kCGColorSpaceLinearSRGB : srgb ? kCGColorSpaceSRGB : kCGColorSpaceITUR_709;
    else if (is(a, kCVImageBufferColorPrimariesKey, kCVImageBufferColorPrimaries_P3_D65))
        name = kCGColorSpaceDisplayP3;
    else if (is(a, kCVImageBufferColorPrimariesKey, kCVImageBufferColorPrimaries_DCI_P3))
        name = kCGColorSpaceDCIP3;
    else if (is(a, kCVImageBufferColorPrimariesKey, kCVImageBufferColorPrimaries_ITU_R_2020))
        name = kCGColorSpaceITUR_2020;
    else if (is(a, kCVImageBufferColorPrimariesKey, kCVImageBufferColorPrimaries_EBU_3213) ||
             is(a, kCVImageBufferColorPrimariesKey, kCVImageBufferColorPrimaries_SMPTE_C))
        name = kCGColorSpaceSRGB;
    return name != NULL ? CGColorSpaceCreateWithName(name) : NULL;
}

/* The buffer's own color space attachment, or one made from its others
   once and kept as its color space. */
CGColorSpaceRef CVImageBufferGetColorSpace(CVImageBufferRef b) {
    if (b == NULL)
        return NULL;
    CFTypeRef cs = CVBufferGetAttachment(b, kCVImageBufferCGColorSpaceKey, NULL);
    if (cs != NULL)
        return (CGColorSpaceRef)cs;
    CFDictionaryRef a = CVBufferGetAttachments(b, kCVAttachmentMode_ShouldPropagate);
    CGColorSpaceRef made = CVImageBufferCreateColorSpaceFromAttachments(a);
    if (made == NULL)
        return NULL;
    CVBufferSetAttachment(b, kCVImageBufferCGColorSpaceKey, made, kCVAttachmentMode_ShouldPropagate);
    CFRelease(made);
    return (CGColorSpaceRef)CVBufferGetAttachment(b, kCVImageBufferCGColorSpaceKey, NULL);
}

/* Several attribute dictionaries made one: pixel formats gathered (one
   kept as a number, several as an array), sizes and alignments the
   largest, compatibilities if any asks, other keys the first one's. */
CVReturn CVPixelBufferCreateResolvedAttributesDictionary(CFAllocatorRef allocator, CFArrayRef attributes,
                                                         CFDictionaryRef *out) {
    if (out == NULL)
        return kCVReturnInvalidArgument;
    CFMutableDictionaryRef r = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    CFMutableArrayRef formats = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);
    CFIndex n = attributes != NULL ? CFArrayGetCount(attributes) : 0;
    for (CFIndex i = 0; i < n; i++) {
        CFDictionaryRef d = CFArrayGetValueAtIndex(attributes, i);
        if (d == NULL || CFGetTypeID(d) != CFDictionaryGetTypeID())
            continue;
        CFIndex count = CFDictionaryGetCount(d);
        const void *keys[count > 0 ? count : 1], *values[count > 0 ? count : 1];
        CFDictionaryGetKeysAndValues(d, keys, values);
        for (CFIndex k = 0; k < count; k++) {
            CFStringRef key = keys[k];
            CFTypeRef value = values[k], have = CFDictionaryGetValue(r, key);
            if (CFEqual(key, kCVPixelBufferPixelFormatTypeKey)) {
                CFArrayRef list = CFGetTypeID(value) == CFArrayGetTypeID() ? value : NULL;
                CFIndex m = list != NULL ? CFArrayGetCount(list) : 1;
                for (CFIndex j = 0; j < m; j++) {
                    CFTypeRef f = list != NULL ? CFArrayGetValueAtIndex(list, j) : value;
                    if (!CFArrayContainsValue(formats, CFRangeMake(0, CFArrayGetCount(formats)), f))
                        CFArrayAppendValue(formats, f);
                }
            } else if (have == NULL)
                CFDictionarySetValue(r, key, value);
            else if (CFGetTypeID(value) == CFNumberGetTypeID() && CFGetTypeID(have) == CFNumberGetTypeID()) {
                if (CFNumberCompare(value, have, NULL) == kCFCompareGreaterThan)
                    CFDictionarySetValue(r, key, value);
            } else if (CFGetTypeID(value) == CFBooleanGetTypeID() && CFBooleanGetValue(value))
                CFDictionarySetValue(r, key, kCFBooleanTrue);
        }
    }
    if (CFArrayGetCount(formats) == 1)
        CFDictionarySetValue(r, kCVPixelBufferPixelFormatTypeKey, CFArrayGetValueAtIndex(formats, 0));
    else if (CFArrayGetCount(formats) > 1)
        CFDictionarySetValue(r, kCVPixelBufferPixelFormatTypeKey, formats);
    CFRelease(formats);
    *out = r;
    return kCVReturnSuccess;
}
