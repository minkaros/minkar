/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSGridView (API 19): views in rows and columns, each column as wide and
 * each row as tall as its largest view (or as set), placed in their cells.
 */

#import <AppKit/NSView.h>

@class NSGridRow, NSGridColumn, NSGridCell;

typedef NS_ENUM(NSInteger, NSGridCellPlacement) {
    NSGridCellPlacementInherited = 0,
    NSGridCellPlacementNone,
    NSGridCellPlacementLeading,
    NSGridCellPlacementTop = NSGridCellPlacementLeading,
    NSGridCellPlacementTrailing,
    NSGridCellPlacementBottom = NSGridCellPlacementTrailing,
    NSGridCellPlacementCenter,
    NSGridCellPlacementFill,
};

typedef NS_ENUM(NSInteger, NSGridRowAlignment) {
    NSGridRowAlignmentInherited = 0,
    NSGridRowAlignmentNone,
    NSGridRowAlignmentFirstBaseline,
    NSGridRowAlignmentLastBaseline,
};

/* A row height or column width that follows the content. */
APPKIT_EXPORT const CGFloat NSGridViewSizeForContent;

@interface NSGridView : NSView {
    NSMutableArray *_rows;
    NSMutableArray *_columns;
    NSMutableDictionary *_cells;
    NSGridCellPlacement _xPlacement;
    NSGridCellPlacement _yPlacement;
    NSGridRowAlignment _rowAlignment;
    CGFloat _rowSpacing;
    CGFloat _columnSpacing;
    BOOL _layingOut;
}

+ (instancetype)gridViewWithNumberOfColumns:(NSInteger)columnCount rows:(NSInteger)rowCount;
/* Rows of views; a shorter row leaves its last cells empty. */
+ (instancetype)gridViewWithViews:(NSArray<NSArray<NSView *> *> *)rows;

@property (readonly) NSInteger numberOfRows;
@property (readonly) NSInteger numberOfColumns;
- (NSGridRow *)rowAtIndex:(NSInteger)index;
- (NSInteger)indexOfRow:(NSGridRow *)row;
- (NSGridColumn *)columnAtIndex:(NSInteger)index;
- (NSInteger)indexOfColumn:(NSGridColumn *)column;
- (NSGridCell *)cellAtColumnIndex:(NSInteger)columnIndex rowIndex:(NSInteger)rowIndex;
- (NSGridCell *)cellForView:(NSView *)view;

- (NSGridRow *)addRowWithViews:(NSArray<NSView *> *)views;
- (NSGridRow *)insertRowAtIndex:(NSInteger)index withViews:(NSArray<NSView *> *)views;
- (void)moveRowAtIndex:(NSInteger)fromIndex toIndex:(NSInteger)toIndex;
- (void)removeRowAtIndex:(NSInteger)index;
- (NSGridColumn *)addColumnWithViews:(NSArray<NSView *> *)views;
- (NSGridColumn *)insertColumnAtIndex:(NSInteger)index withViews:(NSArray<NSView *> *)views;
- (void)moveColumnAtIndex:(NSInteger)fromIndex toIndex:(NSInteger)toIndex;
- (void)removeColumnAtIndex:(NSInteger)index;

@property NSGridCellPlacement xPlacement;
@property NSGridCellPlacement yPlacement;
@property NSGridRowAlignment rowAlignment;
@property CGFloat rowSpacing;
@property CGFloat columnSpacing;

/* The cells of the ranges become one, shown by the top leading cell. */
- (void)mergeCellsInHorizontalRange:(NSRange)hRange verticalRange:(NSRange)vRange;

@end

@interface NSGridRow : NSObject <NSCoding> {
    NSGridView *_gridView;
    NSGridCellPlacement _yPlacement;
    NSGridRowAlignment _rowAlignment;
    CGFloat _height;
    CGFloat _topPadding;
    CGFloat _bottomPadding;
    BOOL _hidden;
}
@property (readonly, assign) NSGridView *gridView;
@property (readonly) NSInteger numberOfCells;
- (NSGridCell *)cellAtIndex:(NSInteger)index;
@property NSGridCellPlacement yPlacement;
@property NSGridRowAlignment rowAlignment;
@property CGFloat height;
@property CGFloat topPadding;
@property CGFloat bottomPadding;
@property (getter=isHidden) BOOL hidden;
- (void)mergeCellsInRange:(NSRange)range;
@end

@interface NSGridColumn : NSObject <NSCoding> {
    NSGridView *_gridView;
    NSGridCellPlacement _xPlacement;
    CGFloat _width;
    CGFloat _leadingPadding;
    CGFloat _trailingPadding;
    BOOL _hidden;
}
@property (readonly, assign) NSGridView *gridView;
@property (readonly) NSInteger numberOfCells;
- (NSGridCell *)cellAtIndex:(NSInteger)index;
@property NSGridCellPlacement xPlacement;
@property CGFloat width;
@property CGFloat leadingPadding;
@property CGFloat trailingPadding;
@property (getter=isHidden) BOOL hidden;
- (void)mergeCellsInRange:(NSRange)range;
@end

@interface NSGridCell : NSObject <NSCoding> {
    NSGridView *_gridView;
    NSView *_contentView;
    NSGridCellPlacement _xPlacement;
    NSGridCellPlacement _yPlacement;
    NSGridRowAlignment _rowAlignment;
    NSArray *_customPlacementConstraints;
    NSGridCell *_headOfMergedCell;
    NSGridRow *_row;
    NSGridColumn *_column;
}
@property (strong) NSView *contentView;
/* What an empty cell shows: nothing. */
@property (class, readonly, strong) NSView *emptyContentView;
@property (readonly, assign) NSGridRow *row;
@property (readonly, assign) NSGridColumn *column;
@property NSGridCellPlacement xPlacement;
@property NSGridCellPlacement yPlacement;
@property NSGridRowAlignment rowAlignment;
@property (copy) NSArray *customPlacementConstraints;
@end
