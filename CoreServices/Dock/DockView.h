/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "DockModel.h"

NS_ASSUME_NONNULL_BEGIN

/* The Dock's window, at the bottom of the main display. It never becomes
   key: clicking a tile leaves the front app's keyboard focus alone. */
@interface DockWindow : NSWindow
- (instancetype)initWithModel:(DockModel *)model;
/* Size and place for the model's tiles on the main display. */
- (void)layout;
@end

NS_ASSUME_NONNULL_END
