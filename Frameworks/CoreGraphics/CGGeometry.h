/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreFoundation/CFBase.h>
#import <CoreFoundation/CFDictionary.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* As on the reference platform: double on 64-bit, float on 32-bit. */
#if defined(__LP64__) && __LP64__
typedef double CGFloat;
#define CGFLOAT_IS_DOUBLE 1
#define CGFLOAT_MIN DBL_MIN
#define CGFLOAT_MAX DBL_MAX
#define CGFLOAT_SCAN "%lg"
#else
typedef float CGFloat;
#define CGFLOAT_IS_DOUBLE 0
#define CGFLOAT_MIN FLT_MIN
#define CGFLOAT_MAX FLT_MAX
#define CGFLOAT_SCAN "%g"
#endif
#define CGFLOAT_DEFINED 1

typedef struct CGPoint {
    CGFloat x;
    CGFloat y;
} CGPoint;

typedef struct CGSize {
    CGFloat width;
    CGFloat height;
} CGSize;

typedef struct CGRect {
    CGPoint origin;
    CGSize size;
} CGRect;

COREGRAPHICS_EXPORT const CGRect CGRectZero;
COREGRAPHICS_EXPORT const CGRect CGRectNull;
COREGRAPHICS_EXPORT const CGPoint CGPointZero;
COREGRAPHICS_EXPORT const CGSize CGSizeZero;

static inline CGRect CGRectMake(CGFloat x, CGFloat y, CGFloat width, CGFloat height) {
    CGRect result = {{x, y}, {width, height}};
    return result;
}

static inline CGPoint CGPointMake(CGFloat x, CGFloat y) {
    CGPoint result = {x, y};
    return result;
}

static inline CGSize CGSizeMake(CGFloat x, CGFloat y) {
    CGSize result = {x, y};
    return result;
}

CF_ASSUME_NONNULL_BEGIN

/* Exported functions, as the reference platform has them; a rectangle with
 * a negative width or height is taken as its standardized form. */
COREGRAPHICS_EXPORT bool CGPointEqualToPoint(CGPoint a, CGPoint b);
COREGRAPHICS_EXPORT bool CGSizeEqualToSize(CGSize a, CGSize b);
COREGRAPHICS_EXPORT CGRect CGRectStandardize(CGRect rect);

CF_SWIFT_NAME(getter:CGRect.minX(self:))
COREGRAPHICS_EXPORT CGFloat CGRectGetMinX(CGRect rect);
CF_SWIFT_NAME(getter:CGRect.midX(self:))
COREGRAPHICS_EXPORT CGFloat CGRectGetMidX(CGRect rect);
CF_SWIFT_NAME(getter:CGRect.maxX(self:))
COREGRAPHICS_EXPORT CGFloat CGRectGetMaxX(CGRect rect);
CF_SWIFT_NAME(getter:CGRect.minY(self:))
COREGRAPHICS_EXPORT CGFloat CGRectGetMinY(CGRect rect);
CF_SWIFT_NAME(getter:CGRect.midY(self:))
COREGRAPHICS_EXPORT CGFloat CGRectGetMidY(CGRect rect);
CF_SWIFT_NAME(getter:CGRect.maxY(self:))
COREGRAPHICS_EXPORT CGFloat CGRectGetMaxY(CGRect rect);
CF_SWIFT_NAME(getter:CGRect.width(self:))
COREGRAPHICS_EXPORT CGFloat CGRectGetWidth(CGRect rect);
CF_SWIFT_NAME(getter:CGRect.height(self:))
COREGRAPHICS_EXPORT CGFloat CGRectGetHeight(CGRect rect);

COREGRAPHICS_EXPORT bool CGRectEqualToRect(CGRect a, CGRect b);
COREGRAPHICS_EXPORT bool CGRectIsEmpty(CGRect rect);
COREGRAPHICS_EXPORT bool CGRectIsNull(CGRect rect);
COREGRAPHICS_EXPORT bool CGRectIsInfinite(CGRect rect);
COREGRAPHICS_EXPORT CGRect CGRectInset(CGRect rect, CGFloat dx, CGFloat dy);
COREGRAPHICS_EXPORT CGRect CGRectOffset(CGRect rect, CGFloat dx, CGFloat dy);
COREGRAPHICS_EXPORT bool CGRectContainsPoint(CGRect rect, CGPoint point);
COREGRAPHICS_EXPORT bool CGRectContainsRect(CGRect a, CGRect b);
COREGRAPHICS_EXPORT bool CGRectIntersectsRect(CGRect a, CGRect b);

/* The infinite rectangle. */
COREGRAPHICS_EXPORT const CGRect CGRectInfinite;

CF_ASSUME_NONNULL_END

/* A vector (API 19). */
typedef struct CGVector {
    CGFloat dx;
    CGFloat dy;
} CGVector;

static inline CGVector CGVectorMake(CGFloat dx, CGFloat dy) {
    CGVector result = {dx, dy};
    return result;
}

/* The edges of a rectangle, for CGRectDivide. */
typedef CF_ENUM(uint32_t, CGRectEdge) {
    CGRectMinXEdge,
    CGRectMinYEdge,
    CGRectMaxXEdge,
    CGRectMaxYEdge,
};

CF_ASSUME_NONNULL_BEGIN

/* Splits rect at amount from edge: slice is that part, remainder the rest. */
COREGRAPHICS_EXPORT void CGRectDivide(CGRect rect, CGRect *slice, CGRect *remainder, CGFloat amount, CGRectEdge edge);

COREGRAPHICS_EXPORT CGRect CGRectUnion(CGRect a, CGRect b);
COREGRAPHICS_EXPORT CGRect CGRectIntersection(CGRect a, CGRect b);
COREGRAPHICS_EXPORT CGRect CGRectIntegral(CGRect rect);

/* Dictionary representations (keys "X", "Y", "Width", "Height"). */
COREGRAPHICS_EXPORT CFDictionaryRef CGPointCreateDictionaryRepresentation(CGPoint point);
COREGRAPHICS_EXPORT bool CGPointMakeWithDictionaryRepresentation(CFDictionaryRef _Nullable dict, CGPoint * _Nullable point);
COREGRAPHICS_EXPORT CFDictionaryRef CGSizeCreateDictionaryRepresentation(CGSize size);
COREGRAPHICS_EXPORT bool CGSizeMakeWithDictionaryRepresentation(CFDictionaryRef _Nullable dict, CGSize * _Nullable size);
COREGRAPHICS_EXPORT CFDictionaryRef CGRectCreateDictionaryRepresentation(CGRect rect);
COREGRAPHICS_EXPORT bool CGRectMakeWithDictionaryRepresentation(CFDictionaryRef _Nullable dict, CGRect * _Nullable rect);

CF_ASSUME_NONNULL_END


