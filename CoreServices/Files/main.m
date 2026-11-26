/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Files (Atlas.md, K1): the file manager. Its menus have the
 * reference platform's commands and keys: the app's (Empty Trash, Hide),
 * File, Edit, View, Go (the usual folders, Recent Folders, Go to Folder)
 * and Window.
 */

#import "FilesWindowController.h"
#import "FilesInfoWindow.h"
#import "FilesOperations.h"
#import "FilesDesktop.h"
#import "FilesCommands.h"
#import "FilesItem.h"
#import "FilesPreferences.h"
#import "FilesFind.h"
#include <CoreServices/CoreServices.h>
#include <os/log.h>

@interface FilesApplication : NSObject <NSApplicationDelegate, NSMenuDelegate>
@end

@implementation FilesApplication {
    NSMutableArray<FilesWindowController *> *_windows;
    NSMenu *_openWithMenu, *_recentMenu;
    id _keyMonitor;
    FilesDesktop *_desktop;
}

- (FilesWindowController *)front
{
    for (FilesWindowController *c in _windows)
        if ([c.window isKeyWindow] || [c.window isMainWindow])
            return c;
    return [_windows lastObject];
}

- (FilesWindowController *)openWindowAt:(NSURL *)folder
{
    if (_windows == nil)
        _windows = [NSMutableArray array];
    FilesWindowController *c = [[FilesWindowController alloc] initWithFolder:folder];
    [_windows addObject:c];
    [c.window makeKeyAndOrderFront:nil];
    return c;
}

/* A window on the folder: one showing it already, else a new one. */
- (FilesWindowController *)windowFor:(NSURL *)folder
{
    NSString *path = [[folder path] stringByStandardizingPath];

    for (FilesWindowController *c in _windows)
        if ([[[c.folder path] stringByStandardizingPath] isEqualToString:path]) {
            [c.window makeKeyAndOrderFront:nil];
            return c;
        }
    return [self openWindowAt:folder];
}

/* LaunchServices: folders to open (Files opens public.folder). */
- (void)application:(NSApplication *)app openURLs:(NSArray<NSURL *> *)urls
{
    for (NSURL *url in urls) {
        BOOL dir = NO;
        if (![url isFileURL] || ![[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&dir])
            continue;
        os_log_info(OS_LOG_DEFAULT, "open request %{public}s", [[url path] fileSystemRepresentation]);
        if (dir)
            [self windowFor:url];
        else
            [self revealURL:url];
    }
}

/* LSRevealInFiles (open -R, NSWorkspace's reveal methods): each item
 * selected in a window on its folder. */
- (void)application:(NSApplication *)app revealURLs:(NSArray<NSURL *> *)urls
{
    for (NSURL *url in urls)
        if ([url isFileURL])
            [self revealURL:url];
}

- (void)revealURL:(NSURL *)url
{
    NSString *path = [[url path] stringByStandardizingPath];

    if (![[NSFileManager defaultManager] fileExistsAtPath:path])
        return;
    os_log_info(OS_LOG_DEFAULT, "reveal %{public}s", [path fileSystemRepresentation]);
    if ([path isEqualToString:@"/"]) {
        [self windowFor:[NSURL fileURLWithPath:@"/"]];
        return;
    }
    FilesWindowController *c = [self windowFor:[NSURL fileURLWithPath:[path stringByDeletingLastPathComponent]]];
    [c selectItemNamed:[path lastPathComponent]];
}

static NSMenuItem *item(NSMenu *menu, NSString *title, id target, SEL action, NSString *key, NSUInteger modifiers)
{
    NSMenuItem *i = [menu addItemWithTitle:title action:action keyEquivalent:key];
    [i setTarget:target];
    if (modifiers)
        [i setKeyEquivalentModifierMask:modifiers];
    return i;
}

static NSMenuItem *alternate(NSMenu *menu, NSString *title, id target, SEL action, NSString *key, NSUInteger modifiers)
{
    NSMenuItem *i = item(menu, title, target, action, key, modifiers);
    [i setAlternate:YES];
    return i;
}

static NSMenu *submenu(NSMenu *menu, NSString *title)
{
    NSMenuItem *top = [menu addItemWithTitle:title action:NULL keyEquivalent:@""];
    NSMenu *sub = [[NSMenu alloc] initWithTitle:title];
    [menu setSubmenu:sub forItem:top];
    return sub;
}

/* The reference platform's file manager's menus, the commands Files has
   (FilesCommands.h); Option shows the alternates (Close All, Copy as
   Pathname, Deselect All, Empty Trash without asking...). No Quit: Files
   is always running in a session. */
- (void)buildMenus
{
    NSUInteger cmd = NSCommandKeyMask, shift = NSCommandKeyMask | NSShiftKeyMask;
    NSUInteger opt = NSCommandKeyMask | NSAlternateKeyMask, ctrl = NSCommandKeyMask | NSControlKeyMask;
    NSString *up = [NSString stringWithFormat:@"%C", (unichar)NSUpArrowFunctionKey];
    NSString *backspace = [NSString stringWithFormat:@"%C", (unichar)NSBackspaceCharacter];
    NSString *name = NSLocalizedString(@"Files", nil);
    NSMenu *main = [[NSMenu alloc] initWithTitle:@"Main"];

    NSMenu *app = submenu(main, name);
    item(app, [NSString stringWithFormat:NSLocalizedString(@"About %@", nil), name], nil,
         @selector(orderFrontStandardAboutPanel:), @"", 0);
    [app addItem:[NSMenuItem separatorItem]];
    item(app, NSLocalizedString(@"Preferences…", nil), self, @selector(showPreferences:), @",", cmd);
    [app addItem:[NSMenuItem separatorItem]];
    item(app, NSLocalizedString(@"Empty Trash…", nil), self, @selector(emptyTrash:), backspace, shift);
    alternate(app, NSLocalizedString(@"Empty Trash", nil), self, @selector(emptyTrashNow:), backspace,
              shift | NSAlternateKeyMask);
    [app addItem:[NSMenuItem separatorItem]];
    [NSApp setServicesMenu:submenu(app, NSLocalizedString(@"Services", nil))];
    [app addItem:[NSMenuItem separatorItem]];
    item(app, [NSString stringWithFormat:NSLocalizedString(@"Hide %@", nil), name], nil, @selector(hide:), @"h", cmd);
    item(app, NSLocalizedString(@"Hide Others", nil), nil, @selector(hideOtherApplications:), @"h", opt);
    item(app, NSLocalizedString(@"Show All", nil), nil, @selector(unhideAllApplications:), @"", 0);

    NSMenu *file = submenu(main, NSLocalizedString(@"File", nil));
    item(file, NSLocalizedString(@"New Window", nil), self, @selector(newWindow:), @"n", cmd);
    item(file, NSLocalizedString(@"New Folder", nil), self, @selector(newFolder:), @"n", shift);
    item(file, NSLocalizedString(@"New Folder with Selection", nil), self, @selector(newFolderWithSelection:), @"n",
         ctrl);
    item(file, NSLocalizedString(@"Open", nil), self, @selector(openSelection:), @"o", cmd);
    NSMenuItem *openWith = item(file, NSLocalizedString(@"Open With", nil), nil, NULL, @"", 0);
    _openWithMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Open With", nil)];
    [_openWithMenu setDelegate:self];
    [file setSubmenu:_openWithMenu forItem:openWith];
    item(file, NSLocalizedString(@"Close Window", nil), self, @selector(closeWindow:), @"w", cmd);
    alternate(file, NSLocalizedString(@"Close All", nil), self, @selector(closeAll:), @"w", opt);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Get Info", nil), self, @selector(getInfo:), @"i", cmd);
    item(file, NSLocalizedString(@"Rename", nil), self, @selector(rename:), @"", 0);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Compress", nil), self, @selector(compress:), @"", 0);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Duplicate", nil), self, @selector(duplicate:), @"d", cmd);
    item(file, NSLocalizedString(@"Make Alias", nil), self, @selector(makeAlias:), @"a", ctrl);
    item(file, NSLocalizedString(@"Show Original", nil), self, @selector(showOriginal:), @"r", cmd);
    item(file, NSLocalizedString(@"Add to Dock", nil), self, @selector(addToDock:), @"t", ctrl | NSShiftKeyMask);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Move to Trash", nil), self, @selector(moveToTrash:), backspace, cmd);
    alternate(file, NSLocalizedString(@"Delete Immediately…", nil), self, @selector(deleteImmediately:), backspace,
              opt);
    item(file, NSLocalizedString(@"Put Back", nil), self, @selector(putBack:), @"", 0);
    item(file, NSLocalizedString(@"Eject", nil), self, @selector(eject:), @"e", cmd);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Find", nil), self, @selector(find:), @"f", cmd);

    NSMenu *edit = submenu(main, NSLocalizedString(@"Edit", nil));
    item(edit, NSLocalizedString(@"Undo", nil), self, @selector(undo:), @"z", cmd);
    item(edit, NSLocalizedString(@"Redo", nil), self, @selector(redo:), @"z", shift);
    [edit addItem:[NSMenuItem separatorItem]];
    item(edit, NSLocalizedString(@"Copy", nil), self, @selector(copy:), @"c", cmd);
    alternate(edit, NSLocalizedString(@"Copy as Pathname", nil), self, @selector(copyPathname:), @"c", opt);
    item(edit, NSLocalizedString(@"Paste Item", nil), self, @selector(paste:), @"v", cmd);
    alternate(edit, NSLocalizedString(@"Move Item Here", nil), self, @selector(moveItemsHere:), @"v", opt);
    item(edit, NSLocalizedString(@"Select All", nil), self, @selector(selectAll:), @"a", cmd);
    alternate(edit, NSLocalizedString(@"Deselect All", nil), self, @selector(deselectAll:), @"a", opt);

    NSMenu *view = submenu(main, NSLocalizedString(@"View", nil));
    item(view, NSLocalizedString(@"as Icons", nil), self, @selector(showAsIcons:), @"1", cmd);
    item(view, NSLocalizedString(@"as List", nil), self, @selector(showAsList:), @"2", cmd);
    item(view, NSLocalizedString(@"as Columns", nil), self, @selector(showAsColumns:), @"3", cmd);
    [view addItem:[NSMenuItem separatorItem]];
    NSMenu *sort = submenu(view, NSLocalizedString(@"Sort By", nil));
    NSArray *keys = @[ @[ @"Name", @(FilesSortByName), @"1" ], @[ @"Kind", @(FilesSortByKind), @"2" ],
                       @[ @"Date Modified", @(FilesSortByDateModified), @"5" ],
                       @[ @"Date Created", @(FilesSortByDateCreated), @"" ], @[ @"Size", @(FilesSortBySize), @"6" ] ];
    for (NSArray *k in keys)
        [item(sort, NSLocalizedString(k[0], nil), self, @selector(sortBy:), k[2],
              [k[2] length] ? (ctrl | NSAlternateKeyMask) : 0) setTag:[k[1] integerValue]];
    [view addItem:[NSMenuItem separatorItem]];
    item(view, NSLocalizedString(@"Hide Sidebar", nil), self, @selector(toggleSidebar:), @"s", opt);
    [view addItem:[NSMenuItem separatorItem]];
    item(view, NSLocalizedString(@"Show Path Bar", nil), self, @selector(togglePathBar:), @"p", opt);
    item(view, NSLocalizedString(@"Hide Status Bar", nil), self, @selector(toggleStatusBar:), @"/", cmd);
    [view addItem:[NSMenuItem separatorItem]];
    item(view, NSLocalizedString(@"Show Hidden Files", nil), self, @selector(toggleHiddenFiles:), @".", shift);

    NSMenu *go = submenu(main, NSLocalizedString(@"Go", nil));
    item(go, NSLocalizedString(@"Back", nil), self, @selector(goBack:), @"[", cmd);
    item(go, NSLocalizedString(@"Forward", nil), self, @selector(goForward:), @"]", cmd);
    item(go, NSLocalizedString(@"Enclosing Folder", nil), self, @selector(goToEnclosingFolder:), up, cmd);
    alternate(go, NSLocalizedString(@"Enclosing Folder in New Window", nil), self,
              @selector(enclosingFolderInNewWindow:), up, ctrl);
    [go addItem:[NSMenuItem separatorItem]];
    item(go, NSLocalizedString(@"Documents", nil), self, @selector(goDocuments:), @"o", shift);
    item(go, NSLocalizedString(@"Desktop", nil), self, @selector(goDesktop:), @"d", shift);
    item(go, NSLocalizedString(@"Downloads", nil), self, @selector(goDownloads:), @"l", opt);
    item(go, NSLocalizedString(@"Home", nil), self, @selector(goHome:), @"h", shift);
    item(go, NSLocalizedString(@"Library", nil), self, @selector(goLibrary:), @"l", shift);
    item(go, NSLocalizedString(@"Computer", nil), self, @selector(goComputer:), @"c", shift);
    item(go, NSLocalizedString(@"Applications", nil), self, @selector(goApplications:), @"a", shift);
    item(go, NSLocalizedString(@"Utilities", nil), self, @selector(goUtilities:), @"u", shift);
    [go addItem:[NSMenuItem separatorItem]];
    NSMenuItem *recent = item(go, NSLocalizedString(@"Recent Folders", nil), nil, NULL, @"", 0);
    _recentMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Recent Folders", nil)];
    [_recentMenu setDelegate:self];
    [go setSubmenu:_recentMenu forItem:recent];
    [go addItem:[NSMenuItem separatorItem]];
    item(go, NSLocalizedString(@"Go to Folder…", nil), self, @selector(goToFolder:), @"g", shift);

    NSMenu *window = submenu(main, NSLocalizedString(@"Window", nil));
    item(window, NSLocalizedString(@"Minimize", nil), nil, @selector(performMiniaturize:), @"m", cmd);
    alternate(window, NSLocalizedString(@"Minimize All", nil), nil, @selector(miniaturizeAll:), @"m", opt);
    item(window, NSLocalizedString(@"Zoom", nil), nil, @selector(performZoom:), @"", 0);
    [window addItem:[NSMenuItem separatorItem]];
    item(window, NSLocalizedString(@"Bring All to Front", nil), nil, @selector(arrangeInFront:), @"", 0);
    [NSApp setWindowsMenu:window];

    [NSApp setMainMenu:main];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self buildMenus];
    /* Shift-Command-Period by its key (as the reference matches it): the
       event's characters, Shift applied, are ">" on most layouts, which
       the menu's "." does not match. */
    __weak FilesApplication *weakApp = self;
    _keyMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask handler:^NSEvent *(NSEvent *event) {
        NSUInteger mods = [event modifierFlags] & (NSCommandKeyMask | NSShiftKeyMask | NSAlternateKeyMask | NSControlKeyMask);
        if ([event keyCode] == 0x2F && mods == (NSCommandKeyMask | NSShiftKeyMask)) {
            [weakApp toggleHiddenFiles:nil];
            return nil;
        }
        return event;
    }];
    /* The desktop's icons (K3): folders and volumes open in a window here,
       disk images are attached, the rest goes to its application. */
    __weak FilesApplication *weakSelf = self;
    _desktop = [[FilesDesktop alloc] initWithOpener:^(NSURL *url) {
        BOOL dir = NO;
        [[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&dir];
        if (dir && ![[url pathExtension] isEqualToString:@"app"])
            [weakSelf windowFor:url];
        else if ([[[url pathExtension] lowercaseString] isEqualToString:@"dmg"])
            [[FilesOperations shared] attachDiskImage:url done:^(NSURL *volume) {
                if (volume != nil)
                    [weakSelf windowFor:volume];
            }];
        else
            [[NSWorkspace sharedWorkspace] openURL:url];
    }];
    [_desktop show];
    /* Launched to open or show something: those windows only. */
    if ([_windows count] == 0)
        [self openWindowAt:FilesNewWindowFolder()];
}

/* Files stays when its last window closes, as the reference platform's
   file manager does. */
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
    return NO;
}

- (void)applicationDidBecomeActive:(NSNotification *)note
{
    BOOL visible = NO;
    for (FilesWindowController *c in _windows)
        visible |= [c.window isVisible];
    if (!visible && [_windows count] == 0)
        [self openWindowAt:FilesNewWindowFolder()];
}

- (void)newWindow:(id)sender
{
    [self openWindowAt:FilesNewWindowFolder()];
}

- (void)closeWindow:(id)sender
{
    FilesWindowController *c = [self front];
    if (c == nil)
        return;
    [c.window orderOut:nil];
    [_windows removeObject:c];
}

- (void)openSelection:(id)sender { [[self front] openSelection:sender]; }

/* -- File operations (K2) -- */

- (void)copy:(id)sender { [[self front] copy:sender]; }
- (void)selectAll:(id)sender { [[self front] selectAll:sender]; }
- (void)paste:(id)sender { [[self front] paste:sender]; }
- (void)moveItemsHere:(id)sender { [[self front] moveItemsHere:sender]; }
- (void)duplicate:(id)sender { [[self front] duplicate:sender]; }
- (void)newFolder:(id)sender { [[self front] newFolder:sender]; }
- (void)rename:(id)sender { [[self front] rename:sender]; }
- (void)moveToTrash:(id)sender { [[self front] moveToTrash:sender]; }
- (void)putBack:(id)sender { [[self front] putBack:sender]; }
- (void)deselectAll:(id)sender { [[self front] deselectAll:sender]; }

/* -- The commands beyond K2 (FilesCommands.h) -- */

- (void)closeAll:(id)sender
{
    for (FilesWindowController *c in [_windows copy])
        [c.window orderOut:nil];
    [_windows removeAllObjects];
}

- (void)emptyTrash:(id)sender { FilesEmptyTrash(YES); }
- (void)emptyTrashNow:(id)sender { FilesEmptyTrash(NO); }

- (void)deleteImmediately:(id)sender
{
    FilesWindowController *front = [self front];
    if (FilesDeleteImmediately(front.selectedURLs))
        [front reloadSelecting:nil];
}

- (void)compress:(id)sender
{
    FilesWindowController *front = [self front];
    FilesCompress(front.selectedURLs, ^(NSURL *archive) {
        if (archive != nil && [[[front.folder path] stringByStandardizingPath]
                                   isEqualToString:[[[archive URLByDeletingLastPathComponent] path] stringByStandardizingPath]])
            [front reloadSelecting:[archive lastPathComponent]];
    });
}

- (void)makeAlias:(id)sender
{
    FilesWindowController *front = [self front];
    NSArray<NSURL *> *made = FilesMakeAliases(front.selectedURLs);
    if ([made count] > 0)
        [front reloadSelecting:[[made lastObject] lastPathComponent]];
}

- (void)showOriginal:(id)sender
{
    NSURL *original = FilesOriginalOf([[[self front] selectedURLs] firstObject]);
    if (original == nil) {
        NSBeep();
        return;
    }
    [self revealURL:original];
}

- (void)newFolderWithSelection:(id)sender
{
    FilesWindowController *front = [self front];
    NSURL *made = FilesNewFolderWithItems(front.selectedURLs, front.folder);
    if (made == nil)
        return;
    [front reloadSelecting:[made lastPathComponent]];
    [front rename:nil];
}

- (void)addToDock:(id)sender { FilesAddToDock([[self front] selectedURLs]); }
- (void)copyPathname:(id)sender
{
    NSArray<NSURL *> *urls = [[self front] selectedURLs];
    FilesCopyPathnames([urls count] > 0 ? urls : ([self front] ? @[ [self front].folder ] : @[]));
}

- (void)eject:(id)sender
{
    NSMutableArray<NSURL *> *volumes = [NSMutableArray array];
    for (NSURL *url in [[self front] selectedURLs])
        if ([[[url URLByDeletingLastPathComponent] path] isEqualToString:@"/Volumes"])
            [volumes addObject:url];
    if ([volumes count] == 0) {
        NSBeep();
        return;
    }
    [_desktop eject:volumes];
}

- (void)enclosingFolderInNewWindow:(id)sender
{
    FilesWindowController *front = [self front];
    NSURL *folder = front.folder;
    if (folder == nil || [[folder path] isEqualToString:@"/"]) {
        NSBeep();
        return;
    }
    FilesWindowController *c = [self openWindowAt:[folder URLByDeletingLastPathComponent]];
    [c selectItemNamed:[folder lastPathComponent]];
}

- (void)goToFolder:(id)sender
{
    NSURL *url = FilesAskFolder([[self front] window]);
    if (url == nil)
        return;
    BOOL dir = NO;
    [[NSFileManager defaultManager] fileExistsAtPath:[url path] isDirectory:&dir];
    if (dir)
        [self go:[url path]];
    else
        [self revealURL:url];
}

/* View: the sort and the bars, for every window. */
- (void)sortBy:(NSMenuItem *)sender
{
    FilesSetSortKey((FilesSortKey)[sender tag]);
    for (FilesWindowController *c in _windows)
        [c sortChanged];
}

- (void)toggleBar:(NSString *)key byDefault:(BOOL)byDefault
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    id value = [defaults objectForKey:key];
    [defaults setBool:!(value != nil ? [value boolValue] : byDefault) forKey:key];
    [[NSNotificationCenter defaultCenter] postNotificationName:FilesBarsChangedNotification object:nil];
}

- (void)find:(id)sender
{
    __weak FilesApplication *weakSelf = self;
    NSURL *folder = [self front] != nil ? [self front].folder : FilesNewWindowFolder();
    [FilesFind showInFolder:folder reveal:^(NSURL *url) {
        [weakSelf revealURL:url];
    }];
}

- (void)showPreferences:(id)sender { [FilesPreferences show]; }
- (void)toggleHiddenFiles:(id)sender { FilesSetShowsHiddenFiles(!FilesShowsHiddenFiles()); }
- (void)toggleSidebar:(id)sender { [self toggleBar:@"FilesShowSidebar" byDefault:YES]; }
- (void)toggleStatusBar:(id)sender { [self toggleBar:@"FilesShowStatusBar" byDefault:YES]; }
- (void)togglePathBar:(id)sender { [self toggleBar:@"FilesShowPathBar" byDefault:NO]; }

- (void)openRecentFolder:(NSMenuItem *)sender { [self go:[[sender representedObject] path]]; }
- (void)clearRecentFolders:(id)sender { FilesClearRecentFolders(); }

- (void)undo:(id)sender
{
    [[FilesOperations shared].undoManager undo];
    [[self front] reloadSelecting:nil];
}

- (void)redo:(id)sender
{
    [[FilesOperations shared].undoManager redo];
    [[self front] reloadSelecting:nil];
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = [menuItem action];
    FilesWindowController *front = [self front];
    NSArray<NSURL *> *selected = front != nil ? front.selectedURLs : @[];
    NSUndoManager *undo = [FilesOperations shared].undoManager;
    BOOL inTrash = [selected count] > 0 && [FilesOperations isInTrash:selected[0]];

    if (action == @selector(undo:)) {
        [menuItem setTitle:[undo undoMenuItemTitle]];
        return [undo canUndo];
    }
    if (action == @selector(redo:)) {
        [menuItem setTitle:[undo redoMenuItemTitle]];
        return [undo canRedo];
    }
    if (action == @selector(paste:) || action == @selector(moveItemsHere:))
        return front != nil && [[FilesWindowController pasteboardURLs] count] > 0;
    if (action == @selector(newFolder:))
        return front != nil;
    if (action == @selector(putBack:))
        return inTrash;
    if (action == @selector(moveToTrash:))
        return [selected count] > 0 && !inTrash;
    if (action == @selector(copy:) || action == @selector(duplicate:) || action == @selector(rename:) ||
        action == @selector(makeAlias:) || action == @selector(deselectAll:))
        return [selected count] > 0;
    if (action == @selector(newFolderWithSelection:))
        return [selected count] > 0 && !inTrash;
    if (action == @selector(deleteImmediately:))
        return [selected count] > 0;
    if (action == @selector(showOriginal:))
        return [selected count] == 1 && FilesOriginalOf(selected[0]) != nil;
    if (action == @selector(addToDock:)) {
        for (NSURL *url in selected)
            if ([[[url pathExtension] lowercaseString] isEqualToString:@"app"])
                return YES;
        return NO;
    }
    if (action == @selector(eject:)) {
        for (NSURL *url in selected)
            if ([[[url URLByDeletingLastPathComponent] path] isEqualToString:@"/Volumes"])
                return YES;
        return NO;
    }
    if (action == @selector(emptyTrash:) || action == @selector(emptyTrashNow:))
        return !FilesTrashIsEmpty();
    if (action == @selector(enclosingFolderInNewWindow:))
        return front != nil && ![[front.folder path] isEqualToString:@"/"];
    if (action == @selector(toggleHiddenFiles:)) {
        [menuItem setTitle:NSLocalizedString(FilesShowsHiddenFiles() ? @"Hide Hidden Files" : @"Show Hidden Files", nil)];
        return YES;
    }
    if (action == @selector(goBack:))
        return front.canGoBack;
    if (action == @selector(goForward:))
        return front.canGoForward;
    if (action == @selector(sortBy:)) {
        [menuItem setState:[menuItem tag] == FilesCurrentSortKey() ? NSControlStateValueOn : NSControlStateValueOff];
        return front != nil;
    }
    if (action == @selector(toggleSidebar:) || action == @selector(toggleStatusBar:) ||
        action == @selector(togglePathBar:)) {
        NSString *key = action == @selector(toggleSidebar:) ? @"FilesShowSidebar"
                      : action == @selector(toggleStatusBar:) ? @"FilesShowStatusBar" : @"FilesShowPathBar";
        NSString *what = action == @selector(toggleSidebar:) ? @"Sidebar"
                       : action == @selector(toggleStatusBar:) ? @"Status Bar" : @"Path Bar";
        id value = [[NSUserDefaults standardUserDefaults] objectForKey:key];
        BOOL shown = value != nil ? [value boolValue] : action != @selector(togglePathBar:);
        [menuItem setTitle:NSLocalizedString([NSString stringWithFormat:shown ? @"Hide %@" : @"Show %@", what], nil)];
        return YES;
    }
    /* With one item, its name in the title, as the reference shows it. */
    if (action == @selector(compress:) || action == @selector(copyPathname:)) {
        NSString *base = action == @selector(compress:) ? NSLocalizedString(@"Compress", nil)
                                                        : NSLocalizedString(@"Copy as Pathname", nil);
        NSString *one = action == @selector(compress:) ? NSLocalizedString(@"Compress “%@”", nil)
                                                       : NSLocalizedString(@"Copy “%@” as Pathname", nil);
        NSURL *target = [selected count] == 1 ? selected[0] : ([selected count] == 0 ? front.folder : nil);
        if (target != nil)
            [menuItem setTitle:[NSString stringWithFormat:one, [[NSFileManager defaultManager]
                                                                   displayNameAtPath:[target path]]]];
        else if (action == @selector(compress:))
            [menuItem setTitle:[NSString stringWithFormat:NSLocalizedString(@"Compress %lu Items", nil),
                                                          (unsigned long)[selected count]]];
        else
            [menuItem setTitle:base];
        return action == @selector(compress:) ? [selected count] > 0 : front != nil;
    }
    return YES;
}

/* Open With: the applications LaunchServices has for the selection, its
   default first. */
- (void)menuNeedsUpdate:(NSMenu *)menu
{
    NSURL *url = [[[self front] selectedURLs] firstObject];

    if (menu == _recentMenu) {
        [menu removeAllItems];
        for (NSURL *folder in FilesRecentFolders()) {
            NSMenuItem *i = [menu addItemWithTitle:[[NSFileManager defaultManager] displayNameAtPath:[folder path]]
                                            action:@selector(openRecentFolder:) keyEquivalent:@""];
            [i setTarget:self];
            [i setRepresentedObject:folder];
            NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:[folder path]];
            [icon setSize:NSMakeSize(16, 16)];
            [i setImage:icon];
        }
        [menu addItem:[NSMenuItem separatorItem]];
        [[menu addItemWithTitle:NSLocalizedString(@"Clear Menu", nil) action:@selector(clearRecentFolders:)
                  keyEquivalent:@""] setTarget:self];
        return;
    }
    if (menu != _openWithMenu)
        return;
    [menu removeAllItems];
    if (url == nil)
        return;
    NSURL *preferred = [[NSWorkspace sharedWorkspace] URLForApplicationToOpenURL:url];
    NSMutableArray<NSURL *> *apps = [NSMutableArray array];
    if (preferred != nil)
        [apps addObject:preferred];
    for (NSURL *app in CFBridgingRelease(LSCopyApplicationURLsForURL((__bridge CFURLRef)url, kLSRolesAll)) ?: @[])
        if (![apps containsObject:app])
            [apps addObject:app];
    for (NSURL *app in apps) {
        NSString *name = [[NSFileManager defaultManager] displayNameAtPath:[app path]];
        if ([app isEqual:preferred])
            name = [NSString stringWithFormat:NSLocalizedString(@"%@ (default)", nil), name];
        NSMenuItem *i = [menu addItemWithTitle:name action:@selector(openWithApplication:) keyEquivalent:@""];
        [i setTarget:self];
        [i setRepresentedObject:app];
        NSImage *icon = [[NSWorkspace sharedWorkspace] iconForFile:[app path]];
        [icon setSize:NSMakeSize(16, 16)];
        [i setImage:icon];
    }
    if ([apps count] == 0)
        [[menu addItemWithTitle:NSLocalizedString(@"No Applications", nil) action:NULL keyEquivalent:@""] setEnabled:NO];
}

- (void)openWithApplication:(NSMenuItem *)sender
{
    NSArray<NSURL *> *urls = [[self front] selectedURLs];
    NSError *error = nil;

    if ([urls count] == 0)
        return;
    if ([[NSWorkspace sharedWorkspace] openURLs:urls withApplicationAtURL:[sender representedObject]
                                        options:NSWorkspaceLaunchDefault configuration:@{} error:&error] == nil)
        NSBeep();
}
- (void)getInfo:(id)sender
{
    FilesItem *item = [[self front] selectedItem];
    if (item == nil && [self front] != nil)
        item = [[FilesItem alloc] initWithURL:[[self front] folder]];
    if (item != nil)
        [FilesInfoWindow showForItem:item];
}
- (void)showAsIcons:(id)sender { [[self front] showAsIcons:sender]; }
- (void)showAsList:(id)sender { [[self front] showAsList:sender]; }
- (void)showAsColumns:(id)sender { [[self front] showAsColumns:sender]; }
- (void)goBack:(id)sender { [[self front] goBack:sender]; }
- (void)goForward:(id)sender { [[self front] goForward:sender]; }
- (void)goToEnclosingFolder:(id)sender { [[self front] goToEnclosingFolder:sender]; }

- (void)go:(NSString *)path
{
    if ([self front] == nil)
        [self openWindowAt:[NSURL fileURLWithPath:path]];
    else
        [[self front] goToFolder:[NSURL fileURLWithPath:path]];
}

- (void)goComputer:(id)sender { [self go:@"/"]; }
- (void)goHome:(id)sender { [self go:NSHomeDirectory()]; }
- (void)goDesktop:(id)sender { [self go:[NSHomeDirectory() stringByAppendingPathComponent:@"Desktop"]]; }
- (void)goDocuments:(id)sender { [self go:[NSHomeDirectory() stringByAppendingPathComponent:@"Documents"]]; }
- (void)goApplications:(id)sender { [self go:@"/Applications"]; }
- (void)goUtilities:(id)sender { [self go:@"/Applications/Utilities"]; }
- (void)goDownloads:(id)sender { [self go:[NSHomeDirectory() stringByAppendingPathComponent:@"Downloads"]]; }
- (void)goLibrary:(id)sender { [self go:[NSHomeDirectory() stringByAppendingPathComponent:@"Library"]]; }

@end

int main(void)
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        FilesApplication *delegate = [[FilesApplication alloc] init];

        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
