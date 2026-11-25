/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * HIShape: an area made of rectangles (a region), immutable or mutable, as
 * HIToolbox documents it. Minkar's keeps whole-pixel rectangles (pixman's
 * regions): coordinates are rounded out to the pixels they touch.
 */

#ifndef __HISHAPE__
#define __HISHAPE__

#include <MacTypes.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef CGRect HIRect;
typedef CGPoint HIPoint;
typedef CGSize HISize;

typedef const struct __HIShape *HIShapeRef;
typedef struct __HIShape *HIMutableShapeRef;

CFTypeID HIShapeGetTypeID(void);

HIShapeRef HIShapeCreateEmpty(void);
HIShapeRef HIShapeCreateWithRect(const HIRect *inRect);
HIShapeRef HIShapeCreateCopy(HIShapeRef inShape);
HIShapeRef HIShapeCreateIntersection(HIShapeRef inShape1, HIShapeRef inShape2);
HIShapeRef HIShapeCreateDifference(HIShapeRef inShape1, HIShapeRef inShape2);
HIShapeRef HIShapeCreateUnion(HIShapeRef inShape1, HIShapeRef inShape2);
HIShapeRef HIShapeCreateXor(HIShapeRef inShape1, HIShapeRef inShape2);

HIMutableShapeRef HIShapeCreateMutable(void);
HIMutableShapeRef HIShapeCreateMutableCopy(HIShapeRef inOrig);
HIMutableShapeRef HIShapeCreateMutableWithRect(const HIRect *inRect);

Boolean HIShapeIsEmpty(HIShapeRef inShape);
Boolean HIShapeIsRectangular(HIShapeRef inShape);
Boolean HIShapeContainsPoint(HIShapeRef inShape, const HIPoint *inPoint);
Boolean HIShapeIntersectsRect(HIShapeRef inShape, const HIRect *inRect);
HIRect *HIShapeGetBounds(HIShapeRef inShape, HIRect *outRect);
OSStatus HIShapeReplacePathInCGContext(HIShapeRef inShape, CGContextRef inContext);

/* The result goes to outResult (which may be one of the inputs). */
OSStatus HIShapeUnion(HIShapeRef inShape1, HIShapeRef inShape2, HIMutableShapeRef outResult);
OSStatus HIShapeIntersect(HIShapeRef inShape1, HIShapeRef inShape2, HIMutableShapeRef outResult);
OSStatus HIShapeDifference(HIShapeRef inShape1, HIShapeRef inShape2, HIMutableShapeRef outResult);
OSStatus HIShapeXor(HIShapeRef inShape1, HIShapeRef inShape2, HIMutableShapeRef outResult);
OSStatus HIShapeUnionWithRect(HIMutableShapeRef inShape, const HIRect *inRect);
OSStatus HIShapeOffset(HIMutableShapeRef inShape, CGFloat inDX, CGFloat inDY);
OSStatus HIShapeSetEmpty(HIMutableShapeRef inShape);
OSStatus HIShapeSetWithShape(HIMutableShapeRef inDestShape, HIShapeRef inSrcShape);

/* Its rectangles, in turn: the callback is called with
   kHIShapeEnumerateInit first, kHIShapeEnumerateRect for each rectangle,
   and kHIShapeEnumerateTerminate last; a non-zero result stops it. */
enum {
    kHIShapeEnumerateInit = 1,
    kHIShapeEnumerateRect = 2,
    kHIShapeEnumerateTerminate = 3
};
enum {
    kHIShapeParseFromTop = 0,
    kHIShapeParseFromBottom = (1 << 0),
    kHIShapeParseFromLeft = 0,
    kHIShapeParseFromRight = (1 << 1),
    kHIShapeParseFromTopLeft = kHIShapeParseFromTop | kHIShapeParseFromLeft,
    kHIShapeParseFromBottomRight = kHIShapeParseFromBottom | kHIShapeParseFromRight
};
typedef OSStatus (*HIShapeEnumerateProcPtr)(int inMessage, HIShapeRef inShape, const CGRect *inRect, void *inRefcon);
OSStatus HIShapeEnumerate(HIShapeRef inShape, OptionBits inOptions, HIShapeEnumerateProcPtr inProc, void *inRefcon);

#ifdef __cplusplus
}
#endif

#endif
