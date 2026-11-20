// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSScrubber.h>
#import <AppKit/NSTextField.h>
#import <AppKit/NSImageView.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSFont.h>
#import <AppKit/NSGraphics.h>

@implementation NSScrubberArrangedView {
    BOOL _selected, _highlighted;
}

- (BOOL)isSelected { return _selected; }
- (void)setSelected:(BOOL)value { _selected = value; [self setNeedsDisplay:YES]; }
- (BOOL)isHighlighted { return _highlighted; }
- (void)setHighlighted:(BOOL)value { _highlighted = value; [self setNeedsDisplay:YES]; }

- (void)applyLayoutAttributes:(NSScrubberLayoutAttributes *)attributes {
    [self setFrame:[attributes frame]];
    [self setAlphaValue:[attributes alpha]];
}

@end

@implementation NSScrubberSelectionView
@end

@implementation NSScrubberItemView
@end

@implementation NSScrubberTextItemView {
    NSTextField *_textField;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame]) != nil) {
        _textField = [[NSTextField alloc] initWithFrame:[self bounds]];
        [_textField setEditable:NO];
        [_textField setSelectable:NO];
        [_textField setBordered:NO];
        [_textField setDrawsBackground:NO];
        [_textField setAlignment:NSTextAlignmentCenter];
        [_textField setStringValue:@""];
        [_textField setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:_textField];
    }
    return self;
}

- (void)dealloc {
    [_textField release];
    [super dealloc];
}

- (NSTextField *)textField { return _textField; }
- (NSString *)title { return [_textField stringValue]; }
- (void)setTitle:(NSString *)title { [_textField setStringValue:(title != nil) ? title : @""]; }

@end

@implementation NSScrubberImageItemView {
    NSImageView *_imageView;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame]) != nil) {
        _imageView = [[NSImageView alloc] initWithFrame:[self bounds]];
        [_imageView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:_imageView];
    }
    return self;
}

- (void)dealloc {
    [_imageView release];
    [super dealloc];
}

- (NSImageView *)imageView { return _imageView; }
- (NSImage *)image { return [_imageView image]; }
- (void)setImage:(NSImage *)image { [_imageView setImage:image]; }
- (NSImageAlignment)imageAlignment { return [_imageView imageAlignment]; }
- (void)setImageAlignment:(NSImageAlignment)alignment { [_imageView setImageAlignment:alignment]; }

@end

/* The two styles of the selection: an outline over the item, and a
   rounded background under it. */
@interface _NSScrubberOutlineSelectionStyle : NSScrubberSelectionStyle
@end
@interface _NSScrubberRoundedBackgroundSelectionStyle : NSScrubberSelectionStyle
@end

@interface _NSScrubberStyledSelectionView : NSScrubberSelectionView {
@public
    BOOL _outline;
}
@end

@implementation _NSScrubberStyledSelectionView

- (void)drawRect:(NSRect)rect {
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect([self bounds], 1, 1) xRadius:6 yRadius:6];

    if (_outline) {
        [[NSColor selectedControlColor] setStroke];
        [path setLineWidth:2];
        [path stroke];
    } else {
        [[NSColor selectedControlColor] setFill];
        [path fill];
    }
}

@end

@implementation NSScrubberSelectionStyle

+ (NSScrubberSelectionStyle *)outlineOverlayStyle {
    static NSScrubberSelectionStyle *style;
    if (style == nil)
        style = [[_NSScrubberOutlineSelectionStyle alloc] init];
    return style;
}

+ (NSScrubberSelectionStyle *)roundedBackgroundStyle {
    static NSScrubberSelectionStyle *style;
    if (style == nil)
        style = [[_NSScrubberRoundedBackgroundSelectionStyle alloc] init];
    return style;
}

- (instancetype)init {
    return [super init];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super init];
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (NSScrubberSelectionView *)makeSelectionView {
    return nil;
}

@end

@implementation _NSScrubberOutlineSelectionStyle
- (NSScrubberSelectionView *)makeSelectionView {
    _NSScrubberStyledSelectionView *view = [[[_NSScrubberStyledSelectionView alloc] initWithFrame:NSZeroRect] autorelease];
    view->_outline = YES;
    return view;
}
@end

@implementation _NSScrubberRoundedBackgroundSelectionStyle
- (NSScrubberSelectionView *)makeSelectionView {
    return [[[_NSScrubberStyledSelectionView alloc] initWithFrame:NSZeroRect] autorelease];
}
@end
