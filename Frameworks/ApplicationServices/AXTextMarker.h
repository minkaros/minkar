/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Text markers: an app's own bytes naming a place in its text, handed to
 * assistive apps and back unchanged; a range is two of them.
 */

#ifndef _APPLICATIONSERVICES_AXTEXTMARKER_H_
#define _APPLICATIONSERVICES_AXTEXTMARKER_H_

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

CF_IMPLICIT_BRIDGING_ENABLED

typedef const struct CF_BRIDGED_TYPE(id) __AXTextMarker *AXTextMarkerRef;
typedef const struct CF_BRIDGED_TYPE(id) __AXTextMarkerRange *AXTextMarkerRangeRef;

CFTypeID AXTextMarkerGetTypeID(void);
AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef _Nullable allocator, const UInt8 *bytes, CFIndex length);
CFIndex AXTextMarkerGetLength(AXTextMarkerRef marker);
const UInt8 *AXTextMarkerGetBytePtr(AXTextMarkerRef marker);

CFTypeID AXTextMarkerRangeGetTypeID(void);
AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef _Nullable allocator, AXTextMarkerRef startMarker,
                                             AXTextMarkerRef endMarker);
AXTextMarkerRangeRef AXTextMarkerRangeCreateWithBytes(CFAllocatorRef _Nullable allocator, const UInt8 *startMarkerBytes,
                                                      CFIndex startMarkerLength, const UInt8 *endMarkerBytes,
                                                      CFIndex endMarkerLength);
AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef textMarkerRange);
AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef textMarkerRange);

CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
