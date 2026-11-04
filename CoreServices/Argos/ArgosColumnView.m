/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Argos's column view (Dock-Argos.md, K1): a folder's items in a column,
 * the chosen folder's in the next one, and so on to the right. A click
 * chooses, a double click opens; Up and Down move in a column, Right goes
 * into the chosen folder's column, Left back to the one before;
 * Command-Down opens. Atlas's provisional look.
 */

#import "ArgosColumnView.h"

static const CGFloat kColumnWidth = 220, kRowHeight = 20;

static NSColor *selectionColor(void)
{
    return [NSColor colorWithCalibratedRed:0.25 green:0.50 blue:0.85 alpha:1];
}

/* What is chosen in a column but not where the keyboard acts. */
static NSColor *inactiveSelectionColor(void)
{
    return [NSColor colorWithCalibratedWhite:0.84 alpha:1];
}

/* One column: a folder, its items, the one chosen. */
@interface ArgosColumn : NSObject
@property (nonatomic, copy) NSURL *folder;
@property (nonatomic, copy) NSArray<ArgosItem *> *items;
@property (nonatomic) NSInteger selected;
@end

@implementation ArgosColumn
@end

@implementation ArgosColumnView {
    NSMutableArray<ArgosColumn *> *_columns;
    NSUInteger _active;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil)
        _columns = [NSMutableArray array];
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

static ArgosColumn *columnFor(NSURL *folder)
{
    NSArray *items = [ArgosItem itemsInFolder:folder error:NULL];

    if (items == nil)
        return nil;
    ArgosColumn *column = [[ArgosColumn alloc] init];
    column.folder = folder;
    column.items = items;
    column.selected = -1;
    return column;
}

- (BOOL)showRootFolder:(NSURL *)folder
{
    ArgosColumn *column = columnFor(folder);

    if (column == nil)
        return NO;
    [_columns removeAllObjects];
    [_columns addObject:column];
    _active = 0;
    [self relayout];
    return YES;
}

- (NSURL *)rootFolder
{
    return [_columns count] ? _columns[0].folder : [NSURL fileURLWithPath:@"/"];
}

- (NSURL *)deepestFolder
{
    return [_columns count] ? [_columns lastObject].folder : [NSURL fileURLWithPath:@"/"];
}

- (NSUInteger)deepestItemCount
{
    return [[_columns lastObject].items count];
}

- (ArgosItem *)selectedItem
{
    if (_active >= [_columns count])
        return nil;
    ArgosColumn *column = _columns[_active];
    return column.selected >= 0 && (NSUInteger)column.selected < [column.items count]
        ? column.items[(NSUInteger)column.selected] : nil;
}

- (void)reload
{
    for (NSUInteger i = 0; i < [_columns count]; i++) {
        ArgosColumn *column = _columns[i];
        NSString *chosen = column.selected >= 0 ? [column.items[(NSUInteger)column.selected].URL lastPathComponent] : nil;
        NSArray<ArgosItem *> *items = [ArgosItem itemsInFolder:column.folder error:NULL];
        if (items == nil) {
            /* Gone: the columns from here on with it. */
            [_columns removeObjectsInRange:NSMakeRange(i, [_columns count] - i)];
            break;
        }
        column.items = items;
        column.selected = -1;
        for (NSUInteger j = 0; chosen != nil && j < [items count]; j++)
            if ([[items[j].URL lastPathComponent] isEqualToString:chosen])
                column.selected = (NSInteger)j;
        /* What it showed is no longer there: no next column. */
        if (column.selected < 0 && i + 1 < [_columns count]) {
            [_columns removeObjectsInRange:NSMakeRange(i + 1, [_columns count] - i - 1)];
            break;
        }
    }
    if ([_columns count] == 0) {
        ArgosColumn *root = columnFor([NSURL fileURLWithPath:@"/"]);
        if (root != nil)
            [_columns addObject:root];
    }
    if (_active >= [_columns count])
        _active = [_columns count] ? [_columns count] - 1 : 0;
    [self relayout];
}

/* -- Layout -- */

- (void)relayout
{
    NSScrollView *scroll = [self enclosingScrollView];
    NSSize visible = scroll ? [scroll frame].size : [self frame].size;
    CGFloat height = 0, width = [_columns count] * kColumnWidth;

    for (ArgosColumn *column in _columns)
        height = MAX(height, [column.items count] * kRowHeight + 4);
    /* What the scrollers leave (from the scroll view's own size: the clip
     * view's changes with them). */
    if (scroll != nil && height > visible.height)
        visible.width -= [NSScroller scrollerWidth];
    if (scroll != nil && width > visible.width)
        visible.height -= [NSScroller scrollerWidth];
    [self setFrameSize:NSMakeSize(MAX(visible.width, width), MAX(visible.height, height))];
    [self setNeedsDisplay:YES];
}

- (NSRect)rectForRow:(NSUInteger)row column:(NSUInteger)column
{
    return NSMakeRect(column * kColumnWidth, 2 + row * kRowHeight, kColumnWidth - 1, kRowHeight);
}

/* -- Drawing -- */

static void drawText(NSString *text, NSRect rect, NSDictionary *attributes)
{
    NSString *shown = text;
    NSSize size = [shown sizeWithAttributes:attributes];

    while (size.width > NSWidth(rect) && [shown length] > 2) {
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
    NSDictionary *white = @{ NSFontAttributeName: font, NSForegroundColorAttributeName: [NSColor whiteColor] };
    NSDictionary *chevron = @{ NSFontAttributeName: font,
                               NSForegroundColorAttributeName: [NSColor colorWithCalibratedWhite:0.45 alpha:1] };

    [[NSColor whiteColor] set];
    NSRectFill(dirty);

    for (NSUInteger c = 0; c < [_columns count]; c++) {
        ArgosColumn *column = _columns[c];
        CGFloat right = (c + 1) * kColumnWidth - 1;
        [[NSColor colorWithCalibratedWhite:0.85 alpha:1] set];
        NSRectFill(NSMakeRect(right, NSMinY(dirty), 1, NSHeight(dirty)));
        for (NSUInteger r = 0; r < [column.items count]; r++) {
            NSRect row = [self rectForRow:r column:c];
            if (!NSIntersectsRect(row, dirty))
                continue;
            ArgosItem *item = column.items[r];
            BOOL chosen = (NSInteger)r == column.selected, active = chosen && c == _active;
            if (chosen) {
                [(active ? selectionColor() : inactiveSelectionColor()) set];
                NSRectFill(row);
            }
            [item.icon drawInRect:NSMakeRect(NSMinX(row) + 8, NSMinY(row) + 2, 16, 16) fromRect:NSZeroRect
                        operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
            NSDictionary *text = active ? white : plain;
            drawText(item.name, NSMakeRect(NSMinX(row) + 30, NSMinY(row), NSWidth(row) - 52, kRowHeight), text);
            if (item.isFolder)
                drawText(@"›", NSMakeRect(NSMaxX(row) - 16, NSMinY(row), 12, kRowHeight), active ? white : chevron);
        }
    }
}

/* -- Choosing -- */

/* Row r of column c chosen: the columns after c go; a folder gets one. */
- (void)choose:(NSInteger)r inColumn:(NSUInteger)c
{
    ArgosColumn *column = _columns[c];
    NSURL *before = self.deepestFolder;

    _active = c;
    column.selected = r;
    if (c + 1 < [_columns count])
        [_columns removeObjectsInRange:NSMakeRange(c + 1, [_columns count] - c - 1)];
    if (r >= 0) {
        ArgosItem *item = column.items[(NSUInteger)r];
        ArgosColumn *next = item.isFolder ? columnFor(item.URL) : nil;
        if (next != nil)
            [_columns addObject:next];
    }
    [self relayout];
    if (r >= 0)
        [self scrollRectToVisible:[self rectForRow:(NSUInteger)r column:c]];
    if ([_columns count] > c + 1)
        [self scrollRectToVisible:NSMakeRect((c + 1) * kColumnWidth, NSMinY([self visibleRect]), kColumnWidth, 1)];
    if (![self.deepestFolder isEqual:before])
        [self.delegate columnView:self showsFolder:self.deepestFolder];
}

- (void)selectItemNamed:(NSString *)name
{
    if (_active >= [_columns count])
        return;
    NSArray<ArgosItem *> *items = _columns[_active].items;
    for (NSUInteger i = 0; i < [items count]; i++)
        if ([[items[i].URL lastPathComponent] isEqualToString:name])
            [self choose:(NSInteger)i inColumn:_active];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSUInteger c = (NSUInteger)MAX(floor(p.x / kColumnWidth), 0);

    [[self window] makeFirstResponder:self];
    if (c >= [_columns count])
        return;
    NSInteger r = (NSInteger)floor((p.y - 2) / kRowHeight);
    if (p.y < 2 || r >= (NSInteger)[_columns[c].items count])
        r = -1;
    [self choose:r inColumn:c];
    if ([event clickCount] == 2 && r >= 0)
        [self.delegate columnView:self openItem:_columns[c].items[(NSUInteger)r]];
}

- (void)keyDown:(NSEvent *)event
{
    NSString *chars = [event charactersIgnoringModifiers];
    unichar k = [chars length] ? [chars characterAtIndex:0] : 0;

    if (_active >= [_columns count]) {
        [super keyDown:event];
        return;
    }
    ArgosColumn *column = _columns[_active];
    NSInteger count = (NSInteger)[column.items count];

    switch (k) {
    case NSDownArrowFunctionKey:
        if ([event modifierFlags] & NSCommandKeyMask) {
            if (self.selectedItem != nil)
                [self.delegate columnView:self openItem:self.selectedItem];
            return;
        }
        if (count > 0)
            [self choose:column.selected < 0 ? 0 : MIN(column.selected + 1, count - 1) inColumn:_active];
        return;
    case NSUpArrowFunctionKey:
        if ([event modifierFlags] & NSCommandKeyMask)
            break;
        if (count > 0)
            [self choose:column.selected < 0 ? 0 : MAX(column.selected - 1, 0) inColumn:_active];
        return;
    case NSRightArrowFunctionKey:
        /* Into the chosen folder's column, at its first item. */
        if (_active + 1 < [_columns count] && [_columns[_active + 1].items count] > 0)
            [self choose:0 inColumn:_active + 1];
        return;
    case NSLeftArrowFunctionKey:
        /* Back to the column before: its chosen folder stays shown. */
        if (_active > 0) {
            _active--;
            [self setNeedsDisplay:YES];
        }
        return;
    }
    [super keyDown:event];
}

@end
