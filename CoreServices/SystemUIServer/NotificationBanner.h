/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/* The notification center's presenter (Atlas.md, 5.6, N0): checked in with
   notificationd, it shows each notification an app delivers as a banner at
   the top right of the screen, below the menu bar, and plays its sound. A
   banner goes after a few seconds; a click on it brings its app to the
   front and tells the app. */
@interface NotificationPresenter : NSObject
- (void)start;
@end

NS_ASSUME_NONNULL_END
