// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>

@class NSScrubber, NSIndexSet, NSSet;

NS_ASSUME_NONNULL_BEGIN

/* Where a scrubber's item goes, and how opaque it is. */
@interface NSScrubberLayoutAttributes : NSObject <NSCopying>
+ (instancetype)layoutAttributesForItemAtIndex:(NSInteger)index;
@property NSInteger itemIndex;
@property NSRect frame;
@property CGFloat alpha;
@end

/* How a scrubber lays out its items. */
@interface NSScrubberLayout : NSObject <NSCoding>
@property (class, readonly) Class layoutAttributesClass;
@property (readonly, assign, nullable) NSScrubber *scrubber;
@property (readonly) NSRect visibleRect;
- (instancetype)init;
- (nullable instancetype)initWithCoder:(NSCoder *)coder;
- (void)invalidateLayout;
- (void)prepareLayout;
@property (readonly) NSSize scrubberContentSize;
- (nullable __kindof NSScrubberLayoutAttributes *)layoutAttributesForItemAtIndex:(NSInteger)index;
- (NSSet<__kindof NSScrubberLayoutAttributes *> *)layoutAttributesForItemsInRect:(NSRect)rect;
@property (readonly) BOOL shouldInvalidateLayoutForSelectionChange;
@property (readonly) BOOL shouldInvalidateLayoutForHighlightChange;
- (BOOL)shouldInvalidateLayoutForChangeFromVisibleRect:(NSRect)fromVisibleRect toVisibleRect:(NSRect)toVisibleRect;
@property (readonly) BOOL automaticallyMirrorsInRightToLeftLayout;
@end

@protocol NSScrubberFlowLayoutDelegate;

/* Items one after another, each its own size (the delegate's) or the
   layout's. */
@interface NSScrubberFlowLayout : NSScrubberLayout
@property CGFloat itemSpacing;
@property NSSize itemSize;
- (void)invalidateLayoutForItemsAtIndexes:(NSIndexSet *)invalidItemIndexes;
@end

/* As many items as asked for in the visible width, all as wide. */
@interface NSScrubberProportionalLayout : NSScrubberLayout
- (instancetype)initWithNumberOfVisibleItems:(NSInteger)numberOfVisibleItems;
@property NSInteger numberOfVisibleItems;
@end

NS_ASSUME_NONNULL_END
