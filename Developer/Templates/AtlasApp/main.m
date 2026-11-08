/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Sample, the template Atlas's apps start from (README.md).
 */

#import "SampleAppDelegate.h"

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        SampleAppDelegate *delegate = [SampleAppDelegate new];

        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
