/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Pools of pixel buffers alike: a buffer the app is done with goes back
 * to its pool and is given out again, so a decoder's frames do not each
 * allocate (an IOSurface, usually) anew.
 */

#ifndef __CVPIXELBUFFERPOOL__
#define __CVPIXELBUFFERPOOL__

#include <CoreVideo/CVPixelBuffer.h>

typedef struct __CVPixelBufferPool *CVPixelBufferPoolRef;

extern const CFStringRef kCVPixelBufferPoolMinimumBufferCountKey;
extern const CFStringRef kCVPixelBufferPoolMaximumBufferAgeKey;
extern const CFStringRef kCVPixelBufferPoolAllocationThresholdKey;
extern const CFStringRef kCVPixelBufferPoolFreeBufferNotification;

typedef CVOptionFlags CVPixelBufferPoolFlushFlags;
enum {
    kCVPixelBufferPoolFlushExcessBuffers = 1,
};

CFTypeID CVPixelBufferPoolGetTypeID(void);
CVPixelBufferPoolRef CVPixelBufferPoolRetain(CVPixelBufferPoolRef pixelBufferPool);
void CVPixelBufferPoolRelease(CVPixelBufferPoolRef pixelBufferPool);
CVReturn CVPixelBufferPoolCreate(CFAllocatorRef allocator, CFDictionaryRef poolAttributes,
    CFDictionaryRef pixelBufferAttributes, CVPixelBufferPoolRef *poolOut);
CFDictionaryRef CVPixelBufferPoolGetAttributes(CVPixelBufferPoolRef pool);
CFDictionaryRef CVPixelBufferPoolGetPixelBufferAttributes(CVPixelBufferPoolRef pool);
CVReturn CVPixelBufferPoolCreatePixelBuffer(CFAllocatorRef allocator, CVPixelBufferPoolRef pixelBufferPool,
    CVPixelBufferRef *pixelBufferOut);
CVReturn CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(CFAllocatorRef allocator,
    CVPixelBufferPoolRef pixelBufferPool, CFDictionaryRef auxAttributes, CVPixelBufferRef *pixelBufferOut);
void CVPixelBufferPoolFlush(CVPixelBufferPoolRef pool, CVPixelBufferPoolFlushFlags options);

#endif
