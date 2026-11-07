/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGPath.h>
#import <Onyx2D/O2Path.h>
#import <Onyx2D/O2MutablePath.h>
#import "CGConversions.h"
#include <minkar/stub.h>

void CGPathRelease(CGPathRef self) {
   O2PathRelease(self);
}

CGPathRef CGPathRetain(CGPathRef self) {
   return (CGPathRef)O2PathRetain(self);
}

bool CGPathEqualToPath(CGPathRef self,CGPathRef other) {
   return O2PathEqualToPath(self,other);
}

CGRect CGPathGetBoundingBox(CGPathRef self) {
   return O2PathGetBoundingBox(self);
}

CGPoint CGPathGetCurrentPoint(CGPathRef self) {
   return O2PathGetCurrentPoint(self);
}

bool CGPathIsEmpty(CGPathRef self) {
   return O2PathIsEmpty(self);
}

bool CGPathIsRect(CGPathRef self,CGRect *rect) {
   return O2PathIsRect(self,rect);
}

void CGPathApply(CGPathRef self,void *info,CGPathApplierFunction function) {
   return O2PathApply(self,info,O2PathApplierFunctionFromCG(function));
}

CGMutablePathRef CGPathCreateMutableCopy(CGPathRef self) {
   return O2PathCreateMutableCopy(self);
}

CGPathRef CGPathCreateCopy(CGPathRef self) {
   return (CGPathRef)O2PathCreateCopy(self);
}

bool CGPathContainsPoint(CGPathRef self,const CGAffineTransform *xform,CGPoint point,bool evenOdd) {
   return O2PathContainsPoint(self,O2AffineTransformPtrFromCG(xform),point,evenOdd);
}

CGMutablePathRef CGPathCreateMutable(void) {
   return O2PathCreateMutable();
}

void CGPathMoveToPoint(CGMutablePathRef self,const CGAffineTransform *xform,CGFloat x,CGFloat y) {
   O2PathMoveToPoint(self,O2AffineTransformPtrFromCG(xform),x,y);
}

void CGPathAddLineToPoint(CGMutablePathRef self,const CGAffineTransform *xform,CGFloat x,CGFloat y) {
   O2PathAddLineToPoint(self,O2AffineTransformPtrFromCG(xform),x,y);
}

void CGPathAddCurveToPoint(CGMutablePathRef self,const CGAffineTransform *xform,CGFloat cp1x,CGFloat cp1y,CGFloat cp2x,CGFloat cp2y,CGFloat x,CGFloat y) {
   O2PathAddCurveToPoint(self,O2AffineTransformPtrFromCG(xform),cp1x,cp1y,cp2x,cp2y,x,y);
}

void CGPathAddQuadCurveToPoint(CGMutablePathRef self,const CGAffineTransform *xform,CGFloat cpx,CGFloat cpy,CGFloat x,CGFloat y) {
   O2PathAddQuadCurveToPoint(self,O2AffineTransformPtrFromCG(xform),cpx,cpy,x,y);
}

void CGPathCloseSubpath(CGMutablePathRef self) {
   O2PathCloseSubpath(self);
}

void CGPathAddLines(CGMutablePathRef self,const CGAffineTransform *xform,const CGPoint *points,size_t count) {
   O2PathAddLines(self,O2AffineTransformPtrFromCG(xform),points,count);
}

void CGPathAddRect(CGMutablePathRef self,const CGAffineTransform *xform,CGRect rect) {
   O2PathAddRect(self,O2AffineTransformPtrFromCG(xform),rect);
}

void CGPathAddRects(CGMutablePathRef self,const CGAffineTransform *xform,const CGRect *rects,size_t count) {
   O2PathAddRects(self,O2AffineTransformPtrFromCG(xform),rects,count);
}

void CGPathAddArc(CGMutablePathRef self,const CGAffineTransform *xform,CGFloat x,CGFloat y,CGFloat radius,CGFloat startRadian,CGFloat endRadian,bool clockwise) {
   O2PathAddArc(self,O2AffineTransformPtrFromCG(xform),x,y,radius,startRadian,endRadian,clockwise);
}

void CGPathAddArcToPoint(CGMutablePathRef self,const CGAffineTransform *xform,CGFloat tx1,CGFloat ty1,CGFloat tx2,CGFloat ty2,CGFloat radius) {
   O2PathAddArcToPoint(self,O2AffineTransformPtrFromCG(xform),tx1,ty1,tx2,ty2,radius);
}

void CGPathAddEllipseInRect(CGMutablePathRef self,const CGAffineTransform *xform,CGRect rect) {
   O2PathAddEllipseInRect(self,O2AffineTransformPtrFromCG(xform),rect);
}

void CGPathAddPath(CGMutablePathRef self,const CGAffineTransform *xform,CGPathRef other) {
   O2PathAddPath(self,O2AffineTransformPtrFromCG(xform),other);
}

/* Rounded corners as elliptical quarter arcs (cubic curves, the usual
 * 0.5523 control distance). */
void CGPathAddRoundedRect(CGMutablePathRef self, const CGAffineTransform *xform, CGRect rect, CGFloat cornerWidth, CGFloat cornerHeight) {
    const CGFloat k = 0.55228474983;
    CGFloat minX = CGRectGetMinX(rect), maxX = CGRectGetMaxX(rect);
    CGFloat minY = CGRectGetMinY(rect), maxY = CGRectGetMaxY(rect);
    CGFloat rx = cornerWidth, ry = cornerHeight;

    if (rx <= 0 || ry <= 0) {
        CGPathAddRect(self, xform, rect);
        return;
    }
    if (rx > rect.size.width / 2)
        rx = rect.size.width / 2;
    if (ry > rect.size.height / 2)
        ry = rect.size.height / 2;
    CGPathMoveToPoint(self, xform, minX + rx, minY);
    CGPathAddLineToPoint(self, xform, maxX - rx, minY);
    CGPathAddCurveToPoint(self, xform, maxX - rx + rx * k, minY, maxX, minY + ry - ry * k, maxX, minY + ry);
    CGPathAddLineToPoint(self, xform, maxX, maxY - ry);
    CGPathAddCurveToPoint(self, xform, maxX, maxY - ry + ry * k, maxX - rx + rx * k, maxY, maxX - rx, maxY);
    CGPathAddLineToPoint(self, xform, minX + rx, maxY);
    CGPathAddCurveToPoint(self, xform, minX + rx - rx * k, maxY, minX, maxY - ry + ry * k, minX, maxY - ry);
    CGPathAddLineToPoint(self, xform, minX, minY + ry);
    CGPathAddCurveToPoint(self, xform, minX, minY + ry - ry * k, minX + rx - rx * k, minY, minX + rx, minY);
    CGPathCloseSubpath(self);
}

void CGPathAddRelativeArc(CGMutablePathRef self, const CGAffineTransform *xform, CGFloat x, CGFloat y, CGFloat radius, CGFloat startAngle, CGFloat delta) {
    CGPathAddArc(self, xform, x, y, radius, startAngle, startAngle + delta, delta < 0);
}

CGPathRef CGPathCreateWithRect(CGRect rect, const CGAffineTransform *xform) {
    CGMutablePathRef path = CGPathCreateMutable();

    CGPathAddRect(path, xform, rect);
    return path;
}

CGPathRef CGPathCreateWithEllipseInRect(CGRect rect, const CGAffineTransform *xform) {
    CGMutablePathRef path = CGPathCreateMutable();

    CGPathAddEllipseInRect(path, xform, rect);
    return path;
}

CGPathRef CGPathCreateWithRoundedRect(CGRect rect, CGFloat cornerWidth, CGFloat cornerHeight, const CGAffineTransform *xform) {
    CGMutablePathRef path = CGPathCreateMutable();

    CGPathAddRoundedRect(path, xform, rect, cornerWidth, cornerHeight);
    return path;
}

/* Not yet the dashed or stroked outline: a copy of the path (and a note in
 * the log the first time). */
CGPathRef CGPathCreateCopyByDashingPath(CGPathRef path, const CGAffineTransform *xform, CGFloat phase, const CGFloat *lengths, size_t count) {
    (void)xform; (void)phase; (void)lengths; (void)count;
    MINKAR_STUB();
    return path ? CGPathCreateCopy(path) : NULL;
}

CGPathRef CGPathCreateCopyByStrokingPath(CGPathRef path, const CGAffineTransform *xform, CGFloat lineWidth, CGLineCap lineCap, CGLineJoin lineJoin, CGFloat miterLimit) {
    (void)xform; (void)lineWidth; (void)lineCap; (void)lineJoin; (void)miterLimit;
    MINKAR_STUB();
    return path ? CGPathCreateCopy(path) : NULL;
}
