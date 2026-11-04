/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SystemUIServer (Menus.md, MB1): an agent app (LSUIElement) whose status
 * items are the Menu Extras, laid out by the window server at the right of
 * the menu bar.
 */

#import "ClockExtra.h"

@interface SystemUIServer : NSObject <NSApplicationDelegate>
@end

@implementation SystemUIServer {
    NSMutableArray *_extras;
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSStatusBar *bar = [NSStatusBar systemStatusBar];

    _extras = [NSMutableArray array];
    for (Class cls in @[ [ClockExtra class] ]) {
        NSStatusItem *item = [bar statusItemWithLength:NSVariableStatusItemLength];
        [_extras addObject:[[cls alloc] initWithStatusItem:item]];
    }
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
