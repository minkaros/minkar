/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * System Monitor (Atlas, A3): the processes and what they use (CPU,
 * memory, disk) and the network's traffic; a process's information, and
 * quitting or force quitting it. Made from the Atlas app template
 * (Developer/Templates/AtlasApp).
 */

#import "SMAppDelegate.h"

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        SMAppDelegate *delegate = [SMAppDelegate new];

        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
