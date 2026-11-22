/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The notification center's settings (Atlas.md, 5.6, N1), as
 * SystemUIServer follows them: Do Not Disturb and each app's (allowed,
 * banner or alert or none, sound). Keys in NotificationCenterTypes.h.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, NotificationStyle) {
    NotificationStyleNone,
    NotificationStyleBanner,
    NotificationStyleAlert,
};

@interface NotificationSettings : NSObject

+ (instancetype)shared;

@property (nonatomic, getter=isDoNotDisturb) BOOL doNotDisturb;

/* The app a notification comes from, added with the defaults (allowed,
   banners, sound) the first time it posts one. */
- (void)noteAppOf:(NSDictionary *)notification;
- (BOOL)allowsNotification:(NSDictionary *)notification;
- (NotificationStyle)styleOfNotification:(NSDictionary *)notification;
- (BOOL)soundsNotification:(NSDictionary *)notification;
/* The reference's other settings: kept in the center's list; shown while
   the screen is locked; its text shown (else its title alone). */
- (BOOL)listsNotification:(NSDictionary *)notification;
- (BOOL)showsNotificationWhileLocked:(NSDictionary *)notification;
- (BOOL)previewsNotification:(NSDictionary *)notification;
/* The screen is locked (loginwindow's kNCScreenLocked). */
@property (nonatomic, readonly, getter=isScreenLocked) BOOL screenLocked;
/* The center's order: grouped by app ("recentsByApp"), else newest first. */
@property (nonatomic, readonly) BOOL sortsByApp;

@end

/* Posted on the main queue when the settings change (the pane, or Do Not
   Disturb from the menu bar). */
extern NSNotificationName const NotificationSettingsDidChange;

NS_ASSUME_NONNULL_END
