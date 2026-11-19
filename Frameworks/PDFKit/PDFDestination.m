/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <PDFKit/PDFDestination.h>
#import <PDFKit/PDFDocument.h>
#import <float.h>

const CGFloat kPDFDestinationUnspecifiedValue = FLT_MAX;

@implementation PDFDestination

- (instancetype)initWithPage:(PDFPage *)page atPoint:(PDFPoint)point {
    if ((self = [super init])) {
        _page = page;
        _point = point;
        _zoom = kPDFDestinationUnspecifiedValue;
    }
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    PDFDestination *copy = [[PDFDestination allocWithZone:zone] initWithPage:_page atPoint:_point];

    copy.zoom = _zoom;
    return copy;
}

/* Which comes first in the document: by page, then from the top down, then
   left to right; an unspecified coordinate as the page's edge. */
- (NSComparisonResult)compare:(PDFDestination *)destination {
    PDFDocument *document = _page.document;
    NSUInteger mine = document ? [document indexForPage:_page] : 0;
    NSUInteger theirs = document ? [document indexForPage:destination.page] : 0;

    if (destination.page.document != document)
        return NSOrderedSame;
    if (mine != theirs)
        return (mine < theirs) ? NSOrderedAscending : NSOrderedDescending;
    CGFloat myY = (_point.y == kPDFDestinationUnspecifiedValue) ? CGFLOAT_MAX : _point.y;
    CGFloat theirY = (destination.point.y == kPDFDestinationUnspecifiedValue) ? CGFLOAT_MAX : destination.point.y;
    if (myY != theirY)
        return (myY > theirY) ? NSOrderedAscending : NSOrderedDescending;
    CGFloat myX = (_point.x == kPDFDestinationUnspecifiedValue) ? 0 : _point.x;
    CGFloat theirX = (destination.point.x == kPDFDestinationUnspecifiedValue) ? 0 : destination.point.x;
    if (myX != theirX)
        return (myX < theirX) ? NSOrderedAscending : NSOrderedDescending;
    return NSOrderedSame;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p page %@ point {%g, %g} zoom %g>", [self class], self, _page.label, _point.x, _point.y, _zoom];
}

@end
