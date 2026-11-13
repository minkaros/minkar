/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CVBuffer and CVPixelBuffer: a CF object with attachments (kept to be
 * passed on, or not), and the pixels in planes: in an IOSurface when the
 * attributes ask for one (kCVPixelBufferIOSurfacePropertiesKey), in memory
 * of its own otherwise, or in the caller's memory, released through its
 * callback. Locking counts; the memory does not move.
 */

#include "CVPixelBuffer.h"
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLANES 3

typedef struct {
    size_t width, height, bytesPerRow;
    void *base;
} Plane;

struct __CVBuffer {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    CFMutableDictionaryRef propagated;
    CFMutableDictionaryRef unpropagated;
    size_t width, height;
    OSType format;
    size_t planeCount;		/* 0: not planar */
    Plane planes[MAX_PLANES];
    void *data;			/* the whole (non-planar) buffer, or plane 0's */
    size_t bytesPerRow;
    size_t dataSize;
    IOSurfaceRef surface;
    void *owned;		/* our own memory */
    CVPixelBufferReleaseBytesCallback releaseBytes;
    CVPixelBufferReleasePlanarBytesCallback releasePlanar;
    void *releaseRefCon;
    void *releaseData;
    int lockCount;
};

static CFTypeID pixelBufferTypeID;

static void finalize(CFTypeRef cf) {
    struct __CVBuffer *b = (struct __CVBuffer *)cf;
    if (b->releaseBytes != NULL)
        b->releaseBytes(b->releaseRefCon, b->data);
    if (b->releasePlanar != NULL) {
        const void *addresses[MAX_PLANES];
        for (size_t i = 0; i < b->planeCount; i++)
            addresses[i] = b->planes[i].base;
        b->releasePlanar(b->releaseRefCon, b->releaseData, b->dataSize, b->planeCount, addresses);
    }
    if (b->surface != NULL)
        CFRelease(b->surface);
    free(b->owned);
    if (b->propagated != NULL)
        CFRelease(b->propagated);
    if (b->unpropagated != NULL)
        CFRelease(b->unpropagated);
    pthread_mutex_destroy(&b->lock);
}

static CFStringRef describe(CFTypeRef cf) {
    struct __CVBuffer *b = (struct __CVBuffer *)cf;
    char f[5] = { (char)(b->format >> 24), (char)(b->format >> 16), (char)(b->format >> 8), (char)b->format, 0 };
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("<CVPixelBuffer %p width=%zu height=%zu format=%s planes=%zu%s>"),
        cf, b->width, b->height, b->format >= 0x20202020 ? f : "?", b->planeCount, b->surface ? " iosurface" : "");
}

static const CFRuntimeClass pixelBufferClass = {
    0, "CVPixelBuffer", NULL, NULL, finalize, NULL, NULL, NULL, describe
};

CFTypeID CVPixelBufferGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ pixelBufferTypeID = _CFRuntimeRegisterClass(&pixelBufferClass); });
    return pixelBufferTypeID;
}

static struct __CVBuffer *createBuffer(CFAllocatorRef allocator, size_t width, size_t height, OSType format) {
    size_t extra = sizeof(struct __CVBuffer) - sizeof(CFRuntimeBase);
    struct __CVBuffer *b = (struct __CVBuffer *)_CFRuntimeCreateInstance(allocator, CVPixelBufferGetTypeID(), extra, NULL);
    if (b == NULL)
        return NULL;
    memset((char *)b + sizeof(CFRuntimeBase), 0, extra);
    pthread_mutex_init(&b->lock, NULL);
    b->width = width;
    b->height = height;
    b->format = format;
    return b;
}

/* -- Formats: planes, their subsampling and bytes per element -- */

typedef struct {
    OSType format;
    int planes;
    struct { int bytesPerElement, xShift, yShift; } plane[MAX_PLANES];
} FormatInfo;

static const FormatInfo formats[] = {
    { kCVPixelFormatType_32ARGB, 0, { { 4, 0, 0 } } },
    { kCVPixelFormatType_32BGRA, 0, { { 4, 0, 0 } } },
    { kCVPixelFormatType_32RGBA, 0, { { 4, 0, 0 } } },
    { kCVPixelFormatType_24RGB, 0, { { 3, 0, 0 } } },
    { kCVPixelFormatType_OneComponent8, 0, { { 1, 0, 0 } } },
    { kCVPixelFormatType_422YpCbCr8, 0, { { 2, 0, 0 } } },
    { kCVPixelFormatType_422YpCbCr8_yuvs, 0, { { 2, 0, 0 } } },
    { kCVPixelFormatType_64RGBAHalf, 0, { { 8, 0, 0 } } },
    { kCVPixelFormatType_420YpCbCr8Planar, 3, { { 1, 0, 0 }, { 1, 1, 1 }, { 1, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr8PlanarFullRange, 3, { { 1, 0, 0 }, { 1, 1, 1 }, { 1, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, 2, { { 1, 0, 0 }, { 2, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, 2, { { 1, 0, 0 }, { 2, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, 2, { { 2, 0, 0 }, { 4, 1, 1 } } },
    { kCVPixelFormatType_420YpCbCr10BiPlanarFullRange, 2, { { 2, 0, 0 }, { 4, 1, 1 } } },
};

static const FormatInfo *formatInfo(OSType format) {
    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
        if (formats[i].format == format)
            return &formats[i];
    return NULL;
}

static size_t alignTo(size_t n, size_t a) {
    return (n + a - 1) / a * a;
}

static size_t numberAttribute(CFDictionaryRef attributes, CFStringRef key, size_t fallback) {
    CFNumberRef n = attributes != NULL ? CFDictionaryGetValue(attributes, key) : NULL;
    long long v;
    if (n == NULL || CFGetTypeID(n) != CFNumberGetTypeID() || !CFNumberGetValue(n, kCFNumberLongLongType, &v) || v <= 0)
        return fallback;
    return (size_t)v;
}

static void setNumber(CFMutableDictionaryRef d, CFStringRef key, long long v) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberLongLongType, &v);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

/* The planes' sizes and rows of a format at a size, with rows aligned. */
static void layoutPlanes(const FormatInfo *info, size_t width, size_t height, size_t alignment,
                         Plane *planes, size_t *count, size_t *total) {
    size_t n = info->planes > 0 ? (size_t)info->planes : 1;
    size_t offset = 0;
    for (size_t i = 0; i < n; i++) {
        size_t w = (width + ((1u << info->plane[i].xShift) - 1)) >> info->plane[i].xShift;
        size_t h = (height + ((1u << info->plane[i].yShift) - 1)) >> info->plane[i].yShift;
        planes[i].width = w;
        planes[i].height = h;
        planes[i].bytesPerRow = alignTo(w * info->plane[i].bytesPerElement, alignment);
        planes[i].base = (void *)offset;	/* an offset until placed */
        offset += planes[i].bytesPerRow * h;
    }
    *count = info->planes > 0 ? n : 0;
    *total = offset;
}

/* -- Creating -- */

static void readSurface(struct __CVBuffer *b) {
    b->planeCount = IOSurfaceGetPlaneCount(b->surface);
    if (b->planeCount > MAX_PLANES)
        b->planeCount = MAX_PLANES;
    if (b->planeCount == 0) {
        b->data = IOSurfaceGetBaseAddress(b->surface);
        b->bytesPerRow = IOSurfaceGetBytesPerRow(b->surface);
    } else {
        for (size_t i = 0; i < b->planeCount; i++) {
            b->planes[i].width = IOSurfaceGetWidthOfPlane(b->surface, i);
            b->planes[i].height = IOSurfaceGetHeightOfPlane(b->surface, i);
            b->planes[i].bytesPerRow = IOSurfaceGetBytesPerRowOfPlane(b->surface, i);
            b->planes[i].base = IOSurfaceGetBaseAddressOfPlane(b->surface, i);
        }
        b->data = b->planes[0].base;
        b->bytesPerRow = b->planes[0].bytesPerRow;
    }
    b->dataSize = IOSurfaceGetAllocSize(b->surface);
}

CVReturn CVPixelBufferCreate(CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType,
    CFDictionaryRef attributes, CVPixelBufferRef *out) {
    if (out == NULL)
        return kCVReturnInvalidArgument;
    *out = NULL;
    if (width == 0 || height == 0)
        return kCVReturnInvalidSize;
    const FormatInfo *info = formatInfo(pixelFormatType);
    if (info == NULL)
        return kCVReturnInvalidPixelFormat;
    size_t alignment = numberAttribute(attributes, kCVPixelBufferBytesPerRowAlignmentKey, 64);
    Plane planes[MAX_PLANES];
    size_t count, total;
    layoutPlanes(info, width, height, alignment, planes, &count, &total);

    struct __CVBuffer *b = createBuffer(allocator, width, height, pixelFormatType);
    if (b == NULL)
        return kCVReturnAllocationFailed;

    if (attributes != NULL && CFDictionaryContainsKey(attributes, kCVPixelBufferIOSurfacePropertiesKey)) {
        CFMutableDictionaryRef props = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        setNumber(props, kIOSurfaceWidth, (long long)width);
        setNumber(props, kIOSurfaceHeight, (long long)height);
        setNumber(props, kIOSurfacePixelFormat, pixelFormatType);
        setNumber(props, kIOSurfaceAllocSize, (long long)total);
        if (count == 0) {
            setNumber(props, kIOSurfaceBytesPerElement, info->plane[0].bytesPerElement);
            setNumber(props, kIOSurfaceBytesPerRow, (long long)planes[0].bytesPerRow);
        } else {
            CFMutableArrayRef infos = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            for (size_t i = 0; i < count; i++) {
                CFMutableDictionaryRef p = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
                setNumber(p, kIOSurfacePlaneWidth, (long long)planes[i].width);
                setNumber(p, kIOSurfacePlaneHeight, (long long)planes[i].height);
                setNumber(p, kIOSurfacePlaneBytesPerRow, (long long)planes[i].bytesPerRow);
                setNumber(p, kIOSurfacePlaneOffset, (long long)(uintptr_t)planes[i].base);
                setNumber(p, kIOSurfacePlaneSize, (long long)(planes[i].bytesPerRow * planes[i].height));
                setNumber(p, kIOSurfacePlaneBytesPerElement, info->plane[i].bytesPerElement);
                CFArrayAppendValue(infos, p);
                CFRelease(p);
            }
            CFDictionarySetValue(props, kIOSurfacePlaneInfo, infos);
            CFRelease(infos);
        }
        b->surface = IOSurfaceCreate(props);
        CFRelease(props);
        if (b->surface == NULL) {
            CFRelease(b);
            return kCVReturnAllocationFailed;
        }
        readSurface(b);
    } else {
        b->owned = calloc(1, total > 0 ? total : 1);
        if (b->owned == NULL) {
            CFRelease(b);
            return kCVReturnAllocationFailed;
        }
        b->planeCount = count;
        for (size_t i = 0; i < (count > 0 ? count : 1); i++) {
            b->planes[i] = planes[i];
            b->planes[i].base = (char *)b->owned + (uintptr_t)planes[i].base;
        }
        b->data = b->planes[0].base;
        b->bytesPerRow = planes[0].bytesPerRow;
        b->dataSize = total;
    }
    *out = (CVPixelBufferRef)b;
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferCreateWithBytes(CFAllocatorRef allocator, size_t width, size_t height, OSType pixelFormatType,
    void *baseAddress, size_t bytesPerRow, CVPixelBufferReleaseBytesCallback releaseCallback, void *releaseRefCon,
    CFDictionaryRef attributes, CVPixelBufferRef *out) {
    if (out == NULL || baseAddress == NULL)
        return kCVReturnInvalidArgument;
    *out = NULL;
    const FormatInfo *info = formatInfo(pixelFormatType);
    if (info != NULL && info->planes > 0)
        return kCVReturnInvalidPixelFormat;
    struct __CVBuffer *b = createBuffer(allocator, width, height, pixelFormatType);
    if (b == NULL)
        return kCVReturnAllocationFailed;
    b->data = baseAddress;
    b->bytesPerRow = bytesPerRow;
    b->dataSize = bytesPerRow * height;
    b->planes[0] = (Plane){ width, height, bytesPerRow, baseAddress };
    b->releaseBytes = releaseCallback;
    b->releaseRefCon = releaseRefCon;
    *out = (CVPixelBufferRef)b;
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferCreateWithPlanarBytes(CFAllocatorRef allocator, size_t width, size_t height,
    OSType pixelFormatType, void *dataPtr, size_t dataSize, size_t numberOfPlanes, void *planeBaseAddress[],
    size_t planeWidth[], size_t planeHeight[], size_t planeBytesPerRow[],
    CVPixelBufferReleasePlanarBytesCallback releaseCallback, void *releaseRefCon,
    CFDictionaryRef attributes, CVPixelBufferRef *out) {
    if (out == NULL || numberOfPlanes == 0 || numberOfPlanes > MAX_PLANES || planeBaseAddress == NULL)
        return kCVReturnInvalidArgument;
    *out = NULL;
    struct __CVBuffer *b = createBuffer(allocator, width, height, pixelFormatType);
    if (b == NULL)
        return kCVReturnAllocationFailed;
    b->planeCount = numberOfPlanes;
    for (size_t i = 0; i < numberOfPlanes; i++)
        b->planes[i] = (Plane){ planeWidth[i], planeHeight[i], planeBytesPerRow[i], planeBaseAddress[i] };
    b->data = dataPtr != NULL ? dataPtr : planeBaseAddress[0];
    b->bytesPerRow = planeBytesPerRow[0];
    b->dataSize = dataSize;
    b->releasePlanar = releaseCallback;
    b->releaseRefCon = releaseRefCon;
    b->releaseData = dataPtr;
    *out = (CVPixelBufferRef)b;
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferCreateWithIOSurface(CFAllocatorRef allocator, IOSurfaceRef surface,
    CFDictionaryRef attributes, CVPixelBufferRef *out) {
    if (out == NULL || surface == NULL)
        return kCVReturnInvalidArgument;
    *out = NULL;
    struct __CVBuffer *b = createBuffer(allocator, IOSurfaceGetWidth(surface), IOSurfaceGetHeight(surface),
        IOSurfaceGetPixelFormat(surface));
    if (b == NULL)
        return kCVReturnAllocationFailed;
    b->surface = (IOSurfaceRef)CFRetain(surface);
    readSurface(b);
    *out = (CVPixelBufferRef)b;
    return kCVReturnSuccess;
}

IOSurfaceRef CVPixelBufferGetIOSurface(CVPixelBufferRef pb) {
    return pb != NULL ? ((struct __CVBuffer *)pb)->surface : NULL;
}

/* -- Retain, release, attachments -- */

CVBufferRef CVBufferRetain(CVBufferRef buffer) {
    return buffer != NULL ? (CVBufferRef)CFRetain(buffer) : NULL;
}

void CVBufferRelease(CVBufferRef buffer) {
    if (buffer != NULL)
        CFRelease(buffer);
}

CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef pb) {
    return CVBufferRetain(pb);
}

void CVPixelBufferRelease(CVPixelBufferRef pb) {
    CVBufferRelease(pb);
}

static CFMutableDictionaryRef attachments(struct __CVBuffer *b, CVAttachmentMode mode) {
    CFMutableDictionaryRef *d = mode == kCVAttachmentMode_ShouldPropagate ? &b->propagated : &b->unpropagated;
    if (*d == NULL)
        *d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return *d;
}

void CVBufferSetAttachment(CVBufferRef buffer, CFStringRef key, CFTypeRef value, CVAttachmentMode mode) {
    struct __CVBuffer *b = (struct __CVBuffer *)buffer;
    if (b == NULL || key == NULL || value == NULL)
        return;
    pthread_mutex_lock(&b->lock);
    CFDictionaryRemoveValue(attachments(b, !mode), key);
    CFDictionarySetValue(attachments(b, mode), key, value);
    pthread_mutex_unlock(&b->lock);
}

CFTypeRef CVBufferGetAttachment(CVBufferRef buffer, CFStringRef key, CVAttachmentMode *mode) {
    struct __CVBuffer *b = (struct __CVBuffer *)buffer;
    if (b == NULL || key == NULL)
        return NULL;
    pthread_mutex_lock(&b->lock);
    CFTypeRef v = b->propagated ? CFDictionaryGetValue(b->propagated, key) : NULL;
    CVAttachmentMode m = kCVAttachmentMode_ShouldPropagate;
    if (v == NULL && b->unpropagated != NULL) {
        v = CFDictionaryGetValue(b->unpropagated, key);
        m = kCVAttachmentMode_ShouldNotPropagate;
    }
    pthread_mutex_unlock(&b->lock);
    if (v != NULL && mode != NULL)
        *mode = m;
    return v;
}

CFTypeRef CVBufferCopyAttachment(CVBufferRef buffer, CFStringRef key, CVAttachmentMode *mode) {
    CFTypeRef v = CVBufferGetAttachment(buffer, key, mode);
    return v != NULL ? CFRetain(v) : NULL;
}

void CVBufferRemoveAttachment(CVBufferRef buffer, CFStringRef key) {
    struct __CVBuffer *b = (struct __CVBuffer *)buffer;
    if (b == NULL || key == NULL)
        return;
    pthread_mutex_lock(&b->lock);
    if (b->propagated != NULL)
        CFDictionaryRemoveValue(b->propagated, key);
    if (b->unpropagated != NULL)
        CFDictionaryRemoveValue(b->unpropagated, key);
    pthread_mutex_unlock(&b->lock);
}

void CVBufferRemoveAllAttachments(CVBufferRef buffer) {
    struct __CVBuffer *b = (struct __CVBuffer *)buffer;
    if (b == NULL)
        return;
    pthread_mutex_lock(&b->lock);
    if (b->propagated != NULL)
        CFDictionaryRemoveAllValues(b->propagated);
    if (b->unpropagated != NULL)
        CFDictionaryRemoveAllValues(b->unpropagated);
    pthread_mutex_unlock(&b->lock);
}

CFDictionaryRef CVBufferGetAttachments(CVBufferRef buffer, CVAttachmentMode mode) {
    struct __CVBuffer *b = (struct __CVBuffer *)buffer;
    if (b == NULL)
        return NULL;
    return mode == kCVAttachmentMode_ShouldPropagate ? b->propagated : b->unpropagated;
}

CFDictionaryRef CVBufferCopyAttachments(CVBufferRef buffer, CVAttachmentMode mode) {
    struct __CVBuffer *b = (struct __CVBuffer *)buffer;
    if (b == NULL)
        return NULL;
    pthread_mutex_lock(&b->lock);
    CFDictionaryRef d = CVBufferGetAttachments(buffer, mode);
    CFDictionaryRef copy = d != NULL ? CFDictionaryCreateCopy(NULL, d) : NULL;
    pthread_mutex_unlock(&b->lock);
    return copy;
}

static void setOne(const void *key, const void *value, void *context) {
    void **c = context;
    CVBufferSetAttachment((CVBufferRef)c[0], key, value, (CVAttachmentMode)(uintptr_t)c[1]);
}

void CVBufferSetAttachments(CVBufferRef buffer, CFDictionaryRef theAttachments, CVAttachmentMode mode) {
    if (buffer == NULL || theAttachments == NULL)
        return;
    void *context[2] = { (void *)buffer, (void *)(uintptr_t)mode };
    CFDictionaryApplyFunction(theAttachments, setOne, context);
}

void CVBufferPropagateAttachments(CVBufferRef source, CVBufferRef destination) {
    CFDictionaryRef d = CVBufferCopyAttachments(source, kCVAttachmentMode_ShouldPropagate);
    if (d != NULL) {
        CVBufferSetAttachments(destination, d, kCVAttachmentMode_ShouldPropagate);
        CFRelease(d);
    }
}

/* -- The pixels -- */

CVReturn CVPixelBufferLockBaseAddress(CVPixelBufferRef pb, CVOptionFlags lockFlags) {
    struct __CVBuffer *b = (struct __CVBuffer *)pb;
    if (b == NULL)
        return kCVReturnInvalidArgument;
    if (b->surface != NULL)
        IOSurfaceLock(b->surface, (IOSurfaceLockOptions)(lockFlags & kCVPixelBufferLock_ReadOnly), NULL);
    pthread_mutex_lock(&b->lock);
    b->lockCount++;
    pthread_mutex_unlock(&b->lock);
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferUnlockBaseAddress(CVPixelBufferRef pb, CVOptionFlags unlockFlags) {
    struct __CVBuffer *b = (struct __CVBuffer *)pb;
    if (b == NULL)
        return kCVReturnInvalidArgument;
    pthread_mutex_lock(&b->lock);
    Boolean locked = b->lockCount > 0;
    if (locked)
        b->lockCount--;
    pthread_mutex_unlock(&b->lock);
    if (!locked)
        return kCVReturnError;
    if (b->surface != NULL)
        IOSurfaceUnlock(b->surface, (IOSurfaceLockOptions)(unlockFlags & kCVPixelBufferLock_ReadOnly), NULL);
    return kCVReturnSuccess;
}

size_t CVPixelBufferGetWidth(CVPixelBufferRef pb) { return pb ? ((struct __CVBuffer *)pb)->width : 0; }
size_t CVPixelBufferGetHeight(CVPixelBufferRef pb) { return pb ? ((struct __CVBuffer *)pb)->height : 0; }
OSType CVPixelBufferGetPixelFormatType(CVPixelBufferRef pb) { return pb ? ((struct __CVBuffer *)pb)->format : 0; }
void *CVPixelBufferGetBaseAddress(CVPixelBufferRef pb) { return pb ? ((struct __CVBuffer *)pb)->data : NULL; }
size_t CVPixelBufferGetBytesPerRow(CVPixelBufferRef pb) { return pb ? ((struct __CVBuffer *)pb)->bytesPerRow : 0; }
size_t CVPixelBufferGetDataSize(CVPixelBufferRef pb) { return pb ? ((struct __CVBuffer *)pb)->dataSize : 0; }
Boolean CVPixelBufferIsPlanar(CVPixelBufferRef pb) { return pb ? ((struct __CVBuffer *)pb)->planeCount > 0 : false; }
size_t CVPixelBufferGetPlaneCount(CVPixelBufferRef pb) { return pb ? ((struct __CVBuffer *)pb)->planeCount : 0; }

static const Plane *plane(CVPixelBufferRef pb, size_t i) {
    struct __CVBuffer *b = (struct __CVBuffer *)pb;
    return b != NULL && i < b->planeCount ? &b->planes[i] : NULL;
}

size_t CVPixelBufferGetWidthOfPlane(CVPixelBufferRef pb, size_t i) { const Plane *p = plane(pb, i); return p ? p->width : 0; }
size_t CVPixelBufferGetHeightOfPlane(CVPixelBufferRef pb, size_t i) { const Plane *p = plane(pb, i); return p ? p->height : 0; }
void *CVPixelBufferGetBaseAddressOfPlane(CVPixelBufferRef pb, size_t i) { const Plane *p = plane(pb, i); return p ? p->base : NULL; }
size_t CVPixelBufferGetBytesPerRowOfPlane(CVPixelBufferRef pb, size_t i) { const Plane *p = plane(pb, i); return p ? p->bytesPerRow : 0; }

/* No padding is added around the image. */
void CVPixelBufferGetExtendedPixels(CVPixelBufferRef pb, size_t *left, size_t *right, size_t *top, size_t *bottom) {
    if (left) *left = 0;
    if (right) *right = 0;
    if (top) *top = 0;
    if (bottom) *bottom = 0;
}

CVReturn CVPixelBufferFillExtendedPixels(CVPixelBufferRef pb) {
    return pb != NULL ? kCVReturnSuccess : kCVReturnInvalidArgument;
}
