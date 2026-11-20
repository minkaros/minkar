// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSCollectionView.h>
#import <AppKit/NSCollectionViewLayout.h>
#import <AppKit/NSScrollView.h>
#import <Foundation/Foundation.h>

NSCollectionViewSupplementaryElementKind const NSCollectionElementKindSectionHeader = @"UICollectionElementKindSectionHeader";
NSCollectionViewSupplementaryElementKind const NSCollectionElementKindSectionFooter = @"UICollectionElementKindSectionFooter";
NSCollectionViewSupplementaryElementKind const NSCollectionElementKindInterItemGapIndicator = @"NSCollectionElementKindInterItemGapIndicator";

@interface NSCollectionView (NSCollectionViewLayoutPrivate)
- (void)_layoutDidInvalidate;
@end

@implementation NSCollectionViewLayoutAttributes {
    NSRect _frame;
    CGFloat _alpha;
    NSInteger _zIndex;
    BOOL _hidden;
    NSIndexPath *_indexPath;
    NSCollectionElementCategory _category;
    NSString *_kind;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _alpha = 1;
    return self;
}

+ (instancetype)_attributesWithCategory:(NSCollectionElementCategory)category kind:(NSString *)kind indexPath:(NSIndexPath *)indexPath {
    NSCollectionViewLayoutAttributes *result = [[[self alloc] init] autorelease];
    result->_category = category;
    result->_kind = [kind copy];
    result->_indexPath = [indexPath retain];
    return result;
}

+ (instancetype)layoutAttributesForItemWithIndexPath:(NSIndexPath *)indexPath {
    return [self _attributesWithCategory:NSCollectionElementCategoryItem kind:nil indexPath:indexPath];
}

+ (instancetype)layoutAttributesForInterItemGapBeforeIndexPath:(NSIndexPath *)indexPath {
    return [self _attributesWithCategory:NSCollectionElementCategoryInterItemGap kind:nil indexPath:indexPath];
}

+ (instancetype)layoutAttributesForSupplementaryViewOfKind:(NSString *)kind withIndexPath:(NSIndexPath *)indexPath {
    return [self _attributesWithCategory:NSCollectionElementCategorySupplementaryView kind:kind indexPath:indexPath];
}

+ (instancetype)layoutAttributesForDecorationViewOfKind:(NSString *)kind withIndexPath:(NSIndexPath *)indexPath {
    return [self _attributesWithCategory:NSCollectionElementCategoryDecorationView kind:kind indexPath:indexPath];
}

- (void)dealloc {
    [_indexPath release];
    [_kind release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    NSCollectionViewLayoutAttributes *copy = [[[self class] allocWithZone:zone] init];
    copy->_frame = _frame;
    copy->_alpha = _alpha;
    copy->_zIndex = _zIndex;
    copy->_hidden = _hidden;
    copy->_indexPath = [_indexPath retain];
    copy->_category = _category;
    copy->_kind = [_kind copy];
    return copy;
}

- (NSRect)frame { return _frame; }
- (void)setFrame:(NSRect)frame { _frame = frame; }
- (NSSize)size { return _frame.size; }
- (void)setSize:(NSSize)size { _frame.size = size; }
- (CGFloat)alpha { return _alpha; }
- (void)setAlpha:(CGFloat)alpha { _alpha = alpha; }
- (NSInteger)zIndex { return _zIndex; }
- (void)setZIndex:(NSInteger)z { _zIndex = z; }
- (BOOL)isHidden { return _hidden; }
- (void)setHidden:(BOOL)hidden { _hidden = hidden; }
- (NSIndexPath *)indexPath { return _indexPath; }
- (void)setIndexPath:(NSIndexPath *)indexPath { [indexPath retain]; [_indexPath release]; _indexPath = indexPath; }
- (NSCollectionElementCategory)representedElementCategory { return _category; }
- (NSString *)representedElementKind { return _kind; }

@end

@implementation NSCollectionViewLayoutInvalidationContext {
@package
    BOOL _everything, _counts;
    NSMutableSet *_items;
    NSPoint _offsetAdjustment;
    NSSize _sizeAdjustment;
}

- (void)dealloc {
    [_items release];
    [super dealloc];
}

- (BOOL)invalidateEverything { return _everything; }
- (BOOL)invalidateDataSourceCounts { return _counts; }

- (void)invalidateItemsAtIndexPaths:(NSSet *)indexPaths {
    if (_items == nil)
        _items = [[NSMutableSet alloc] init];
    [_items unionSet:indexPaths];
}

- (NSSet *)invalidatedItemIndexPaths { return _items; }
- (NSPoint)contentOffsetAdjustment { return _offsetAdjustment; }
- (void)setContentOffsetAdjustment:(NSPoint)p { _offsetAdjustment = p; }
- (NSSize)contentSizeAdjustment { return _sizeAdjustment; }
- (void)setContentSizeAdjustment:(NSSize)s { _sizeAdjustment = s; }

@end

@implementation NSCollectionViewFlowLayoutInvalidationContext {
    BOOL _delegateMetrics, _attributes;
}

- (BOOL)invalidateFlowLayoutDelegateMetrics { return _delegateMetrics; }
- (void)setInvalidateFlowLayoutDelegateMetrics:(BOOL)v { _delegateMetrics = v; }
- (BOOL)invalidateFlowLayoutAttributes { return _attributes; }
- (void)setInvalidateFlowLayoutAttributes:(BOOL)v { _attributes = v; }

@end

@implementation NSCollectionViewLayout {
@package
    NSCollectionView *_collectionView;
}

+ (Class)layoutAttributesClass { return [NSCollectionViewLayoutAttributes class]; }
+ (Class)invalidationContextClass { return [NSCollectionViewLayoutInvalidationContext class]; }

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super init];
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (NSCollectionView *)collectionView { return _collectionView; }
- (void)_setCollectionView:(NSCollectionView *)view { _collectionView = view; }

- (void)invalidateLayout {
    NSCollectionViewLayoutInvalidationContext *context = [[[[[self class] invalidationContextClass] alloc] init] autorelease];
    context->_everything = YES;
    context->_counts = YES;
    [self invalidateLayoutWithContext:context];
}

- (void)invalidateLayoutWithContext:(NSCollectionViewLayoutInvalidationContext *)context {
    [_collectionView _layoutDidInvalidate];
}

- (void)prepareLayout {
}

- (NSArray *)layoutAttributesForElementsInRect:(NSRect)rect {
    return [NSArray array];
}

- (NSCollectionViewLayoutAttributes *)layoutAttributesForItemAtIndexPath:(NSIndexPath *)indexPath {
    return nil;
}

- (NSCollectionViewLayoutAttributes *)layoutAttributesForSupplementaryViewOfKind:(NSString *)kind atIndexPath:(NSIndexPath *)indexPath {
    return nil;
}

- (BOOL)shouldInvalidateLayoutForBoundsChange:(NSRect)newBounds {
    return NO;
}

- (NSSize)collectionViewContentSize {
    return NSZeroSize;
}

@end

/* As measured on the reference: lines of items (rows scrolling
   vertically, columns horizontally) inside each section's insets, as many
   in a line as fit with the minimum spacing and the space left spread
   between them; the last line spaced as a full one; lines apart by the
   minimum line spacing; a section's header before it and footer after.
   Sizes from the delegate (collectionView:layout:sizeForItemAtIndexPath:)
   or the layout's. */
@implementation NSCollectionViewFlowLayout {
    CGFloat _minimumLineSpacing, _minimumInteritemSpacing;
    NSSize _itemSize, _estimatedItemSize, _headerReferenceSize, _footerReferenceSize;
    NSCollectionViewScrollDirection _scrollDirection;
    NSEdgeInsets _sectionInset;
    BOOL _headersPin, _footersPin;
    NSMutableDictionary *_itemAttributes;      /* index path: attributes */
    NSMutableArray *_supplementaryAttributes;
    NSSize _contentSize;
    BOOL _prepared;
}

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _minimumLineSpacing = 10;
        _minimumInteritemSpacing = 10;
        _itemSize = NSMakeSize(50, 50);
    }
    return self;
}

- (void)dealloc {
    [_itemAttributes release];
    [_supplementaryAttributes release];
    [super dealloc];
}

#define FLOW_PROPERTY(type, getter, setter, ivar) \
- (type)getter { return ivar; } \
- (void)setter:(type)value { ivar = value; [self invalidateLayout]; }

FLOW_PROPERTY(CGFloat, minimumLineSpacing, setMinimumLineSpacing, _minimumLineSpacing)
FLOW_PROPERTY(CGFloat, minimumInteritemSpacing, setMinimumInteritemSpacing, _minimumInteritemSpacing)
FLOW_PROPERTY(NSSize, itemSize, setItemSize, _itemSize)
FLOW_PROPERTY(NSSize, estimatedItemSize, setEstimatedItemSize, _estimatedItemSize)
FLOW_PROPERTY(NSCollectionViewScrollDirection, scrollDirection, setScrollDirection, _scrollDirection)
FLOW_PROPERTY(NSSize, headerReferenceSize, setHeaderReferenceSize, _headerReferenceSize)
FLOW_PROPERTY(NSSize, footerReferenceSize, setFooterReferenceSize, _footerReferenceSize)
FLOW_PROPERTY(NSEdgeInsets, sectionInset, setSectionInset, _sectionInset)
FLOW_PROPERTY(BOOL, sectionHeadersPinToVisibleBounds, setSectionHeadersPinToVisibleBounds, _headersPin)
FLOW_PROPERTY(BOOL, sectionFootersPinToVisibleBounds, setSectionFootersPinToVisibleBounds, _footersPin)

+ (Class)invalidationContextClass {
    return [NSCollectionViewFlowLayoutInvalidationContext class];
}

- (void)invalidateLayoutWithContext:(NSCollectionViewLayoutInvalidationContext *)context {
    _prepared = NO;
    [super invalidateLayoutWithContext:context];
}

/* The space the collection view gives: its scroll view's visible size,
   or its own. */
- (NSSize)_viewportSize {
    NSCollectionView *view = [self collectionView];
    NSScrollView *scrollView = [view enclosingScrollView];

    if (scrollView != nil)
        return [scrollView contentSize];
    return [view bounds].size;
}

- (NSSize)_sizeForItemAtIndexPath:(NSIndexPath *)indexPath {
    NSCollectionView *view = [self collectionView];
    id delegate = [view delegate];

    if ([delegate respondsToSelector:@selector(collectionView:layout:sizeForItemAtIndexPath:)])
        return [delegate collectionView:view layout:self sizeForItemAtIndexPath:indexPath];
    return _itemSize;
}

- (void)prepareLayout {
    NSCollectionView *view = [self collectionView];
    BOOL vertical = (_scrollDirection == NSCollectionViewScrollDirectionVertical);
    NSSize viewport = [self _viewportSize];
    NSInteger sections = [view numberOfSections];
    CGFloat along = 0;      /* where the next section starts, down or across */

    [_itemAttributes release];
    _itemAttributes = [[NSMutableDictionary alloc] init];
    [_supplementaryAttributes release];
    _supplementaryAttributes = [[NSMutableArray alloc] init];

    for (NSInteger section = 0; section < sections; section++) {
        NSInteger count = [view numberOfItemsInSection:section];
        NSIndexPath *sectionPath = [NSIndexPath indexPathForItem:0 inSection:section];
        CGFloat headerExtent = vertical ? _headerReferenceSize.height : _headerReferenceSize.width;
        CGFloat footerExtent = vertical ? _footerReferenceSize.height : _footerReferenceSize.width;
        CGFloat leadInset = vertical ? _sectionInset.top : _sectionInset.left;
        CGFloat trailInset = vertical ? _sectionInset.bottom : _sectionInset.right;
        CGFloat crossStart = vertical ? _sectionInset.left : _sectionInset.top;
        CGFloat available = (vertical ? viewport.width - _sectionInset.left - _sectionInset.right
                                      : viewport.height - _sectionInset.top - _sectionInset.bottom);
        NSMutableArray *sizes = [NSMutableArray arrayWithCapacity:count];
        BOOL uniform = YES;
        NSSize first = NSZeroSize;

        if (headerExtent > 0) {
            NSCollectionViewLayoutAttributes *header = [[[self class] layoutAttributesClass] layoutAttributesForSupplementaryViewOfKind:NSCollectionElementKindSectionHeader withIndexPath:sectionPath];
            [header setFrame:vertical ? NSMakeRect(0, along, viewport.width, headerExtent) : NSMakeRect(along, 0, headerExtent, viewport.height)];
            [_supplementaryAttributes addObject:header];
            along += headerExtent;
        }
        along += leadInset;

        for (NSInteger item = 0; item < count; item++) {
            NSSize size = [self _sizeForItemAtIndexPath:[NSIndexPath indexPathForItem:item inSection:section]];
            if (item == 0)
                first = size;
            else if (!NSEqualSizes(size, first))
                uniform = NO;
            [sizes addObject:[NSValue valueWithSize:size]];
        }

        /* A full line's count and spacing when every item is the same
           size: the last line is spaced the same. */
        CGFloat itemExtent = vertical ? first.width : first.height;
        NSInteger perLine = (itemExtent + _minimumInteritemSpacing > 0) ? (NSInteger)floor((available + _minimumInteritemSpacing) / (itemExtent + _minimumInteritemSpacing)) : count;
        if (perLine < 1)
            perLine = 1;
        CGFloat uniformSpacing = (perLine > 1) ? (available - perLine * itemExtent) / (perLine - 1) : 0;

        NSInteger item = 0;
        BOOL firstLine = YES;
        while (item < count) {
            NSInteger lineCount = 0;
            CGFloat used = 0, lineCross = 0;

            /* The items of this line. */
            if (uniform)
                lineCount = MIN(perLine, count - item);
            else {
                while (item + lineCount < count) {
                    NSSize size = [[sizes objectAtIndex:item + lineCount] sizeValue];
                    CGFloat extent = vertical ? size.width : size.height;
                    CGFloat next = used + (lineCount > 0 ? _minimumInteritemSpacing : 0) + extent;
                    if (lineCount > 0 && next > available)
                        break;
                    used = next;
                    lineCount++;
                }
            }
            CGFloat spacing = uniformSpacing;
            if (!uniform) {
                CGFloat extents = 0;
                for (NSInteger i = 0; i < lineCount; i++) {
                    NSSize size = [[sizes objectAtIndex:item + i] sizeValue];
                    extents += vertical ? size.width : size.height;
                }
                BOOL last = (item + lineCount >= count);
                spacing = (lineCount > 1 && !last) ? (available - extents) / (lineCount - 1) : _minimumInteritemSpacing;
            }
            for (NSInteger i = 0; i < lineCount; i++) {
                NSSize size = [[sizes objectAtIndex:item + i] sizeValue];
                lineCross = MAX(lineCross, vertical ? size.height : size.width);
            }
            if (!firstLine)
                along += _minimumLineSpacing;
            CGFloat cross = crossStart;
            for (NSInteger i = 0; i < lineCount; i++) {
                NSIndexPath *path = [NSIndexPath indexPathForItem:item + i inSection:section];
                NSSize size = [[sizes objectAtIndex:item + i] sizeValue];
                NSCollectionViewLayoutAttributes *attributes = [[[self class] layoutAttributesClass] layoutAttributesForItemWithIndexPath:path];
                [attributes setFrame:vertical ? NSMakeRect(cross, along, size.width, size.height) : NSMakeRect(along, cross, size.width, size.height)];
                [_itemAttributes setObject:attributes forKey:path];
                cross += (vertical ? size.width : size.height) + spacing;
            }
            along += lineCross;
            item += lineCount;
            firstLine = NO;
        }
        along += trailInset;
        if (footerExtent > 0) {
            NSCollectionViewLayoutAttributes *footer = [[[self class] layoutAttributesClass] layoutAttributesForSupplementaryViewOfKind:NSCollectionElementKindSectionFooter withIndexPath:sectionPath];
            [footer setFrame:vertical ? NSMakeRect(0, along, viewport.width, footerExtent) : NSMakeRect(along, 0, footerExtent, viewport.height)];
            [_supplementaryAttributes addObject:footer];
            along += footerExtent;
        }
    }
    /* The viewport's width and the sections' height (or across). */
    _contentSize = vertical ? NSMakeSize(viewport.width, along) : NSMakeSize(along, viewport.height);
    if (sections == 0)
        _contentSize = NSZeroSize;
    _prepared = YES;
}

- (void)_prepareIfNeeded {
    if (!_prepared && [self collectionView] != nil)
        [self prepareLayout];
}

- (NSSize)collectionViewContentSize {
    [self _prepareIfNeeded];
    return [self collectionView] != nil ? _contentSize : NSZeroSize;
}

- (NSCollectionViewLayoutAttributes *)layoutAttributesForItemAtIndexPath:(NSIndexPath *)indexPath {
    [self _prepareIfNeeded];
    return [[[_itemAttributes objectForKey:indexPath] copy] autorelease];
}

- (NSCollectionViewLayoutAttributes *)layoutAttributesForSupplementaryViewOfKind:(NSString *)kind atIndexPath:(NSIndexPath *)indexPath {
    [self _prepareIfNeeded];
    for (NSCollectionViewLayoutAttributes *attributes in _supplementaryAttributes)
        if ([[attributes representedElementKind] isEqualToString:kind] && [[attributes indexPath] section] == [indexPath section])
            return [[attributes copy] autorelease];
    return nil;
}

- (NSArray *)layoutAttributesForElementsInRect:(NSRect)rect {
    NSMutableArray *result = [NSMutableArray array];

    [self _prepareIfNeeded];
    for (NSCollectionViewLayoutAttributes *attributes in [_itemAttributes allValues])
        if (NSIntersectsRect([attributes frame], rect))
            [result addObject:[[attributes copy] autorelease]];
    for (NSCollectionViewLayoutAttributes *attributes in _supplementaryAttributes)
        if (NSIntersectsRect([attributes frame], rect))
            [result addObject:[[attributes copy] autorelease]];
    return result;
}

/* A change of the width (vertical) or height (horizontal) moves lines. */
- (BOOL)shouldInvalidateLayoutForBoundsChange:(NSRect)newBounds {
    NSSize viewport = [self _viewportSize];
    return (_scrollDirection == NSCollectionViewScrollDirectionVertical) ? NSWidth(newBounds) != viewport.width : NSHeight(newBounds) != viewport.height;
}

@end
