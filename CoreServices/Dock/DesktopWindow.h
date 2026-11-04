/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/* The desktop picture of one display, behind every other window. */
@interface DesktopWindow : NSWindow
- (instancetype)initWithScreen:(NSScreen *)screen;
@end

/* The desktop's color until desktop pictures come (D1): Atlas's
   provisional solid background, RGB(99, 130, 145). */
NSColor *DesktopDefaultColor(void);

NS_ASSUME_NONNULL_END
