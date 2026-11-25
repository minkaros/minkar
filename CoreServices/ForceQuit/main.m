/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Force Quit (Atlas.md, MB3): the regular applications running, kept
 * current through NSWorkspace's notifications; the one selected is forced
 * to quit (after asking) with the Force Quit button or Return. Closing
 * the window quits it. An agent: no Dock tile or menu bar of its own.
 *
 * Out of memory for applications (loginwindow opens it then, and sets
 * org.minkar.memory.low's state to the time): it says so, and lists the
 * applications by the memory each takes, the largest first.
 */

#import <AppKit/AppKit.h>
#include <libproc.h>
#include <notify.h>
#include <os/log.h>
#include <sys/resource.h>

#define kLowMemoryNotification "org.minkar.memory.low"

@interface ForceQuit : NSObject <NSApplicationDelegate, NSTableViewDataSource, NSTableViewDelegate, NSWindowDelegate>
@end

@implementation ForceQuit {
    NSWindow *_window;
    NSTableView *_table;
    NSButton *_button;
    NSArray<NSRunningApplication *> *_apps;
    pid_t _front;       /* the app in front when it opened: selected first */
    NSTextField *_intro;
    BOOL _lowMemory;
    int _lowMemoryToken;
}

/* The memory an app takes (its footprint), in bytes. */
static uint64_t footprintOf(NSRunningApplication *app)
{
    struct rusage_info_v2 usage;
    if (proc_pid_rusage([app processIdentifier], RUSAGE_INFO_V2, (rusage_info_t *)&usage) != 0)
        return 0;
    return usage.ri_phys_footprint;
}

/* Out of memory now: loginwindow said so in the last five minutes. */
- (BOOL)memoryIsLow
{
    uint64_t when = 0;
    if (_lowMemoryToken < 0 || notify_get_state(_lowMemoryToken, &when) != NOTIFY_STATUS_OK || when == 0)
        return NO;
    return time(NULL) - (time_t)when < 300;
}

- (void)showMemoryState
{
    _lowMemory = [self memoryIsLow];
    [_intro setStringValue:_lowMemory
        ? NSLocalizedString(@"The system has run out of memory for applications. Quit the ones you are not using.", nil)
        : NSLocalizedString(@"If an app doesn’t respond for a while, select its name and click Force Quit.", nil)];
    [self reload];
}

- (void)reload
{
    NSMutableArray *apps = [NSMutableArray array];
    pid_t me = getpid();
    for (NSRunningApplication *app in [[NSWorkspace sharedWorkspace] runningApplications])
        if ([app activationPolicy] == NSApplicationActivationPolicyRegular && ![app isTerminated] &&
            [app processIdentifier] != me)
            [apps addObject:app];
    BOOL low = _lowMemory;
    [apps sortUsingComparator:^NSComparisonResult(NSRunningApplication *a, NSRunningApplication *b) {
        if (low) {
            uint64_t fa = footprintOf(a), fb = footprintOf(b);
            if (fa != fb)
                return fa > fb ? NSOrderedAscending : NSOrderedDescending;
        }
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
    [intro setEditable:NO];
    [intro setBordered:NO];
    [intro setDrawsBackground:NO];
    [content addSubview:intro];
    _intro = intro;
    _lowMemoryToken = -1;
    notify_register_check(kLowMemoryNotification, &_lowMemoryToken);
    _lowMemory = [self memoryIsLow];
    [intro setStringValue:_lowMemory
        ? NSLocalizedString(@"The system has run out of memory for applications. Quit the ones you are not using.", nil)
        : NSLocalizedString(@"If an app doesn’t respond for a while, select its name and click Force Quit.", nil)];
    int changed;
    notify_register_dispatch(kLowMemoryNotification, &changed, dispatch_get_main_queue(), ^(int t) {
        [self showMemoryState];
    });

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
    if (row < 0 || (NSUInteger)row >= [_apps count])
        return @"";
    NSRunningApplication *app = _apps[(NSUInteger)row];
    if (!_lowMemory)
        return [app localizedName];
    /* With the memory it takes, as the reference's window shows it then. */
    NSString *size = [NSByteCountFormatter stringFromByteCount:(long long)footprintOf(app)
                                                    countStyle:NSByteCountFormatterCountStyleMemory];
    return [NSString stringWithFormat:@"%@ (%@)", [app localizedName], size];
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
