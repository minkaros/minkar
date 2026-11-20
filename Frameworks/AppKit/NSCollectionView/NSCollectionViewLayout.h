// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <Foundation/NSIndexPath.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSSet.h>
#import <AppKit/AppKitExport.h>

@class NSCollectionView;

NS_ASSUME_NONNULL_BEGIN

typedef NSString *NSCollectionViewSupplementaryElementKind;

typedef NS_ENUM(NSInteger, NSCollectionElementCategory) {
    NSCollectionElementCategoryItem,
    NSCollectionElementCategorySupplementaryView,
    NSCollectionElementCategoryDecorationView,
    NSCollectionElementCategoryInterItemGap
};

/* Where an element of a collection view goes. */
@interface NSCollectionViewLayoutAttributes : NSObject <NSCopying>
+ (instancetype)layoutAttributesForItemWithIndexPath:(NSIndexPath *)indexPath;
+ (instancetype)layoutAttributesForInterItemGapBeforeIndexPath:(NSIndexPath *)indexPath;
+ (instancetype)layoutAttributesForSupplementaryViewOfKind:(NSCollectionViewSupplementaryElementKind)elementKind withIndexPath:(NSIndexPath *)indexPath;
+ (instancetype)layoutAttributesForDecorationViewOfKind:(NSString *)decorationViewKind withIndexPath:(NSIndexPath *)indexPath;
@property NSRect frame;
@property NSSize size;
@property CGFloat alpha;
@property NSInteger zIndex;
@property (getter=isHidden) BOOL hidden;
@property (nullable, strong) NSIndexPath *indexPath;
@property (readonly) NSCollectionElementCategory representedElementCategory;
@property (nullable, readonly) NSString *representedElementKind;
@end

/* What to recompute when a layout is invalidated. */
@interface NSCollectionViewLayoutInvalidationContext : NSObject
@property (readonly) BOOL invalidateEverything;
@property (readonly) BOOL invalidateDataSourceCounts;
- (void)invalidateItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths;
@property (nullable, readonly) NSSet<NSIndexPath *> *invalidatedItemIndexPaths;
@property NSPoint contentOffsetAdjustment;
@property NSSize contentSizeAdjustment;
@end

/* How a collection view lays out its elements. */
@interface NSCollectionViewLayout : NSObject <NSCoding>
@property (nullable, readonly, assign) NSCollectionView *collectionView;
- (void)invalidateLayout;
- (void)invalidateLayoutWithContext:(NSCollectionViewLayoutInvalidationContext *)context;
@property (class, readonly) Class layoutAttributesClass;
@property (class, readonly) Class invalidationContextClass;
- (void)prepareLayout;
- (NSArray<__kindof NSCollectionViewLayoutAttributes *> *)layoutAttributesForElementsInRect:(NSRect)rect;
- (nullable NSCollectionViewLayoutAttributes *)layoutAttributesForItemAtIndexPath:(NSIndexPath *)indexPath;
- (nullable NSCollectionViewLayoutAttributes *)layoutAttributesForSupplementaryViewOfKind:(NSCollectionViewSupplementaryElementKind)elementKind atIndexPath:(NSIndexPath *)indexPath;
- (BOOL)shouldInvalidateLayoutForBoundsChange:(NSRect)newBounds;
@property (readonly) NSSize collectionViewContentSize;
@end

typedef NS_ENUM(NSInteger, NSCollectionViewScrollDirection) {
    NSCollectionViewScrollDirectionVertical,
    NSCollectionViewScrollDirectionHorizontal
};

/* Items in lines (rows when it scrolls vertically, columns when
   horizontally), as many in a line as fit with the minimum spacing, the
   space left spread between them. */
@interface NSCollectionViewFlowLayout : NSCollectionViewLayout
@property CGFloat minimumLineSpacing;
@property CGFloat minimumInteritemSpacing;
@property NSSize itemSize;
@property NSSize estimatedItemSize;
@property NSCollectionViewScrollDirection scrollDirection;
@property NSSize headerReferenceSize;
@property NSSize footerReferenceSize;
@property NSEdgeInsets sectionInset;
@property BOOL sectionHeadersPinToVisibleBounds;
@property BOOL sectionFootersPinToVisibleBounds;
@end

@interface NSCollectionViewFlowLayoutInvalidationContext : NSCollectionViewLayoutInvalidationContext
@property BOOL invalidateFlowLayoutDelegateMetrics;
@property BOOL invalidateFlowLayoutAttributes;
@end

APPKIT_EXPORT NSCollectionViewSupplementaryElementKind const NSCollectionElementKindSectionHeader;
APPKIT_EXPORT NSCollectionViewSupplementaryElementKind const NSCollectionElementKindSectionFooter;
APPKIT_EXPORT NSCollectionViewSupplementaryElementKind const NSCollectionElementKindInterItemGapIndicator;

NS_ASSUME_NONNULL_END
