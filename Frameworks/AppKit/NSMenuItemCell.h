/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSMenuItemCell (API 10.0): a cell that shows a menu item, its state
 * image, image, title and key equivalent; NSPopUpButtonCell is one.
 */

#import <AppKit/NSButtonCell.h>

@class NSMenuItem;

@interface NSMenuItemCell : NSButtonCell

@property (strong) NSMenuItem *menuItem;
@property BOOL needsSizing;
@property BOOL needsDisplay;

- (void)calcSize;
@property (readonly) CGFloat stateImageWidth;
@property (readonly) CGFloat imageWidth;
@property (readonly) CGFloat titleWidth;
@property (readonly) CGFloat keyEquivalentWidth;

- (NSRect)stateImageRectForBounds:(NSRect)cellFrame;
- (NSRect)titleRectForBounds:(NSRect)cellFrame;
- (NSRect)keyEquivalentRectForBounds:(NSRect)cellFrame;

- (void)drawSeparatorItemWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;
- (void)drawStateImageWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;
- (void)drawImageWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;
- (void)drawTitleWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;
- (void)drawKeyEquivalentWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;
- (void)drawBorderAndBackgroundWithFrame:(NSRect)cellFrame inView:(NSView *)controlView;

@end
