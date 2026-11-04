/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The desktop: one window per display at the desktop level, filled with
 * the desktop color. It takes no clicks: they reach whatever is below.
 */

#import "DesktopWindow.h"

NSColor *DesktopDefaultColor(void)
{
    return [NSColor colorWithCalibratedRed:99 / 255.0 green:130 / 255.0 blue:145 / 255.0 alpha:1];
}

@implementation DesktopWindow

- (instancetype)initWithScreen:(NSScreen *)screen
{
    self = [super initWithContentRect:[screen frame]
                            styleMask:NSBorderlessWindowMask
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (self == nil)
        return nil;
    [self setLevel:kCGDesktopWindowLevelKey];
    [self setBackgroundColor:DesktopDefaultColor()];
    [self setOpaque:YES];
    [self setHasShadow:NO];
    [self setHidesOnDeactivate:NO];
    [self setExcludedFromWindowsMenu:YES];
    [self setReleasedWhenClosed:NO];
    [self setIgnoresMouseEvents:YES];
    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (BOOL)canBecomeMainWindow
{
    return NO;
}

@end
