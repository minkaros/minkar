/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Files (Atlas.md, K1): the file manager. Its menus have the
 * reference platform's commands and keys: File (New Window, Open, Close
 * Window), View (as Icons, as List, as Columns), Go (Back, Forward, Enclosing Folder,
 * the usual folders).
 */

#import "FilesWindowController.h"
#import "FilesInfoWindow.h"
#import "FilesOperations.h"
#include <CoreServices/CoreServices.h>
#include <os/log.h>

@interface FilesApplication : NSObject <NSApplicationDelegate, NSMenuDelegate>
@end

@implementation FilesApplication {
    NSMutableArray<FilesWindowController *> *_windows;
    NSMenu *_openWithMenu;
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

static void addMenu(NSString *title, NSMenu *menu)
{
    NSMenuItem *top = [[NSMenuItem alloc] initWithTitle:title action:NULL keyEquivalent:@""];
    [top setSubmenu:menu];
    [[NSApp mainMenu] addItem:top];
}

- (void)buildMenus
{
    NSUInteger cmd = NSCommandKeyMask, shift = NSCommandKeyMask | NSShiftKeyMask;
    NSString *up = [NSString stringWithFormat:@"%C", (unichar)NSUpArrowFunctionKey];

    NSMenu *file = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"File", nil)];
    item(file, NSLocalizedString(@"New Window", nil), self, @selector(newWindow:), @"n", cmd);
    item(file, NSLocalizedString(@"Open", nil), self, @selector(openSelection:), @"o", cmd);
    NSMenuItem *openWith = item(file, NSLocalizedString(@"Open With", nil), nil, NULL, @"", 0);
    _openWithMenu = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Open With", nil)];
    [_openWithMenu setDelegate:self];
    [file setSubmenu:_openWithMenu forItem:openWith];
    item(file, NSLocalizedString(@"New Folder", nil), self, @selector(newFolder:), @"n", shift);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Get Info", nil), self, @selector(getInfo:), @"i", cmd);
    item(file, NSLocalizedString(@"Rename", nil), self, @selector(rename:), @"", 0);
    item(file, NSLocalizedString(@"Duplicate", nil), self, @selector(duplicate:), @"d", cmd);
    [file addItem:[NSMenuItem separatorItem]];
    NSString *backspace = [NSString stringWithFormat:@"%C", (unichar)NSBackspaceCharacter];
    item(file, NSLocalizedString(@"Move to Trash", nil), self, @selector(moveToTrash:), backspace, cmd);
    item(file, NSLocalizedString(@"Put Back", nil), self, @selector(putBack:), @"", 0);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Close Window", nil), self, @selector(closeWindow:), @"w", cmd);
    addMenu(NSLocalizedString(@"File", nil), file);

    NSMenu *edit = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Edit", nil)];
    item(edit, NSLocalizedString(@"Undo", nil), self, @selector(undo:), @"z", cmd);
    item(edit, NSLocalizedString(@"Redo", nil), self, @selector(redo:), @"z", shift);
    [edit addItem:[NSMenuItem separatorItem]];
    item(edit, NSLocalizedString(@"Copy", nil), self, @selector(copy:), @"c", cmd);
    item(edit, NSLocalizedString(@"Paste Item", nil), self, @selector(paste:), @"v", cmd);
    item(edit, NSLocalizedString(@"Move Item Here", nil), self, @selector(moveItemsHere:), @"v",
         NSCommandKeyMask | NSAlternateKeyMask);
    addMenu(NSLocalizedString(@"Edit", nil), edit);

    NSMenu *view = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", nil)];
    item(view, NSLocalizedString(@"as Icons", nil), self, @selector(showAsIcons:), @"1", cmd);
    item(view, NSLocalizedString(@"as List", nil), self, @selector(showAsList:), @"2", cmd);
    item(view, NSLocalizedString(@"as Columns", nil), self, @selector(showAsColumns:), @"3", cmd);
    addMenu(NSLocalizedString(@"View", nil), view);

    NSMenu *go = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"Go", nil)];
    item(go, NSLocalizedString(@"Back", nil), self, @selector(goBack:), @"[", cmd);
    item(go, NSLocalizedString(@"Forward", nil), self, @selector(goForward:), @"]", cmd);
    item(go, NSLocalizedString(@"Enclosing Folder", nil), self, @selector(goToEnclosingFolder:), up, cmd);
    [go addItem:[NSMenuItem separatorItem]];
    item(go, NSLocalizedString(@"Computer", nil), self, @selector(goComputer:), @"c", shift);
    item(go, NSLocalizedString(@"Home", nil), self, @selector(goHome:), @"h", shift);
    item(go, NSLocalizedString(@"Desktop", nil), self, @selector(goDesktop:), @"d", shift);
    item(go, NSLocalizedString(@"Documents", nil), self, @selector(goDocuments:), @"o", shift);
    item(go, NSLocalizedString(@"Applications", nil), self, @selector(goApplications:), @"a", shift);
    item(go, NSLocalizedString(@"Utilities", nil), self, @selector(goUtilities:), @"u", shift);
    addMenu(NSLocalizedString(@"Go", nil), go);
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self buildMenus];
    /* Launched to open or show something: those windows only. */
    if ([_windows count] == 0)
        [self openWindowAt:[NSURL fileURLWithPath:NSHomeDirectory()]];
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
        [self openWindowAt:[NSURL fileURLWithPath:NSHomeDirectory()]];
}

- (void)newWindow:(id)sender
{
    [self openWindowAt:[NSURL fileURLWithPath:NSHomeDirectory()]];
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
- (void)paste:(id)sender { [[self front] paste:sender]; }
- (void)moveItemsHere:(id)sender { [[self front] moveItemsHere:sender]; }
- (void)duplicate:(id)sender { [[self front] duplicate:sender]; }
- (void)newFolder:(id)sender { [[self front] newFolder:sender]; }
- (void)rename:(id)sender { [[self front] rename:sender]; }
- (void)moveToTrash:(id)sender { [[self front] moveToTrash:sender]; }
- (void)putBack:(id)sender { [[self front] putBack:sender]; }

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
    if (action == @selector(copy:) || action == @selector(duplicate:) || action == @selector(rename:))
        return [selected count] > 0;
    return YES;
}

/* Open With: the applications LaunchServices has for the selection, its
   default first. */
- (void)menuNeedsUpdate:(NSMenu *)menu
{
    NSURL *url = [[[self front] selectedURLs] firstObject];

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
