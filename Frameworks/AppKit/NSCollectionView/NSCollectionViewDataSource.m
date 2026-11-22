// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSCollectionView.h>
#import <AppKit/NSScrollView.h>
#import <AppKit/NSClipView.h>
#import <AppKit/NSNib.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

@interface NSCollectionViewLayout (NSCollectionViewPrivate)
- (void)_setCollectionView:(NSCollectionView *)view;
@end

@interface NSCollectionViewItem (NSCollectionViewPrivate)
- (void)_setCollectionView:(NSCollectionView *)view;
@end

/* What a collection view with a data source keeps. */
@interface _NSCollectionViewDataSourceState : NSObject {
@public
    id _dataSource, _delegate;
    NSCollectionViewLayout *_layout;
    NSMutableDictionary *_registered;   /* identifier: class or nib */
    NSMutableDictionary *_items;        /* index path: item shown */
    NSMutableArray *_sectionCounts;
    NSSet *_selection;
    BOOL _tiling;
}
@end

@implementation _NSCollectionViewDataSourceState

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _registered = [[NSMutableDictionary alloc] init];
        _items = [[NSMutableDictionary alloc] init];
        _sectionCounts = [[NSMutableArray alloc] init];
        _selection = [[NSSet alloc] init];
    }
    return self;
}

- (void)dealloc {
    [_layout _setCollectionView:nil];
    [_layout release];
    [_registered release];
    [_items release];
    [_sectionCounts release];
    [_selection release];
    [super dealloc];
}

@end

static char stateKey;

static _NSCollectionViewDataSourceState *stateOf(NSCollectionView *self) {
    _NSCollectionViewDataSourceState *state = objc_getAssociatedObject(self, &stateKey);

    if (state == nil) {
        state = [[[_NSCollectionViewDataSourceState alloc] init] autorelease];
        objc_setAssociatedObject(self, &stateKey, state, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    return state;
}

@implementation NSIndexPath (NSCollectionViewAdditions)

+ (NSIndexPath *)indexPathForItem:(NSInteger)item inSection:(NSInteger)section {
    NSUInteger indexes[2] = { (NSUInteger)section, (NSUInteger)item };
    return [self indexPathWithIndexes:indexes length:2];
}

- (NSInteger)item {
    return ([self length] > 1) ? (NSInteger)[self indexAtPosition:1] : NSNotFound;
}

- (NSInteger)section {
    return ([self length] > 0) ? (NSInteger)[self indexAtPosition:0] : NSNotFound;
}

@end

@implementation NSCollectionView (NSCollectionViewDataSourceItems)

- (id<NSCollectionViewDataSource>)dataSource { return stateOf(self)->_dataSource; }
- (void)setDataSource:(id<NSCollectionViewDataSource>)source { stateOf(self)->_dataSource = source; [self reloadData]; }
- (id<NSCollectionViewDelegate>)delegate { return stateOf(self)->_delegate; }
- (void)setDelegate:(id<NSCollectionViewDelegate>)delegate { stateOf(self)->_delegate = delegate; [stateOf(self)->_layout invalidateLayout]; }
- (NSCollectionViewLayout *)collectionViewLayout { return stateOf(self)->_layout; }

- (void)setCollectionViewLayout:(NSCollectionViewLayout *)layout {
    _NSCollectionViewDataSourceState *state = stateOf(self);

    if (layout == state->_layout)
        return;
    [state->_layout _setCollectionView:nil];
    [layout retain];
    [state->_layout release];
    state->_layout = layout;
    [layout _setCollectionView:self];
    [layout invalidateLayout];
}

- (NSInteger)numberOfSections {
    return (NSInteger)[stateOf(self)->_sectionCounts count];
}

- (NSInteger)numberOfItemsInSection:(NSInteger)section {
    NSArray *counts = stateOf(self)->_sectionCounts;
    return (section >= 0 && section < (NSInteger)[counts count]) ? [[counts objectAtIndex:section] integerValue] : 0;
}

- (void)registerClass:(Class)itemClass forItemWithIdentifier:(NSUserInterfaceItemIdentifier)identifier {
    if (itemClass != Nil)
        [stateOf(self)->_registered setObject:itemClass forKey:identifier];
    else
        [stateOf(self)->_registered removeObjectForKey:identifier];
}

- (void)registerNib:(NSNib *)nib forItemWithIdentifier:(NSUserInterfaceItemIdentifier)identifier {
    if (nib != nil)
        [stateOf(self)->_registered setObject:nib forKey:identifier];
    else
        [stateOf(self)->_registered removeObjectForKey:identifier];
}

/* An item of a registered class (its view made by loadView), or the
   first item in a registered nib; a nib named as the identifier when
   nothing is registered. */
- (NSCollectionViewItem *)makeItemWithIdentifier:(NSUserInterfaceItemIdentifier)identifier forIndexPath:(NSIndexPath *)indexPath {
    id kind = [stateOf(self)->_registered objectForKey:identifier];
    NSCollectionViewItem *item = nil;

    if (kind == nil)
        kind = [[[NSNib alloc] initWithNibNamed:identifier bundle:nil] autorelease];
    if ([kind isKindOfClass:[NSNib class]]) {
        NSArray *objects = nil;
        if ([(NSNib *)kind instantiateWithOwner:nil topLevelObjects:&objects])
            for (id object in objects)
                if ([object isKindOfClass:[NSCollectionViewItem class]]) {
                    item = object;
                    break;
                }
    } else if (kind != nil)
        item = [[[(Class)kind alloc] initWithNibName:nil bundle:nil] autorelease];
    if (item == nil)
        [NSException raise:NSInternalInconsistencyException format:@"-[NSCollectionView makeItemWithIdentifier:forIndexPath:] could not make an item with the identifier %@", identifier];
    [item _setCollectionView:self];
    /* Its view loaded, so its outlets (textField, imageView) are there
       for the data source to fill in, as on the reference. */
    [item view];
    return item;
}

- (NSCollectionViewItem *)itemAtIndexPath:(NSIndexPath *)indexPath {
    return [stateOf(self)->_items objectForKey:indexPath];
}

- (NSCollectionViewItem *)itemAtIndex:(NSUInteger)index {
    return [self itemAtIndexPath:[NSIndexPath indexPathForItem:(NSInteger)index inSection:0]];
}

- (NSIndexPath *)indexPathForItem:(NSCollectionViewItem *)item {
    NSDictionary *items = stateOf(self)->_items;
    for (NSIndexPath *path in items)
        if ([items objectForKey:path] == item)
            return path;
    return nil;
}

- (NSArray *)visibleItems {
    return [stateOf(self)->_items allValues];
}

- (NSSet *)indexPathsForVisibleItems {
    return [NSSet setWithArray:[stateOf(self)->_items allKeys]];
}

- (NSCollectionViewLayoutAttributes *)layoutAttributesForItemAtIndexPath:(NSIndexPath *)indexPath {
    return [stateOf(self)->_layout layoutAttributesForItemAtIndexPath:indexPath];
}

- (NSRect)frameForItemAtIndex:(NSUInteger)index {
    return [[self layoutAttributesForItemAtIndexPath:[NSIndexPath indexPathForItem:(NSInteger)index inSection:0]] frame];
}

- (NSSet *)selectionIndexPaths {
    return stateOf(self)->_selection;
}

- (void)setSelectionIndexPaths:(NSSet *)paths {
    _NSCollectionViewDataSourceState *state = stateOf(self);
    paths = [paths copy];
    [state->_selection release];
    state->_selection = (paths != nil) ? paths : [[NSSet alloc] init];
    for (NSIndexPath *path in state->_items)
        [[state->_items objectForKey:path] setSelected:[state->_selection containsObject:path]];
}

- (void)selectItemsAtIndexPaths:(NSSet *)paths scrollPosition:(NSCollectionViewScrollPosition)position {
    NSMutableSet *selection;

    if ([paths count] == 0)
        return;
    if ([self allowsMultipleSelection])
        selection = [NSMutableSet setWithSet:[self selectionIndexPaths]];
    else {
        selection = [NSMutableSet set];
        paths = [NSSet setWithObject:[paths anyObject]];
    }
    [selection unionSet:paths];
    [self setSelectionIndexPaths:selection];
    [self scrollToItemsAtIndexPaths:paths scrollPosition:position];
}

- (void)deselectItemsAtIndexPaths:(NSSet *)paths {
    NSMutableSet *selection = [NSMutableSet setWithSet:[self selectionIndexPaths]];

    [selection minusSet:paths];
    [self setSelectionIndexPaths:selection];
}

/* Minkar: the items' frames brought into view; the positions other than
   None are not told apart yet (each scrolls as little as it takes). */
- (void)scrollToItemsAtIndexPaths:(NSSet *)paths scrollPosition:(NSCollectionViewScrollPosition)position {
    NSRect target = NSZeroRect;

    if (position == NSCollectionViewScrollPositionNone)
        return;
    for (NSIndexPath *path in paths) {
        NSRect frame = [[self layoutAttributesForItemAtIndexPath:path] frame];
        target = NSIsEmptyRect(target) ? frame : NSUnionRect(target, frame);
    }
    if (!NSIsEmptyRect(target))
        [self scrollRectToVisible:target];
}

/* The counts asked again, the items made again. */
- (void)reloadData {
    _NSCollectionViewDataSourceState *state = stateOf(self);
    id source = state->_dataSource;
    NSInteger sections = 0;

    for (NSCollectionViewItem *item in [state->_items allValues])
        [[item view] removeFromSuperview];
    [state->_items removeAllObjects];
    [state->_sectionCounts removeAllObjects];
    if (source != nil)
        sections = [source respondsToSelector:@selector(numberOfSectionsInCollectionView:)] ? [source numberOfSectionsInCollectionView:self] : 1;
    for (NSInteger section = 0; section < sections; section++)
        [state->_sectionCounts addObject:[NSNumber numberWithInteger:[source collectionView:self numberOfItemsInSection:section]]];
    [state->_layout invalidateLayout];
    [self _tileItems];
}

- (void)_layoutDidInvalidate {
    [self _tileItems];
}

/* Sized to the layout's content (at least its scroll view's visible
   size), the items in view made and placed. */
- (void)_tileItems {
    _NSCollectionViewDataSourceState *state = stateOf(self);
    NSCollectionViewLayout *layout = state->_layout;
    id source = state->_dataSource;

    if (layout == nil || source == nil || state->_tiling)
        return;
    state->_tiling = YES;
    [layout prepareLayout];
    NSSize content = [layout collectionViewContentSize];
    NSScrollView *scrollView = [self enclosingScrollView];
    if (scrollView != nil) {
        NSSize visible = [scrollView contentSize];
        content.width = MAX(content.width, visible.width);
        content.height = MAX(content.height, visible.height);
    }
    if (!NSEqualSizes(content, [self frame].size) && (content.width > 0 || content.height > 0))
        [self setFrameSize:content];

    NSRect visibleRect = (scrollView != nil) ? [self visibleRect] : [self bounds];
    if (NSIsEmptyRect(visibleRect))
        visibleRect = [self bounds];
    NSMutableDictionary *shown = [NSMutableDictionary dictionary];
    for (NSCollectionViewLayoutAttributes *attributes in [layout layoutAttributesForElementsInRect:visibleRect]) {
        if ([attributes representedElementCategory] != NSCollectionElementCategoryItem)
            continue;
        NSIndexPath *path = [attributes indexPath];
        NSCollectionViewItem *item = [state->_items objectForKey:path];
        if (item == nil)
            item = [source collectionView:self itemForRepresentedObjectAtIndexPath:path];
        if (item == nil)
            continue;
        NSView *view = [item view];
        [view setFrame:[attributes frame]];
        [view setHidden:[attributes isHidden]];
        [view setAlphaValue:[attributes alpha]];
        if ([view superview] != self)
            [self addSubview:view];
        [item setSelected:[state->_selection containsObject:path]];
        [shown setObject:item forKey:path];
    }
    for (NSIndexPath *path in [state->_items allKeys])
        if ([shown objectForKey:path] == nil)
            [[[state->_items objectForKey:path] view] removeFromSuperview];
    [state->_items setDictionary:shown];
    state->_tiling = NO;
}

@end
