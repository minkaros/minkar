/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/NSObject.h>

@class NSString, NSDictionary<KeyType, ObjectType>, NSArray<ObjectType>, NSDate, NSDateComponents,
    NSTimeZone, NSAttributedString, NSImage;
@protocol NSUserNotificationCenterDelegate;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, NSUserNotificationActivationType) {
    NSUserNotificationActivationTypeNone = 0,
    NSUserNotificationActivationTypeContentsClicked = 1,
    NSUserNotificationActivationTypeActionButtonClicked = 2,
    NSUserNotificationActivationTypeReplied = 3,
    NSUserNotificationActivationTypeAdditionalActionClicked = 4,
};

// An action a notification offers besides its action button.
@interface NSUserNotificationAction : NSObject <NSCopying>
+ (instancetype)actionWithIdentifier:(nullable NSString *)identifier title:(nullable NSString *)title;
@property (nullable, readonly, copy) NSString *identifier;
@property (nullable, readonly, copy) NSString *title;
@end

// A notification an app shows the user, now or at a date.
@interface NSUserNotification : NSObject <NSCopying>
@property (nullable, copy) NSString *title;
@property (nullable, copy) NSString *subtitle;
@property (nullable, copy) NSString *informativeText;
@property (copy) NSString *actionButtonTitle;
@property (nullable, copy) NSDictionary<NSString *, id> *userInfo;
@property (nullable, copy) NSDate *deliveryDate;
@property (nullable, copy) NSTimeZone *deliveryTimeZone;
@property (nullable, copy) NSDateComponents *deliveryRepeatInterval;
@property (nullable, readonly, copy) NSDate *actualDeliveryDate;
@property (readonly, getter=isPresented) BOOL presented;
@property (readonly, getter=isRemote) BOOL remote;
@property (nullable, copy) NSString *soundName;
@property BOOL hasActionButton;
@property (readonly) NSUserNotificationActivationType activationType;
@property (copy) NSString *otherButtonTitle;
@property (nullable, copy) NSString *identifier;
@property (nullable, copy) NSImage *contentImage;
@property BOOL hasReplyButton;
@property (nullable, copy) NSString *responsePlaceholder;
@property (nullable, readonly, copy) NSAttributedString *response;
@property (nullable, copy) NSArray<NSUserNotificationAction *> *additionalActions;
@property (nullable, readonly, copy) NSUserNotificationAction *additionalActivationAction;
@end

FOUNDATION_EXPORT NSString *const NSUserNotificationDefaultSoundName;

// The app's notifications: those scheduled for later and those delivered.
@interface NSUserNotificationCenter : NSObject
@property (class, readonly, strong) NSUserNotificationCenter *defaultUserNotificationCenter;
@property (nullable, assign) id<NSUserNotificationCenterDelegate> delegate;
@property (copy) NSArray<NSUserNotification *> *scheduledNotifications;
- (void)scheduleNotification:(NSUserNotification *)notification;
- (void)removeScheduledNotification:(NSUserNotification *)notification;
@property (readonly, copy) NSArray<NSUserNotification *> *deliveredNotifications;
- (void)deliverNotification:(NSUserNotification *)notification;
- (void)removeDeliveredNotification:(NSUserNotification *)notification;
- (void)removeAllDeliveredNotifications;
@end

@protocol NSUserNotificationCenterDelegate <NSObject>
@optional
- (void)userNotificationCenter:(NSUserNotificationCenter *)center
        didDeliverNotification:(NSUserNotification *)notification;
- (void)userNotificationCenter:(NSUserNotificationCenter *)center
       didActivateNotification:(NSUserNotification *)notification;
- (BOOL)userNotificationCenter:(NSUserNotificationCenter *)center
     shouldPresentNotification:(NSUserNotification *)notification;
@end

NS_ASSUME_NONNULL_END
