/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Force Quit (Atlas.md, MB3): the regular applications running, kept
 * current through NSWorkspace's notifications; the one selected is forced
 * to quit (after asking) with the Force Quit button or Return. Closing
 * the window quits it. An agent: no Dock tile or menu bar of its own.
 */

#import <AppKit/AppKit.h>
#include <os/log.h>

@interface ForceQuit : NSObject <NSApplicationDelegate, NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>
@end

@implementation ForceQuit {
    NSWindow *_window;
    NSTableView *_table;
    NSButton *_button;
    NSArray<NSRunningApplication *> *_apps;
    pid_t _front;       /* the app in front when it opened: selected first */
}

- (void)reload
{
    NSMutableArray *apps = [NSMutableArray array];
    pid_t me = getpid();
    for (NSRunningApplication *app in [[NSWorkspace sharedWorkspace] runningApplications])
        if ([app activationPolicy] == NSApplicationActivationPolicyRegular && ![app isTerminated] &&
            [app processIdentifier] != me)
            [apps addObject:app];
    [apps sortUsingComparator:^NSComparisonResult(NSRunningApplication *a, NSRunningApplication *b) {
        return [[a localizedName] localizedCaseInsensitiveCompare:[b localizedName]];
    }];
    NSString *selected = [_table selectedRow] >= 0 && (NSUInteger)[_table selectedRow] < [_apps count]
        ? [_apps[(NSUInteger)[_table selectedRow]] localizedName] : nil;
    if (_apps == nil)
        for (NSRunningApplication *app in apps)
            if ([app processIdentifier] == _front)
                selected = [app localizedName];
    _apps = apps;
    [_table reloadData];
    NSUInteger row = 0;
    for (NSUInteger i = 0; i < [apps count]; i++)
        if ([[apps[i] localizedName] isEqualToString:selected])
            row = i;
    if ([apps count] > 0)
        [_table selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
    [_button setEnabled:[apps count] > 0];
    os_log_info(OS_LOG_DEFAULT, "force quit lists %{public}s",
                [[[apps valueForKey:@"localizedName"] componentsJoinedByString:@", "] UTF8String]);
}

- (void)applicationWillFinishLaunching:(NSNotification *)note
{
    _front = [[[NSWorkspace sharedWorkspace] frontmostApplication] processIdentifier];
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 380, 330)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
    [_window setTitle:NSLocalizedString(@"Force Quit Applications", nil)];
    [_window setDelegate:self];
    NSView *content = [_window contentView];

    NSTextField *intro = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 270, 340, 40)];
    [intro setStringValue:NSLocalizedString(@"If an app doesn’t respond for a while, select its name and click Force Quit.", nil)];
    [intro setEditable:NO];
    [intro setBordered:NO];
    [intro setDrawsBackground:NO];
    [content addSubview:intro];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(20, 60, 340, 200)];
    [scroll setHasVerticalScroller:YES];
    [scroll setBorderType:NSBezelBorder];
    _table = [[NSTableView alloc] initWithFrame:[[scroll contentView] bounds]];
    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:@"name"];
    [column setWidth:320];
    [_table addTableColumn:column];
    [_table setHeaderView:nil];
    [_table setDataSource:self];
    [_table setDelegate:self];
    [_table setRowHeight:22];
    [scroll setDocumentView:_table];
    [content addSubview:scroll];

    _button = [[NSButton alloc] initWithFrame:NSMakeRect(250, 14, 110, 32)];
    [_button setTitle:NSLocalizedString(@"Force Quit", nil)];
    [_button setBezelStyle:NSRoundedBezelStyle];
    [_button setKeyEquivalent:@"\r"];
    [_button setTarget:self];
    [_button setAction:@selector(forceQuit:)];
    [content addSubview:_button];

    NSNotificationCenter *ws = [[NSWorkspace sharedWorkspace] notificationCenter];
    for (NSString *name in @[ NSWorkspaceDidLaunchApplicationNotification, NSWorkspaceDidTerminateApplicationNotification ])
        [ws addObserver:self selector:@selector(appsChanged:) name:name object:nil];
    [self reload];
    [_window center];
    [_window makeKeyAndOrderFront:nil];
    [_window makeFirstResponder:_table];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)appsChanged:(NSNotification *)note
{
    [self reload];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)table
{
    return (NSInteger)[_apps count];
}

- (id)tableView:(NSTableView *)table objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    return row >= 0 && (NSUInteger)row < [_apps count] ? [_apps[(NSUInteger)row] localizedName] : @"";
}

- (BOOL)tableView:(NSTableView *)table shouldEditTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
    return NO;
}

- (void)forceQuit:(id)sender
{
    NSInteger row = [_table selectedRow];
    if (row < 0 || (NSUInteger)row >= [_apps count])
        return;
    NSRunningApplication *app = _apps[(NSUInteger)row];
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:[NSString stringWithFormat:NSLocalizedString(@"Do you want to force “%@” to quit?", nil),
                           [app localizedName]]];
    [alert setInformativeText:NSLocalizedString(@"You will lose any unsaved changes.", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Force Quit", nil)];
    [alert addButtonWithTitle:NSLocalizedString(@"Cancel", nil)];
    if ([alert runModal] != NSAlertFirstButtonReturn)
        return;
    os_log_info(OS_LOG_DEFAULT, "force quit %{public}s (pid %d)", [[app localizedName] UTF8String],
                [app processIdentifier]);
    [app forceTerminate];
}

- (void)windowWillClose:(NSNotification *)note
{
    [NSApp terminate:nil];
}

/* Opened again while running: the window to the front. */
- (BOOL)applicationShouldHandleReopen:(NSApplication *)app hasVisibleWindows:(BOOL)visible
{
    [_window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    return NO;
}

@end

int main(void)
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        ForceQuit *delegate = [[ForceQuit alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
