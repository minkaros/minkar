/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSSwitch.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSActionCell.h>

#define WIDTH 38.0
#define HEIGHT 22.0

@implementation NSSwitch

/* Its target and action live in an action cell, as NSControl keeps them. */
+ (Class)cellClass {
    return [NSActionCell class];
}

- (instancetype)initWithFrame:(NSRect)frame {
    if((self = [super initWithFrame:frame]) != nil)
        _switchState = NSOffState;
    return self;
}

- (NSControlStateValue)state {
    return _switchState;
}

- (void)setState:(NSControlStateValue)state {
    _switchState = (state == NSOffState) ? NSOffState : NSOnState;
    [self setNeedsDisplay:YES];
}

- (id)objectValue {
    return [NSNumber numberWithInteger:_switchState];
}

- (void)setObjectValue:(id)value {
    [self setState:[value integerValue]];
}

- (NSInteger)integerValue {
    return _switchState;
}

- (int)intValue {
    return (int)_switchState;
}

- (NSSize)intrinsicContentSize {
    return NSMakeSize(WIDTH, HEIGHT);
}

- (void)sizeToFit {
    [self setFrameSize:NSMakeSize(WIDTH, HEIGHT)];
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return [self isEnabled];
}

/* The pill, centered in the bounds: the accent color when on, light grey
   when off; a white knob at the side of the state. */
- (void)drawRect:(NSRect)rect {
    NSRect   bounds = [self bounds];
    CGFloat  height = MIN(HEIGHT, NSHeight(bounds)), width = MIN(WIDTH, NSWidth(bounds));
    NSRect   track = NSMakeRect(NSMinX(bounds), floor(NSMidY(bounds) - height / 2), width, height);
    BOOL     on = (_switchState == NSOnState);
    NSColor *fill = on ? [NSColor controlAccentColor] : [NSColor colorWithCalibratedWhite:0.82 alpha:1];
    NSBezierPath *pill = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(track, 0.5, 0.5)
        xRadius:height / 2 - 0.5 yRadius:height / 2 - 0.5];
    CGFloat  knob = height - 4;
    NSRect   knobRect = NSMakeRect(on ? NSMaxX(track) - 2 - knob : NSMinX(track) + 2, NSMinY(track) + 2, knob, knob);

    if(![self isEnabled])
        fill = [fill colorWithAlphaComponent:0.5];
    if(_tracking)
        fill = [fill shadowWithLevel:0.15] ?: fill;
    [fill setFill];
    [pill fill];
    [[NSColor colorWithCalibratedWhite:0 alpha:0.12] setStroke];
    [pill stroke];
    [[NSColor colorWithCalibratedWhite:[self isEnabled] ? 1.0 : 0.96 alpha:1] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:knobRect] fill];
    [[NSColor colorWithCalibratedWhite:0 alpha:0.18] setStroke];
    [[NSBezierPath bezierPathWithOvalInRect:NSInsetRect(knobRect, 0.5, 0.5)] stroke];
}

/* A click (or a drag ending past the middle) toggles it and sends the action. */
- (void)mouseDown:(NSEvent *)event {
    NSInteger was = _switchState;
    NSPoint   point;

    if(![self isEnabled])
        return;
    _tracking = YES;
    [self setNeedsDisplay:YES];
    point = [self convertPoint:[event locationInWindow] fromView:nil];
    for(;;) {
        event = [[self window] nextEventMatchingMask:NSLeftMouseUpMask | NSLeftMouseDraggedMask];
        point = [self convertPoint:[event locationInWindow] fromView:nil];
        if([event type] == NSLeftMouseUp)
            break;
    }
    _tracking = NO;
    if(NSPointInRect(point, [self bounds]))
        _switchState = (was == NSOnState) ? NSOffState : NSOnState;
    [self setNeedsDisplay:YES];
    if(_switchState != was)
        [self sendAction:[self action] to:[self target]];
}

- (void)keyDown:(NSEvent *)event {
    if([[event characters] isEqualToString:@" "]) {
        [self setState:(_switchState == NSOnState) ? NSOffState : NSOnState];
        [self sendAction:[self action] to:[self target]];
    }
    else
        [super keyDown:event];
}

@end
