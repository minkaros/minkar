/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Glass views (NSGlassEffectView.h). Atlas draws glass as its menus'
 * translucent surface (lighter for the clear style), with the tint over
 * it; the content view fills the glass and is clipped to its corners.
 */

#import <AppKit/NSGlassEffectView.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSGraphicsContext.h>

BOOL _NSSystemAppearanceIsDark(void);

@implementation NSGlassEffectView

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame]) != nil)
        _style = NSGlassEffectViewStyleRegular;
    return self;
}

- (void)dealloc {
    [_contentView release];
    [_tintColor release];
    [super dealloc];
}

- (NSView *)contentView { return _contentView; }

- (void)setContentView:(NSView *)view {
    if (view == _contentView)
        return;
    [_contentView removeFromSuperview];
    [_contentView release];
    _contentView = [view retain];
    if (view != nil) {
        [view setFrame:[self bounds]];
        [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:view];
    }
}

- (CGFloat)cornerRadius { return _cornerRadius; }
- (void)setCornerRadius:(CGFloat)radius { _cornerRadius = MAX(0, radius); [self setNeedsDisplay:YES]; }
- (NSColor *)tintColor { return _tintColor; }
- (void)setTintColor:(NSColor *)color { color = [color copy]; [_tintColor release]; _tintColor = color; [self setNeedsDisplay:YES]; }
- (NSGlassEffectViewStyle)style { return _style; }
- (void)setStyle:(NSGlassEffectViewStyle)style { _style = style; [self setNeedsDisplay:YES]; }

- (BOOL)isOpaque { return NO; }

/* The glass's surface; what is behind layers the view hosts too (NSView.m). */
- (NSColor *)_materialColor {
    BOOL dark = _NSSystemAppearanceIsDark();
    CGFloat alpha = _style == NSGlassEffectViewStyleClear ? 0.70 : 0.94;
    return [NSColor colorWithGenericGamma22White:dark ? 0.16 : 0.97 alpha:alpha];
}

- (void)drawRect:(NSRect)rect {
    NSRect bounds = [self bounds];
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:bounds xRadius:_cornerRadius yRadius:_cornerRadius];

    [[NSColor clearColor] setFill];
    NSRectFillUsingOperation(NSIntersectionRect(rect, bounds), NSCompositingOperationCopy);
    [[self _materialColor] setFill];
    [shape fill];
    if (_tintColor != nil) {
        [[_tintColor colorWithAlphaComponent:[_tintColor alphaComponent] * 0.35] setFill];
        [shape fill];
    }
}

@end
