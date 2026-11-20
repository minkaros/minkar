// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSColorPickerTouchBarItem.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSColorList.h>
#import <AppKit/NSImage.h>
#import <Foundation/NSArray.h>

/* As on the reference: black, no alpha, enabled, labeled "Color
   Picker", whichever kind is asked for. */
@implementation NSColorPickerTouchBarItem {
    NSColor *_color;
    BOOL _showsAlpha;
    NSArray *_allowedColorSpaces;
    NSColorList *_colorList;
    NSString *_label;
    id _target;
    SEL _action;
    BOOL _enabled;
    NSImage *_buttonImage;
}

- (instancetype)initWithIdentifier:(NSTouchBarItemIdentifier)identifier {
    if ((self = [super initWithIdentifier:identifier]) != nil) {
        _color = [[NSColor colorWithGenericGamma22White:0 alpha:1] copy];
        _enabled = YES;
    }
    return self;
}

+ (instancetype)colorPickerWithIdentifier:(NSTouchBarItemIdentifier)identifier {
    return [[[self alloc] initWithIdentifier:identifier] autorelease];
}

+ (instancetype)textColorPickerWithIdentifier:(NSTouchBarItemIdentifier)identifier {
    return [self colorPickerWithIdentifier:identifier];
}

+ (instancetype)strokeColorPickerWithIdentifier:(NSTouchBarItemIdentifier)identifier {
    return [self colorPickerWithIdentifier:identifier];
}

+ (instancetype)colorPickerWithIdentifier:(NSTouchBarItemIdentifier)identifier buttonImage:(NSImage *)image {
    NSColorPickerTouchBarItem *result = [self colorPickerWithIdentifier:identifier];
    result->_buttonImage = [image retain];
    return result;
}

- (void)dealloc {
    [_color release];
    [_allowedColorSpaces release];
    [_colorList release];
    [_label release];
    [_buttonImage release];
    [super dealloc];
}

- (NSColor *)color { return _color; }
- (void)setColor:(NSColor *)color { color = [color copy]; [_color release]; _color = color; }
- (BOOL)showsAlpha { return _showsAlpha; }
- (void)setShowsAlpha:(BOOL)value { _showsAlpha = value; }
- (NSArray *)allowedColorSpaces { return _allowedColorSpaces; }
- (void)setAllowedColorSpaces:(NSArray *)spaces { spaces = [spaces copy]; [_allowedColorSpaces release]; _allowedColorSpaces = spaces; }

/* The system's colors until a list is set. */
- (NSColorList *)colorList {
    if (_colorList == nil) {
        NSColorList *list = [NSColorList colorListNamed:@"System"];
        if (list == nil)
            list = [[NSColorList availableColorLists] firstObject];
        if (list == nil)
            list = [[[NSColorList alloc] initWithName:@"Color Picker"] autorelease];
        _colorList = [list retain];
    }
    return _colorList;
}

- (void)setColorList:(NSColorList *)list { [list retain]; [_colorList release]; _colorList = list; }
- (NSString *)customizationLabel { return (_label != nil) ? _label : @"Color Picker"; }
- (void)setCustomizationLabel:(NSString *)label { label = [label copy]; [_label release]; _label = label; }
- (id)target { return _target; }
- (void)setTarget:(id)target { _target = target; }
- (SEL)action { return _action; }
- (void)setAction:(SEL)action { _action = action; }
- (BOOL)isEnabled { return _enabled; }
- (void)setEnabled:(BOOL)value { _enabled = value; }

@end
