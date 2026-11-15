/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A pool keeps the buffers it made (their memory or IOSurface) and gives
 * the app a buffer of its own over one of them each time; when the app's
 * buffer is freed, the pool's is free again (and the pool says so, with
 * kCVPixelBufferPoolFreeBufferNotification). A free buffer older than the
 * maximum age goes, but the minimum count is kept; an allocation
 * threshold, when asked for, refuses a new buffer while that many are out.
 */

#include "CVPixelBufferPool.h"
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

const CFStringRef kCVPixelBufferPoolMinimumBufferCountKey = CFSTR("MinimumBufferCount");
const CFStringRef kCVPixelBufferPoolMaximumBufferAgeKey = CFSTR("MaximumBufferAge");
const CFStringRef kCVPixelBufferPoolAllocationThresholdKey = CFSTR("kCVPixelBufferPoolAllocationThresholdKey");
const CFStringRef kCVPixelBufferPoolFreeBufferNotification = CFSTR("kCVPixelBufferPoolFreeBufferNotification");

void _CVPixelBufferSetFinalizeHook(CVPixelBufferRef buffer, void (*hook)(void *refcon), void *refcon);

typedef struct {
    CVPixelBufferRef buffer;    /* the pool's own */
    Boolean out;                /* the app holds a buffer over it */
    CFAbsoluteTime freedAt;
} Slot;

struct __CVPixelBufferPool {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    CFDictionaryRef poolAttributes, bufferAttributes;
    size_t width, height;
    OSType format;
    CFIndex minimumCount;
    CFTimeInterval maximumAge;
    Slot *slots;
    CFIndex count, capacity;
};

static CFTypeID typeID;

static void poolFinalize(CFTypeRef cf) {
    CVPixelBufferPoolRef p = (CVPixelBufferPoolRef)cf;
    for (CFIndex i = 0; i < p->count; i++)
        CFRelease(p->slots[i].buffer);
    free(p->slots);
    if (p->poolAttributes != NULL)
        CFRelease(p->poolAttributes);
    if (p->bufferAttributes != NULL)
        CFRelease(p->bufferAttributes);
    pthread_mutex_destroy(&p->lock);
}

static const CFRuntimeClass poolClass = { 0, "CVPixelBufferPool", NULL, NULL, poolFinalize, NULL, NULL, NULL, NULL };

CFTypeID CVPixelBufferPoolGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ typeID = _CFRuntimeRegisterClass(&poolClass); });
    return typeID;
}

CVPixelBufferPoolRef CVPixelBufferPoolRetain(CVPixelBufferPoolRef p) {
    if (p != NULL)
        CFRetain(p);
    return p;
}

void CVPixelBufferPoolRelease(CVPixelBufferPoolRef p) {
    if (p != NULL)
        CFRelease(p);
}

static Boolean number(CFDictionaryRef d, CFStringRef key, CFNumberType type, void *value) {
    CFTypeRef v = d != NULL ? CFDictionaryGetValue(d, key) : NULL;
    return v != NULL && CFGetTypeID(v) == CFNumberGetTypeID() && CFNumberGetValue(v, type, value);
}

CVReturn CVPixelBufferPoolCreate(CFAllocatorRef allocator, CFDictionaryRef poolAttributes,
                                 CFDictionaryRef bufferAttributes, CVPixelBufferPoolRef *out) {
    if (out == NULL)
        return kCVReturnInvalidArgument;
    *out = NULL;
    long long width = 0, height = 0;
    SInt32 format = 0;
    if (!number(bufferAttributes, kCVPixelBufferWidthKey, kCFNumberLongLongType, &width) ||
        !number(bufferAttributes, kCVPixelBufferHeightKey, kCFNumberLongLongType, &height) ||
        !number(bufferAttributes, kCVPixelBufferPixelFormatTypeKey, kCFNumberSInt32Type, &format) || width <= 0 ||
        height <= 0)
        return kCVReturnInvalidPixelBufferAttributes;
    size_t extra = sizeof(struct __CVPixelBufferPool) - sizeof(CFRuntimeBase);
    CVPixelBufferPoolRef p = (CVPixelBufferPoolRef)_CFRuntimeCreateInstance(allocator, CVPixelBufferPoolGetTypeID(),
                                                                              extra, NULL);
    if (p == NULL)
        return kCVReturnAllocationFailed;
    pthread_mutex_init(&p->lock, NULL);
    p->width = (size_t)width;
    p->height = (size_t)height;
    p->format = (OSType)format;
    p->bufferAttributes = CFDictionaryCreateCopy(allocator, bufferAttributes);
    p->poolAttributes = poolAttributes != NULL ? CFDictionaryCreateCopy(allocator, poolAttributes) : NULL;
    long long minimum = 0;
    double age = 1.0;
    number(poolAttributes, kCVPixelBufferPoolMinimumBufferCountKey, kCFNumberLongLongType, &minimum);
    number(poolAttributes, kCVPixelBufferPoolMaximumBufferAgeKey, kCFNumberDoubleType, &age);
    p->minimumCount = minimum > 0 ? (CFIndex)minimum : 0;
    p->maximumAge = age;
    /* The minimum count made at once. */
    for (CFIndex i = 0; i < p->minimumCount; i++) {
        CVPixelBufferRef b = NULL;
        if (CVPixelBufferCreate(allocator, p->width, p->height, p->format, p->bufferAttributes, &b) != kCVReturnSuccess)
            break;
        if (p->count == p->capacity) {
            p->capacity = p->capacity > 0 ? p->capacity * 2 : 8;
            p->slots = realloc(p->slots, (size_t)p->capacity * sizeof(Slot));
        }
        p->slots[p->count++] = (Slot){ b, false, CFAbsoluteTimeGetCurrent() };
    }
    *out = p;
    return kCVReturnSuccess;
}

CFDictionaryRef CVPixelBufferPoolGetAttributes(CVPixelBufferPoolRef p) {
    return p != NULL ? p->poolAttributes : NULL;
}

CFDictionaryRef CVPixelBufferPoolGetPixelBufferAttributes(CVPixelBufferPoolRef p) {
    return p != NULL ? p->bufferAttributes : NULL;
}

/* Free buffers older than the maximum age go, down to the minimum count;
   the lock is held. */
static void ageOut(CVPixelBufferPoolRef p, Boolean all) {
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    for (CFIndex i = p->count - 1; i >= 0 && p->count > p->minimumCount; i--) {
        Slot *s = &p->slots[i];
        if (s->out || (!all && now - s->freedAt < p->maximumAge))
            continue;
        CFRelease(s->buffer);
        p->slots[i] = p->slots[--p->count];
    }
}

typedef struct {
    CVPixelBufferPoolRef pool;
    CVPixelBufferRef buffer;
} Loan;

static void returned(void *refcon) {
    Loan *loan = refcon;
    CVPixelBufferPoolRef p = loan->pool;
    pthread_mutex_lock(&p->lock);
    for (CFIndex i = 0; i < p->count; i++)
        if (p->slots[i].buffer == loan->buffer) {
            p->slots[i].out = false;
            p->slots[i].freedAt = CFAbsoluteTimeGetCurrent();
        }
    pthread_mutex_unlock(&p->lock);
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(), kCVPixelBufferPoolFreeBufferNotification,
                                         p, NULL, true);
    CFRelease(loan->buffer);
    CFRelease(p);
    free(loan);
}

/* The app's buffer over one of the pool's: the same IOSurface, or the
   same memory. */
static CVReturn wrap(CFAllocatorRef allocator, CVPixelBufferRef own, CFDictionaryRef attributes,
                     CVPixelBufferRef *out) {
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(own);
    if (surface != NULL)
        return CVPixelBufferCreateWithIOSurface(allocator, surface, attributes, out);
    size_t w = CVPixelBufferGetWidth(own), h = CVPixelBufferGetHeight(own);
    OSType f = CVPixelBufferGetPixelFormatType(own);
    if (!CVPixelBufferIsPlanar(own))
        return CVPixelBufferCreateWithBytes(allocator, w, h, f, CVPixelBufferGetBaseAddress(own),
                                            CVPixelBufferGetBytesPerRow(own), NULL, NULL, attributes, out);
    size_t n = CVPixelBufferGetPlaneCount(own);
    void *bases[4];
    size_t widths[4], heights[4], rows[4];
    for (size_t i = 0; i < n && i < 4; i++) {
        bases[i] = CVPixelBufferGetBaseAddressOfPlane(own, i);
        widths[i] = CVPixelBufferGetWidthOfPlane(own, i);
        heights[i] = CVPixelBufferGetHeightOfPlane(own, i);
        rows[i] = CVPixelBufferGetBytesPerRowOfPlane(own, i);
    }
    return CVPixelBufferCreateWithPlanarBytes(allocator, w, h, f, NULL, CVPixelBufferGetDataSize(own), n, bases, widths,
                                              heights, rows, NULL, NULL, attributes, out);
}

CVReturn CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(CFAllocatorRef allocator, CVPixelBufferPoolRef p,
                                                             CFDictionaryRef aux, CVPixelBufferRef *out) {
    if (p == NULL || out == NULL)
        return kCVReturnInvalidArgument;
    *out = NULL;
    long long threshold = 0;
    number(aux, kCVPixelBufferPoolAllocationThresholdKey, kCFNumberLongLongType, &threshold);
    pthread_mutex_lock(&p->lock);
    ageOut(p, false);
    Slot *slot = NULL;
    CFIndex out_ = 0;
    for (CFIndex i = 0; i < p->count; i++) {
        if (p->slots[i].out)
            out_++;
        else if (slot == NULL)
            slot = &p->slots[i];
    }
    if (slot == NULL) {
        if (threshold > 0 && out_ >= threshold) {
            pthread_mutex_unlock(&p->lock);
            return kCVReturnWouldExceedAllocationThreshold;
        }
        CVPixelBufferRef b = NULL;
        CVReturn err = CVPixelBufferCreate(allocator, p->width, p->height, p->format, p->bufferAttributes, &b);
        if (err != kCVReturnSuccess) {
            pthread_mutex_unlock(&p->lock);
            return err == kCVReturnAllocationFailed ? kCVReturnPoolAllocationFailed : err;
        }
        if (p->count == p->capacity) {
            p->capacity = p->capacity > 0 ? p->capacity * 2 : 8;
            p->slots = realloc(p->slots, (size_t)p->capacity * sizeof(Slot));
        }
        p->slots[p->count] = (Slot){ b, false, 0 };
        slot = &p->slots[p->count++];
    }
    CVPixelBufferRef handed = NULL;
    CVReturn err = wrap(allocator, slot->buffer, p->bufferAttributes, &handed);
    if (err != kCVReturnSuccess) {
        pthread_mutex_unlock(&p->lock);
        return err;
    }
    slot->out = true;
    Loan *loan = malloc(sizeof(Loan));
    loan->pool = (CVPixelBufferPoolRef)CFRetain(p);
    loan->buffer = (CVPixelBufferRef)CFRetain(slot->buffer);
    _CVPixelBufferSetFinalizeHook(handed, returned, loan);
    pthread_mutex_unlock(&p->lock);
    *out = handed;
    return kCVReturnSuccess;
}

CVReturn CVPixelBufferPoolCreatePixelBuffer(CFAllocatorRef allocator, CVPixelBufferPoolRef p, CVPixelBufferRef *out) {
    return CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(allocator, p, NULL, out);
}

void CVPixelBufferPoolFlush(CVPixelBufferPoolRef p, CVPixelBufferPoolFlushFlags options) {
    if (p == NULL)
        return;
    pthread_mutex_lock(&p->lock);
    CFIndex keep = p->minimumCount;
    if (options & kCVPixelBufferPoolFlushExcessBuffers)
        p->minimumCount = 0;
    ageOut(p, (options & kCVPixelBufferPoolFlushExcessBuffers) != 0);
    p->minimumCount = keep;
    pthread_mutex_unlock(&p->lock);
}
