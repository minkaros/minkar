/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Symbol configurations (NSImageSymbolConfiguration.h). Minkar's symbols
 * are drawings without weights, so a configuration sizes the image (its
 * point size, its scale) and colors a template one with its hierarchical
 * color or the first of its palette; the weight is kept and reported.
 */

#import <AppKit/NSImageSymbolConfiguration.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGraphics.h>
#import <objc/runtime.h>

@implementation NSImageSymbolConfiguration

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)configurationWithPointSize:(CGFloat)pointSize weight:(NSFontWeight)weight scale:(NSImageSymbolScale)scale {
    NSImageSymbolConfiguration *c = [[[self alloc] init] autorelease];
    c->_pointSize = pointSize;
    c->_weight = weight;
    c->_scale = scale;
    return c;
}

+ (instancetype)configurationWithPointSize:(CGFloat)pointSize weight:(NSFontWeight)weight {
    return [self configurationWithPointSize:pointSize weight:weight scale:0];
}

+ (instancetype)configurationWithTextStyle:(NSString *)style scale:(NSImageSymbolScale)scale {
    NSImageSymbolConfiguration *c = [[[self alloc] init] autorelease];
    c->_textStyle = [style copy];
    c->_scale = scale;
    return c;
}

+ (instancetype)configurationWithTextStyle:(NSString *)style {
    return [self configurationWithTextStyle:style scale:0];
}

+ (instancetype)configurationWithScale:(NSImageSymbolScale)scale {
    NSImageSymbolConfiguration *c = [[[self alloc] init] autorelease];
    c->_scale = scale;
    return c;
}

+ (instancetype)configurationWithHierarchicalColor:(NSColor *)hierarchicalColor {
    NSImageSymbolConfiguration *c = [[[self alloc] init] autorelease];
    c->_hierarchicalColor = [hierarchicalColor retain];
    return c;
}

+ (instancetype)configurationWithPaletteColors:(NSArray<NSColor *> *)paletteColors {
    NSImageSymbolConfiguration *c = [[[self alloc] init] autorelease];
    c->_paletteColors = [paletteColors copy];
    return c;
}

+ (instancetype)configurationPreferringMulticolor {
    NSImageSymbolConfiguration *c = [[[self alloc] init] autorelease];
    c->_prefersMulticolor = YES;
    return c;
}

- (void)dealloc {
    [_textStyle release];
    [_hierarchicalColor release];
    [_paletteColors release];
    [super dealloc];
}

/* The other's settings over this one's, where it has them. */
- (instancetype)configurationByApplyingConfiguration:(NSImageSymbolConfiguration *)other {
    NSImageSymbolConfiguration *c = [[self copy] autorelease];
    if (other == nil)
        return c;
    if (other->_pointSize > 0) {
        c->_pointSize = other->_pointSize;
        c->_weight = other->_weight;
        [c->_textStyle release];
        c->_textStyle = nil;
    }
    if (other->_textStyle != nil) {
        [c->_textStyle release];
        c->_textStyle = [other->_textStyle copy];
        c->_pointSize = 0;
    }
    if (other->_scale != 0)
        c->_scale = other->_scale;
    if (other->_hierarchicalColor != nil) {
        [c->_hierarchicalColor release];
        c->_hierarchicalColor = [other->_hierarchicalColor retain];
    }
    if (other->_paletteColors != nil) {
        [c->_paletteColors release];
        c->_paletteColors = [other->_paletteColors copy];
    }
    if (other->_prefersMulticolor)
        c->_prefersMulticolor = YES;
    return c;
}

- (id)copyWithZone:(NSZone *)zone {
    NSImageSymbolConfiguration *c = [[[self class] allocWithZone:zone] init];
    c->_pointSize = _pointSize;
    c->_weight = _weight;
    c->_scale = _scale;
    c->_textStyle = [_textStyle copy];
    c->_hierarchicalColor = [_hierarchicalColor retain];
    c->_paletteColors = [_paletteColors copy];
    c->_prefersMulticolor = _prefersMulticolor;
    return c;
}

- (BOOL)isEqual:(id)other {
    if (![other isKindOfClass:[NSImageSymbolConfiguration class]])
        return NO;
    NSImageSymbolConfiguration *o = other;
    return o->_pointSize == _pointSize && o->_weight == _weight && o->_scale == _scale &&
           (o->_textStyle == _textStyle || [o->_textStyle isEqual:_textStyle]) &&
           (o->_hierarchicalColor == _hierarchicalColor || [o->_hierarchicalColor isEqual:_hierarchicalColor]) &&
           (o->_paletteColors == _paletteColors || [o->_paletteColors isEqual:_paletteColors]) &&
           o->_prefersMulticolor == _prefersMulticolor;
}

- (NSUInteger)hash {
    return (NSUInteger)(_pointSize * 31) ^ (NSUInteger)_scale ^ [_textStyle hash];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeDouble:_pointSize forKey:@"NSPointSize"];
    [coder encodeDouble:_weight forKey:@"NSWeight"];
    [coder encodeInteger:_scale forKey:@"NSScale"];
    [coder encodeObject:_textStyle forKey:@"NSTextStyle"];
    [coder encodeObject:_hierarchicalColor forKey:@"NSHierarchicalColor"];
    [coder encodeObject:_paletteColors forKey:@"NSPaletteColors"];
    [coder encodeBool:_prefersMulticolor forKey:@"NSPrefersMulticolor"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]) != nil) {
        _pointSize = [coder decodeDoubleForKey:@"NSPointSize"];
        _weight = [coder decodeDoubleForKey:@"NSWeight"];
        _scale = [coder decodeIntegerForKey:@"NSScale"];
        _textStyle = [[coder decodeObjectForKey:@"NSTextStyle"] copy];
        _hierarchicalColor = [[coder decodeObjectForKey:@"NSHierarchicalColor"] retain];
        _paletteColors = [[coder decodeObjectForKey:@"NSPaletteColors"] copy];
        _prefersMulticolor = [coder decodeBoolForKey:@"NSPrefersMulticolor"];
    }
    return self;
}

/* The height the symbol is drawn at: the point size (the system font's
   for a text style), times the scale. 0: as the image is. */
- (CGFloat)_symbolHeight {
    CGFloat size = _pointSize;
    if (size <= 0 && _textStyle != nil)
        size = [NSFont systemFontSize];
    if (size <= 0)
        return 0;
    switch (_scale) {
    case NSImageSymbolScaleSmall:
        return round(size * 0.8);
    case NSImageSymbolScaleLarge:
        return round(size * 1.3);
    default:
        return size;
    }
}

- (NSColor *)_symbolColor {
    if (_hierarchicalColor != nil)
        return _hierarchicalColor;
    return [_paletteColors count] > 0 ? [_paletteColors objectAtIndex:0] : nil;
}

@end

static char symbolConfigurationKey;

@implementation NSImage (SymbolConfiguration)

- (NSImageSymbolConfiguration *)symbolConfiguration {
    NSImageSymbolConfiguration *c = objc_getAssociatedObject(self, &symbolConfigurationKey);
    return c != nil ? [[c copy] autorelease] : [[[NSImageSymbolConfiguration alloc] init] autorelease];
}

- (NSImage *)imageWithSymbolConfiguration:(NSImageSymbolConfiguration *)configuration {
    NSImageSymbolConfiguration *applied =
        [objc_getAssociatedObject(self, &symbolConfigurationKey) ?: [[[NSImageSymbolConfiguration alloc] init] autorelease]
            configurationByApplyingConfiguration:configuration];
    NSImage *image = [[self copy] autorelease];
    CGFloat height = [applied _symbolHeight];
    NSSize size = [self size];

    if (height > 0 && size.height > 0)
        [image setSize:NSMakeSize(round(size.width * height / size.height), height)];
    NSColor *color = [applied _symbolColor];
    if (color != nil && [self isTemplate]) {
        NSSize drawn = [image size];
        NSImage *tinted = [[[NSImage alloc] initWithSize:drawn] autorelease];
        [tinted lockFocus];
        [image drawInRect:NSMakeRect(0, 0, drawn.width, drawn.height) fromRect:NSZeroRect
                operation:NSCompositingOperationSourceOver fraction:1];
        [color setFill];
        NSRectFillUsingOperation(NSMakeRect(0, 0, drawn.width, drawn.height), NSCompositingOperationSourceIn);
        [tinted unlockFocus];
        [tinted setAccessibilityDescription:[self accessibilityDescription]];
        image = tinted;
    }
    objc_setAssociatedObject(image, &symbolConfigurationKey, applied, OBJC_ASSOCIATION_RETAIN);
    return image;
}

/* The image in one color: its shape (its alpha) filled with it. */
- (NSImage *)imageWithTintColor:(NSColor *)color {
    NSSize size = [self size];
    NSImage *tinted = [[[NSImage alloc] initWithSize:size] autorelease];
    if (color == nil || size.width <= 0 || size.height <= 0)
        return [[self copy] autorelease];
    [tinted lockFocus];
    [self drawInRect:NSMakeRect(0, 0, size.width, size.height) fromRect:NSZeroRect
           operation:NSCompositingOperationSourceOver fraction:1];
    [color setFill];
    NSRectFillUsingOperation(NSMakeRect(0, 0, size.width, size.height), NSCompositingOperationSourceIn);
    [tinted unlockFocus];
    [tinted setAccessibilityDescription:[self accessibilityDescription]];
    return tinted;
}

@end
