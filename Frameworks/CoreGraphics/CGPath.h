/* Copyright(c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CoreGraphicsExport.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreGraphics/CGAffineTransform.h>

/* CF's naming rules for what the functions return (Create, Copy). */
CF_IMPLICIT_BRIDGING_ENABLED

typedef enum {
    kCGPathElementMoveToPoint,
    kCGPathElementAddLineToPoint,
    kCGPathElementAddQuadCurveToPoint,
    kCGPathElementAddCurveToPoint,
    kCGPathElementCloseSubpath,
} CGPathElementType;

typedef struct {
    CGPathElementType type;
    CGPoint *points;
} CGPathElement;

typedef void (*CGPathApplierFunction)(void *info, const CGPathElement *element);

typedef CF_ENUM(int32_t, CGLineCap) {
    kCGLineCapButt,
    kCGLineCapRound,
    kCGLineCapSquare,
};

typedef CF_ENUM(int32_t, CGLineJoin) {
    kCGLineJoinMiter,
    kCGLineJoinRound,
    kCGLineJoinBevel,
};

typedef struct O2MutablePath *CGPathRef;
typedef struct O2MutablePath *CGMutablePathRef;

COREGRAPHICS_EXPORT void CGPathRelease(CGPathRef self);
COREGRAPHICS_EXPORT CGPathRef CGPathRetain(CGPathRef self);

COREGRAPHICS_EXPORT bool CGPathEqualToPath(CGPathRef self, CGPathRef other);
COREGRAPHICS_EXPORT CGRect CGPathGetBoundingBox(CGPathRef self)
    CF_SWIFT_NAME(getter:CGPath.boundingBox(self:));
COREGRAPHICS_EXPORT CGPoint CGPathGetCurrentPoint(CGPathRef self)
    CF_SWIFT_NAME(getter:CGPath.currentPoint(self:));
COREGRAPHICS_EXPORT bool CGPathIsEmpty(CGPathRef self);
COREGRAPHICS_EXPORT bool CGPathIsRect(CGPathRef self, CGRect *rect);
COREGRAPHICS_EXPORT void CGPathApply(CGPathRef self, void *info, CGPathApplierFunction function);
COREGRAPHICS_EXPORT CGMutablePathRef CGPathCreateMutableCopy(CGPathRef self);
COREGRAPHICS_EXPORT CGPathRef CGPathCreateCopy(CGPathRef self);
COREGRAPHICS_EXPORT bool CGPathContainsPoint(CGPathRef self, const CGAffineTransform *xform, CGPoint point, bool evenOdd);

COREGRAPHICS_EXPORT CGMutablePathRef CGPathCreateMutable(void);

COREGRAPHICS_EXPORT void CGPathMoveToPoint(CGMutablePathRef self, const CGAffineTransform *xform, CGFloat x, CGFloat y);
COREGRAPHICS_EXPORT void CGPathAddLineToPoint(CGMutablePathRef self, const CGAffineTransform *xform, CGFloat x, CGFloat y);
COREGRAPHICS_EXPORT void CGPathAddCurveToPoint(CGMutablePathRef self, const CGAffineTransform *xform, CGFloat cp1x, CGFloat cp1y, CGFloat cp2x, CGFloat cp2y, CGFloat x, CGFloat y);
COREGRAPHICS_EXPORT void CGPathAddQuadCurveToPoint(CGMutablePathRef self, const CGAffineTransform *xform, CGFloat cpx, CGFloat cpy, CGFloat x, CGFloat y);
COREGRAPHICS_EXPORT void CGPathCloseSubpath(CGMutablePathRef self);

COREGRAPHICS_EXPORT void CGPathAddLines(CGMutablePathRef self, const CGAffineTransform *xform, const CGPoint *points, size_t count);
COREGRAPHICS_EXPORT void CGPathAddRect(CGMutablePathRef self, const CGAffineTransform *xform, CGRect rect);
COREGRAPHICS_EXPORT void CGPathAddRects(CGMutablePathRef self, const CGAffineTransform *xform, const CGRect *rects, size_t count);

COREGRAPHICS_EXPORT void CGPathAddArc(CGMutablePathRef self, const CGAffineTransform *xform, CGFloat x, CGFloat y, CGFloat radius, CGFloat startRadian, CGFloat endRadian, bool clockwise);
COREGRAPHICS_EXPORT void CGPathAddArcToPoint(CGMutablePathRef self, const CGAffineTransform *xform, CGFloat tx1, CGFloat ty1, CGFloat tx2, CGFloat ty2, CGFloat radius);

COREGRAPHICS_EXPORT void CGPathAddEllipseInRect(CGMutablePathRef self, const CGAffineTransform *xform, CGRect rect);
COREGRAPHICS_EXPORT void CGPathAddRoundedRect(CGMutablePathRef self, const CGAffineTransform *xform, CGRect rect, CGFloat cornerWidth, CGFloat cornerHeight);
COREGRAPHICS_EXPORT void CGPathAddRelativeArc(CGMutablePathRef self, const CGAffineTransform *xform, CGFloat x, CGFloat y, CGFloat radius, CGFloat startAngle, CGFloat delta);
COREGRAPHICS_EXPORT CGPathRef CGPathCreateWithRect(CGRect rect, const CGAffineTransform *xform)
    CF_SWIFT_NAME(CGPath.init(rect:transform:));
COREGRAPHICS_EXPORT CGPathRef CGPathCreateWithEllipseInRect(CGRect rect, const CGAffineTransform *xform)
    CF_SWIFT_NAME(CGPath.init(ellipseIn:transform:));
COREGRAPHICS_EXPORT CGPathRef CGPathCreateWithRoundedRect(CGRect rect, CGFloat cornerWidth, CGFloat cornerHeight, const CGAffineTransform *xform)
    CF_SWIFT_NAME(CGPath.init(roundedRect:cornerWidth:cornerHeight:transform:));
COREGRAPHICS_EXPORT CGPathRef CGPathCreateCopyByDashingPath(CGPathRef path, const CGAffineTransform *xform, CGFloat phase, const CGFloat *lengths, size_t count);
COREGRAPHICS_EXPORT CGPathRef CGPathCreateCopyByStrokingPath(CGPathRef path, const CGAffineTransform *xform, CGFloat lineWidth, CGLineCap lineCap, CGLineJoin lineJoin, CGFloat miterLimit);

COREGRAPHICS_EXPORT void CGPathAddPath(CGMutablePathRef self, const CGAffineTransform *xform, CGPathRef other);


CF_IMPLICIT_BRIDGING_DISABLED