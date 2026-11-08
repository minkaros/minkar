/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Terminal (Atlas, A1): shells in windows and tabs, a VT100/xterm
 * emulator (libvterm) drawn in Agave. Made from the Atlas app template
 * (Developer/Templates/AtlasApp).
 */

#import "TMAppDelegate.h"

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        TMAppDelegate *delegate = [TMAppDelegate new];

        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
