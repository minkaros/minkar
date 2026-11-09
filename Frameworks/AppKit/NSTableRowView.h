/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSTableRowView (API 19): a row of a view-based table. It holds the
 * row's cell views and draws its background, selection and separator.
 */

#import <AppKit/NSView.h>
#import <AppKit/NSCell.h>
#import <AppKit/NSTableView.h>

@class NSColor;

@interface NSTableRowView : NSView {
    NSTableViewSelectionHighlightStyle _selectionHighlightStyle;
    NSColor *_backgroundColor;
    BOOL _emphasized;
    BOOL _selected;
    BOOL _groupRowStyle;
    BOOL _floating;
    BOOL _targetForDropOperation;
    BOOL _previousRowSelected;
    BOOL _nextRowSelected;
    NSTableViewDraggingDestinationFeedbackStyle _draggingDestinationFeedbackStyle;
    CGFloat _indentationForDropOperation;
}

@property NSTableViewSelectionHighlightStyle selectionHighlightStyle;
@property (getter=isEmphasized) BOOL emphasized;
@property (getter=isGroupRowStyle) BOOL groupRowStyle;
@property (getter=isSelected) BOOL selected;
@property (getter=isPreviousRowSelected) BOOL previousRowSelected;
@property (getter=isNextRowSelected) BOOL nextRowSelected;
@property (getter=isFloating) BOOL floating;
@property (getter=isTargetForDropOperation) BOOL targetForDropOperation;
@property NSTableViewDraggingDestinationFeedbackStyle draggingDestinationFeedbackStyle;
@property CGFloat indentationForDropOperation;
/* Light, or dark when selected and emphasized. */
@property (readonly) NSBackgroundStyle interiorBackgroundStyle;
@property (copy) NSColor *backgroundColor;
@property (readonly) NSInteger numberOfColumns;

- (void)drawBackgroundInRect:(NSRect)dirtyRect;
- (void)drawSelectionInRect:(NSRect)dirtyRect;
- (void)drawSeparatorInRect:(NSRect)dirtyRect;
- (void)drawDraggingDestinationFeedbackInRect:(NSRect)dirtyRect;
- (id)viewAtColumn:(NSInteger)column;

@end
