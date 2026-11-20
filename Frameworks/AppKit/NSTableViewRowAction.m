// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSTableViewRowAction.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSImage.h>

@implementation NSTableViewRowAction {
    NSTableViewRowActionStyle _style;
    NSString *_title;
    NSColor *_backgroundColor;
    NSImage *_image;
    void (^_handler)(NSTableViewRowAction *, NSInteger);
}

@synthesize style = _style;

+ (instancetype)rowActionWithStyle:(NSTableViewRowActionStyle)style title:(NSString *)title handler:(void (^)(NSTableViewRowAction *, NSInteger))handler {
    NSTableViewRowAction *result = [[[self alloc] init] autorelease];
    result->_style = style;
    result->_title = [title copy];
    result->_handler = [handler copy];
    return result;
}

- (void)dealloc {
    [_title release];
    [_backgroundColor release];
    [_image release];
    [_handler release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    NSTableViewRowAction *copy = [[[self class] allocWithZone:zone] init];
    copy->_style = _style;
    copy->_title = [_title copy];
    copy->_backgroundColor = [_backgroundColor copy];
    copy->_image = [_image retain];
    copy->_handler = [_handler copy];
    return copy;
}

- (NSString *)title {
    return _title;
}

- (void)setTitle:(NSString *)title {
    title = [title copy];
    [_title release];
    _title = title;
}

/* Red for a destructive action, blue for a regular one, as on the
   reference, until one is set. */
- (NSColor *)backgroundColor {
    if (_backgroundColor != nil)
        return _backgroundColor;
    return (_style == NSTableViewRowActionStyleDestructive) ? [NSColor systemRedColor] : [NSColor systemBlueColor];
}

- (void)setBackgroundColor:(NSColor *)color {
    color = [color copy];
    [_backgroundColor release];
    _backgroundColor = color;
}

- (NSImage *)image {
    return _image;
}

- (void)setImage:(NSImage *)image {
    [image retain];
    [_image release];
    _image = image;
}

/* The table runs it for the row the action was shown on. */
- (void)_performForRow:(NSInteger)row {
    if (_handler != NULL)
        _handler(self, row);
}

@end
