/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The window bar's segmented controls (FilesSegmentedControl.h).
 */

#import "FilesSegmentedControl.h"

static const CGFloat kSegmentWidth = 28;
static const CGFloat kHeight = 24;

@implementation FilesSegmentedControl {
    NSArray<NSNumber *> *_glyphs;
    NSArray<NSString *> *_actions;
    __weak id _target;
    NSMutableArray<NSNumber *> *_enabled;
    NSMutableArray<NSString *> *_labels;
    NSInteger _pressed;     /* the segment under a press, or -1 */
    NSInteger _down;        /* where the press began */
}

- (instancetype)initWithGlyphs:(NSArray<NSNumber *> *)glyphs actions:(NSArray<NSString *> *)actions
                        target:(id)target
{
    if ((self = [super initWithFrame:NSMakeRect(0, 0, kSegmentWidth * [glyphs count], kHeight)]) != nil) {
        _glyphs = [glyphs copy];
        _actions = [actions copy];
        _target = target;
        _enabled = [NSMutableArray array];
        _labels = [NSMutableArray array];
        for (NSUInteger i = 0; i < [glyphs count]; i++) {
            [_enabled addObject:@YES];
            [_labels addObject:@""];
        }
        _selectedSegment = -1;
        _pressed = _down = -1;
    }
    return self;
}

- (NSSize)naturalSize
{
    return NSMakeSize(kSegmentWidth * [_glyphs count], kHeight);
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event
{
    return YES;
}

- (void)setSelectedSegment:(NSInteger)segment
{
    _selectedSegment = segment;
    [self setNeedsDisplay:YES];
}

- (void)setEnabled:(BOOL)enabled forSegment:(NSInteger)segment
{
    if (segment < 0 || (NSUInteger)segment >= [_enabled count])
        return;
    _enabled[(NSUInteger)segment] = @(enabled);
    [self setNeedsDisplay:YES];
}

- (BOOL)isEnabledForSegment:(NSInteger)segment
{
    return segment >= 0 && (NSUInteger)segment < [_enabled count] && [_enabled[(NSUInteger)segment] boolValue];
}

- (void)setLabel:(NSString *)label forSegment:(NSInteger)segment
{
    if (segment < 0 || (NSUInteger)segment >= [_labels count])
        return;
    _labels[(NSUInteger)segment] = [label copy];
    [self setToolTip:[_labels componentsJoinedByString:@" · "]];
}

- (NSRect)rectForSegment:(NSInteger)segment
{
    CGFloat width = NSWidth([self bounds]) / MAX([_glyphs count], 1);

    return NSMakeRect(width * segment, 0, width, NSHeight([self bounds]));
}

- (NSInteger)segmentAtPoint:(NSPoint)p
{
    if (!NSPointInRect(p, [self bounds]))
        return -1;
    NSInteger segment = (NSInteger)(p.x / (NSWidth([self bounds]) / MAX([_glyphs count], 1)));
    return MIN(segment, (NSInteger)[_glyphs count] - 1);
}

/* -- Drawing -- */

static void strokePath(NSBezierPath *path)
{
    [path setLineWidth:1.6];
    [path setLineCapStyle:NSRoundLineCapStyle];
    [path setLineJoinStyle:NSRoundLineJoinStyle];
    [path stroke];
}

/* A glyph in a 14-point box centered in rect (y grows downward). */
static void drawGlyph(FilesGlyph glyph, NSRect rect)
{
    CGFloat x = round(NSMidX(rect) - 7), y = round(NSMidY(rect) - 7);
    NSBezierPath *path = [NSBezierPath bezierPath];

    switch (glyph) {
    case FilesGlyphBack:
        [path moveToPoint:NSMakePoint(x + 9, y + 2)];
        [path lineToPoint:NSMakePoint(x + 4, y + 7)];
        [path lineToPoint:NSMakePoint(x + 9, y + 12)];
        strokePath(path);
        break;
    case FilesGlyphForward:
        [path moveToPoint:NSMakePoint(x + 5, y + 2)];
        [path lineToPoint:NSMakePoint(x + 10, y + 7)];
        [path lineToPoint:NSMakePoint(x + 5, y + 12)];
        strokePath(path);
        break;
    case FilesGlyphIcons:
        for (int r = 0; r < 2; r++)
            for (int c = 0; c < 2; c++)
                [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(x + 1 + c * 7, y + 1 + r * 7, 5, 5)
                                                 xRadius:1 yRadius:1] fill];
        break;
    case FilesGlyphList:
        for (int r = 0; r < 4; r++) {
            NSRectFill(NSMakeRect(x + 1, y + 1.5 + r * 3.5, 2, 2));
            NSRectFill(NSMakeRect(x + 4.5, y + 2 + r * 3.5, 8.5, 1));
        }
        break;
    case FilesGlyphColumns: {
        NSBezierPath *frame = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(x + 0.8, y + 1.8, 12.4, 10.4)
                                                              xRadius:1.5 yRadius:1.5];
        [frame setLineWidth:1.3];
        [frame stroke];
        NSRectFill(NSMakeRect(x + 4.8, y + 2, 1.2, 10));
        NSRectFill(NSMakeRect(x + 8.8, y + 2, 1.2, 10));
        break;
    }
    }
}

- (void)drawRect:(NSRect)dirty
{
    NSRect bounds = NSInsetRect([self bounds], 0.5, 0.5);
    NSBezierPath *capsule = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:6 yRadius:6];
    NSUInteger count = [_glyphs count];

    [[NSColor colorWithCalibratedWhite:0.985 alpha:1] setFill];
    [capsule fill];

    [NSGraphicsContext saveGraphicsState];
    [capsule addClip];
    for (NSUInteger i = 0; i < count; i++) {
        NSRect r = [self rectForSegment:(NSInteger)i];
        if ((NSInteger)i == _selectedSegment) {
            [[NSColor colorWithCalibratedWhite:0.80 alpha:1] setFill];
            NSRectFill(r);
        } else if ((NSInteger)i == _pressed && [self isEnabledForSegment:(NSInteger)i]) {
            [[NSColor colorWithCalibratedWhite:0.87 alpha:1] setFill];
            NSRectFill(r);
        }
        if (i > 0) {
            [[NSColor colorWithCalibratedWhite:0.78 alpha:1] setFill];
            NSRectFill(NSMakeRect(floor(NSMinX(r)), 0, 1, NSHeight([self bounds])));
        }
    }
    [NSGraphicsContext restoreGraphicsState];

    [[NSColor colorWithCalibratedWhite:0.70 alpha:1] setStroke];
    [capsule setLineWidth:1];
    [capsule stroke];

    for (NSUInteger i = 0; i < count; i++) {
        BOOL enabled = [self isEnabledForSegment:(NSInteger)i];
        NSColor *ink = (NSInteger)i == _selectedSegment ? [NSColor colorWithCalibratedWhite:0.12 alpha:1]
                                                        : [NSColor colorWithCalibratedWhite:0.30 alpha:1];
        if (!enabled)
            ink = [NSColor colorWithCalibratedWhite:0.30 alpha:0.30];
        [ink setFill];
        [ink setStroke];
        drawGlyph((FilesGlyph)[_glyphs[i] integerValue], [self rectForSegment:(NSInteger)i]);
    }
}

/* -- The pointer -- */

- (void)mouseDown:(NSEvent *)event
{
    NSInteger segment = [self segmentAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

    _down = [self isEnabledForSegment:segment] ? segment : -1;
    _pressed = _down;
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)event
{
    NSInteger segment = [self segmentAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];
    NSInteger pressed = segment == _down ? _down : -1;

    if (pressed != _pressed) {
        _pressed = pressed;
        [self setNeedsDisplay:YES];
    }
}

- (void)mouseUp:(NSEvent *)event
{
    NSInteger segment = [self segmentAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];
    NSInteger chosen = segment == _down ? _down : -1;

    _pressed = _down = -1;
    [self setNeedsDisplay:YES];
    if (chosen < 0)
        return;
    SEL action = NSSelectorFromString(_actions[(NSUInteger)chosen]);
    [NSApp sendAction:action to:_target from:self];
}

@end
