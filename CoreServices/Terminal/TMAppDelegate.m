/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Terminal's menus (Shell, Edit, View, Window, Help) and its windows: one
 * at launch, another for each ⌘N, and one running each script it is asked
 * to open.
 */

#import "TMAppDelegate.h"
#import "TMPreferencesController.h"

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

@implementation TMAppDelegate {
    NSMutableArray<TMWindowController *> *_controllers;
    BOOL _openedFiles;
}

- (instancetype)init
{
    if ((self = [super init]) != nil)
        _controllers = [NSMutableArray array];
    return self;
}

- (NSArray *)windowControllers
{
    return [_controllers copy];
}

- (void)buildMenus
{
    NSUInteger cmd = NSCommandKeyMask, shift = NSCommandKeyMask | NSShiftKeyMask;
    NSUInteger opt = NSCommandKeyMask | NSAlternateKeyMask;
    NSString *name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"] ?: @"Terminal";
    NSMenu *main = [[NSMenu alloc] initWithTitle:@"Main"];

    NSMenu *app = submenu(main, name);
    item(app, [NSString stringWithFormat:NSLocalizedString(@"About %@", nil), name],
        @selector(orderFrontStandardAboutPanel:), nil, 0);
    [app addItem:[NSMenuItem separatorItem]];
    [item(app, NSLocalizedString(@"Preferences…", nil), @selector(showPreferences:), @",", cmd) setTarget:self];
    [app addItem:[NSMenuItem separatorItem]];
    [NSApp setServicesMenu:submenu(app, NSLocalizedString(@"Services", nil))];
    [app addItem:[NSMenuItem separatorItem]];
    item(app, [NSString stringWithFormat:NSLocalizedString(@"Hide %@", nil), name], @selector(hide:), @"h", cmd);
    item(app, NSLocalizedString(@"Hide Others", nil), @selector(hideOtherApplications:), @"h", opt);
    item(app, NSLocalizedString(@"Show All", nil), @selector(unhideAllApplications:), nil, 0);
    [app addItem:[NSMenuItem separatorItem]];
    item(app, [NSString stringWithFormat:NSLocalizedString(@"Quit %@", nil), name], @selector(terminate:), @"q", cmd);

    NSMenu *shell = submenu(main, NSLocalizedString(@"Shell", nil));
    [item(shell, NSLocalizedString(@"New Window", nil), @selector(newWindow:), @"n", cmd) setTarget:self];
    item(shell, NSLocalizedString(@"New Tab", nil), @selector(newTab:), @"t", cmd);
    [shell addItem:[NSMenuItem separatorItem]];
    item(shell, NSLocalizedString(@"Close Tab", nil), @selector(closeTab:), @"w", cmd);
    item(shell, NSLocalizedString(@"Close Window", nil), @selector(performClose:), @"w", shift);

    NSMenu *edit = submenu(main, NSLocalizedString(@"Edit", nil));
    item(edit, NSLocalizedString(@"Copy", nil), @selector(copy:), @"c", cmd);
    item(edit, NSLocalizedString(@"Paste", nil), @selector(paste:), @"v", cmd);
    item(edit, NSLocalizedString(@"Select All", nil), @selector(selectAll:), @"a", cmd);
    [edit addItem:[NSMenuItem separatorItem]];
    item(edit, NSLocalizedString(@"Clear Scrollback", nil), @selector(clearScrollback:), @"k", cmd);

    NSMenu *view = submenu(main, NSLocalizedString(@"View", nil));
    item(view, NSLocalizedString(@"Bigger", nil), @selector(biggerText:), @"+", cmd);
    item(view, NSLocalizedString(@"Smaller", nil), @selector(smallerText:), @"-", cmd);
    item(view, NSLocalizedString(@"Default Size", nil), @selector(defaultTextSize:), @"0", cmd);

    NSMenu *window = submenu(main, NSLocalizedString(@"Window", nil));
    item(window, NSLocalizedString(@"Minimize", nil), @selector(performMiniaturize:), @"m", cmd);
    item(window, NSLocalizedString(@"Zoom", nil), @selector(performZoom:), nil, 0);
    [window addItem:[NSMenuItem separatorItem]];
    item(window, NSLocalizedString(@"Show Previous Tab", nil), @selector(selectPreviousTab:), @"{", cmd);
    item(window, NSLocalizedString(@"Show Next Tab", nil), @selector(selectNextTab:), @"}", cmd);
    [window addItem:[NSMenuItem separatorItem]];
    item(window, NSLocalizedString(@"Bring All to Front", nil), @selector(arrangeInFront:), nil, 0);
    [NSApp setWindowsMenu:window];

    NSMenu *help = submenu(main, NSLocalizedString(@"Help", nil));
    item(help, [NSString stringWithFormat:NSLocalizedString(@"%@ Help", nil), name], @selector(showHelp:), @"?", cmd);

    [NSApp setMainMenu:main];
}

- (TMWindowController *)newWindowWithArguments:(NSArray<NSString *> *)arguments
{
    TMWindowController *controller = [[TMWindowController alloc] initWithArguments:arguments];
    __weak TMAppDelegate *weakSelf = self;

    if (controller == nil)
        return nil;
    controller.closed = ^(TMWindowController *c) {
        TMAppDelegate *me = weakSelf;
        if (me != nil)
            [me->_controllers removeObject:c];
    };
    [_controllers addObject:controller];
    [controller showWindow:nil];
    return controller;
}

- (IBAction)showPreferences:(id)sender
{
    TMPreferencesController *controller = [TMPreferencesController sharedController];

    [controller updateControls];
    [controller showWindow:sender];
}

- (IBAction)newWindow:(id)sender
{
    [self newWindowWithArguments:nil];
}

- (void)applicationWillFinishLaunching:(NSNotification *)note
{
    [self buildMenus];
}

/* A script to run (a .command, .sh or .tool): in a window of its own. */
- (void)application:(NSApplication *)app openFiles:(NSArray<NSString *> *)files
{
    _openedFiles = YES;
    for (NSString *path in files)
        [self newWindowWithArguments:@[ @"/bin/sh", path ]];
    [app replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    if (!_openedFiles && [_controllers count] == 0)
        [self newWindowWithArguments:nil];
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)app hasVisibleWindows:(BOOL)visible
{
    if (!visible)
        [self newWindowWithArguments:nil];
    return YES;
}

/* Quitting ends the shells (SIGHUP, as closing a terminal does). */
- (void)applicationWillTerminate:(NSNotification *)note
{
    for (TMWindowController *c in [_controllers copy])
        for (TMTerminalView *t in c.terminals)
            [t.session terminate];
}

@end
