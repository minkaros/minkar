/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "MenuExtra.h"
#import "NotificationBanner.h"

NS_ASSUME_NONNULL_BEGIN

/* The notification center's Menu Extra (Atlas.md, 5.6, N1), at the right
   end of the menu bar: a panel with the delivered notifications, newest
   first (a click on one is a click on its banner), Do Not Disturb, and
   the way to the Notifications pane. */
@interface NotificationCenterExtra : NSObject <MenuExtra>
- (instancetype)initWithStatusItem:(NSStatusItem *)item presenter:(NotificationPresenter *)presenter;
@end

NS_ASSUME_NONNULL_END
