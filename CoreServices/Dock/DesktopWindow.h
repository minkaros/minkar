/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/* The desktop of one display, behind every other window: its picture
   from -[NSWorkspace desktopImageURLForScreen:], or its color. */
@interface DesktopWindow : NSWindow
- (instancetype)initWithScreen:(NSScreen *)screen;
/* The picture and its options again (they changed). */
- (void)reload;
@end

/* The desktop's color with no picture: Atlas's provisional solid
   background, RGB(99, 130, 145). */
NSColor *DesktopDefaultColor(void);

/* Where an image goes in a desktop, for its scaling and clipping. */
NSRect DesktopImageRect(NSSize image, NSRect bounds, NSImageScaling scaling, BOOL clipping);

NS_ASSUME_NONNULL_END
