/* Copyright (c) 2009 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <AppKit/NSView.h>
#import <AppKit/NSCollectionViewItem.h>
#import <AppKit/NSCollectionViewLayout.h>

@class NSNib;
@protocol NSCollectionViewDataSource, NSCollectionViewDelegate;

typedef NS_OPTIONS(NSUInteger, NSCollectionViewScrollPosition) {
    NSCollectionViewScrollPositionNone = 0,
    NSCollectionViewScrollPositionTop = 1 << 0,
    NSCollectionViewScrollPositionCenteredVertically = 1 << 1,
    NSCollectionViewScrollPositionBottom = 1 << 2,
    NSCollectionViewScrollPositionNearestHorizontalEdge = 1 << 9,
    NSCollectionViewScrollPositionLeft = 1 << 3,
    NSCollectionViewScrollPositionCenteredHorizontally = 1 << 4,
    NSCollectionViewScrollPositionRight = 1 << 5,
    NSCollectionViewScrollPositionLeadingEdge = 1 << 6,
    NSCollectionViewScrollPositionTrailingEdge = 1 << 7,
    NSCollectionViewScrollPositionNearestVerticalEdge = 1 << 8,
};

@interface NSCollectionView : NSView {
    NSArray *_content;
    NSCollectionViewItem *_itemPrototype;
    BOOL _isSelectable;
    NSSize _minItemSize;
    NSSize _maxItemSize;
    NSUInteger _maxNumberOfRows;
    NSUInteger _maxNumberOfColumns;
    NSArray *_backgroundColors;
    BOOL _allowsMultipleSelection;
    NSIndexSet *_selectionIndexes;
}

- (NSArray *)content;
- (NSCollectionViewItem *)itemPrototype;

- (BOOL)isSelectable;

- (NSSize)minItemSize;
- (NSSize)maxItemSize;

- (NSUInteger)maxNumberOfRows;
- (NSUInteger)maxNumberOfColumns;

- (NSArray *)backgroundColors;
- (BOOL)allowsMultipleSelection;
- (NSIndexSet *)selectionIndexes;

- (void)setContent:(NSArray *)value;
- (void)setItemPrototype:(NSCollectionViewItem *)value;
- (void)setSelectable:(BOOL)value;

- (void)setMinItemSize:(NSSize)value;
- (void)setMaxItemSize:(NSSize)value;
- (void)setMaxNumberOfRows:(NSUInteger)value;
- (void)setMaxNumberOfColumns:(NSUInteger)value;

- (void)setBackgroundColors:(NSArray *)value;
- (void)setAllowsMultipleSelection:(BOOL)value;
- (void)setSelectionIndexes:(NSIndexSet *)value;

- (BOOL)isFirstResponder;
- (NSCollectionViewItem *)newItemForRepresentedObject:object;

/* Minkar: as properties too, for dot syntax. */
@property (getter=isSelectable) BOOL selectable;
@end

NS_ASSUME_NONNULL_BEGIN

/* The collection view with a data source (API 19): its items by section
   and index path, made from registered classes or nibs, placed by its
   layout. */
@interface NSCollectionView (NSCollectionViewDataSourceItems)
@property (nullable, assign) id<NSCollectionViewDataSource> dataSource;
@property (nullable, assign) id<NSCollectionViewDelegate> delegate;
@property (nullable, strong) NSCollectionViewLayout *collectionViewLayout;
@property (readonly) NSInteger numberOfSections;
- (NSInteger)numberOfItemsInSection:(NSInteger)section;
- (void)reloadData;
- (void)registerClass:(nullable Class)itemClass forItemWithIdentifier:(NSUserInterfaceItemIdentifier)identifier;
- (void)registerNib:(nullable NSNib *)nib forItemWithIdentifier:(NSUserInterfaceItemIdentifier)identifier;
- (__kindof NSCollectionViewItem *)makeItemWithIdentifier:(NSUserInterfaceItemIdentifier)identifier forIndexPath:(NSIndexPath *)indexPath;
- (nullable NSCollectionViewItem *)itemAtIndexPath:(NSIndexPath *)indexPath;
- (nullable NSCollectionViewItem *)itemAtIndex:(NSUInteger)index;
- (nullable NSIndexPath *)indexPathForItem:(NSCollectionViewItem *)item;
- (NSArray<NSCollectionViewItem *> *)visibleItems;
- (NSSet<NSIndexPath *> *)indexPathsForVisibleItems;
- (nullable NSCollectionViewLayoutAttributes *)layoutAttributesForItemAtIndexPath:(NSIndexPath *)indexPath;
- (NSRect)frameForItemAtIndex:(NSUInteger)index;
@property (copy) NSSet<NSIndexPath *> *selectionIndexPaths;
/* Selected or deselected by the program: the delegate is not told. */
- (void)selectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths scrollPosition:(NSCollectionViewScrollPosition)scrollPosition;
- (void)deselectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths;
- (void)scrollToItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths scrollPosition:(NSCollectionViewScrollPosition)scrollPosition;
@end

@protocol NSCollectionViewDataSource <NSObject>
@required
- (NSInteger)collectionView:(NSCollectionView *)collectionView numberOfItemsInSection:(NSInteger)section;
- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath;
@optional
- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView;
- (NSView *)collectionView:(NSCollectionView *)collectionView viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind atIndexPath:(NSIndexPath *)indexPath;
@end

@protocol NSCollectionViewDelegate <NSObject>
@optional
- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths;
- (void)collectionView:(NSCollectionView *)collectionView didDeselectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths;
@end

@protocol NSCollectionViewDelegateFlowLayout <NSCollectionViewDelegate>
@optional
- (NSSize)collectionView:(NSCollectionView *)collectionView layout:(NSCollectionViewLayout *)collectionViewLayout sizeForItemAtIndexPath:(NSIndexPath *)indexPath;
@end

/* An item's place: its section and its index in it. */
@interface NSIndexPath (NSCollectionViewAdditions)
+ (NSIndexPath *)indexPathForItem:(NSInteger)item inSection:(NSInteger)section;
@property (readonly) NSInteger item;
@property (readonly) NSInteger section;
@end

NS_ASSUME_NONNULL_END
