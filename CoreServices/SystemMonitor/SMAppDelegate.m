/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The app's menus (the template's, with View: the four tabs, Quit
 * Process and Get Info) and its one window, shown again from the Dock.
 */

#import "SMAppDelegate.h"

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

@implementation SMAppDelegate

- (void)buildMenus
{
    NSUInteger cmd = NSCommandKeyMask, opt = NSCommandKeyMask | NSAlternateKeyMask;
    NSString *name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"] ?: @"System Monitor";
    NSMenu *main = [[NSMenu alloc] initWithTitle:@"Main"];

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
    item(file, NSLocalizedString(@"Close", nil), @selector(performClose:), @"w", cmd);

    NSMenu *edit = submenu(main, NSLocalizedString(@"Edit", nil));
    item(edit, NSLocalizedString(@"Copy", nil), @selector(copy:), @"c", cmd);
    item(edit, NSLocalizedString(@"Select All", nil), @selector(selectAll:), @"a", cmd);

    NSMenu *view = submenu(main, NSLocalizedString(@"View", nil));
    NSArray *tabs = @[ NSLocalizedString(@"CPU", nil), NSLocalizedString(@"Memory", nil),
        NSLocalizedString(@"Disk", nil), NSLocalizedString(@"Network", nil) ];
    for (NSInteger i = 0; i < 4; i++) {
        NSMenuItem *tab = item(view, tabs[i], @selector(chooseTab:), [NSString stringWithFormat:@"%ld", (long)i + 1], cmd);
        [tab setTag:i];
        [tab setTarget:self.windowController];
    }
    [view addItem:[NSMenuItem separatorItem]];
    NSArray *filters = @[ NSLocalizedString(@"All Processes", nil), NSLocalizedString(@"My Processes", nil),
        NSLocalizedString(@"System Processes", nil), NSLocalizedString(@"Windowed Processes", nil) ];
    for (NSInteger i = 0; i < 4; i++) {
        NSMenuItem *filter = item(view, filters[i], @selector(chooseProcessFilter:), nil, 0);
        [filter setTag:i];
        [filter setTarget:self.windowController];
    }
    [view addItem:[NSMenuItem separatorItem]];
    NSMenu *frequency = submenu(view, NSLocalizedString(@"Update Frequency", nil));
    NSArray *paces = @[ @[ @1, NSLocalizedString(@"Very Often (1 sec)", nil) ],
        @[ @2, NSLocalizedString(@"Often (2 sec)", nil) ], @[ @5, NSLocalizedString(@"Normally (5 sec)", nil) ] ];
    for (NSArray *pace in paces) {
        NSMenuItem *every = item(frequency, pace[1], @selector(chooseUpdateInterval:), nil, 0);
        [every setTag:[pace[0] integerValue]];
        [every setTarget:self.windowController];
    }
    [view addItem:[NSMenuItem separatorItem]];
    [item(view, NSLocalizedString(@"Quit Process", nil), @selector(stopSelectedProcess:), @"q", opt)
        setTarget:self.windowController];
    [item(view, NSLocalizedString(@"Get Info", nil), @selector(showProcessInfo:), @"i", cmd)
        setTarget:self.windowController];

    NSMenu *window = submenu(main, NSLocalizedString(@"Window", nil));
    item(window, NSLocalizedString(@"Minimize", nil), @selector(performMiniaturize:), @"m", cmd);
    item(window, NSLocalizedString(@"Zoom", nil), @selector(performZoom:), nil, 0);
    [window addItem:[NSMenuItem separatorItem]];
    [item(window, NSLocalizedString(@"System Monitor", nil), @selector(showMainWindow:), @"1", opt) setTarget:self];
    [NSApp setWindowsMenu:window];

    NSMenu *help = submenu(main, NSLocalizedString(@"Help", nil));
    item(help, [NSString stringWithFormat:NSLocalizedString(@"%@ Help", nil), name], @selector(showHelp:), @"?", cmd);

    [NSApp setMainMenu:main];
}

- (void)applicationWillFinishLaunching:(NSNotification *)note
{
    _windowController = [SMWindowController new];
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

/* A click on its Dock icon with no window shows the window again. */
- (BOOL)applicationShouldHandleReopen:(NSApplication *)app hasVisibleWindows:(BOOL)visible
{
    if (!visible)
        [self showMainWindow:nil];
    return YES;
}

@end
