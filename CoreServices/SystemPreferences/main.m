/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * System Preferences (ROADMAP Phase 5, item 6). "-pane IDENTIFIER" opens
 * on that pane (for the tests and for apps that open a pane); View has the
 * reference platform's Show All (Command-L) and the panes by name.
 */

#import "PrefsWindowController.h"

@interface PrefsApplication : NSObject <NSApplicationDelegate>
@end

@implementation PrefsApplication {
    PrefsWindowController *_controller;
    NSArray<NSURL *> *_pending;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    _controller = [[PrefsWindowController alloc] init];

    NSMenu *view = [[NSMenu alloc] initWithTitle:NSLocalizedString(@"View", nil)];
    NSMenuItem *all = [view addItemWithTitle:NSLocalizedString(@"Show All Preferences", nil)
                                      action:@selector(showAll:) keyEquivalent:@"l"];
    [all setTarget:_controller];
    NSMenuItem *back = [view addItemWithTitle:NSLocalizedString(@"Back", nil) action:@selector(goBack:) keyEquivalent:@"["];
    [back setTarget:_controller];
    NSMenuItem *forward = [view addItemWithTitle:NSLocalizedString(@"Forward", nil) action:@selector(goForward:) keyEquivalent:@"]"];
    [forward setTarget:_controller];
    NSMenuItem *top = [[NSMenuItem alloc] initWithTitle:NSLocalizedString(@"View", nil) action:NULL keyEquivalent:@""];
    [top setSubmenu:view];
    [[NSApp mainMenu] addItem:top];

    NSString *pane = [[NSUserDefaults standardUserDefaults] stringForKey:@"pane"];
    if ([pane length])
        [_controller showPaneWithIdentifier:pane];
    [[_controller window] makeKeyAndOrderFront:nil];
    [[_controller window] makeFirstResponder:nil];	/* the search field shows its placeholder */
    if (_pending != nil) {
        NSArray *urls = _pending;
        _pending = nil;
        [self application:NSApp openURLs:urls];
    }
}

/* A .prefPane opened (a Menu Extra's "Preferences…", open(1), a double
   click): its pane shown, as on the reference platform. */
- (void)application:(NSApplication *)app openURLs:(NSArray<NSURL *> *)urls
{
    if (_controller == nil) {   /* launched to open them: once it is up */
        _pending = [(_pending ?: @[]) arrayByAddingObjectsFromArray:urls];
        return;
    }
    for (NSURL *url in urls) {
        NSString *identifier = [[NSBundle bundleWithURL:url] bundleIdentifier];
        if (identifier != nil && [_controller showPaneWithIdentifier:identifier])
            [[_controller window] makeKeyAndOrderFront:nil];
            [[_controller window] makeFirstResponder:nil];	/* the search field shows its placeholder */
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)app
{
    return YES;
}

@end

int main(void)
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        PrefsApplication *delegate = [[PrefsApplication alloc] init];

        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
