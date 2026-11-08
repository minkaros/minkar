/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Preferences window: a window titled as the reference platform's,
 * not resizable, its settings in NSUserDefaults as soon as they change
 * (no OK or Apply). Add the app's settings where the example one is.
 */

#import "SamplePreferences.h"

static NSString *const kShowsWelcome = @"SampleShowsWelcome";

@implementation SamplePreferences

+ (instancetype)sharedPreferences
{
    static SamplePreferences *shared;

    if (shared == nil)
        shared = [[SamplePreferences alloc] init];
    return shared;
}

- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 420, 140)
        styleMask:NSTitledWindowMask | NSClosableWindowMask backing:NSBackingStoreBuffered defer:NO];

    if ((self = [super initWithWindow:window]) == nil)
        return nil;
    [window setTitle:NSLocalizedString(@"Preferences", nil)];

    NSButton *welcome = [[NSButton alloc] initWithFrame:NSMakeRect(40, 70, 340, 22)];
    [welcome setButtonType:NSSwitchButton];
    [welcome setTitle:NSLocalizedString(@"Show the welcome window at startup", nil)];
    [welcome setState:[[NSUserDefaults standardUserDefaults] boolForKey:kShowsWelcome] ? NSOnState : NSOffState];
    [welcome setTarget:self];
    [welcome setAction:@selector(showsWelcomeChanged:)];
    [[window contentView] addSubview:welcome];
    [window center];
    return self;
}

- (void)showsWelcomeChanged:(NSButton *)sender
{
    [[NSUserDefaults standardUserDefaults] setBool:[sender state] == NSOnState forKey:kShowsWelcome];
}

@end
