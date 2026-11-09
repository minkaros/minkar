/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#ifndef MIN
#define MIN(a,b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b); (_a < _b) ? _a : _b; })
#else
#warning MIN is already defined, MIN(a, b) may not behave as expected.
#endif

#ifndef MAX
#define MAX(a,b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b); (_a > _b) ? _a : _b; })
#else
#warning MAX is already defined, MAX(a, b) may not not behave as expected.
#endif

#import <CoreGraphics/CGGeometry.h>
#include <math.h>

const CGRect CGRectZero={{0,0},{0,0}};
const CGRect CGRectNull={{INFINITY,INFINITY},{0,0}};
const CGPoint CGPointZero={0,0};
const CGSize CGSizeZero={0,0};

/* -DBL_MAX/2 to DBL_MAX/2 on both axes, as the reference platform has it. */
const CGRect CGRectInfinite={{-8.98846567431158e+307,-8.98846567431158e+307},{1.797693134862316e+308,1.797693134862316e+308}};

bool CGPointEqualToPoint(CGPoint a, CGPoint b) {
	return a.x==b.x && a.y==b.y;
}

bool CGSizeEqualToSize(CGSize a, CGSize b) {
	return a.width==b.width && a.height==b.height;
}

bool CGRectIsNull(CGRect rect) {
	return isinf(rect.origin.x) && rect.origin.x>0 && isinf(rect.origin.y) && rect.origin.y>0;
}

CGRect CGRectStandardize(CGRect rect) {
	if(CGRectIsNull(rect))
		return rect;
	if(rect.size.width<0){
		rect.origin.x+=rect.size.width;
		rect.size.width=-rect.size.width;
	}
	if(rect.size.height<0){
		rect.origin.y+=rect.size.height;
		rect.size.height=-rect.size.height;
	}
	return rect;
}

CGFloat CGRectGetMinX(CGRect rect) {
	return CGRectStandardize(rect).origin.x;
}

CGFloat CGRectGetMaxX(CGRect rect) {
	rect=CGRectStandardize(rect);
	return rect.origin.x+rect.size.width;
}

CGFloat CGRectGetMidX(CGRect rect) {
	rect=CGRectStandardize(rect);
	return rect.origin.x+rect.size.width/2;
}

CGFloat CGRectGetMinY(CGRect rect) {
	return CGRectStandardize(rect).origin.y;
}

CGFloat CGRectGetMaxY(CGRect rect) {
	rect=CGRectStandardize(rect);
	return rect.origin.y+rect.size.height;
}

CGFloat CGRectGetMidY(CGRect rect) {
	rect=CGRectStandardize(rect);
	return rect.origin.y+rect.size.height/2;
}

CGFloat CGRectGetWidth(CGRect rect) {
	return fabs(rect.size.width);
}

CGFloat CGRectGetHeight(CGRect rect) {
	return fabs(rect.size.height);
}

bool CGRectEqualToRect(CGRect a, CGRect b) {
	if(CGRectIsNull(a) || CGRectIsNull(b))
		return CGRectIsNull(a) && CGRectIsNull(b);
	a=CGRectStandardize(a);
	b=CGRectStandardize(b);
	return CGPointEqualToPoint(a.origin,b.origin) && CGSizeEqualToSize(a.size,b.size);
}

/* Null, or no width or no height. */
bool CGRectIsEmpty(CGRect rect) {
	return CGRectIsNull(rect) || rect.size.width==0 || rect.size.height==0;
}

bool CGRectIsInfinite(CGRect rect) {
	return CGRectEqualToRect(rect,CGRectInfinite) || isinf(rect.size.width) || isinf(rect.size.height);
}

/* Smaller by dx on the left and right and dy at the top and bottom
   (larger when negative); null when nothing is left. */
CGRect CGRectInset(CGRect rect, CGFloat dx, CGFloat dy) {
	if(CGRectIsNull(rect))
		return rect;
	rect=CGRectStandardize(rect);
	rect.origin.x+=dx;
	rect.origin.y+=dy;
	rect.size.width-=dx*2;
	rect.size.height-=dy*2;
	if(rect.size.width<0 || rect.size.height<0)
		return CGRectNull;
	return rect;
}

CGRect CGRectOffset(CGRect rect, CGFloat dx, CGFloat dy) {
	if(CGRectIsNull(rect))
		return rect;
	rect=CGRectStandardize(rect);
	rect.origin.x+=dx;
	rect.origin.y+=dy;
	return rect;
}

/* The maximum edges are outside. */
bool CGRectContainsPoint(CGRect rect, CGPoint point) {
	if(CGRectIsNull(rect))
		return false;
	return point.x>=CGRectGetMinX(rect) && point.x<CGRectGetMaxX(rect) &&
	       point.y>=CGRectGetMinY(rect) && point.y<CGRectGetMaxY(rect);
}

bool CGRectContainsRect(CGRect a, CGRect b) {
	if(CGRectIsNull(a) || CGRectIsNull(b))
		return false;
	return CGRectGetMinX(b)>=CGRectGetMinX(a) && CGRectGetMaxX(b)<=CGRectGetMaxX(a) &&
	       CGRectGetMinY(b)>=CGRectGetMinY(a) && CGRectGetMaxY(b)<=CGRectGetMaxY(a);
}

/* Null when they do not meet; rectangles that touch meet in a rectangle
   with no width or height. */
CGRect CGRectIntersection(CGRect rect0, CGRect rect1) {
	CGRect result;

	if(CGRectIsNull(rect0) || CGRectIsNull(rect1))
		return CGRectNull;
	if(CGRectGetMaxX(rect0)<CGRectGetMinX(rect1) || CGRectGetMinX(rect0)>CGRectGetMaxX(rect1) ||
	   CGRectGetMaxY(rect0)<CGRectGetMinY(rect1) || CGRectGetMinY(rect0)>CGRectGetMaxY(rect1))
		return CGRectNull;

	result.origin.x=MAX(CGRectGetMinX(rect0), CGRectGetMinX(rect1));
	result.origin.y=MAX(CGRectGetMinY(rect0), CGRectGetMinY(rect1));
	result.size.width=MIN(CGRectGetMaxX(rect0), CGRectGetMaxX(rect1))-result.origin.x;
	result.size.height=MIN(CGRectGetMaxY(rect0), CGRectGetMaxY(rect1))-result.origin.y;

	return result;
}

bool CGRectIntersectsRect(CGRect a, CGRect b) {
	return !CGRectIsNull(CGRectIntersection(a,b));
}

CGRect CGRectIntegral(CGRect rect) {
	if(CGRectIsNull(rect))
		return rect;
	rect=CGRectStandardize(rect);
	if(rect.size.width!=0 || rect.size.height!=0){
		CGFloat maxX=ceil(rect.origin.x+rect.size.width);
		CGFloat maxY=ceil(rect.origin.y+rect.size.height);

		rect.origin.x=floor(rect.origin.x);
		rect.origin.y=floor(rect.origin.y);
		rect.size.width=maxX-rect.origin.x;
		rect.size.height=maxY-rect.origin.y;
	}
	return rect;
}

CGRect CGRectUnion(CGRect a, CGRect b) {
	if(CGRectIsNull(a))
		return CGRectStandardize(b);
	if(CGRectIsNull(b))
		return CGRectStandardize(a);

	CGFloat minX=MIN(CGRectGetMinX(a), CGRectGetMinX(b));
	CGFloat minY=MIN(CGRectGetMinY(a), CGRectGetMinY(b));
	CGFloat maxX=MAX(CGRectGetMaxX(a), CGRectGetMaxX(b));
	CGFloat maxY=MAX(CGRectGetMaxY(a), CGRectGetMaxY(b));

	return CGRectMake(minX, minY, maxX-minX, maxY-minY);
}

void CGRectDivide(CGRect rect, CGRect *slice, CGRect *remainder, CGFloat amount, CGRectEdge edge) {
    CGRect s, r;

    if (rect.size.width < 0) {
        rect.origin.x += rect.size.width;
        rect.size.width = -rect.size.width;
    }
    if (rect.size.height < 0) {
        rect.origin.y += rect.size.height;
        rect.size.height = -rect.size.height;
    }
    if (amount < 0)
        amount = 0;
    s = r = rect;
    switch (edge) {
    case CGRectMinXEdge:
    case CGRectMaxXEdge:
        if (amount > rect.size.width)
            amount = rect.size.width;
        s.size.width = amount;
        r.size.width = rect.size.width - amount;
        if (edge == CGRectMinXEdge)
            r.origin.x += amount;
        else
            s.origin.x += rect.size.width - amount;
        break;
    default:
        if (amount > rect.size.height)
            amount = rect.size.height;
        s.size.height = amount;
        r.size.height = rect.size.height - amount;
        if (edge == CGRectMinYEdge)
            r.origin.y += amount;
        else
            s.origin.y += rect.size.height - amount;
        break;
    }
    if (slice != NULL)
        *slice = s;
    if (remainder != NULL)
        *remainder = r;
}
