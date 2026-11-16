/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The app switcher (Atlas.md, D3): Command-Tab and Command-Shift-Tab,
 * claimed from the window server's symbolic hotkeys, show the regular apps
 * running, the most recently used first, in a panel at the middle of the
 * main screen; each press moves along, and letting go of Command
 * activates the one chosen.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface DockSwitcher : NSObject
- (instancetype)init;
@end

NS_ASSUME_NONNULL_END
