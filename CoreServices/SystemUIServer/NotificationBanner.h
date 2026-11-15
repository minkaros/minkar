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
   front and tells the app.

   N1: each app's settings and Do Not Disturb decide whether the banner
   shows (or an alert, which stays until it is closed) and whether its
   sound plays; every notification an allowed app delivers is listed, for
   the notification center's panel. */
@interface NotificationPresenter : NSObject
- (void)start;

/* The delivered notifications, newest first. */
@property (readonly, copy) NSArray<NSDictionary *> *delivered;
/* As a click on its banner: its app to the front and told; then off the list. */
- (void)activateDelivered:(NSDictionary *)notification;
- (void)clearDelivered;
@end

/* Posted when the list changes. */
extern NSNotificationName const NotificationPresenterListDidChange;

NS_ASSUME_NONNULL_END
