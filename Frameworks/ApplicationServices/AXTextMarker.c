/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Text markers and their ranges (AXTextMarker.h), as CF types that
 * compare by their bytes.
 */

#include "AXTextMarker.h"
#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>
#include <string.h>

struct __AXTextMarker {
    CFRuntimeBase base;
    CFIndex length;
    UInt8 bytes[];
};

struct __AXTextMarkerRange {
    CFRuntimeBase base;
    AXTextMarkerRef start, end;
};

static CFTypeID sMarkerTypeID, sRangeTypeID;
static pthread_once_t sOnce = PTHREAD_ONCE_INIT;

static Boolean markerEqual(CFTypeRef a, CFTypeRef b) {
    const struct __AXTextMarker *x = a, *y = b;
    return x->length == y->length && memcmp(x->bytes, y->bytes, (size_t)x->length) == 0;
}

static CFHashCode markerHash(CFTypeRef cf) {
    const struct __AXTextMarker *m = cf;
    CFHashCode h = (CFHashCode)m->length;
    for (CFIndex i = 0; i < m->length; i++)
        h = h * 31 + m->bytes[i];
    return h;
}

static void rangeFinalize(CFTypeRef cf) {
    struct __AXTextMarkerRange *r = (struct __AXTextMarkerRange *)cf;
    CFRelease(r->start);
    CFRelease(r->end);
}

static Boolean rangeEqual(CFTypeRef a, CFTypeRef b) {
    const struct __AXTextMarkerRange *x = a, *y = b;
    return CFEqual(x->start, y->start) && CFEqual(x->end, y->end);
}

static CFHashCode rangeHash(CFTypeRef cf) {
    const struct __AXTextMarkerRange *r = cf;
    return CFHash(r->start) * 31 + CFHash(r->end);
}

static void registerTypes(void) {
    static CFRuntimeClass marker = { 0, "AXTextMarker", NULL, NULL, NULL, markerEqual, markerHash, NULL, NULL };
    static CFRuntimeClass range = { 0, "AXTextMarkerRange", NULL, NULL, rangeFinalize, rangeEqual, rangeHash, NULL, NULL };
    sMarkerTypeID = _CFRuntimeRegisterClass(&marker);
    sRangeTypeID = _CFRuntimeRegisterClass(&range);
}

CFTypeID AXTextMarkerGetTypeID(void) {
    pthread_once(&sOnce, registerTypes);
    return sMarkerTypeID;
}

CFTypeID AXTextMarkerRangeGetTypeID(void) {
    pthread_once(&sOnce, registerTypes);
    return sRangeTypeID;
}

AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef allocator, const UInt8 *bytes, CFIndex length) {
    struct __AXTextMarker *m;
    if (length < 0 || (length > 0 && bytes == NULL))
        return NULL;
    m = (struct __AXTextMarker *)_CFRuntimeCreateInstance(allocator, AXTextMarkerGetTypeID(),
        sizeof(struct __AXTextMarker) - sizeof(CFRuntimeBase) + (CFIndex)length, NULL);
    if (m == NULL)
        return NULL;
    m->length = length;
    if (length > 0)
        memcpy(m->bytes, bytes, (size_t)length);
    return m;
}

CFIndex AXTextMarkerGetLength(AXTextMarkerRef marker) {
    return marker != NULL ? marker->length : 0;
}

const UInt8 *AXTextMarkerGetBytePtr(AXTextMarkerRef marker) {
    return marker != NULL ? marker->bytes : NULL;
}

AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef allocator, AXTextMarkerRef startMarker,
                                             AXTextMarkerRef endMarker) {
    struct __AXTextMarkerRange *r;
    if (startMarker == NULL || endMarker == NULL)
        return NULL;
    r = (struct __AXTextMarkerRange *)_CFRuntimeCreateInstance(allocator, AXTextMarkerRangeGetTypeID(),
        sizeof(struct __AXTextMarkerRange) - sizeof(CFRuntimeBase), NULL);
    if (r == NULL)
        return NULL;
    r->start = CFRetain(startMarker);
    r->end = CFRetain(endMarker);
    return r;
}

AXTextMarkerRangeRef AXTextMarkerRangeCreateWithBytes(CFAllocatorRef allocator, const UInt8 *startMarkerBytes,
                                                      CFIndex startMarkerLength, const UInt8 *endMarkerBytes,
                                                      CFIndex endMarkerLength) {
    AXTextMarkerRef start = AXTextMarkerCreate(allocator, startMarkerBytes, startMarkerLength);
    AXTextMarkerRef end = AXTextMarkerCreate(allocator, endMarkerBytes, endMarkerLength);
    AXTextMarkerRangeRef range = (start != NULL && end != NULL) ? AXTextMarkerRangeCreate(allocator, start, end) : NULL;
    if (start != NULL)
        CFRelease(start);
    if (end != NULL)
        CFRelease(end);
    return range;
}

AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef textMarkerRange) {
    return textMarkerRange != NULL ? CFRetain(textMarkerRange->start) : NULL;
}

AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef textMarkerRange) {
    return textMarkerRange != NULL ? CFRetain(textMarkerRange->end) : NULL;
}
