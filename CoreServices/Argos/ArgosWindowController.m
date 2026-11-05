/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Argos's browser window (Atlas.md, K1): navigation with history,
 * the sidebar and the browser, opening folders here and applications with
 * NSWorkspace; icons, a list or columns. Atlas's provisional look.
 */

#import "ArgosWindowController.h"
#import "ArgosSidebarView.h"
#import "ArgosColumnView.h"
#include <sys/mount.h>
#include <os/log.h>
#include <CoreServices/CoreServices.h>

static const CGFloat kBarHeight = 38, kStatusHeight = 22, kSidebarWidth = 170;

@interface ArgosWindowController () <ArgosBrowserViewDelegate, ArgosColumnViewDelegate, ArgosSidebarViewDelegate,
                                     NSWindowDelegate>
@end

/* The window's content: it lays out its parts when resized. */
@interface ArgosContentView : NSView
@property (nonatomic, weak) ArgosWindowController *controller;
@end

@implementation ArgosWindowController {
    NSWindow *_window;
    NSURL *_folder;
    NSMutableArray<NSURL *> *_back, *_forward;
    ArgosSidebarView *_sidebar;
    NSScrollView *_scroll, *_columnScroll;
    ArgosBrowserView *_browser;
    ArgosColumnView *_columns;
    NSButton *_backButton, *_forwardButton, *_iconsButton, *_listButton, *_columnsButton;
    NSTextField *_title, *_status;
    FSEventStreamRef _watch;      /* the folder shown, while it changes */
    dispatch_queue_t _watchQueue;
}

static NSButton *barButton(NSString *title, id target, SEL action)
{
    NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(0, 0, 30, 24)];

    [button setTitle:title];
    [button setBezelStyle:NSRoundedBezelStyle];
    [button setTarget:target];
    [button setAction:action];
    return button;
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

    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(160, 140, 860, 520)
        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
          backing:NSBackingStoreBuffered defer:NO];
    [_window setReleasedWhenClosed:NO];
    [_window setMinSize:NSMakeSize(480, 280)];
    [_window setDelegate:self];

    ArgosContentView *content = [[ArgosContentView alloc] initWithFrame:[[_window contentView] frame]];
    content.controller = self;
    [_window setContentView:content];

    _backButton = barButton(@"‹", self, @selector(goBack:));
    _forwardButton = barButton(@"›", self, @selector(goForward:));
    _iconsButton = barButton(NSLocalizedString(@"Icons", nil), self, @selector(showAsIcons:));
    _listButton = barButton(NSLocalizedString(@"List", nil), self, @selector(showAsList:));
    _columnsButton = barButton(NSLocalizedString(@"Columns", nil), self, @selector(showAsColumns:));
    _title = label(13, YES);
    _status = label(11, NO);
    [_status setAlignment:NSCenterTextAlignment];
    [_status setTextColor:[NSColor colorWithCalibratedWhite:0.35 alpha:1]];

    _sidebar = [[ArgosSidebarView alloc] initWithFrame:NSMakeRect(0, 0, kSidebarWidth, 100)];
    _sidebar.delegate = self;
    _scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    [_scroll setHasVerticalScroller:YES];
    [_scroll setAutohidesScrollers:YES];
    [_scroll setBorderType:NSNoBorder];
    _browser = [[ArgosBrowserView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    _browser.delegate = self;
    [_scroll setDocumentView:_browser];
    _columnScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    [_columnScroll setHasVerticalScroller:YES];
    [_columnScroll setHasHorizontalScroller:YES];
    [_columnScroll setAutohidesScrollers:YES];
    [_columnScroll setBorderType:NSNoBorder];
    _columns = [[ArgosColumnView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
    _columns.delegate = self;
    [_columnScroll setDocumentView:_columns];
    [_columnScroll setHidden:YES];

    for (NSView *view in @[ _backButton, _forwardButton, _title, _iconsButton, _listButton, _columnsButton,
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

- (ArgosItem *)selectedItem
{
    return _browser.mode == ArgosViewModeColumns ? _columns.selectedItem : _browser.selectedItem;
}

- (void)layout
{
    NSRect b = [[_window contentView] bounds];
    CGFloat top = NSMaxY(b) - kBarHeight;

    [_backButton setFrame:NSMakeRect(kSidebarWidth + 8, top + 7, 30, 24)];
    [_forwardButton setFrame:NSMakeRect(kSidebarWidth + 40, top + 7, 30, 24)];
    [_title setFrame:NSMakeRect(kSidebarWidth + 80, top + 9, NSWidth(b) - kSidebarWidth - 300, 20)];
    [_iconsButton setFrame:NSMakeRect(NSWidth(b) - 212, top + 7, 64, 24)];
    [_listButton setFrame:NSMakeRect(NSWidth(b) - 144, top + 7, 64, 24)];
    [_columnsButton setFrame:NSMakeRect(NSWidth(b) - 76, top + 7, 68, 24)];
    [_sidebar setFrame:NSMakeRect(0, 0, kSidebarWidth, NSHeight(b))];
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
    NSArray *items = [ArgosItem itemsInFolder:folder error:&error];

    if (items == nil) {
        NSBeep();
        return;
    }
    if (remember && _folder != nil) {
        [_back addObject:_folder];
        [_forward removeAllObjects];
    }
    _browser.items = items;
    if (_browser.mode == ArgosViewModeColumns)
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
    if (_browser.mode == ArgosViewModeColumns)
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
    ArgosWindowController *self = (__bridge ArgosWindowController *)info;
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
        _watchQueue = dispatch_queue_create("org.minkar.argos.folder", NULL);
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
    NSString *selected = [_browser.selectedItem.URL lastPathComponent];
    NSArray *items = [ArgosItem itemsInFolder:_folder error:NULL];

    if (_browser.mode == ArgosViewModeColumns) {
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
    if (selected != nil)
        [_browser selectItemNamed:selected];
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
    [_backButton setEnabled:[_back count] > 0];
    [_forwardButton setEnabled:[_forward count] > 0];
    [_iconsButton setState:_browser.mode == ArgosViewModeIcons ? NSOnState : NSOffState];
    [_listButton setState:_browser.mode == ArgosViewModeList ? NSOnState : NSOffState];
    [_columnsButton setState:_browser.mode == ArgosViewModeColumns ? NSOnState : NSOffState];
}

- (void)updateStatus
{
    NSUInteger count = _browser.mode == ArgosViewModeColumns ? _columns.deepestItemCount : [_browser.items count];
    struct statfs fs;
    NSString *available = @"";

    if (statfs([[_folder path] fileSystemRepresentation], &fs) == 0)
        available = [NSString stringWithFormat:NSLocalizedString(@", %@ available", nil),
                     ArgosSizeString((unsigned long long)fs.f_bavail * fs.f_bsize)];
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
    if (_browser.mode == ArgosViewModeColumns)
        [_columns selectItemNamed:child];
    else
        [_browser selectItemNamed:child];
}

- (IBAction)openSelection:(id)sender
{
    if (self.selectedItem != nil)
        [self openItem:self.selectedItem];
}

- (void)setMode:(ArgosViewMode)mode
{
    BOOL columns = mode == ArgosViewModeColumns;

    if (columns && _browser.mode != ArgosViewModeColumns)
        [_columns showRootFolder:_folder];
    _browser.mode = mode;
    [_scroll setHidden:columns];
    [_columnScroll setHidden:!columns];
    [_columns relayout];
    [_window makeFirstResponder:columns ? (NSView *)_columns : (NSView *)_browser];
    os_log_info(OS_LOG_DEFAULT, "view %{public}s", columns ? "columns" : mode == ArgosViewModeList ? "list" : "icons");
    [self updateButtons];
    [self updateStatus];
}

- (IBAction)showAsIcons:(id)sender
{
    [self setMode:ArgosViewModeIcons];
}

- (IBAction)showAsList:(id)sender
{
    [self setMode:ArgosViewModeList];
}

- (IBAction)showAsColumns:(id)sender
{
    [self setMode:ArgosViewModeColumns];
}

/* Folders here; applications and documents with NSWorkspace. */
- (void)openItem:(ArgosItem *)item
{
    os_log_info(OS_LOG_DEFAULT, "open %{public}s", [[item.URL path] fileSystemRepresentation]);
    if (item.isFolder && _browser.mode == ArgosViewModeColumns) {
        /* Its column is already shown, next to it. */
        [_window makeFirstResponder:_columns];
    } else if (item.isFolder)
        [self goToFolder:item.URL];
    else if (![[NSWorkspace sharedWorkspace] openFile:[item.URL path]])
        NSBeep();   /* documents open with LaunchServices' open code (Phase 3, item 4) */
}

/* -- Delegates -- */

- (void)browserView:(ArgosBrowserView *)view openItem:(ArgosItem *)item
{
    [self openItem:item];
}

- (void)browserViewSelectionChanged:(ArgosBrowserView *)view
{
}

- (void)columnView:(ArgosColumnView *)view openItem:(ArgosItem *)item
{
    [self openItem:item];
}

/* A folder chosen in the columns: the window's folder, kept in Back. */
- (void)columnView:(ArgosColumnView *)view showsFolder:(NSURL *)folder
{
    if (_folder != nil && ![_folder isEqual:folder]) {
        [_back addObject:_folder];
        [_forward removeAllObjects];
    }
    [self showFolder:folder];
}

- (void)sidebarView:(ArgosSidebarView *)view chooseURL:(NSURL *)url
{
    [self goToFolder:url];
}

- (void)windowDidResize:(NSNotification *)note
{
    [self layout];
}

@end

@implementation ArgosContentView

- (void)drawRect:(NSRect)dirty
{
    NSRect b = [self bounds];

    [[NSColor colorWithCalibratedWhite:0.97 alpha:1] set];
    NSRectFill(dirty);
    [[NSColor colorWithCalibratedWhite:0.82 alpha:1] set];
    NSRectFill(NSMakeRect(0, NSMaxY(b) - 38, NSWidth(b), 1));
    NSRectFill(NSMakeRect(0, 21, NSWidth(b), 1));
}

@end
