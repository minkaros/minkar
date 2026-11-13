/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSSetFocusRingStyle: the fills that follow are drawn as a focus ring
 * around their shape (only the ring, or the ring below or above the fill),
 * until the graphics state it was set in is restored. Fills here are
 * NSRectFill and -[NSBezierPath fill], which call _NSFillWithFocusRing.
 */

#import <AppKit/NSGraphics.h>
#import <AppKit/NSGraphicsContext.h>
#import <AppKit/NSColor.h>
#import <objc/runtime.h>
#import "NSFocusRingStyle.h"

static char focusRingStackKey;

/* One entry per saved graphics state: the style set in it, or -1. */
static NSMutableArray *styleStack(NSGraphicsContext *context) {
    NSMutableArray *stack = objc_getAssociatedObject(context, &focusRingStackKey);
    if (stack == nil) {
        stack = [NSMutableArray arrayWithObject:@(-1)];
        objc_setAssociatedObject(context, &focusRingStackKey, stack, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return stack;
}

void NSSetFocusRingStyle(NSFocusRingPlacement placement) {
    NSGraphicsContext *context = [NSGraphicsContext currentContext];
    if (context == nil)
        return;
    NSMutableArray *stack = styleStack(context);
    [stack replaceObjectAtIndex:[stack count] - 1 withObject:@(placement)];
}

void _NSFocusRingSaveState(NSGraphicsContext *context) {
    NSMutableArray *stack = objc_getAssociatedObject(context, &focusRingStackKey);
    if (stack != nil)
        [stack addObject:[stack lastObject]];
}

void _NSFocusRingRestoreState(NSGraphicsContext *context) {
    NSMutableArray *stack = objc_getAssociatedObject(context, &focusRingStackKey);
    if ([stack count] > 1)
        [stack removeLastObject];
}

static NSInteger currentStyle(void) {
    NSGraphicsContext *context = [NSGraphicsContext currentContext];
    NSMutableArray *stack = context ? objc_getAssociatedObject(context, &focusRingStackKey) : nil;
    return stack ? [[stack lastObject] integerValue] : -1;
}

/* The ring: the shape's outline, widened outward, in the focus color. */
static void drawRing(CGContextRef context, CGPathRef path) {
    CGContextSaveGState(context);
    CGContextBeginPath(context);
    CGContextAddPath(context, path);
    [[[NSColor keyboardFocusIndicatorColor] colorWithAlphaComponent:0.6] setStroke];
    CGContextSetLineWidth(context, 4.0);
    CGContextSetLineJoin(context, kCGLineJoinRound);
    CGContextStrokePath(context);
    CGContextRestoreGState(context);
}

BOOL _NSFillWithFocusRing(CGContextRef context, CGPathRef path, void (^fill)(void)) {
    NSInteger style = currentStyle();
    if (style < 0 || path == NULL)
        return NO;
    if (style == NSFocusRingBelow)
        drawRing(context, path);
    if (style != NSFocusRingOnly)
        fill();
    if (style == NSFocusRingOnly || style == NSFocusRingAbove)
        drawRing(context, path);
    return YES;
}
