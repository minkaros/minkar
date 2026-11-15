/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SystemUIServer (Atlas.md, MB1): an agent app (LSUIElement) whose status
 * items are the Menu Extras, laid out by the window server at the right of
 * the menu bar. It also shows the notification center's banners (5.6, N0).
 */

#import "ClockExtra.h"
#import "NotificationBanner.h"
#import "VolumeExtra.h"

@interface SystemUIServer : NSObject <NSApplicationDelegate>
@end

@implementation SystemUIServer {
    NSMutableArray *_extras;
    NotificationPresenter *_notifications;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSStatusBar *bar = [NSStatusBar systemStatusBar];

    _extras = [NSMutableArray array];
    /* Right to left: the clock, then the volume. */
    for (Class cls in @[ [ClockExtra class], [VolumeExtra class] ]) {
        NSStatusItem *item = [bar statusItemWithLength:NSVariableStatusItemLength];
        [_extras addObject:[[cls alloc] initWithStatusItem:item]];
    }
    _notifications = [[NotificationPresenter alloc] init];
    [_notifications start];
}

@end

int main(void)
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        SystemUIServer *server = [[SystemUIServer alloc] init];

        [app setDelegate:server];
        [app run];
    }
    return 0;
}
