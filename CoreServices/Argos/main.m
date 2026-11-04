/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Argos (Dock-Argos.md, K1): the file manager. Its menus have the
 * reference platform's commands and keys: File (New Window, Open, Close
 * Window), View (as Icons, as List), Go (Back, Forward, Enclosing Folder,
 * the usual folders).
 */

#import "ArgosWindowController.h"
#import "ArgosInfoWindow.h"

@interface ArgosApplication : NSObject <NSApplicationDelegate>
@end

@implementation ArgosApplication {
    NSMutableArray<ArgosWindowController *> *_windows;
}

- (ArgosWindowController *)front
{
    for (ArgosWindowController *c in _windows)
        if ([c.window isKeyWindow] || [c.window isMainWindow])
            return c;
    return [_windows lastObject];
}

- (void)openWindowAt:(NSURL *)folder
{
    ArgosWindowController *c = [[ArgosWindowController alloc] initWithFolder:folder];
    [_windows addObject:c];
    [c.window makeKeyAndOrderFront:nil];
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
    item(file, NSLocalizedString(@"Get Info", nil), self, @selector(getInfo:), @"i", cmd);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Close Window", nil), self, @selector(closeWindow:), @"w", cmd);
    addMenu(NSLocalizedString(@"File", nil), file);

    NSMenu *view = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", nil)];
    item(view, NSLocalizedString(@"as Icons", nil), self, @selector(showAsIcons:), @"1", cmd);
    item(view, NSLocalizedString(@"as List", nil), self, @selector(showAsList:), @"2", cmd);
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
    _windows = [NSMutableArray array];
    [self buildMenus];
    [self openWindowAt:[NSURL fileURLWithPath:NSHomeDirectory()]];
}

/* Argos stays when its last window closes, as the reference platform's
   file manager does. */
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
    return NO;
}

- (void)applicationDidBecomeActive:(NSNotification *)note
{
    BOOL visible = NO;
    for (ArgosWindowController *c in _windows)
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
    ArgosWindowController *c = [self front];
    if (c == nil)
        return;
    [c.window orderOut:nil];
    [_windows removeObject:c];
}

- (void)openSelection:(id)sender { [[self front] openSelection:sender]; }
- (void)getInfo:(id)sender
{
    ArgosItem *item = [[self front] selectedItem];
    if (item == nil && [self front] != nil)
        item = [[ArgosItem alloc] initWithURL:[[self front] folder]];
    if (item != nil)
        [ArgosInfoWindow showForItem:item];
}
- (void)showAsIcons:(id)sender { [[self front] showAsIcons:sender]; }
- (void)showAsList:(id)sender { [[self front] showAsList:sender]; }
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
        ArgosApplication *delegate = [[ArgosApplication alloc] init];

        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
