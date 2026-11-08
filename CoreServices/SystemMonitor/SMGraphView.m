/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "SMGraphView.h"

#define POINTS 120

@implementation SMGraphView {
    NSMutableArray<NSArray<NSNumber *> *> *_history;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil) {
        _history = [NSMutableArray array];
        _colors = @[ [NSColor controlAccentColor], [NSColor systemRedColor] ];
    }
    return self;
}

- (void)addValues:(NSArray<NSNumber *> *)values
{
    [_history addObject:values];
    while ([_history count] > POINTS)
        [_history removeObjectAtIndex:0];
    [self setNeedsDisplay:YES];
}

- (void)clear
{
    [_history removeAllObjects];
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)rect
{
    NSRect bounds = NSInsetRect([self bounds], 0.5, 0.5);
    NSBezierPath *frame = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:4 yRadius:4];
    double top = self.maximum;

    [[NSColor controlBackgroundColor] setFill];
    [frame fill];
    [[NSColor gridColor] setStroke];
    for (int i = 1; i < 4; i++) {
        CGFloat y = floor(NSMinY(bounds) + NSHeight(bounds) * i / 4) + 0.5;
        NSBezierPath *line = [NSBezierPath bezierPath];
        [line moveToPoint:NSMakePoint(NSMinX(bounds), y)];
        [line lineToPoint:NSMakePoint(NSMaxX(bounds), y)];
        [line setLineWidth:0.5];
        [line stroke];
    }

    if (top <= 0)
        for (NSArray *values in _history)
            for (NSNumber *v in values)
                top = MAX(top, [v doubleValue]);
    if (top <= 0)
        top = 1;

    NSUInteger series = [[_history lastObject] count];
    CGFloat step = NSWidth(bounds) / (POINTS - 1);
    for (NSUInteger s = 0; s < series; s++) {
        NSBezierPath *path = [NSBezierPath bezierPath];
        NSUInteger n = [_history count];

        for (NSUInteger i = 0; i < n; i++) {
            double v = [_history[i][s] doubleValue];
            NSPoint p = NSMakePoint(NSMaxX(bounds) - (n - 1 - i) * step,
                NSMinY(bounds) + NSHeight(bounds) * MIN(1.0, v / top));
            if (i == 0)
                [path moveToPoint:p];
            else
                [path lineToPoint:p];
        }
        [(s < [self.colors count] ? self.colors[s] : [NSColor labelColor]) setStroke];
        [path setLineWidth:1.5];
        [path setLineJoinStyle:NSRoundLineJoinStyle];
        [path stroke];
    }
    [[NSColor separatorColor] setStroke];
    [frame setLineWidth:1];
    [frame stroke];
}

@end
