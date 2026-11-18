/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
// MRC, as the rest of AppKit.

#import <AppKit/NSMenuItemCell.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSStringDrawing.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSAttributedString.h>
#import <AppKit/NSEvent.h>

/* Widths, as a menu lays its items out: a column for the state image, the
   image, the title and the key equivalent. */
static const CGFloat kStateColumn = 16;

@interface NSMenuItemCell () {
    NSMenuItem *_cellMenuItem;
    BOOL _cellNeedsSizing;
    BOOL _cellNeedsDisplay;
    CGFloat _cellWidths[4];   /* state image, image, title, key equivalent */
}
@end

@implementation NSMenuItemCell

- (void)dealloc {
    [_cellMenuItem release];
    [super dealloc];
}

- (NSMenuItem *)menuItem {
    return _cellMenuItem;
}

- (void)setMenuItem:(NSMenuItem *)item {
    if(item == _cellMenuItem)
        return;
    [_cellMenuItem release];
    _cellMenuItem = [item retain];
    _cellNeedsSizing = YES;
}

- (BOOL)needsSizing {
    return _cellNeedsSizing;
}

- (void)setNeedsSizing:(BOOL)flag {
    _cellNeedsSizing = flag;
}

- (BOOL)needsDisplay {
    return _cellNeedsDisplay;
}

- (void)setNeedsDisplay:(BOOL)flag {
    _cellNeedsDisplay = flag;
}

/* The key equivalent as a menu shows it: the modifiers' symbols, then
   the key. */
static NSString *keyEquivalentString(NSMenuItem *item) {
    NSString *key = [item keyEquivalent];
    NSUInteger mask = [item keyEquivalentModifierMask];
    NSMutableString *s;

    if([key length] == 0)
        return @"";
    s = [NSMutableString string];
    if(mask & NSEventModifierFlagControl) [s appendString:@"⌃"];
    if(mask & NSEventModifierFlagOption) [s appendString:@"⌥"];
    if((mask & NSEventModifierFlagShift) || ![key isEqualToString:[key lowercaseString]]) [s appendString:@"⇧"];
    if(mask & NSEventModifierFlagCommand) [s appendString:@"⌘"];
    [s appendString:[key uppercaseString]];
    return s;
}

- (NSDictionary *)_textAttributes {
    NSFont *font = [self font] ?: [NSFont menuFontOfSize:0];

    return [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
}

- (void)calcSize {
    NSMenuItem *item = [self menuItem];
    NSDictionary *attributes = [self _textAttributes];

    _cellWidths[0] = ([item onStateImage] != nil || [item state] != NSControlStateValueOff) ? kStateColumn : 0;
    _cellWidths[1] = [item image] != nil ? [[item image] size].width : 0;
    _cellWidths[2] = [item title] != nil ? ceil([[item title] sizeWithAttributes:attributes].width) : 0;
    _cellWidths[3] = ceil([keyEquivalentString(item) sizeWithAttributes:attributes].width);
    _cellNeedsSizing = NO;
}

- (void)_sizeIfNeeded {
    if(_cellNeedsSizing || _cellWidths[2] == 0)
        [self calcSize];
}

- (CGFloat)stateImageWidth {
    [self _sizeIfNeeded];
    return _cellWidths[0];
}

- (CGFloat)imageWidth {
    [self _sizeIfNeeded];
    return _cellWidths[1];
}

- (CGFloat)titleWidth {
    [self _sizeIfNeeded];
    return _cellWidths[2];
}

- (CGFloat)keyEquivalentWidth {
    [self _sizeIfNeeded];
    return _cellWidths[3];
}

- (NSRect)stateImageRectForBounds:(NSRect)cellFrame {
    return NSMakeRect(NSMinX(cellFrame), NSMinY(cellFrame), [self stateImageWidth], NSHeight(cellFrame));
}

- (NSRect)titleRectForBounds:(NSRect)cellFrame {
    CGFloat left = kStateColumn + ([self imageWidth] > 0 ? [self imageWidth] + 4 : 0);

    return NSMakeRect(NSMinX(cellFrame) + left, NSMinY(cellFrame),
                      MAX(0, NSWidth(cellFrame) - left - [self keyEquivalentWidth]), NSHeight(cellFrame));
}

- (NSRect)keyEquivalentRectForBounds:(NSRect)cellFrame {
    CGFloat width = [self keyEquivalentWidth];

    return NSMakeRect(NSMaxX(cellFrame) - width, NSMinY(cellFrame), width, NSHeight(cellFrame));
}

- (void)drawSeparatorItemWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
    [[NSColor separatorColor] setFill];
    NSRectFill(NSMakeRect(NSMinX(cellFrame), floor(NSMidY(cellFrame)), NSWidth(cellFrame), 1));
}

- (void)drawStateImageWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
    NSMenuItem *item = [self menuItem];
    NSImage *image = [item state] == NSControlStateValueOn ? [item onStateImage] :
                     [item state] == NSControlStateValueMixed ? [item mixedStateImage] : [item offStateImage];
    NSRect rect = [self stateImageRectForBounds:cellFrame];

    if(image != nil)
        [image drawInRect:NSMakeRect(NSMidX(rect) - [image size].width / 2, NSMidY(rect) - [image size].height / 2,
                                     [image size].width, [image size].height)];
    else if([item state] == NSControlStateValueOn)
        [@"✓" drawInRect:rect withAttributes:[self _textAttributes]];
}

- (void)drawImageWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
    NSImage *image = [[self menuItem] image];

    if(image != nil)
        [image drawInRect:NSMakeRect(NSMinX(cellFrame) + kStateColumn, NSMidY(cellFrame) - [image size].height / 2,
                                     [image size].width, [image size].height)];
}

- (void)drawTitleWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
    NSString *title = [[self menuItem] title];

    if(title != nil)
        [title drawInRect:[self titleRectForBounds:cellFrame] withAttributes:[self _textAttributes]];
}

- (void)drawKeyEquivalentWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
    NSString *key = keyEquivalentString([self menuItem]);

    if([key length] > 0)
        [key drawInRect:[self keyEquivalentRectForBounds:cellFrame] withAttributes:[self _textAttributes]];
}

- (void)drawBorderAndBackgroundWithFrame:(NSRect)cellFrame inView:(NSView *)controlView {
    if([self isHighlighted]) {
        [[NSColor selectedMenuItemColor] setFill];
        NSRectFill(cellFrame);
    }
}

@end
