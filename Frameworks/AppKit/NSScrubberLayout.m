// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSScrubber.h>
#import <Foundation/Foundation.h>

@interface NSScrubber (NSScrubberLayoutPrivate)
- (void)_layoutDidInvalidate;
@end

@implementation NSScrubberLayoutAttributes

@synthesize itemIndex = _itemIndex, frame = _frame, alpha = _alpha;

+ (instancetype)layoutAttributesForItemAtIndex:(NSInteger)index {
    NSScrubberLayoutAttributes *result = [[[self alloc] init] autorelease];
    result.itemIndex = index;
    return result;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _alpha = 1;
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    NSScrubberLayoutAttributes *copy = [[[self class] allocWithZone:zone] init];
    copy->_itemIndex = _itemIndex;
    copy->_frame = _frame;
    copy->_alpha = _alpha;
    return copy;
}

- (BOOL)isEqual:(id)other {
    if (![other isKindOfClass:[NSScrubberLayoutAttributes class]])
        return NO;
    NSScrubberLayoutAttributes *o = other;
    return o->_itemIndex == _itemIndex && NSEqualRects(o->_frame, _frame) && o->_alpha == _alpha;
}

- (NSUInteger)hash {
    return (NSUInteger)_itemIndex;
}

@end

@implementation NSScrubberLayout {
@package
    NSScrubber *_scrubber;
}

+ (Class)layoutAttributesClass {
    return [NSScrubberLayoutAttributes class];
}

- (instancetype)init {
    return [super init];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super init];
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (NSScrubber *)scrubber {
    return _scrubber;
}

- (void)_setScrubber:(NSScrubber *)scrubber {
    _scrubber = scrubber;
}

/* The scrubber's bounds (its whole width is shown, scrolled by its own
   offset). */
- (NSRect)visibleRect {
    return (_scrubber != nil) ? [_scrubber visibleRect].size.width > 0 ? [_scrubber visibleRect] : [_scrubber bounds] : NSZeroRect;
}

- (void)invalidateLayout {
    [_scrubber _layoutDidInvalidate];
}

- (void)prepareLayout {
}

/* No width until laid out, as high as the scrubber, as on the reference. */
- (NSSize)scrubberContentSize {
    return NSMakeSize(0, NSHeight([self visibleRect]));
}

- (NSScrubberLayoutAttributes *)layoutAttributesForItemAtIndex:(NSInteger)index {
    return nil;
}

- (NSSet *)layoutAttributesForItemsInRect:(NSRect)rect {
    NSMutableSet *result = [NSMutableSet set];
    NSInteger count = [_scrubber numberOfItems];

    for (NSInteger i = 0; i < count; i++) {
        NSScrubberLayoutAttributes *attributes = [self layoutAttributesForItemAtIndex:i];
        if (attributes != nil && NSIntersectsRect([attributes frame], rect))
            [result addObject:attributes];
    }
    return result;
}

- (BOOL)shouldInvalidateLayoutForSelectionChange { return NO; }
- (BOOL)shouldInvalidateLayoutForHighlightChange { return NO; }
- (BOOL)shouldInvalidateLayoutForChangeFromVisibleRect:(NSRect)from toVisibleRect:(NSRect)to { return NO; }
- (BOOL)automaticallyMirrorsInRightToLeftLayout { return YES; }

@end

@implementation NSScrubberFlowLayout {
    CGFloat _itemSpacing;
    NSSize _itemSize;
    NSMutableArray *_frames;
}

/* 50 by 30, no spacing, as on the reference. */
- (instancetype)init {
    if ((self = [super init]) != nil)
        _itemSize = NSMakeSize(50, 30);
    return self;
}

- (void)dealloc {
    [_frames release];
    [super dealloc];
}

- (CGFloat)itemSpacing { return _itemSpacing; }
- (void)setItemSpacing:(CGFloat)value { _itemSpacing = value; [self invalidateLayout]; }
- (NSSize)itemSize { return _itemSize; }
- (void)setItemSize:(NSSize)value { _itemSize = value; [self invalidateLayout]; }

- (void)invalidateLayout {
    [_frames release];
    _frames = nil;
    [super invalidateLayout];
}

- (void)invalidateLayoutForItemsAtIndexes:(NSIndexSet *)indexes {
    [self invalidateLayout];
}

/* Each item after the one before it and the spacing, its size the
   delegate's (scrubber:layout:sizeForItemAtIndex:) or the layout's. */
- (void)prepareLayout {
    NSScrubber *scrubber = [self scrubber];
    NSInteger count = [scrubber numberOfItems];
    id delegate = [scrubber delegate];
    CGFloat x = 0;

    [_frames release];
    _frames = [[NSMutableArray alloc] initWithCapacity:count];
    for (NSInteger i = 0; i < count; i++) {
        NSSize size = _itemSize;
        if ([delegate respondsToSelector:@selector(scrubber:layout:sizeForItemAtIndex:)])
            size = [delegate scrubber:scrubber layout:self sizeForItemAtIndex:i];
        [_frames addObject:[NSValue valueWithRect:NSMakeRect(x, 0, size.width, size.height)]];
        x += size.width + _itemSpacing;
    }
}

- (NSRect)_frameAtIndex:(NSInteger)index {
    if (_frames == nil || (NSInteger)[_frames count] != [[self scrubber] numberOfItems])
        [self prepareLayout];
    if (index < 0 || index >= (NSInteger)[_frames count]) {
        /* Not laid out by a scrubber: where it would be with the layout's
           size. */
        return NSMakeRect(index * (_itemSize.width + _itemSpacing), 0, _itemSize.width, _itemSize.height);
    }
    return [[_frames objectAtIndex:index] rectValue];
}

- (NSSize)scrubberContentSize {
    NSInteger count = [[self scrubber] numberOfItems];
    if (count == 0)
        return [super scrubberContentSize];
    return NSMakeSize(NSMaxX([self _frameAtIndex:count - 1]), NSHeight([self visibleRect]));
}

- (NSScrubberLayoutAttributes *)layoutAttributesForItemAtIndex:(NSInteger)index {
    NSScrubberLayoutAttributes *result = [[[self class] layoutAttributesClass] layoutAttributesForItemAtIndex:index];
    [result setFrame:[self _frameAtIndex:index]];
    return result;
}

@end

@implementation NSScrubberProportionalLayout {
    NSInteger _numberOfVisibleItems;
}

- (instancetype)initWithNumberOfVisibleItems:(NSInteger)count {
    if ((self = [super init]) != nil)
        _numberOfVisibleItems = count;
    return self;
}

- (instancetype)init {
    return [self initWithNumberOfVisibleItems:1];
}

- (NSInteger)numberOfVisibleItems { return _numberOfVisibleItems; }
- (void)setNumberOfVisibleItems:(NSInteger)count { _numberOfVisibleItems = count; [self invalidateLayout]; }

- (CGFloat)_itemWidth {
    return (_numberOfVisibleItems > 0) ? NSWidth([self visibleRect]) / _numberOfVisibleItems : 0;
}

- (NSSize)scrubberContentSize {
    return NSMakeSize([self _itemWidth] * [[self scrubber] numberOfItems], NSHeight([self visibleRect]));
}

- (NSScrubberLayoutAttributes *)layoutAttributesForItemAtIndex:(NSInteger)index {
    NSScrubberLayoutAttributes *result = [[[self class] layoutAttributesClass] layoutAttributesForItemAtIndex:index];
    CGFloat width = [self _itemWidth];
    [result setFrame:NSMakeRect(index * width, 0, width, NSHeight([self visibleRect]))];
    return result;
}

/* Its items' widths follow the visible width. */
- (BOOL)shouldInvalidateLayoutForChangeFromVisibleRect:(NSRect)from toVisibleRect:(NSRect)to {
    return NSWidth(from) != NSWidth(to);
}

@end
