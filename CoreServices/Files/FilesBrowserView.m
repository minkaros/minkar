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

#import "FilesOperations.h"
#import "FilesBrowserView.h"
#import "FilesColors.h"
#include <sys/stat.h>

static const CGFloat kIconCellWidth = 104, kIconCellHeight = 96, kIconSize = 56;
static const CGFloat kRowHeight = 22, kHeaderHeight = 24;

static NSColor *selectionColor(void)
{
    return [NSColor selectedContentBackgroundColor];
}

@interface FilesBrowserView () <NSTextFieldDelegate>
@end

@implementation FilesBrowserView {
    NSInteger _selected;        /* the item clicked last: the anchor of a range */
    NSMutableIndexSet *_selection;
    NSDateFormatter *_dateFormatter;
    NSPoint _downPoint;
    NSInteger _pressed;         /* the item a drag would take, or -1 */
    NSInteger _dropTarget;      /* the folder item a drop would go into */
    NSTextField *_renameField;  /* the name being edited in place */
    FilesItem *_renaming;
    BOOL _renameCancelled;
}

/* The volume a path is on, to tell a move from a copy. */
static dev_t deviceOf(NSURL *url)
{
    struct stat st;
    return stat([[url path] fileSystemRepresentation], &st) == 0 ? st.st_dev : (dev_t)-1;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil) {
        _selected = -1;
        _selection = [NSMutableIndexSet indexSet];
        _pressed = -1;
        _dropTarget = -1;
        _items = @[];
        [self registerForDraggedTypes:@[ NSFilenamesPboardType, NSPasteboardTypeFileURL ]];
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
    [_selection removeAllIndexes];
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
    if ([_selection count] == 0)
        return nil;
    if (_selected >= 0 && [_selection containsIndex:(NSUInteger)_selected])
        return _items[(NSUInteger)_selected];
    return _items[[_selection firstIndex]];
}

- (NSArray<FilesItem *> *)selectedItems
{
    NSMutableArray *items = [NSMutableArray array];
    [_selection enumerateIndexesUsingBlock:^(NSUInteger i, BOOL *stop) {
        if (i < [self->_items count])
            [items addObject:self->_items[i]];
    }];
    return items;
}

- (void)selectItemsNamed:(NSArray<NSString *> *)names
{
    [_selection removeAllIndexes];
    for (NSUInteger i = 0; i < [_items count]; i++)
        if ([names containsObject:[_items[i].URL lastPathComponent]]) {
            [_selection addIndex:i];
            _selected = (NSInteger)i;
        }
    [self setNeedsDisplay:YES];
    [self.delegate browserViewSelectionChanged:self];
}

- (IBAction)selectAll:(id)sender
{
    [_selection removeAllIndexes];
    if ([_items count] > 0)
        [_selection addIndexesInRange:NSMakeRange(0, [_items count])];
    [self setNeedsDisplay:YES];
    [self.delegate browserViewSelectionChanged:self];
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
    NSDictionary *plain = @{ NSFontAttributeName: font, NSForegroundColorAttributeName: FilesGray(0, 1) };
    NSDictionary *light = @{ NSFontAttributeName: font,
                             NSForegroundColorAttributeName: FilesGray(0.4, 1) };
    NSDictionary *selected = @{ NSFontAttributeName: font, NSForegroundColorAttributeName: [NSColor whiteColor] };

    [FilesGray(1, 1) set];
    NSRectFill(dirty);

    if (_mode == FilesViewModeList) {
        CGFloat name, date, size, kind;
        [self columnsName:&name date:&date size:&size kind:&kind];
        NSDictionary *header = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:11],
                                  NSForegroundColorAttributeName: FilesGray(0.35, 1) };
        [FilesGray(0.95, 1) set];
        NSRectFill(NSMakeRect(0, 0, NSWidth([self bounds]), kHeaderHeight));
        [FilesGray(0.85, 1) set];
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
            BOOL isSelected = [_selection containsIndex:i];
            if ((NSInteger)i == _dropTarget && !isSelected) {
                [[selectionColor() colorWithAlphaComponent:0.35] set];
                NSRectFill(row);
            } else if (isSelected) {
                [selectionColor() set];
                NSRectFill(row);
            } else if (i % 2) {
                [FilesGray(0.965, 1) set];
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
        BOOL isSelected = [_selection containsIndex:i];
        NSRect icon = NSMakeRect(NSMidX(cell) - kIconSize / 2, NSMinY(cell) + 6, kIconSize, kIconSize);
        if ((NSInteger)i == _dropTarget) {
            [[selectionColor() colorWithAlphaComponent:0.35] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(icon, -6, -6) xRadius:8 yRadius:8] fill];
        }
        if (isSelected) {
            [FilesGray(0.85, 1) setFill];
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
    if (index == _selected && [_selection count] == (index >= 0 ? 1u : 0u) &&
        (index < 0 || [_selection containsIndex:(NSUInteger)index]))
        return;
    _selected = index;
    [_selection removeAllIndexes];
    if (index >= 0)
        [_selection addIndex:(NSUInteger)index];
    if (index >= 0)
        [self scrollRectToVisible:[self rectForIndex:(NSUInteger)index]];
    [self setNeedsDisplay:YES];
    [self.delegate browserViewSelectionChanged:self];
}

- (void)mouseDown:(NSEvent *)event
{
    NSInteger index = [self indexAtPoint:[self convertPoint:[event locationInWindow] fromView:nil]];

    [[self window] makeFirstResponder:self];
    NSUInteger modifiers = [event modifierFlags];
    if ((modifiers & NSCommandKeyMask) && index >= 0) {
        /* Command: this item in or out of the selection. */
        if ([_selection containsIndex:(NSUInteger)index])
            [_selection removeIndex:(NSUInteger)index];
        else
            [_selection addIndex:(NSUInteger)index];
        _selected = index;
        [self setNeedsDisplay:YES];
        [self.delegate browserViewSelectionChanged:self];
    } else if ((modifiers & NSShiftKeyMask) && index >= 0 && _selected >= 0) {
        /* Shift: from the last item clicked to this one. */
        NSUInteger a = (NSUInteger)MIN(_selected, index), b = (NSUInteger)MAX(_selected, index);
        [_selection addIndexesInRange:NSMakeRange(a, b - a + 1)];
        [self setNeedsDisplay:YES];
        [self.delegate browserViewSelectionChanged:self];
    } else if (index < 0 || ![_selection containsIndex:(NSUInteger)index])
        [self select:index];    /* a click on a selected item keeps the others, to drag them */
    else
        _selected = index;
    _downPoint = [self convertPoint:[event locationInWindow] fromView:nil];
    _pressed = [event clickCount] == 1 ? index : -1;
    if ([event clickCount] == 2 && index >= 0)
        [self.delegate browserView:self openItem:_items[(NSUInteger)index]];
}

/* -- Renaming in place (K2) -- */

/* Where an item's name is drawn: under its icon, or the list's Name
   column. */
- (NSRect)nameRectForIndex:(NSUInteger)i
{
    NSRect cell = [self rectForIndex:i];
    if (_mode == FilesViewModeList) {
        CGFloat name, date, size, kind;
        [self columnsName:&name date:&date size:&size kind:&kind];
        return NSMakeRect(name - 3, NSMinY(cell), date - name - 4, kRowHeight);
    }
    return NSMakeRect(NSMinX(cell) + 2, NSMinY(cell) + 6 + kIconSize + 4, kIconCellWidth - 4, 20);
}

- (BOOL)beginRenaming
{
    if ([_selection count] != 1 || _renameField != nil)
        return NO;
    NSUInteger i = [_selection firstIndex];
    _renaming = _items[i];
    _renameCancelled = NO;
    _renameField = [[NSTextField alloc] initWithFrame:[self nameRectForIndex:i]];
    [_renameField setFont:[NSFont systemFontOfSize:12]];
    [_renameField setAlignment:_mode == FilesViewModeIcons ? NSCenterTextAlignment : NSLeftTextAlignment];
    [_renameField setStringValue:_renaming.name];
    [_renameField setDelegate:self];
    [self addSubview:_renameField];
    [[self window] makeFirstResponder:_renameField];
    /* The name without its extension selected, as on the reference
       platform. */
    NSString *stem = [_renaming.name stringByDeletingPathExtension];
    NSText *editor = [_renameField currentEditor];
    if (!_renaming.isFolder && [stem length] > 0 && [stem length] < [_renaming.name length])
        [editor setSelectedRange:NSMakeRange(0, [stem length])];
    return YES;
}

- (void)endRenaming
{
    NSTextField *field = _renameField;
    FilesItem *item = _renaming;

    if (field == nil)
        return;
    _renameField = nil;
    _renaming = nil;
    NSString *name = [field stringValue];
    [field removeFromSuperview];
    [[self window] makeFirstResponder:self];
    [self setNeedsDisplay:YES];
    if (!_renameCancelled && [name length] > 0 && ![name isEqualToString:item.name])
        [self.delegate browserView:self renameItem:item to:name];
}

/* Escape cancels (the key bindings call it cancelOperation: or
   complete:). */
- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)selector
{
    if (control == _renameField && (selector == @selector(cancelOperation:) || selector == @selector(complete:))) {
        _renameCancelled = YES;
        [self endRenaming];
        return YES;
    }
    return NO;
}

/* Return, Tab or a click elsewhere: the new name taken. */
- (void)controlTextDidEndEditing:(NSNotification *)note
{
    if ([note object] == _renameField)
        [self endRenaming];
}

/* -- Drag and drop (K2) -- */

/* An item dragged a few points: its URL on the drag pasteboard, its icon
   under the pointer. */
- (void)mouseDragged:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    if (_pressed < 0 || (NSUInteger)_pressed >= [_items count] || hypot(p.x - _downPoint.x, p.y - _downPoint.y) < 4)
        return;
    FilesItem *item = _items[(NSUInteger)_pressed];
    _pressed = -1;
    /* The whole selection goes (the pressed item is in it). */
    NSArray<FilesItem *> *dragged = [self selectedItems];
    if (![dragged containsObject:item])
        dragged = @[ item ];
    NSArray *urls = [dragged valueForKey:@"URL"];
    NSPasteboard *pb = [NSPasteboard pasteboardWithName:NSDragPboard];
    [pb clearContents];
    [pb writeObjects:urls];
    [pb setPropertyList:[urls valueForKey:@"path"] forType:NSFilenamesPboardType];
    NSImage *icon = [item.icon copy];
    [icon setSize:NSMakeSize(kIconSize, kIconSize)];
    NSPoint at = NSMakePoint(p.x - kIconSize / 2, p.y + kIconSize / 2);   /* flipped: the image's bottom left */
    [self dragImage:icon at:at offset:NSZeroSize event:event pasteboard:pb source:self slideBack:YES];
}

- (void)mouseUp:(NSEvent *)event
{
    /* A plain click (no drag) on an item of a larger selection: that
       item alone. */
    if (_pressed >= 0 && !([event modifierFlags] & (NSCommandKeyMask | NSShiftKeyMask)) && [_selection count] > 1)
        [self select:_pressed];
    _pressed = -1;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)local
{
    return NSDragOperationCopy | NSDragOperationMove | NSDragOperationGeneric;
}

static NSArray<NSURL *> *draggedURLs(id<NSDraggingInfo> info)
{
    return FilesDraggedFileURLs([info draggingPasteboard]);
}

/* The folder a drop at the pointer goes into: a folder item under it (not
   one being dragged), else the folder shown. */
- (NSURL *)dropFolderFor:(id<NSDraggingInfo>)info target:(NSInteger *)target
{
    NSPoint p = [self convertPoint:[info draggingLocation] fromView:nil];
    NSInteger index = [self indexAtPoint:p];
    NSArray<NSURL *> *urls = draggedURLs(info);

    *target = -1;
    if (index >= 0 && (NSUInteger)index < [_items count]) {
        FilesItem *item = _items[(NSUInteger)index];
        if (item.isFolder && ![urls containsObject:item.URL]) {
            *target = index;
            return item.URL;
        }
    }
    return [self.delegate folderOfBrowserView:self];
}

/* Option copies; otherwise a move on the same volume, a copy across. A
   drop where the items already are does nothing. */
- (NSDragOperation)operationFor:(id<NSDraggingInfo>)info
{
    NSInteger target;
    NSURL *folder = [self dropFolderFor:info target:&target];
    NSArray<NSURL *> *urls = draggedURLs(info);

    if (target != _dropTarget) {
        _dropTarget = target;
        [self setNeedsDisplay:YES];
    }
    if (folder == nil || [urls count] == 0)
        return NSDragOperationNone;
    if ([NSEvent modifierFlags] & NSAlternateKeyMask)
        return NSDragOperationCopy;
    if ([[[urls[0] URLByDeletingLastPathComponent] path] isEqual:[folder path]])
        return NSDragOperationNone;
    return deviceOf(urls[0]) == deviceOf(folder) ? NSDragOperationMove : NSDragOperationCopy;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info
{
    return [self operationFor:info];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info
{
    return [self operationFor:info];
}

- (void)draggingExited:(id<NSDraggingInfo>)info
{
    _dropTarget = -1;
    [self setNeedsDisplay:YES];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info
{
    NSDragOperation operation = [self operationFor:info];
    NSInteger target;
    NSURL *folder = [self dropFolderFor:info target:&target];

    _dropTarget = -1;
    [self setNeedsDisplay:YES];
    if (operation == NSDragOperationNone || folder == nil)
        return NO;
    [self.delegate browserView:self dropURLs:draggedURLs(info) intoFolder:folder
                       copying:operation == NSDragOperationCopy];
    return YES;
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
    case '\r':
    case 3:     /* Enter */
        /* Return renames, as on the reference platform (Command-O and
           Command-Down open). */
        if (![self beginRenaming])
            NSBeep();
        return;
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
