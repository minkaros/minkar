/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The file manager's browser window (Atlas.md, K1): navigation with history,
 * the sidebar and the browser, opening folders here and applications with
 * NSWorkspace; icons, a list or columns. Atlas's provisional look.
 */

#import "FilesWindowController.h"
#import "FilesColors.h"
#import "FilesSegmentedControl.h"
#import "FilesSidebarView.h"
#import "FilesColumnView.h"
#import "FilesOperations.h"

/* AppKit's (Minkar): the window's buttons centered in a full-size content
   view's own bar, until unified toolbars exist. */
@interface NSWindow (FilesTitlebarHeight)
- (void)_setTitlebarHeight:(CGFloat)height;
@end
#include <sys/mount.h>
#include <os/log.h>
#include <CoreServices/CoreServices.h>

/* The bar is the title bar too (a full-size content view): 52 points, a
   unified toolbar's height on the reference platform, its controls in line
   with the window's buttons. */
static const CGFloat kBarHeight = 52, kStatusHeight = 22, kSidebarWidth = 170;

@interface FilesWindowController () <FilesBrowserViewDelegate, FilesColumnViewDelegate, FilesSidebarViewDelegate,
                                     NSWindowDelegate>
@end

/* The window's content: it lays out its parts when resized. */
@interface FilesContentView : NSView
@property (nonatomic, weak) FilesWindowController *controller;
@end

@implementation FilesWindowController {
    NSWindow *_window;
    NSURL *_folder;
    NSMutableArray<NSURL *> *_back, *_forward;
    FilesSidebarView *_sidebar;
    NSScrollView *_scroll, *_columnScroll;
    FilesBrowserView *_browser;
    FilesColumnView *_columns;
    FilesSegmentedControl *_navigation, *_viewModes;
    NSTextField *_title, *_status;
    FSEventStreamRef _watch;      /* the folder shown, while it changes */
    dispatch_queue_t _watchQueue;
}

static NSTextField *label(CGFloat size, BOOL bold)
{
    NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 20)];

    [field setEditable:NO];
    [field setSelectable:NO];
    [field setBordered:NO];
    [field setDrawsBackground:NO];
    [field setFont:bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size]];
    return field;
}

- (instancetype)initWithFolder:(NSURL *)folder
{
    if ((self = [super init]) == nil)
        return nil;
    _back = [NSMutableArray array];
    _forward = [NSMutableArray array];

    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(160, 140, 860, 552)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
                  NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView
          backing:NSBackingStoreBuffered defer:NO];
    [_window setTitleVisibility:NSWindowTitleHidden];
    [_window setTitlebarAppearsTransparent:YES];
    if ([_window respondsToSelector:@selector(_setTitlebarHeight:)])
        [_window _setTitlebarHeight:kBarHeight];
    [_window setReleasedWhenClosed:NO];
    [_window setMinSize:NSMakeSize(480, 280)];
    [_window setDelegate:self];
    /* Where the last window was left (moved, resized or closed), as on the
       reference; a step down and right when another window is there. */
    if ([_window setFrameUsingName:@"Files Window"])
        for (NSWindow *other in [NSApp windows])
            if (other != _window && [other isVisible] && NSEqualPoints([other frame].origin, [_window frame].origin))
                [_window setFrameTopLeftPoint:[_window cascadeTopLeftFromPoint:NSMakePoint(NSMinX([_window frame]), NSMaxY([_window frame]))]];

    FilesContentView *content = [[FilesContentView alloc] initWithFrame:[[_window contentView] frame]];
    content.controller = self;
    [_window setContentView:content];

    _navigation = [[FilesSegmentedControl alloc] initWithGlyphs:@[ @(FilesGlyphBack), @(FilesGlyphForward) ]
        actions:@[ @"goBack:", @"goForward:" ] target:self];
    [_navigation setLabel:NSLocalizedString(@"Back", nil) forSegment:0];
    [_navigation setLabel:NSLocalizedString(@"Forward", nil) forSegment:1];
    _viewModes = [[FilesSegmentedControl alloc]
        initWithGlyphs:@[ @(FilesGlyphIcons), @(FilesGlyphList), @(FilesGlyphColumns) ]
               actions:@[ @"showAsIcons:", @"showAsList:", @"showAsColumns:" ] target:self];
    [_viewModes setLabel:NSLocalizedString(@"Icons", nil) forSegment:0];
    [_viewModes setLabel:NSLocalizedString(@"List", nil) forSegment:1];
    [_viewModes setLabel:NSLocalizedString(@"Columns", nil) forSegment:2];
    _title = label(13, YES);
    [_title setAlignment:NSCenterTextAlignment];
    _status = label(11, NO);
    [_status setAlignment:NSCenterTextAlignment];
    [_status setTextColor:FilesGray(0.35, 1)];

    _sidebar = [[FilesSidebarView alloc] initWithFrame:NSMakeRect(0, 0, kSidebarWidth, 100)];
    _sidebar.delegate = self;
    _scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    [_scroll setHasVerticalScroller:YES];
    [_scroll setAutohidesScrollers:YES];
    [_scroll setBorderType:NSNoBorder];
    _browser = [[FilesBrowserView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    _browser.delegate = self;
    [_scroll setDocumentView:_browser];
    _columnScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    [_columnScroll setHasVerticalScroller:YES];
    [_columnScroll setHasHorizontalScroller:YES];
    [_columnScroll setAutohidesScrollers:YES];
    [_columnScroll setBorderType:NSNoBorder];
    _columns = [[FilesColumnView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    _columns.delegate = self;
    [_columnScroll setDocumentView:_columns];
    [_columnScroll setHidden:YES];

    for (NSView *view in @[ _navigation, _title, _viewModes,
                            _sidebar, _scroll, _columnScroll, _status ])
        [content addSubview:view];
    [self layout];
    [self goToFolder:folder remember:NO];
    [_window makeFirstResponder:_browser];
    return self;
}

- (NSWindow *)window
{
    return _window;
}

- (NSURL *)folder
{
    return _folder;
}

- (FilesItem *)selectedItem
{
    return _browser.mode == FilesViewModeColumns ? _columns.selectedItem : _browser.selectedItem;
}

- (void)layout
{
    NSRect b = [[_window contentView] bounds];
    CGFloat top = NSMaxY(b) - kBarHeight;

    /* The bar spans the window, above the sidebar. The window's buttons
       are at its left, over the sidebar; back and forward follow, where
       the folder's contents begin, and the view modes are at its right.
       The folder's name is in the middle of what is left. */
    NSSize nav = [_navigation naturalSize], modes = [_viewModes naturalSize];
    CGFloat mid = top + kBarHeight / 2;
    CGFloat navX = kSidebarWidth + 10;
    CGFloat modesX = NSWidth(b) - 10 - modes.width;
    [_navigation setFrame:NSMakeRect(navX, floor(mid - nav.height / 2), nav.width, nav.height)];
    [_viewModes setFrame:NSMakeRect(modesX, floor(mid - modes.height / 2), modes.width, modes.height)];
    CGFloat left = navX + nav.width + 10, right = modesX - 10;
    [_title setFrame:NSMakeRect(left, floor(mid - 10), MAX(right - left, 0), 20)];
    [_sidebar setFrame:NSMakeRect(0, 0, kSidebarWidth, NSHeight(b) - kBarHeight)];
    NSRect content = NSMakeRect(kSidebarWidth, kStatusHeight, NSWidth(b) - kSidebarWidth,
                                NSHeight(b) - kBarHeight - kStatusHeight);
    [_scroll setFrame:content];
    [_columnScroll setFrame:content];
    [_status setFrame:NSMakeRect(kSidebarWidth, 3, NSWidth(b) - kSidebarWidth, 16)];
    [_browser relayout];
    [_columns relayout];
}

/* -- Navigation -- */

- (void)goToFolder:(NSURL *)folder remember:(BOOL)remember
{
    NSError *error = nil;
    NSArray *items = [FilesItem itemsInFolder:folder error:&error];

    if (items == nil) {
        NSBeep();
        return;
    }
    if (remember && _folder != nil) {
        [_back addObject:_folder];
        [_forward removeAllObjects];
    }
    _browser.items = items;
    if (_browser.mode == FilesViewModeColumns)
        [_columns showRootFolder:folder];
    [self showFolder:folder];
}

/* The window shows the folder: its name, the sidebar, the watch. */
- (void)showFolder:(NSURL *)folder
{
    _folder = [folder copy];
    [self watchFolder];
    os_log_info(OS_LOG_DEFAULT, "folder %{public}s", [[folder path] fileSystemRepresentation]);
    _sidebar.currentURL = folder;
    NSString *name = [[folder path] isEqualToString:@"/"] ? @"Minkar" : [folder lastPathComponent];
    [_title setStringValue:name];
    [_window setTitle:name];
    [self updateButtons];
    [self updateStatus];
}

- (void)goToFolder:(NSURL *)folder
{
    [self goToFolder:folder remember:YES];
}

- (void)selectItemNamed:(NSString *)name
{
    if (_browser.mode == FilesViewModeColumns)
        [_columns selectItemNamed:name];
    else
        [_browser selectItemNamed:name];
}

/* The folder shown is read again when it changes (an item made, removed
 * or renamed in it), keeping the selection: an FSEvents stream on it,
 * whose events name the folders changed (a subfolder's change is not this
 * folder's). */
static void folderEvents(ConstFSEventStreamRef stream, void *info, size_t count, void *paths,
    const FSEventStreamEventFlags flags[], const FSEventStreamEventId ids[])
{
    FilesWindowController *self = (__bridge FilesWindowController *)info;
    char resolved[PATH_MAX];
    const char *folder = realpath([[self->_folder path] fileSystemRepresentation], resolved);

    (void)stream; (void)ids;
    for (size_t i = 0; i < count; i++) {
        const char *path = ((char **)paths)[i];
        size_t n = folder ? strlen(folder) : 0;
        BOOL here = folder && strncmp(path, folder, n) == 0 &&
            (strcmp(path + n, "/") == 0 || (strcmp(folder, "/") == 0 && strcmp(path, "/") == 0));
        if (here || (flags[i] & kFSEventStreamEventFlagMustScanSubDirs)) {
            [self performSelectorOnMainThread:@selector(folderChanged) withObject:nil waitUntilDone:NO];
            return;
        }
    }
}

- (void)watchFolder
{
    if (_watch != NULL) {
        FSEventStreamStop(_watch);
        FSEventStreamInvalidate(_watch);
        FSEventStreamRelease(_watch);
        _watch = NULL;
    }
    if (_watchQueue == nil)
        _watchQueue = dispatch_queue_create("org.minkar.Files.folder", NULL);
    FSEventStreamContext context = { 0, (__bridge void *)self, NULL, NULL, NULL };
    _watch = FSEventStreamCreate(NULL, folderEvents, &context, (__bridge CFArrayRef)@[ [_folder path] ],
                                 kFSEventStreamEventIdSinceNow, 0.3, kFSEventStreamCreateFlagNone);
    if (_watch == NULL)
        return;
    FSEventStreamSetDispatchQueue(_watch, _watchQueue);
    if (!FSEventStreamStart(_watch)) {
        FSEventStreamInvalidate(_watch);
        FSEventStreamRelease(_watch);
        _watch = NULL;
    }
}

- (void)folderChanged
{
    NSArray<NSString *> *selected = [_browser.selectedItems valueForKeyPath:@"URL.lastPathComponent"];
    NSArray *items = [FilesItem itemsInFolder:_folder error:NULL];

    if (_browser.mode == FilesViewModeColumns) {
        /* Columns read again; a folder gone takes its columns with it. */
        [_columns reload];
        os_log_info(OS_LOG_DEFAULT, "reload %{public}s", [[_folder path] fileSystemRepresentation]);
        if (![_columns.deepestFolder isEqual:_folder])
            [self showFolder:_columns.deepestFolder];
        else
            [self updateStatus];
        return;
    }

    if (items == nil) {
        /* Gone: its enclosing folder. */
        [self goToEnclosingFolder:nil];
        return;
    }
    os_log_info(OS_LOG_DEFAULT, "reload %{public}s", [[_folder path] fileSystemRepresentation]);
    _browser.items = items;
    if ([selected count] > 0)
        [_browser selectItemsNamed:selected];
    [self updateStatus];
}

- (void)dealloc
{
    if (_watch != NULL) {
        FSEventStreamStop(_watch);
        FSEventStreamInvalidate(_watch);
        FSEventStreamRelease(_watch);
    }
}

- (void)updateButtons
{
    [_navigation setEnabled:[_back count] > 0 forSegment:0];
    [_navigation setEnabled:[_forward count] > 0 forSegment:1];
    [_viewModes setSelectedSegment:_browser.mode == FilesViewModeIcons ? 0
                                 : _browser.mode == FilesViewModeList ? 1 : 2];
}

- (void)updateStatus
{
    NSUInteger count = _browser.mode == FilesViewModeColumns ? _columns.deepestItemCount : [_browser.items count];
    struct statfs fs;
    NSString *available = @"";

    if (statfs([[_folder path] fileSystemRepresentation], &fs) == 0)
        available = [NSString stringWithFormat:NSLocalizedString(@", %@ available", nil),
                     FilesSizeString((unsigned long long)fs.f_bavail * fs.f_bsize)];
    NSString *items = count == 1 ? NSLocalizedString(@"1 item", nil) :
        [NSString stringWithFormat:NSLocalizedString(@"%lu items", nil), (unsigned long)count];
    [_status setStringValue:[items stringByAppendingString:available]];
}

- (IBAction)goBack:(id)sender
{
    if ([_back count] == 0)
        return;
    NSURL *to = [_back lastObject];
    [_back removeLastObject];
    [_forward addObject:_folder];
    [self goToFolder:to remember:NO];
}

- (IBAction)goForward:(id)sender
{
    if ([_forward count] == 0)
        return;
    NSURL *to = [_forward lastObject];
    [_forward removeLastObject];
    [_back addObject:_folder];
    [self goToFolder:to remember:NO];
}

- (IBAction)goToEnclosingFolder:(id)sender
{
    if ([[_folder path] isEqualToString:@"/"])
        return;
    NSString *child = [_folder lastPathComponent];
    [self goToFolder:[_folder URLByDeletingLastPathComponent]];
    if (_browser.mode == FilesViewModeColumns)
        [_columns selectItemNamed:child];
    else
        [_browser selectItemNamed:child];
}

- (IBAction)openSelection:(id)sender
{
    if (self.selectedItem != nil)
        [self openItem:self.selectedItem];
}

- (void)setMode:(FilesViewMode)mode
{
    BOOL columns = mode == FilesViewModeColumns;

    if (columns && _browser.mode != FilesViewModeColumns)
        [_columns showRootFolder:_folder];
    _browser.mode = mode;
    [_scroll setHidden:columns];
    [_columnScroll setHidden:!columns];
    [_columns relayout];
    [_window makeFirstResponder:columns ? (NSView *)_columns : (NSView *)_browser];
    os_log_info(OS_LOG_DEFAULT, "view %{public}s", columns ? "columns" : mode == FilesViewModeList ? "list" : "icons");
    [self updateButtons];
    [self updateStatus];
}

- (IBAction)showAsIcons:(id)sender
{
    [self setMode:FilesViewModeIcons];
}

- (IBAction)showAsList:(id)sender
{
    [self setMode:FilesViewModeList];
}

- (IBAction)showAsColumns:(id)sender
{
    [self setMode:FilesViewModeColumns];
}

/* Folders here; applications and documents with NSWorkspace. */
- (void)openItem:(FilesItem *)item
{
    os_log_info(OS_LOG_DEFAULT, "open %{public}s", [[item.URL path] fileSystemRepresentation]);
    if (item.isFolder && _browser.mode == FilesViewModeColumns) {
        /* Its column is already shown, next to it. */
        [_window makeFirstResponder:_columns];
    } else if (item.isFolder)
        [self goToFolder:item.URL];
    else if ([[[item.URL pathExtension] lowercaseString] isEqualToString:@"dmg"])
        /* A disk image: attached, its volume shown (DMG step 3). */
        [[FilesOperations shared] attachDiskImage:item.URL done:^(NSURL *volume) {
            if (volume != nil)
                [self goToFolder:volume];
        }];
    else if (![[NSWorkspace sharedWorkspace] openFile:[item.URL path]])
        NSBeep();   /* documents open with LaunchServices' open code (Phase 3, item 4) */
}

/* -- File operations (K2) -- */

- (NSArray<NSURL *> *)selectedURLs
{
    if (_browser.mode != FilesViewModeColumns)
        return [_browser.selectedItems valueForKey:@"URL"];
    FilesItem *item = self.selectedItem;
    return item != nil ? @[ item.URL ] : @[];
}

- (void)reloadSelecting:(NSString *)name
{
    [self folderChanged];
    if (name != nil)
        [self selectItemNamed:name];
}

/* What an operation made, selected when it is in the folder shown. */
- (void)selectFirstOf:(NSArray<NSURL *> *)urls
{
    if ([urls count] > 0 && [[[urls[0] URLByDeletingLastPathComponent] path] isEqual:[_folder path]]) {
        [self folderChanged];
        if (_browser.mode == FilesViewModeColumns)
            [self selectItemNamed:[urls[0] lastPathComponent]];
        else
            [_browser selectItemsNamed:[urls valueForKey:@"lastPathComponent"]];
    } else
        [self reloadSelecting:nil];
}

/* Paths (NSFilenamesPboardType) for apps that read the older type, and
   file URLs. */
- (IBAction)selectAll:(id)sender
{
    if (_browser.mode != FilesViewModeColumns)
        [_browser selectAll:sender];
}

- (IBAction)copy:(id)sender
{
    NSArray<NSURL *> *urls = self.selectedURLs;
    NSPasteboard *pb = [NSPasteboard generalPasteboard];

    if ([urls count] == 0) {
        NSBeep();
        return;
    }
    [pb clearContents];
    [pb writeObjects:urls];
    [pb setPropertyList:[urls valueForKey:@"path"] forType:NSFilenamesPboardType];
}

+ (NSArray<NSURL *> *)pasteboardURLs
{
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSMutableArray *urls = [NSMutableArray array];
    id paths = [pb propertyListForType:NSFilenamesPboardType];

    if ([paths isKindOfClass:[NSArray class]])
        for (id p in paths)
            if ([p isKindOfClass:[NSString class]])
                [urls addObject:[NSURL fileURLWithPath:p]];
    if ([urls count] == 0)
        for (id u in [pb readObjectsForClasses:@[ [NSURL class] ] options:nil] ?: @[])
            if ([u isFileURL])
                [urls addObject:u];
    return urls;
}

- (void)pasteMoving:(BOOL)moving
{
    NSArray<NSURL *> *urls = [FilesWindowController pasteboardURLs];
    FilesOperations *ops = [FilesOperations shared];
    void (^done)(NSArray<NSURL *> *) = ^(NSArray<NSURL *> *made) { [self selectFirstOf:made]; };

    if ([urls count] == 0) {
        NSBeep();
        return;
    }
    if (moving)
        [ops moveItems:urls toFolder:_folder done:done];
    else
        [ops copyItems:urls toFolder:_folder done:done];
}

- (IBAction)paste:(id)sender
{
    [self pasteMoving:NO];
}

- (IBAction)moveItemsHere:(id)sender
{
    [self pasteMoving:YES];
}

- (IBAction)duplicate:(id)sender
{
    if ([self.selectedURLs count] == 0) {
        NSBeep();
        return;
    }
    [[FilesOperations shared] duplicateItems:self.selectedURLs done:^(NSArray<NSURL *> *made) {
        [self selectFirstOf:made];
    }];
}

- (IBAction)newFolder:(id)sender
{
    NSURL *made = [[FilesOperations shared] newFolderIn:_folder];
    if (made == nil)
        return;
    [self reloadSelecting:[made lastPathComponent]];
    [self rename:nil];
}

/* The new name asked in a small window (in place, over the item's name,
   later). */
- (IBAction)rename:(id)sender
{
    NSURL *url = [self.selectedURLs firstObject];
    if (url == nil) {
        NSBeep();
        return;
    }
    /* In place in the icon and list views; asked in a window in columns. */
    if (_browser.mode != FilesViewModeColumns && [_browser beginRenaming])
        return;
    NSAlert *alert = [[NSAlert alloc] init];
    NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 260, 24)];
    [field setStringValue:[url lastPathComponent]];
    [alert setMessageText:NSLocalizedString(@"Rename", nil)];
    [alert setInformativeText:[NSString stringWithFormat:NSLocalizedString(@"A new name for “%@”:", nil),
                               [url lastPathComponent]]];
    [alert setAccessoryView:field];
    [alert addButtonWithTitle:NSLocalizedString(@"Rename", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    [[alert window] setInitialFirstResponder:field];
    if ([alert runModal] != NSAlertFirstButtonReturn)
        return;
    NSURL *renamed = [[FilesOperations shared] renameItem:url to:[field stringValue]];
    if (renamed != nil)
        [self reloadSelecting:[renamed lastPathComponent]];
}

- (IBAction)moveToTrash:(id)sender
{
    NSArray<NSURL *> *urls = self.selectedURLs;
    if ([urls count] == 0) {
        NSBeep();
        return;
    }
    [[FilesOperations shared] trashItems:urls];
    [self reloadSelecting:nil];
}

- (IBAction)putBack:(id)sender
{
    NSArray<NSURL *> *urls = self.selectedURLs;
    if ([urls count] == 0 || ![FilesOperations isInTrash:urls[0]]) {
        NSBeep();
        return;
    }
    [[FilesOperations shared] putBackItems:urls];
    [self reloadSelecting:nil];
}

/* -- Delegates -- */

- (void)browserView:(FilesBrowserView *)view openItem:(FilesItem *)item
{
    [self openItem:item];
}

- (void)browserViewSelectionChanged:(FilesBrowserView *)view
{
}

- (void)browserView:(FilesBrowserView *)view renameItem:(FilesItem *)item to:(NSString *)name
{
    NSURL *renamed = [[FilesOperations shared] renameItem:item.URL to:name];
    if (renamed != nil)
        [self reloadSelecting:[renamed lastPathComponent]];
}

- (NSURL *)folderOfBrowserView:(FilesBrowserView *)view
{
    return _folder;
}

- (void)browserView:(FilesBrowserView *)view dropURLs:(NSArray<NSURL *> *)urls
           intoFolder:(NSURL *)folder copying:(BOOL)copying
{
    FilesOperations *ops = [FilesOperations shared];
    void (^done)(NSArray<NSURL *> *) = ^(NSArray<NSURL *> *made) { [self selectFirstOf:made]; };

    os_log_info(OS_LOG_DEFAULT, "drop %{public}s %lu into %{public}s", copying ? "copy" : "move",
                (unsigned long)[urls count], [[folder path] fileSystemRepresentation]);
    if (copying)
        [ops copyItems:urls toFolder:folder done:done];
    else
        [ops moveItems:urls toFolder:folder done:done];
}

- (void)columnView:(FilesColumnView *)view openItem:(FilesItem *)item
{
    [self openItem:item];
}

/* A folder chosen in the columns: the window's folder, kept in Back. */
- (void)columnView:(FilesColumnView *)view showsFolder:(NSURL *)folder
{
    if (_folder != nil && ![_folder isEqual:folder]) {
        [_back addObject:_folder];
        [_forward removeAllObjects];
    }
    [self showFolder:folder];
}

- (void)sidebarView:(FilesSidebarView *)view chooseURL:(NSURL *)url
{
    [self goToFolder:url];
}

- (void)windowDidResize:(NSNotification *)note
{
    [self layout];
    [_window saveFrameUsingName:@"Files Window"];
}

- (void)windowDidMove:(NSNotification *)note
{
    [_window saveFrameUsingName:@"Files Window"];
}

- (void)windowWillClose:(NSNotification *)note
{
    [_window saveFrameUsingName:@"Files Window"];
}

@end

@implementation FilesContentView

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];

    /* White under the folder's items; the bar across the top in gray,
       with a line under it. */
    [FilesGray(1, 1) set];
    NSRectFill(dirty);
    /* The bar: in the dark, the windows' grey (0.17), its line dark. */
    [(FilesDark() ? [NSColor colorWithGenericGamma22White:0.17 alpha:1] : FilesGray(0.82, 1)) set];
    NSRectFill(NSIntersectionRect(dirty, NSMakeRect(0, NSMaxY(b) - kBarHeight, NSWidth(b), kBarHeight)));
    [(FilesDark() ? [NSColor colorWithGenericGamma22White:0.09 alpha:1] : FilesGray(0.74, 1)) set];
    NSRectFill(NSMakeRect(0, NSMaxY(b) - kBarHeight, NSWidth(b), 1));
    [FilesGray(0.85, 1) set];
    NSRectFill(NSMakeRect(0, 21, NSWidth(b), 1));
}

@end
