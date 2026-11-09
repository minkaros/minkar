/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The app's delegate: the standard menus, made in code with the reference
 * platform's commands and keys, and the Preferences window. Documents are
 * NSDocumentController's: New, Open, Open Recent, Save, Revert To and
 * the others reach it and the front document through the responder chain.
 */

#import "SampleAppDelegate.h"
#import "SamplePreferences.h"

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

static NSString *appName(void)
{
    NSString *name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];

    return [name length] ? name : [[NSProcessInfo processInfo] processName];
}

@implementation SampleAppDelegate

- (void)buildMenus
{
    NSUInteger cmd = NSCommandKeyMask, shift = NSCommandKeyMask | NSShiftKeyMask;
    NSUInteger opt = NSCommandKeyMask | NSAlternateKeyMask;
    NSString *name = appName();
    NSMenu *main = [[NSMenu alloc] initWithTitle:@"Main"];

    /* The app menu: the main menu's first item, shown as the app's name. */
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

    NSMenu *file = submenu(main, NSLocalizedString(@"File", nil));
    item(file, NSLocalizedString(@"New", nil), @selector(newDocument:), @"n", cmd);
    item(file, NSLocalizedString(@"Open…", nil), @selector(openDocument:), @"o", cmd);
    NSMenu *recent = submenu(file, NSLocalizedString(@"Open Recent", nil));
    item(recent, NSLocalizedString(@"Clear Menu", nil), @selector(clearRecentDocuments:), nil, 0);
    [file addItem:[NSMenuItem separatorItem]];
    item(file, NSLocalizedString(@"Close", nil), @selector(performClose:), @"w", cmd);
    item(file, NSLocalizedString(@"Save…", nil), @selector(saveDocument:), @"s", cmd);
    item(file, NSLocalizedString(@"Save As…", nil), @selector(saveDocumentAs:), @"s", shift);
    NSMenu *revert = submenu(file, NSLocalizedString(@"Revert To", nil));
    item(revert, NSLocalizedString(@"Last Opened", nil), @selector(revertDocumentToSaved:), nil, 0);
    item(revert, NSLocalizedString(@"Browse All Versions…", nil), @selector(browseDocumentVersions:), nil, 0);

    NSMenu *edit = submenu(main, NSLocalizedString(@"Edit", nil));
    item(edit, NSLocalizedString(@"Undo", nil), @selector(undo:), @"z", cmd);
    item(edit, NSLocalizedString(@"Redo", nil), @selector(redo:), @"z", shift);
    [edit addItem:[NSMenuItem separatorItem]];
    item(edit, NSLocalizedString(@"Cut", nil), @selector(cut:), @"x", cmd);
    item(edit, NSLocalizedString(@"Copy", nil), @selector(copy:), @"c", cmd);
    item(edit, NSLocalizedString(@"Paste", nil), @selector(paste:), @"v", cmd);
    item(edit, NSLocalizedString(@"Select All", nil), @selector(selectAll:), @"a", cmd);

    NSMenu *window = submenu(main, NSLocalizedString(@"Window", nil));
    item(window, NSLocalizedString(@"Minimize", nil), @selector(performMiniaturize:), @"m", cmd);
    item(window, NSLocalizedString(@"Zoom", nil), @selector(performZoom:), nil, 0);
    [window addItem:[NSMenuItem separatorItem]];
    item(window, NSLocalizedString(@"Bring All to Front", nil), @selector(arrangeInFront:), nil, 0);
    [NSApp setWindowsMenu:window];

    NSMenu *help = submenu(main, NSLocalizedString(@"Help", nil));
    item(help, [NSString stringWithFormat:NSLocalizedString(@"%@ Help", nil), name], @selector(showHelp:), @"?", cmd);

    [NSApp setMainMenu:main];
}

- (void)applicationWillFinishLaunching:(NSNotification *)note
{
    [self buildMenus];
}

- (IBAction)showPreferences:(id)sender
{
    [[SamplePreferences sharedPreferences] showWindow:sender];
}

/* Documents autosave: quitting needs no question about unsaved changes. */
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)app
{
    for (NSDocument *document in [[NSDocumentController sharedDocumentController] documents])
        [document autosaveWithImplicitCancellability:NO completionHandler:nil];
    return NSTerminateNow;
}

@end
