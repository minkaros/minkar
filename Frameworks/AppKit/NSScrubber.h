// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSView.h>
#import <AppKit/NSScrubberLayout.h>
#import <AppKit/NSScrubberItemView.h>

@class NSScrubber, NSScrubberSelectionStyle, NSColor, NSNib, NSIndexSet;

NS_ASSUME_NONNULL_BEGIN


@protocol NSScrubberDataSource <NSObject>
- (NSInteger)numberOfItemsForScrubber:(NSScrubber *)scrubber;
- (__kindof NSScrubberItemView *)scrubber:(NSScrubber *)scrubber viewForItemAtIndex:(NSInteger)index;
@end

@protocol NSScrubberDelegate <NSObject>
@optional
- (void)scrubber:(NSScrubber *)scrubber didSelectItemAtIndex:(NSInteger)selectedIndex;
- (void)scrubber:(NSScrubber *)scrubber didHighlightItemAtIndex:(NSInteger)highlightedIndex;
- (void)scrubber:(NSScrubber *)scrubber didChangeVisibleRange:(NSRange)visibleRange;
- (void)didBeginInteractingWithScrubber:(NSScrubber *)scrubber;
- (void)didFinishInteractingWithScrubber:(NSScrubber *)scrubber;
- (void)didCancelInteractingWithScrubber:(NSScrubber *)scrubber;
@end

@protocol NSScrubberFlowLayoutDelegate <NSScrubberDelegate>
@optional
- (NSSize)scrubber:(NSScrubber *)scrubber layout:(NSScrubberFlowLayout *)layout sizeForItemAtIndex:(NSInteger)itemIndex;
@end

typedef NS_ENUM(NSInteger, NSScrubberMode) {
    NSScrubberModeFixed = 0,
    NSScrubberModeFree
};

typedef NS_ENUM(NSInteger, NSScrubberAlignment) {
    NSScrubberAlignmentNone = 0,
    NSScrubberAlignmentLeading,
    NSScrubberAlignmentTrailing,
    NSScrubberAlignmentCenter
};

/* How the selected item is shown: a view made for it, over or under. */
@interface NSScrubberSelectionStyle : NSObject <NSCoding>
@property (class, strong, readonly) NSScrubberSelectionStyle *outlineOverlayStyle;
@property (class, strong, readonly) NSScrubberSelectionStyle *roundedBackgroundStyle;
- (instancetype)init;
- (nullable instancetype)initWithCoder:(NSCoder *)coder;
- (nullable __kindof NSScrubberSelectionView *)makeSelectionView;
@end

/* A row of items to pick from by sliding over them (the Touch Bar's), its
   items given by its data source and placed by its layout. */
@interface NSScrubber : NSView
@property (assign, nullable) id<NSScrubberDataSource> dataSource;
@property (assign, nullable) id<NSScrubberDelegate> delegate;
@property (strong) NSScrubberLayout *scrubberLayout;
@property (readonly) NSInteger numberOfItems;
@property (readonly) NSInteger highlightedIndex;
@property NSInteger selectedIndex;
@property NSScrubberMode mode;
@property NSScrubberAlignment itemAlignment;
@property (getter=isContinuous) BOOL continuous;
@property BOOL floatsSelectionViews;
@property (strong, nullable) NSScrubberSelectionStyle *selectionBackgroundStyle;
@property (strong, nullable) NSScrubberSelectionStyle *selectionOverlayStyle;
@property BOOL showsArrowButtons;
@property BOOL showsAdditionalContentIndicators;
@property (copy, nullable) NSColor *backgroundColor;
@property (strong, nullable) NSView *backgroundView;
- (instancetype)initWithFrame:(NSRect)frameRect;
- (void)reloadData;
- (void)performSequentialBatchUpdates:(void (^)(void))updateBlock;
- (void)insertItemsAtIndexes:(NSIndexSet *)indexes;
- (void)removeItemsAtIndexes:(NSIndexSet *)indexes;
- (void)reloadItemsAtIndexes:(NSIndexSet *)indexes;
- (void)moveItemAtIndex:(NSInteger)oldIndex toIndex:(NSInteger)newIndex;
- (void)scrollItemAtIndex:(NSInteger)index toAlignment:(NSScrubberAlignment)alignment;
- (nullable __kindof NSScrubberItemView *)itemViewForItemAtIndex:(NSInteger)index;
- (void)registerClass:(nullable Class)itemViewClass forItemIdentifier:(NSUserInterfaceItemIdentifier)itemIdentifier;
- (void)registerNib:(nullable NSNib *)nib forItemIdentifier:(NSUserInterfaceItemIdentifier)itemIdentifier;
- (nullable __kindof NSScrubberItemView *)makeItemWithIdentifier:(NSUserInterfaceItemIdentifier)itemIdentifier owner:(nullable id)owner;
@end

NS_ASSUME_NONNULL_END
