// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSSearchToolbarItem.h>
#import <AppKit/NSSearchField.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSToolbar.h>

@implementation NSSearchToolbarItem {
    NSSearchField *_searchField;
    BOOL _resignsFirstResponderWithCancel;
    CGFloat _preferredWidth;
}

- (instancetype)initWithItemIdentifier:(NSString *)identifier {
    if ((self = [super initWithItemIdentifier:identifier]) != nil) {
        _searchField = [[NSSearchField alloc] initWithFrame:NSMakeRect(0, 0, 140, 22)];
        _resignsFirstResponderWithCancel = YES;
        _preferredWidth = 140;
        [self setView:_searchField];
        [self setMinSize:NSMakeSize(30, 22)];
        [self setMaxSize:NSMakeSize(_preferredWidth, 22)];
    }
    return self;
}

- (void)dealloc {
    [_searchField release];
    [super dealloc];
}

- (NSSearchField *)searchField {
    return _searchField;
}

- (void)setSearchField:(NSSearchField *)field {
    [field retain];
    [_searchField release];
    _searchField = field;
    [self setView:field];
}

- (BOOL)resignsFirstResponderWithCancel { return _resignsFirstResponderWithCancel; }
- (void)setResignsFirstResponderWithCancel:(BOOL)value { _resignsFirstResponderWithCancel = value; }
- (CGFloat)preferredWidthForSearchField { return _preferredWidth; }

- (void)setPreferredWidthForSearchField:(CGFloat)width {
    _preferredWidth = width;
    [self setMaxSize:NSMakeSize(width, [self maxSize].height)];
}

/* Searching begins in the field: it becomes its window's first
   responder. */
- (void)beginSearchInteraction {
    NSWindow *window = [_searchField window];

    if (window != nil)
        [window makeFirstResponder:_searchField];
}

- (void)endSearchInteraction {
    NSWindow *window = [_searchField window];

    if (window != nil && [window firstResponder] == [_searchField currentEditor])
        [window makeFirstResponder:nil];
}

@end
