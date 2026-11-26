/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The desktop's icons (Atlas.md, K3). See FilesDesktop.h.
 */

#import "FilesDesktop.h"
#import "FilesItem.h"
#import "FilesInfoWindow.h"
#import "FilesOperations.h"
#import "FilesPreferences.h"
#include <CoreServices/CoreServices.h>
#include <os/log.h>
#include <sys/stat.h>

static const CGFloat kCellWidth = 100, kCellHeight = 92, kIcon = 56, kMargin = 14;

@class FilesDesktopView;

@interface FilesDesktop ()
@property (nonatomic, copy) void (^opener)(NSURL *url);
@property (nonatomic, readwrite) NSArray<NSURL *> *itemURLs;
- (void)open:(NSArray<NSURL *> *)urls;
- (void)eject:(NSArray<NSURL *> *)urls;
@end

static BOOL isVolume(NSURL *url)
{
    return [[[url URLByDeletingLastPathComponent] path] isEqualToString:@"/Volumes"];
}

extern NSString *_NSSystemIconPath(NSString *relative);
extern void FilesForgetSystemIcons(void);

/* A volume's icon: a disk (the system's device icons, or the theme's). */
static NSImage *volumeIconCache;

static NSImage *volumeIcon(void)
{
    if (volumeIconCache == nil)
        volumeIconCache = [[NSImage alloc] initWithContentsOfFile:_NSSystemIconPath(@"devices/drive-harddisk.png")];
    return volumeIconCache;
}

/* ---- The view ---- */

@interface FilesDesktopView : NSView
@property (nonatomic, copy) NSArray<FilesItem *> *items;
@property (nonatomic, weak) FilesDesktop *desktop;
@end

@implementation FilesDesktopView {
    NSMutableIndexSet *_selection;
    NSInteger _pressed, _dropTarget;
    NSPoint _downPoint;
}

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) != nil) {
        _selection = [NSMutableIndexSet indexSet];
        _pressed = _dropTarget = -1;
        _items = @[];
        [self registerForDraggedTypes:@[ NSFilenamesPboardType, NSPasteboardTypeFileURL ]];
    }
    return self;
}

- (BOOL)isFlipped { return YES; }
- (BOOL)isOpaque { return NO; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

- (void)setItems:(NSArray<FilesItem *> *)items
{
    NSArray *selected = [[self selectedItems] valueForKey:@"URL"];
    _items = [items copy];
    [_selection removeAllIndexes];
    for (NSUInteger i = 0; i < [_items count]; i++)
        if ([selected containsObject:_items[i].URL])
            [_selection addIndex:i];
    [self setNeedsDisplay:YES];
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

/* Columns from the right, top to bottom. */
- (NSRect)rectForIndex:(NSUInteger)i
{
    NSUInteger rows = MAX((NSUInteger)floor((NSHeight([self bounds]) - kMargin) / kCellHeight), 1);
    NSUInteger column = i / rows, row = i % rows;
    return NSMakeRect(NSWidth([self bounds]) - kMargin - (column + 1) * kCellWidth, kMargin + row * kCellHeight,
                      kCellWidth, kCellHeight);
}

- (NSInteger)indexAtPoint:(NSPoint)p
{
    for (NSUInteger i = 0; i < [_items count]; i++) {
        NSRect cell = [self rectForIndex:i];
        NSRect hit = NSMakeRect(NSMidX(cell) - kIcon / 2 - 6, NSMinY(cell), kIcon + 12, kCellHeight - 4);
        if (NSPointInRect(p, hit))
            return (NSInteger)i;
    }
    return -1;
}

- (void)drawRect:(NSRect)dirty
{
    NSShadow *shadow = [[NSShadow alloc] init];
    [shadow setShadowColor:[NSColor colorWithGenericGamma22White:0 alpha:0.8]];
    [shadow setShadowOffset:NSMakeSize(0, -1)];
    [shadow setShadowBlurRadius:2];
    NSDictionary *label = @{ NSFontAttributeName: [NSFont systemFontOfSize:12],
                             NSForegroundColorAttributeName: [NSColor whiteColor], NSShadowAttributeName: shadow };

    [[NSColor clearColor] set];
    NSRectFill(dirty);
    for (NSUInteger i = 0; i < [_items count]; i++) {
        NSRect cell = [self rectForIndex:i];
        if (!NSIntersectsRect(cell, dirty))
            continue;
        FilesItem *item = _items[i];
        BOOL selected = [_selection containsIndex:i];
        NSRect icon = NSMakeRect(NSMidX(cell) - kIcon / 2, NSMinY(cell) + 4, kIcon, kIcon);
        if (selected || (NSInteger)i == _dropTarget) {
            [[NSColor colorWithGenericGamma22White:0 alpha:0.3] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:NSInsetRect(icon, -4, -4) xRadius:6 yRadius:6] fill];
        }
        [(isVolume(item.URL) ? volumeIcon() : item.icon) drawInRect:icon fromRect:NSZeroRect
               operation:NSCompositeSourceOver fraction:1 respectFlipped:YES hints:nil];
        NSString *name = isVolume(item.URL) ? [item.URL lastPathComponent] : item.name;
        NSSize size = [name sizeWithAttributes:label];
        CGFloat width = MIN(size.width, kCellWidth - 8);
        NSRect text = NSMakeRect(NSMidX(cell) - width / 2 - 4, NSMaxY(icon) + 5, width + 8, size.height + 2);
        if (selected) {
            [[NSColor colorWithSRGBRed:0.25 green:0.50 blue:0.85 alpha:1] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:text xRadius:4 yRadius:4] fill];
        }
        [name drawInRect:NSInsetRect(text, 4, 0) withAttributes:label];
    }
}

- (void)selectOnly:(NSInteger)index
{
    [_selection removeAllIndexes];
    if (index >= 0)
        [_selection addIndex:(NSUInteger)index];
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSInteger index = [self indexAtPoint:p];

    [[self window] makeFirstResponder:self];
    if (([event modifierFlags] & NSCommandKeyMask) && index >= 0) {
        if ([_selection containsIndex:(NSUInteger)index])
            [_selection removeIndex:(NSUInteger)index];
        else
            [_selection addIndex:(NSUInteger)index];
        [self setNeedsDisplay:YES];
    } else if (index < 0 || ![_selection containsIndex:(NSUInteger)index])
        [self selectOnly:index];
    _pressed = index;
    _downPoint = p;
    if ([event clickCount] == 2 && index >= 0)
        [self.desktop open:[[self selectedItems] valueForKey:@"URL"]];
}

- (void)mouseUp:(NSEvent *)event
{
    _pressed = -1;
}

- (void)mouseDragged:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];

    if (_pressed < 0 || hypot(p.x - _downPoint.x, p.y - _downPoint.y) < 4)
        return;
    FilesItem *item = _items[(NSUInteger)_pressed];
    _pressed = -1;
    NSArray *urls = [[self selectedItems] valueForKey:@"URL"];
    if (![urls containsObject:item.URL])
        urls = @[ item.URL ];
    NSPasteboard *pb = [NSPasteboard pasteboardWithName:NSDragPboard];
    [pb clearContents];
    [pb writeObjects:urls];
    [pb setPropertyList:[urls valueForKey:@"path"] forType:NSFilenamesPboardType];
    NSImage *icon = [item.icon copy];
    [icon setSize:NSMakeSize(kIcon, kIcon)];
    [self dragImage:icon at:NSMakePoint(p.x - kIcon / 2, p.y + kIcon / 2) offset:NSZeroSize event:event
         pasteboard:pb source:self slideBack:YES];
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)local
{
    return NSDragOperationCopy | NSDragOperationMove | NSDragOperationGeneric | NSDragOperationDelete;
}

/* Command-Delete: to the Trash (volumes: ejected); Command-E: eject. */
- (void)keyDown:(NSEvent *)event
{
    NSString *chars = [event charactersIgnoringModifiers];
    unichar c = [chars length] ? [chars characterAtIndex:0] : 0;
    NSArray<NSURL *> *urls = [[self selectedItems] valueForKey:@"URL"];

    if (([event modifierFlags] & NSCommandKeyMask) && (c == NSDeleteCharacter || c == NSBackspaceCharacter)) {
        NSMutableArray *volumes = [NSMutableArray array], *files = [NSMutableArray array];
        for (NSURL *u in urls)
            [isVolume(u) ? volumes : files addObject:u];
        if ([files count] > 0)
            [[FilesOperations shared] trashItems:files];
        if ([volumes count] > 0)
            [self.desktop eject:volumes];
        [self.desktop reload];
        return;
    }
    if (([event modifierFlags] & NSCommandKeyMask) && c == 'e') {
        [self.desktop eject:urls];
        return;
    }
    if (c == NSCarriageReturnCharacter && ([event modifierFlags] & NSCommandKeyMask) == 0 && [urls count] > 0)
        return;
    [super keyDown:event];
}

/* -- The context menu -- */

- (NSMenu *)menuForEvent:(NSEvent *)event
{
    NSPoint p = [self convertPoint:[event locationInWindow] fromView:nil];
    NSInteger index = [self indexAtPoint:p];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem *item;

    if (index >= 0 && ![_selection containsIndex:(NSUInteger)index])
        [self selectOnly:index];
    if (index < 0) {
        [self selectOnly:-1];
        item = [menu addItemWithTitle:NSLocalizedString(@"New Folder", nil) action:@selector(newFolder:) keyEquivalent:@""];
        [item setTarget:self];
        return menu;
    }
    BOOL volume = isVolume(_items[(NSUInteger)index].URL);
    [[menu addItemWithTitle:NSLocalizedString(@"Open", nil) action:@selector(openSelection:) keyEquivalent:@""] setTarget:self];
    [[menu addItemWithTitle:NSLocalizedString(@"Get Info", nil) action:@selector(getInfo:) keyEquivalent:@""] setTarget:self];
    [menu addItem:[NSMenuItem separatorItem]];
    if (volume)
        item = [menu addItemWithTitle:[NSString stringWithFormat:NSLocalizedString(@"Eject “%@”", nil),
                                       [_items[(NSUInteger)index].URL lastPathComponent]]
                               action:@selector(ejectSelection:) keyEquivalent:@""];
    else
        item = [menu addItemWithTitle:NSLocalizedString(@"Move to Trash", nil) action:@selector(trashSelection:)
                        keyEquivalent:@""];
    [item setTarget:self];
    return menu;
}

- (void)openSelection:(id)sender { [self.desktop open:[[self selectedItems] valueForKey:@"URL"]]; }
- (void)ejectSelection:(id)sender { [self.desktop eject:[[self selectedItems] valueForKey:@"URL"]]; }

- (void)getInfo:(id)sender
{
    for (FilesItem *item in [self selectedItems])
        [FilesInfoWindow showForItem:item];
}

- (void)trashSelection:(id)sender
{
    [[FilesOperations shared] trashItems:[[self selectedItems] valueForKey:@"URL"]];
    [self.desktop reload];
}

- (void)newFolder:(id)sender
{
    NSURL *desktop = [NSURL fileURLWithPath:[NSHomeDirectory() stringByAppendingPathComponent:@"Desktop"]];
    [[FilesOperations shared] newFolderIn:desktop];
    [self.desktop reload];
}

/* -- Drops: onto a folder or volume, into it; elsewhere, onto ~/Desktop -- */

static NSArray<NSURL *> *dropURLs(id<NSDraggingInfo> info)
{
    NSMutableArray *urls = [NSMutableArray array];
    id paths = [[info draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if ([paths isKindOfClass:[NSArray class]])
        for (id p in paths)
            if ([p isKindOfClass:[NSString class]])
                [urls addObject:[NSURL fileURLWithPath:p]];
    return urls;
}

- (NSURL *)dropFolderFor:(id<NSDraggingInfo>)info
{
    NSInteger index = [self indexAtPoint:[self convertPoint:[info draggingLocation] fromView:nil]];
    NSArray *urls = dropURLs(info);
    NSInteger target = -1;
    NSURL *folder = [NSURL fileURLWithPath:[NSHomeDirectory() stringByAppendingPathComponent:@"Desktop"]];

    if (index >= 0 && _items[(NSUInteger)index].isFolder && ![urls containsObject:_items[(NSUInteger)index].URL]) {
        target = index;
        folder = _items[(NSUInteger)index].URL;
    }
    if (target != _dropTarget) {
        _dropTarget = target;
        [self setNeedsDisplay:YES];
    }
    return folder;
}

static dev_t deviceOf(NSURL *url)
{
    struct stat st;
    return stat([[url path] fileSystemRepresentation], &st) == 0 ? st.st_dev : (dev_t)-1;
}

- (NSDragOperation)operationFor:(id<NSDraggingInfo>)info
{
    NSURL *folder = [self dropFolderFor:info];
    NSArray<NSURL *> *urls = dropURLs(info);

    if ([urls count] == 0)
        return NSDragOperationNone;
    if ([NSEvent modifierFlags] & NSAlternateKeyMask)
        return NSDragOperationCopy;
    if ([[[urls[0] URLByDeletingLastPathComponent] path] isEqual:[folder path]])
        return NSDragOperationNone;
    return deviceOf(urls[0]) == deviceOf(folder) ? NSDragOperationMove : NSDragOperationCopy;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info { return [self operationFor:info]; }
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info { return [self operationFor:info]; }

- (void)draggingExited:(id<NSDraggingInfo>)info
{
    _dropTarget = -1;
    [self setNeedsDisplay:YES];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info
{
    NSDragOperation operation = [self operationFor:info];
    NSURL *folder = [self dropFolderFor:info];
    NSArray<NSURL *> *urls = dropURLs(info);
    FilesDesktop *desktop = self.desktop;

    _dropTarget = -1;
    [self setNeedsDisplay:YES];
    if (operation == NSDragOperationNone)
        return NO;
    os_log_info(OS_LOG_DEFAULT, "desktop drop %{public}s %lu into %{public}s",
                operation == NSDragOperationCopy ? "copy" : "move", (unsigned long)[urls count],
                [[folder path] fileSystemRepresentation]);
    void (^done)(NSArray<NSURL *> *) = ^(NSArray<NSURL *> *made) { [desktop reload]; };
    if (operation == NSDragOperationCopy)
        [[FilesOperations shared] copyItems:urls toFolder:folder done:done];
    else
        [[FilesOperations shared] moveItems:urls toFolder:folder done:done];
    return YES;
}

@end

/* ---- The window and what it shows ---- */

@implementation FilesDesktop {
    NSWindow *_window;
    FilesDesktopView *_view;
    FSEventStreamRef _watch;
}

- (instancetype)initWithOpener:(void (^)(NSURL *))opener
{
    if ((self = [super init]) != nil) {
        _opener = [opener copy];
        _itemURLs = @[];
    }
    return self;
}

/* The main screen without the menu bar; the Dock's own area is left to it,
   as icons stay clear of it. */
- (NSRect)frame
{
    NSScreen *screen = [[NSScreen screens] firstObject];
    return screen != nil ? [screen visibleFrame] : NSMakeRect(0, 0, 1024, 746);
}

- (void)show
{
    _window = [[NSWindow alloc] initWithContentRect:[self frame] styleMask:NSWindowStyleMaskBorderless
                                            backing:NSBackingStoreBuffered defer:NO];
    [_window setLevel:CGWindowLevelForKey(kCGDesktopIconWindowLevelKey)];
    [_window setOpaque:NO];
    [_window setBackgroundColor:[NSColor clearColor]];
    [_window setHasShadow:NO];
    [_window setReleasedWhenClosed:NO];
    [_window setHidesOnDeactivate:NO];
    [_window setExcludedFromWindowsMenu:YES];
    _view = [[FilesDesktopView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth([self frame]), NSHeight([self frame]))];
    _view.desktop = self;
    [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [_window setContentView:_view];
    [_window orderFront:nil];

    NSNotificationCenter *ws = [[NSWorkspace sharedWorkspace] notificationCenter];
    for (NSString *name in @[ NSWorkspaceDidMountNotification, NSWorkspaceDidUnmountNotification,
                              NSWorkspaceDidRenameVolumeNotification ])
        [ws addObserver:self selector:@selector(volumesChanged:) name:name object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(screensChanged:)
                                                 name:NSApplicationDidChangeScreenParametersNotification object:nil];
    /* An icon theme chosen: the icons made again. */
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(themesChanged:)
                                                 name:@"_NSSystemThemesChangedNotification" object:nil];
    /* Preferences: which disks show, hidden files. */
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(preferencesChanged:)
                                                 name:FilesPreferencesChangedNotification object:nil];
    [self watchDesktopFolder];
    [self reload];
}

- (void)volumesChanged:(NSNotification *)note
{
    [self reload];
}

- (void)screensChanged:(NSNotification *)note
{
    [_window setFrame:[self frame] display:YES];
}

static void desktopEvents(ConstFSEventStreamRef stream, void *info, size_t count, void *paths,
    const FSEventStreamEventFlags flags[], const FSEventStreamEventId ids[])
{
    FilesDesktop *self = (__bridge FilesDesktop *)info;
    (void)stream; (void)count; (void)paths; (void)flags; (void)ids;
    [self performSelectorOnMainThread:@selector(reload) withObject:nil waitUntilDone:NO];
}

- (void)watchDesktopFolder
{
    NSString *desktop = [NSHomeDirectory() stringByAppendingPathComponent:@"Desktop"];
    FSEventStreamContext context = { 0, (__bridge void *)self, NULL, NULL, NULL };

    [[NSFileManager defaultManager] createDirectoryAtPath:desktop withIntermediateDirectories:YES attributes:nil error:NULL];
    _watch = FSEventStreamCreate(NULL, desktopEvents, &context, (__bridge CFArrayRef)@[ desktop, @"/Volumes" ],
                                 kFSEventStreamEventIdSinceNow, 0.3, kFSEventStreamCreateFlagNone);
    if (_watch == NULL)
        return;
    FSEventStreamSetDispatchQueue(_watch, dispatch_queue_create("org.minkar.Files.desktop", NULL));
    FSEventStreamStart(_watch);
}

/* Volumes first (the mounted ones under /Volumes, by name), then the
   desktop folder's items as Files lists them. */
- (void)themesChanged:(NSNotification *)note
{
    volumeIconCache = nil;
    FilesForgetSystemIcons();
    [self reload];
}

- (void)preferencesChanged:(NSNotification *)note
{
    [self reload];
}

- (void)reload
{
    NSMutableArray<FilesItem *> *items = [NSMutableArray array];
    NSMutableArray *volumes = [NSMutableArray array];

    /* As Preferences chose: the startup disk (Hard disks), the mounted
       volumes (External disks). */
    if (FilesDesktopShowsVolumes())
        for (NSString *path in [[NSWorkspace sharedWorkspace] mountedLocalVolumePaths])
            if ([[path stringByDeletingLastPathComponent] isEqualToString:@"/Volumes"])
                [volumes addObject:path];
    [volumes sortUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
    if (FilesDesktopShowsStartupDisk())
        [items addObject:[[FilesItem alloc] initWithURL:[NSURL fileURLWithPath:@"/" isDirectory:YES]]];
    for (NSString *path in volumes)
        [items addObject:[[FilesItem alloc] initWithURL:[NSURL fileURLWithPath:path isDirectory:YES]]];
    NSURL *desktop = [NSURL fileURLWithPath:[NSHomeDirectory() stringByAppendingPathComponent:@"Desktop"]];
    [items addObjectsFromArray:[FilesItem itemsInFolder:desktop error:NULL] ?: @[]];
    _view.items = items;
    self.itemURLs = [items valueForKey:@"URL"];
    os_log_info(OS_LOG_DEFAULT, "desktop %lu items: %{public}s", (unsigned long)[items count],
                [[[items valueForKeyPath:@"URL.lastPathComponent"] componentsJoinedByString:@", "] UTF8String]);
}

- (void)open:(NSArray<NSURL *> *)urls
{
    for (NSURL *url in urls)
        self.opener(url);
}

- (void)eject:(NSArray<NSURL *> *)urls
{
    for (NSURL *url in urls) {
        if (!isVolume(url))
            continue;
        NSError *error = nil;
        if (![[NSWorkspace sharedWorkspace] unmountAndEjectDeviceAtURL:url error:&error]) {
            NSAlert *alert = [[NSAlert alloc] init];
            [alert setMessageText:[NSString stringWithFormat:
                NSLocalizedString(@"The disk “%@” wasn’t ejected because one or more programs may be using it.", nil),
                [url lastPathComponent]]];
            [alert addButtonWithTitle:NSLocalizedString(@"OK", nil)];
            [alert runModal];
        } else
            os_log_info(OS_LOG_DEFAULT, "ejected %{public}s", [[url path] fileSystemRepresentation]);
    }
    [self reload];
}

@end
