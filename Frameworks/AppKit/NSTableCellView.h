/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSTableCellView (API 19): the view a view-based table shows in a cell,
 * with an optional text field and image view, and the row's object value.
 */

#import <AppKit/NSView.h>
#import <AppKit/NSCell.h>

@class NSTextField, NSImageView;

typedef NS_ENUM(NSInteger, NSTableViewRowSizeStyle) {
    NSTableViewRowSizeStyleDefault = -1,
    NSTableViewRowSizeStyleCustom = 0,
    NSTableViewRowSizeStyleSmall = 1,
    NSTableViewRowSizeStyleMedium = 2,
    NSTableViewRowSizeStyleLarge = 3,
};

@interface NSTableCellView : NSView {
    id _objectValue;
    NSTextField *_textField;
    NSImageView *_imageView;
    NSBackgroundStyle _backgroundStyle;
    NSTableViewRowSizeStyle _rowSizeStyle;
    NSArray *_draggingImageComponents;
}

@property (strong) id objectValue;
@property (assign) IBOutlet NSTextField *textField;
@property (assign) IBOutlet NSImageView *imageView;
/* Dark when the row is selected and emphasized: the text field's text
   turns white, as the reference draws it. */
@property NSBackgroundStyle backgroundStyle;
@property NSTableViewRowSizeStyle rowSizeStyle;
@property (readonly, strong) NSArray *draggingImageComponents;

@end
