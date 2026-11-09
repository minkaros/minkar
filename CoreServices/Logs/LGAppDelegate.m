/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The app's menus (the template's, with View: the sources, Pause, Clear
 * and Show in Files) and its one window.
 */

#import "LGAppDelegate.h"

static NSMenuItem *item(NSMenu *menu, NSString *title, SEL action, NSString *key, NSUInteger modifiers)
{
    NSMenuItem *i = [menu addItemWithTitle:title action:action keyEquivalent:key ?: @""];

    if (key != nil)
        [i setKeyEquivalentModifierMask:modifiers];
    return i;
}

static NSMenu *submenu(NSMenu *parent, NSString *title)
{
    NSMenuItem *holder = [parent addItemWithTitle:title action:NULL keyEquivalent:@""];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:title];

    [holder setSubmenu:menu];
    return menu;
}

@implementation LGAppDelegate

- (void)buildMenus
{
    NSUInteger cmd = NSCommandKeyMask, opt = NSCommandKeyMask | NSAlternateKeyMask;
    NSString *name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"] ?: @"Logs";
    NSMenu *main = [[NSMenu alloc] initWithTitle:@"Main"];
    LGWindowController *wc = self.windowController;

    NSMenu *app = submenu(main, name);
    item(app, [NSString stringWithFormat:NSLocalizedString(@"About %@", nil), name],
        @selector(orderFrontStandardAboutPanel:), nil, 0);
    [app addItem:[NSMenuItem separatorItem]];
    [NSApp setServicesMenu:submenu(app, NSLocalizedString(@"Services", nil))];
    [app addItem:[NSMenuItem separatorItem]];
    item(app, [NSString stringWithFormat:NSLocalizedString(@"Hide %@", nil), name], @selector(hide:), @"h", cmd);
    item(app, NSLocalizedString(@"Hide Others", nil), @selector(hideOtherApplications:), @"h", opt);
    item(app, NSLocalizedString(@"Show All", nil), @selector(unhideAllApplications:), nil, 0);
    [app addItem:[NSMenuItem separatorItem]];
    item(app, [NSString stringWithFormat:NSLocalizedString(@"Quit %@", nil), name], @selector(terminate:), @"q", cmd);

    NSMenu *file = submenu(main, NSLocalizedString(@"File", nil));
    [item(file, NSLocalizedString(@"Show in Files", nil), @selector(reveal:), @"r", cmd) setTarget:wc];
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Close", nil), @selector(performClose:), @"w", cmd);

    NSMenu *edit = submenu(main, NSLocalizedString(@"Edit", nil));
    item(edit, NSLocalizedString(@"Copy", nil), @selector(copy:), @"c", cmd);
    item(edit, NSLocalizedString(@"Select All", nil), @selector(selectAll:), @"a", cmd);
    [edit addItem:[NSMenuItem separatorItem]];
    [item(edit, NSLocalizedString(@"Clear", nil), @selector(clear:), @"k", cmd) setTarget:wc];

    NSMenu *view = submenu(main, NSLocalizedString(@"View", nil));
    NSArray *sources = @[ NSLocalizedString(@"Messages", nil), NSLocalizedString(@"Crash Reports", nil),
        NSLocalizedString(@"Log Files", nil), NSLocalizedString(@"My Logs", nil) ];
    for (NSInteger i = 0; i < 4; i++) {
        NSMenuItem *source = item(view, sources[i], @selector(chooseSource:),
            [NSString stringWithFormat:@"%ld", (long)i + 1], cmd);
        [source setTag:i];
        [source setTarget:self];
    }
    [view addItem:[NSMenuItem separatorItem]];
    [item(view, NSLocalizedString(@"Pause", nil), @selector(togglePause:), @"p", opt) setTarget:wc];

    NSMenu *window = submenu(main, NSLocalizedString(@"Window", nil));
    item(window, NSLocalizedString(@"Minimize", nil), @selector(performMiniaturize:), @"m", cmd);
    item(window, NSLocalizedString(@"Zoom", nil), @selector(performZoom:), nil, 0);
    [window addItem:[NSMenuItem separatorItem]];
    [item(window, NSLocalizedString(@"Logs", nil), @selector(showMainWindow:), @"1", opt) setTarget:self];
    [NSApp setWindowsMenu:window];

    NSMenu *help = submenu(main, NSLocalizedString(@"Help", nil));
    item(help, [NSString stringWithFormat:NSLocalizedString(@"%@ Help", nil), name], @selector(showHelp:), @"?", cmd);

    [NSApp setMainMenu:main];
}

- (void)chooseSource:(NSMenuItem *)sender
{
    [self.windowController setSource:[sender tag]];
}

- (void)applicationWillFinishLaunching:(NSNotification *)note
{
    _windowController = [LGWindowController new];
    [self buildMenus];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [self showMainWindow:nil];
}

- (void)showMainWindow:(id)sender
{
    [[self.windowController window] makeKeyAndOrderFront:nil];
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)app hasVisibleWindows:(BOOL)visible
{
    if (!visible)
        [self showMainWindow:nil];
    return YES;
}

- (void)applicationWillTerminate:(NSNotification *)note
{
    [self.windowController.reader stop];
}

@end
