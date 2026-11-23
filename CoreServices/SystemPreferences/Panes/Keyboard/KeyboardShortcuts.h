/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

/* The Keyboard pane's Shortcuts tab: the system-wide shortcuts Atlas has. */
@interface KeyboardShortcuts : NSObject <NSTableViewDataSource, NSTableViewDelegate>
- (instancetype)initWithBundle:(NSBundle *)bundle;
- (NSView *)viewWithFrame:(NSRect)frame;
@end
