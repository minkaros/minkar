/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Logs (Atlas, A4): os_log's messages, the last hour and as they come,
 * filtered by level, process, subsystem and text; crash reports and log
 * files. Made from the Atlas app template (Developer/Templates/AtlasApp).
 */

#import "LGAppDelegate.h"

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        LGAppDelegate *delegate = [LGAppDelegate new];

        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
