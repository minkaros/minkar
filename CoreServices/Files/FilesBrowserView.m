/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The file manager's browser: icons in a grid (as the reference platform's icon view)
 * or rows with Name, Date Modified, Size and Kind (its list view). A click
 * selects, a double click opens, the arrow keys move the selection,
 * Command-Down opens. Atlas's provisional look.
 */

#import "FilesBrowserView.h"

static const CGFloat kIconCellWidth = 104, kIconCellHeight = 96, kIconSize = 56;
static const CGFloat kRowHeight = 22, kHeaderHeight = 24;

static NSColor *selectionColor(void)
{
    return [NSColor colorWithCalibratedRed:0.25 green:0.50 blue:0.85 alpha:1];
}

@implementation FilesBrowserView {
    NSInteger _selected;
    NSDateFormatter *_dateFormatter;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil) {
        _selected = -1;
        _items = @[];
        _dateFormatter = [[NSDateFormatter alloc] init];
        [_dateFormatter setLocale:FilesDisplayLocale()];
        [_dateFormatter setDateFormat:NSLocalizedString(@"d MMM yyyy 'at' HH:mm", @"Date Modified column")];
    }
    return self;
}

- (BOOL)isFlipped
{
    return YES;
}

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)setItems:(NSArray<FilesItem *> *)items
{
    _items = [items copy];
    _selected = -1;
    [self relayout];
    [self.delegate browserViewSelectionChanged:self];
}

- (void)setMode:(FilesViewMode)mode
{
    _mode = mode;
    [self relayout];
}

- (FilesItem *)selectedItem
{
    return _selected >= 0 && (NSUInteger)_selected < [_items count] ? _items[(NSUInteger)_selected] : nil;
}

- (void)selectItemNamed:(NSString *)name
{
    for (NSUInteger i = 0; i < [_items count]; i++)
        if ([[_items[i].URL lastPathComponent] isEqualToString:name])
            [self select:(NSInteger)i];
}

/* -- Layout -- */

- (CGFloat)availableWidth
{
    NSScrollView *scroll = [self enclosingScrollView];

    return scroll ? NSWidth([[scroll contentView] bounds]) : NSWidth([self frame]);
}

- (NSUInteger)columns
{
    return MAX((NSUInteger)floor([self availableWidth] / kIconCellWidth), 1);
}

- (void)relayout
{
    CGFloat width = [self availableWidth], height;
    NSUInteger count = [_items count];

    if (_mode == FilesViewModeIcons)
        height = ceil((double)count / [self columns]) * kIconCellHeight + 12;
    else
        height = kHeaderHeight + count * kRowHeight;
    NSScrollView *scroll = [self enclosingScrollView];
    if (scroll != nil)
        height = MAX(height, NSHeight([[scroll contentView] bounds]));
    [self setFrameSize:NSMakeSize(width, height)];
    [self setNeedsDisplay:YES];
}

- (NSRect)rectForIndex:(NSUInteger)i
{
    if (_mode == FilesViewModeIcons) {
        NSUInteger columns = [self columns];
        CGFloat margin = floor(([self availableWidth] - columns * kIconCellWidth) / 2);
        return NSMakeRect(margin + (i % columns) * kIconCellWidth, 6 + (i / columns) * kIconCellHeight,
                          kIconCellWidth, kIconCellHeight);
    }
    return NSMakeRect(0, kHeaderHeight + i * kRowHeight, NSWidth([self bounds]), kRowHeight);
}

- (NSInteger)indexAtPoint:(NSPoint)p
{
    for (NSUInteger i = 0; i < [_items count]; i++)
        if (NSPointInRect(p, [self rectForIndex:i]))
            return (NSInteger)i;
    return -1;
}

/* The list's columns: name takes what the others leave. */
- (void)columnsName:(CGFloat *)name date:(CGFloat *)date size:(CGFloat *)size kind:(CGFloat *)kind
{
    CGFloat width = NSWidth([self bounds]);

    *kind = width - 130;
    *size = *kind - 90;
    *date = *size - 170;
    *name = 28;
}

/* -- Drawing -- */

static void drawText(NSString *text, NSRect rect, NSDictionary *attributes)
{
    NSString *shown = text;
    NSSize size = [shown sizeWithAttributes:attributes];

    while (size.width > NSWidth(rect) && [shown length] > 2) {
        shown = [[shown substringToIndex:[shown length] - 2] stringByAppendingString:@"…"];
        if ([shown hasSuffix:@"……"])
            shown = [[shown substringToIndex:[shown length] - 2] stringByAppendingString:@"…"];
        size = [shown sizeWithAttributes:attributes];
    }
    [shown drawAtPoint:NSMakePoint(NSMinX(rect), NSMinY(rect) + floor((NSHeight(rect) - size.height) / 2))
        withAttributes:attributes];
}

- (void)drawRect:(NSRect)dirty
{
    NSFont *font = [NSFont systemFontOfSize:12];
    NSDictionary *plain = @{ NSFontAttributeName: font, NSForegroundColorAttributeName: [NSColor blackColor] };
    NSDictionary *light = @{ NSFontAttributeName: font,
                             NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.4 alpha:1] };
    NSDictionary *selected = @{ NSFontAttributeName: font, NSForegroundColorAttributeName: [NSColor whiteColor] };

    [[NSColor whiteColor] set];
    NSRectFill(dirty);

    if (_mode == FilesViewModeList) {
        CGFloat name, date, size, kind;
        [self columnsName:&name date:&date size:&size kind:&kind];
        NSDictionary *header = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:11],
                                  NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.35 alpha:1] };
        [[NSColor colorWithCalibratedWhite:0.95 alpha:1] set];
        NSRectFill(NSMakeRect(0, 0, NSWidth([self bounds]), kHeaderHeight));
        [[NSColor colorWithCalibratedWhite:0.85 alpha:1] set];
        NSRectFill(NSMakeRect(0, kHeaderHeight - 1, NSWidth([self bounds]), 1));
        drawText(NSLocalizedString(@"Name", nil), NSMakeRect(name, 0, date - name - 8, kHeaderHeight), header);
        drawText(NSLocalizedString(@"Date Modified", nil), NSMakeRect(date, 0, size - date - 8, kHeaderHeight), header);
        drawText(NSLocalizedString(@"Size", nil), NSMakeRect(size, 0, kind - size - 8, kHeaderHeight), header);
        drawText(NSLocalizedString(@"Kind", nil), NSMakeRect(kind, 0, NSWidth([self bounds]) - kind - 8, kHeaderHeight), header);

        for (NSUInteger i = 0; i < [_items count]; i++) {
            NSRect row = [self rectForIndex:i];
            if (!NSIntersectsRect(row, dirty))
                continue;
            FilesItem *item = _items[i];
            BOOL isSelected = (NSInteger)i == _selected;
            if (isSelected) {
                [selectionColor() set];
                NSRectFill(row);
            } else if (i % 2) {
                [[NSColor colorWithCalibratedWhite:0.965 alpha:1] set];
                NSRectFill(row);
            }
            [item.icon drawInRect:NSMakeRect(8, NSMinY(row) + 3, 16, 16) fromRect:NSZeroRect
                        operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
            NSDictionary *text = isSelected ? selected : plain, *dim = isSelected ? selected : light;
            drawText(item.name, NSMakeRect(name, NSMinY(row), date - name - 8, kRowHeight), text);
            drawText(item.modified ? [_dateFormatter stringFromDate:item.modified] : @"--",
                     NSMakeRect(date, NSMinY(row), size - date - 8, kRowHeight), dim);
            drawText(item.isFolder || item.isApplication ? @"--" : FilesSizeString(item.size),
                     NSMakeRect(size, NSMinY(row), kind - size - 8, kRowHeight), dim);
            drawText(item.kind, NSMakeRect(kind, NSMinY(row), NSWidth(row) - kind - 8, kRowHeight), dim);
        }
        return;
    }

    for (NSUInteger i = 0; i < [_items count]; i++) {
        NSRect cell = [self rectForIndex:i];
        if (!NSIntersectsRect(cell, dirty))
            continue;
        FilesItem *item = _items[i];
        BOOL isSelected = (NSInteger)i == _selected;
        NSRect icon = NSMakeRect(NSMidX(cell) - kIconSize / 2, NSMinY(cell) + 6, kIconSize, kIconSize);
        if (isSelected) {
            [[NSColor colorWithCalibratedWhite:0.85 alpha:1] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(icon, -4, -4) xRadius:6 yRadius:6] fill];
        }
        [item.icon drawInRect:icon fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1
               respectFlipped:YES hints:nil];
        NSDictionary *attributes = isSelected ? selected : plain;
        NSSize size = [item.name sizeWithAttributes:attributes];
        CGFloat width = MIN(size.width, kIconCellWidth - 8);
        NSRect label = NSMakeRect(NSMidX(cell) - width / 2 - 4, NSMaxY(icon) + 6, width + 8, size.height + 2);
        if (isSelected) {
            [selectionColor() setFill];
            [[NSBezierPath bezierPathWithRoundedRect:label xRadius:4 yRadius:4] fill];
        }
        drawText(item.name, NSInsetRect(label, 4, 0), attributes);
    }
}

/* -- Events -- */

- (void)select:(NSInteger)index
{
    if (index == _selected)
        return;
    _selected = index;
    if (index >= 0)
        [self scrollRectToVisible:[self rectForIndex:(NSUInteger)index]];
    [self setNeedsDisplay:YES];
    [self.delegate browserViewSelectionChanged:self];
}

- (void)mouseDown:(NSEvent *)event
{
    NSInteger index = [self indexAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

    [[self window] makeFirstResponder:self];
    [self select:index];
    if ([event clickCount] == 2 && index >= 0)
        [self.delegate browserView:self openItem:_items[(NSUInteger)index]];
}

- (void)keyDown:(NSEvent *)event
{
    NSString *chars = [event charactersIgnoringModifiers];
    unichar c = [chars length] ? [chars characterAtIndex:0] : 0;
    NSInteger count = (NSInteger)[_items count], step = _mode == FilesViewModeIcons ? (NSInteger)[self columns] : 1;
    NSInteger next = _selected;

    if (count == 0)
        return;
    switch (c) {
    case NSDownArrowFunctionKey:
        if ([event modifierFlags] & NSCommandKeyMask) {
            if (self.selectedItem != nil)
                [self.delegate browserView:self openItem:self.selectedItem];
            return;
        }
        next = _selected < 0 ? 0 : MIN(_selected + step, count - 1);
        break;
    case NSUpArrowFunctionKey:
        if ([event modifierFlags] & NSCommandKeyMask) {
            [super keyDown:event];
            return;
        }
        next = _selected < 0 ? 0 : MAX(_selected - step, 0);
        break;
    case NSRightArrowFunctionKey:
        if (_mode != FilesViewModeIcons)
            return;
        next = _selected < 0 ? 0 : MIN(_selected + 1, count - 1);
        break;
    case NSLeftArrowFunctionKey:
        if (_mode != FilesViewModeIcons)
            return;
        next = _selected < 0 ? 0 : MAX(_selected - 1, 0);
        break;
    default:
        [super keyDown:event];
        return;
    }
    [self select:next];
}

@end
