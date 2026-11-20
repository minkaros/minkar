// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSScrubber.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSNib.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSEvent.h>
#import <Foundation/Foundation.h>

@interface NSScrubberLayout (NSScrubberPrivate)
- (void)_setScrubber:(NSScrubber *)scrubber;
@end

/* The defaults as on the reference: no items, none highlighted or
   selected (-1), fixed, unaligned, a flow layout, the control background
   color. Items are views the data source gives, placed by the layout,
   scrolled by an offset into the content. */
@implementation NSScrubber {
    id _dataSource, _delegate;
    NSScrubberLayout *_layout;
    NSInteger _numberOfItems, _highlightedIndex, _selectedIndex;
    NSScrubberMode _mode;
    NSScrubberAlignment _itemAlignment;
    BOOL _continuous, _floatsSelectionViews, _showsArrowButtons, _showsAdditionalContentIndicators;
    NSScrubberSelectionStyle *_selectionBackgroundStyle, *_selectionOverlayStyle;
    NSColor *_backgroundColor;
    NSView *_backgroundView;
    NSMutableDictionary *_registered;      /* identifier: class or nib */
    NSMutableDictionary *_itemViews;       /* index: view shown */
    NSScrubberSelectionView *_selectionBackgroundView, *_selectionOverlayView;
    CGFloat _offset;
    BOOL _needsReload;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame]) != nil) {
        _highlightedIndex = -1;
        _selectedIndex = -1;
        _layout = [[NSScrubberFlowLayout alloc] init];
        [_layout _setScrubber:self];
        _backgroundColor = [[NSColor controlBackgroundColor] copy];
        _registered = [[NSMutableDictionary alloc] init];
        _itemViews = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil) {
        _highlightedIndex = -1;
        _selectedIndex = -1;
        _layout = [[NSScrubberFlowLayout alloc] init];
        [_layout _setScrubber:self];
        _backgroundColor = [[NSColor controlBackgroundColor] copy];
        _registered = [[NSMutableDictionary alloc] init];
        _itemViews = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void)dealloc {
    [_layout _setScrubber:nil];
    [_layout release];
    [_selectionBackgroundStyle release];
    [_selectionOverlayStyle release];
    [_backgroundColor release];
    [_backgroundView release];
    [_registered release];
    [_itemViews release];
    [_selectionBackgroundView release];
    [_selectionOverlayView release];
    [super dealloc];
}

- (id<NSScrubberDataSource>)dataSource { return _dataSource; }
- (void)setDataSource:(id<NSScrubberDataSource>)source { _dataSource = source; [self reloadData]; }
- (id<NSScrubberDelegate>)delegate { return _delegate; }
- (void)setDelegate:(id<NSScrubberDelegate>)delegate { _delegate = delegate; [_layout invalidateLayout]; }
- (NSScrubberLayout *)scrubberLayout { return _layout; }

- (void)setScrubberLayout:(NSScrubberLayout *)layout {
    if (layout == _layout)
        return;
    [_layout _setScrubber:nil];
    [layout retain];
    [_layout release];
    _layout = layout;
    [_layout _setScrubber:self];
    [self _layoutDidInvalidate];
}

- (NSInteger)numberOfItems { return _numberOfItems; }
- (NSInteger)highlightedIndex { return _highlightedIndex; }
- (NSInteger)selectedIndex { return _selectedIndex; }

/* Selected from code: no delegate message, as for the controls. */
- (void)setSelectedIndex:(NSInteger)index {
    if (index < -1 || index >= _numberOfItems)
        index = -1;
    _selectedIndex = index;
    if ([_layout shouldInvalidateLayoutForSelectionChange])
        [_layout invalidateLayout];
    [self _updateItemStates];
}

- (NSScrubberMode)mode { return _mode; }
- (void)setMode:(NSScrubberMode)mode { _mode = mode; }
- (NSScrubberAlignment)itemAlignment { return _itemAlignment; }
- (void)setItemAlignment:(NSScrubberAlignment)alignment { _itemAlignment = alignment; }
- (BOOL)isContinuous { return _continuous; }
- (void)setContinuous:(BOOL)value { _continuous = value; }
- (BOOL)floatsSelectionViews { return _floatsSelectionViews; }
- (void)setFloatsSelectionViews:(BOOL)value { _floatsSelectionViews = value; }
- (NSScrubberSelectionStyle *)selectionBackgroundStyle { return _selectionBackgroundStyle; }
- (void)setSelectionBackgroundStyle:(NSScrubberSelectionStyle *)style { [style retain]; [_selectionBackgroundStyle release]; _selectionBackgroundStyle = style; [_selectionBackgroundView removeFromSuperview]; [_selectionBackgroundView release]; _selectionBackgroundView = [[style makeSelectionView] retain]; [self _updateItemStates]; }
- (NSScrubberSelectionStyle *)selectionOverlayStyle { return _selectionOverlayStyle; }
- (void)setSelectionOverlayStyle:(NSScrubberSelectionStyle *)style { [style retain]; [_selectionOverlayStyle release]; _selectionOverlayStyle = style; [_selectionOverlayView removeFromSuperview]; [_selectionOverlayView release]; _selectionOverlayView = [[style makeSelectionView] retain]; [self _updateItemStates]; }
- (BOOL)showsArrowButtons { return _showsArrowButtons; }
- (void)setShowsArrowButtons:(BOOL)value { _showsArrowButtons = value; }
- (BOOL)showsAdditionalContentIndicators { return _showsAdditionalContentIndicators; }
- (void)setShowsAdditionalContentIndicators:(BOOL)value { _showsAdditionalContentIndicators = value; }
- (NSColor *)backgroundColor { return _backgroundColor; }
- (void)setBackgroundColor:(NSColor *)color { color = [color copy]; [_backgroundColor release]; _backgroundColor = color; [self setNeedsDisplay:YES]; }
- (NSView *)backgroundView { return _backgroundView; }

- (void)setBackgroundView:(NSView *)view {
    [_backgroundView removeFromSuperview];
    [view retain];
    [_backgroundView release];
    _backgroundView = view;
    if (view != nil) {
        [view setFrame:[self bounds]];
        [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:view positioned:NSWindowBelow relativeTo:nil];
    }
}

- (BOOL)isFlipped {
    return YES;
}

- (void)drawRect:(NSRect)rect {
    if (_backgroundView == nil && _backgroundColor != nil) {
        [_backgroundColor setFill];
        NSRectFill(rect);
    }
}

/* Item views by identifier: a class made with a frame, or a nib's first
   item view. */
- (void)registerClass:(Class)itemViewClass forItemIdentifier:(NSUserInterfaceItemIdentifier)identifier {
    if (itemViewClass != Nil)
        [_registered setObject:itemViewClass forKey:identifier];
    else
        [_registered removeObjectForKey:identifier];
}

- (void)registerNib:(NSNib *)nib forItemIdentifier:(NSUserInterfaceItemIdentifier)identifier {
    if (nib != nil)
        [_registered setObject:nib forKey:identifier];
    else
        [_registered removeObjectForKey:identifier];
}

- (NSScrubberItemView *)makeItemWithIdentifier:(NSUserInterfaceItemIdentifier)identifier owner:(id)owner {
    id kind = [_registered objectForKey:identifier];
    NSScrubberItemView *view = nil;

    if ([kind isKindOfClass:[NSNib class]]) {
        NSArray *objects = nil;
        if ([(NSNib *)kind instantiateWithOwner:owner topLevelObjects:&objects])
            for (id object in objects)
                if ([object isKindOfClass:[NSScrubberItemView class]]) {
                    view = object;
                    break;
                }
    } else if (kind != nil)
        view = [[[(Class)kind alloc] initWithFrame:NSZeroRect] autorelease];
    [view setIdentifier:identifier];
    return view;
}

- (NSScrubberItemView *)itemViewForItemAtIndex:(NSInteger)index {
    return [_itemViews objectForKey:[NSNumber numberWithInteger:index]];
}

- (void)_layoutDidInvalidate {
    [self _tileItems];
    [self setNeedsDisplay:YES];
}

- (void)reloadData {
    _numberOfItems = [_dataSource respondsToSelector:@selector(numberOfItemsForScrubber:)] ? [_dataSource numberOfItemsForScrubber:self] : 0;
    if (_highlightedIndex >= _numberOfItems)
        _highlightedIndex = -1;
    if (_selectedIndex >= _numberOfItems)
        _selectedIndex = -1;
    for (NSView *view in [_itemViews allValues])
        [view removeFromSuperview];
    [_itemViews removeAllObjects];
    [_layout invalidateLayout];
    [self _tileItems];
}

- (void)performSequentialBatchUpdates:(void (^)(void))updateBlock {
    if (updateBlock != NULL)
        updateBlock();
    [self reloadData];
}

- (void)insertItemsAtIndexes:(NSIndexSet *)indexes { [self reloadData]; }
- (void)removeItemsAtIndexes:(NSIndexSet *)indexes { [self reloadData]; }
- (void)reloadItemsAtIndexes:(NSIndexSet *)indexes { [self reloadData]; }
- (void)moveItemAtIndex:(NSInteger)oldIndex toIndex:(NSInteger)newIndex { [self reloadData]; }

/* Scrolled so the item is at the leading or trailing edge, or centered
   (none: just into view). */
- (void)scrollItemAtIndex:(NSInteger)index toAlignment:(NSScrubberAlignment)alignment {
    NSRect frame = [[_layout layoutAttributesForItemAtIndex:index] frame];
    CGFloat width = NSWidth([self bounds]), content = [_layout scrubberContentSize].width;

    switch (alignment) {
        case NSScrubberAlignmentLeading: _offset = NSMinX(frame); break;
        case NSScrubberAlignmentTrailing: _offset = NSMaxX(frame) - width; break;
        case NSScrubberAlignmentCenter: _offset = NSMidX(frame) - width / 2; break;
        default:
            if (NSMinX(frame) < _offset)
                _offset = NSMinX(frame);
            else if (NSMaxX(frame) > _offset + width)
                _offset = NSMaxX(frame) - width;
            break;
    }
    _offset = MAX(0, MIN(_offset, MAX(0, content - width)));
    [self _tileItems];
}

- (void)setFrameSize:(NSSize)size {
    NSRect before = [self bounds];
    [super setFrameSize:size];
    if ([_layout shouldInvalidateLayoutForChangeFromVisibleRect:before toVisibleRect:[self bounds]])
        [_layout invalidateLayout];
    [self _tileItems];
}

/* The items in view, from the data source, where the layout puts them,
   shifted by the scroll offset. */
- (void)_tileItems {
    NSRect visible = NSOffsetRect([self bounds], _offset, 0);
    NSMutableDictionary *shown = [NSMutableDictionary dictionary];

    if (_dataSource == nil || _numberOfItems == 0)
        return;
    [_layout prepareLayout];
    for (NSScrubberLayoutAttributes *attributes in [_layout layoutAttributesForItemsInRect:visible]) {
        NSNumber *key = [NSNumber numberWithInteger:[attributes itemIndex]];
        NSScrubberItemView *view = [_itemViews objectForKey:key];

        if (view == nil)
            view = [_dataSource scrubber:self viewForItemAtIndex:[attributes itemIndex]];
        if (view == nil)
            continue;
        NSScrubberLayoutAttributes *placed = [[attributes copy] autorelease];
        [placed setFrame:NSOffsetRect([attributes frame], -_offset, 0)];
        [view applyLayoutAttributes:placed];
        if ([view superview] != self)
            [self addSubview:view];
        [shown setObject:view forKey:key];
    }
    for (NSNumber *key in [_itemViews allKeys])
        if ([shown objectForKey:key] == nil)
            [[_itemViews objectForKey:key] removeFromSuperview];
    [_itemViews setDictionary:shown];
    [self _updateItemStates];
}

/* The item views' states, and the selection views under and over the
   selected one. */
- (void)_updateItemStates {
    for (NSNumber *key in _itemViews) {
        NSScrubberItemView *view = [_itemViews objectForKey:key];
        [view setSelected:[key integerValue] == _selectedIndex];
        [view setHighlighted:[key integerValue] == _highlightedIndex];
    }
    NSScrubberItemView *selected = (_selectedIndex >= 0) ? [self itemViewForItemAtIndex:_selectedIndex] : nil;
    for (NSScrubberSelectionView *view in [NSArray arrayWithObjects:_selectionBackgroundView ? (id)_selectionBackgroundView : (id)[NSNull null], _selectionOverlayView ? (id)_selectionOverlayView : (id)[NSNull null], nil]) {
        if ((id)view == [NSNull null])
            continue;
        if (selected == nil) {
            [view removeFromSuperview];
            continue;
        }
        [view setFrame:[selected frame]];
        if ([view superview] != self)
            [self addSubview:view positioned:(view == _selectionBackgroundView) ? NSWindowBelow : NSWindowAbove relativeTo:selected];
    }
}

/* A click or a slide picks the item under the pointer: highlighted while
   the pointer is down, selected when it lets go (or at once when
   continuous), the delegate told. */
- (NSInteger)_indexAtPoint:(NSPoint)point {
    for (NSNumber *key in _itemViews)
        if (NSPointInRect(point, [[_itemViews objectForKey:key] frame]))
            return [key integerValue];
    return -1;
}

- (void)mouseDown:(NSEvent *)event {
    if ([_delegate respondsToSelector:@selector(didBeginInteractingWithScrubber:)])
        [_delegate didBeginInteractingWithScrubber:self];
    for (;;) {
        NSInteger index = [self _indexAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

        if (index != _highlightedIndex) {
            _highlightedIndex = index;
            [self _updateItemStates];
            if (index >= 0 && [_delegate respondsToSelector:@selector(scrubber:didHighlightItemAtIndex:)])
                [_delegate scrubber:self didHighlightItemAtIndex:index];
            if (_continuous && index >= 0 && index != _selectedIndex) {
                _selectedIndex = index;
                [self _updateItemStates];
                if ([_delegate respondsToSelector:@selector(scrubber:didSelectItemAtIndex:)])
                    [_delegate scrubber:self didSelectItemAtIndex:index];
            }
        }
        if ([event type] == NSEventTypeLeftMouseUp)
            break;
        event = [[self window] nextEventMatchingMask:NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged];
        if (event == nil)
            break;
    }
    NSInteger picked = _highlightedIndex;
    _highlightedIndex = -1;
    if (picked >= 0 && picked != _selectedIndex) {
        _selectedIndex = picked;
        if ([_delegate respondsToSelector:@selector(scrubber:didSelectItemAtIndex:)])
            [_delegate scrubber:self didSelectItemAtIndex:picked];
    }
    [self _updateItemStates];
    if (picked >= 0) {
        if ([_delegate respondsToSelector:@selector(didFinishInteractingWithScrubber:)])
            [_delegate didFinishInteractingWithScrubber:self];
    } else if ([_delegate respondsToSelector:@selector(didCancelInteractingWithScrubber:)])
        [_delegate didCancelInteractingWithScrubber:self];
}

- (void)scrollWheel:(NSEvent *)event {
    CGFloat content = [_layout scrubberContentSize].width, width = NSWidth([self bounds]);
    _offset = MAX(0, MIN(_offset - [event scrollingDeltaX] - [event scrollingDeltaY], MAX(0, content - width)));
    [self _tileItems];
}

@end
