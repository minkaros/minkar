// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// The app's user notifications: requests with content and a trigger,
// delivered through NSUserNotificationCenter (no banner is shown yet: see
// NSUserNotification.m), authorization (always granted), the delivered
// and pending lists, and the delegate.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, UNAuthorizationOptions) {
    UNAuthorizationOptionBadge = (1 << 0),
    UNAuthorizationOptionSound = (1 << 1),
    UNAuthorizationOptionAlert = (1 << 2),
    UNAuthorizationOptionCarPlay = (1 << 3),
    UNAuthorizationOptionCriticalAlert = (1 << 4),
    UNAuthorizationOptionProvidesAppNotificationSettings = (1 << 5),
    UNAuthorizationOptionProvisional = (1 << 6)
};

typedef NS_ENUM(NSInteger, UNAuthorizationStatus) {
    UNAuthorizationStatusNotDetermined = 0,
    UNAuthorizationStatusDenied,
    UNAuthorizationStatusAuthorized,
    UNAuthorizationStatusProvisional
};

typedef NS_ENUM(NSInteger, UNNotificationSetting) {
    UNNotificationSettingNotSupported = 0,
    UNNotificationSettingDisabled,
    UNNotificationSettingEnabled
};

typedef NS_ENUM(NSInteger, UNAlertStyle) {
    UNAlertStyleNone = 0,
    UNAlertStyleBanner,
    UNAlertStyleAlert
};

typedef NS_OPTIONS(NSUInteger, UNNotificationPresentationOptions) {
    UNNotificationPresentationOptionBadge = (1 << 0),
    UNNotificationPresentationOptionSound = (1 << 1),
    UNNotificationPresentationOptionAlert = (1 << 2)
};

FOUNDATION_EXPORT NSString *const UNErrorDomain;
FOUNDATION_EXPORT NSString *const UNNotificationDefaultActionIdentifier;
FOUNDATION_EXPORT NSString *const UNNotificationDismissActionIdentifier;

@interface UNNotificationSound : NSObject <NSCopying, NSSecureCoding>
@property (class, readonly, copy) UNNotificationSound *defaultSound;
+ (instancetype)soundNamed:(NSString *)name;
@end

@interface UNNotificationAttachment : NSObject <NSCopying, NSSecureCoding>
@property (readonly, copy) NSString *identifier;
@property (readonly, copy) NSURL *URL;
@property (readonly, copy) NSString *type;
+ (nullable instancetype)attachmentWithIdentifier:(NSString *)identifier URL:(NSURL *)URL
                                          options:(nullable NSDictionary *)options error:(NSError **)error;
@end

@interface UNNotificationContent : NSObject <NSCopying, NSMutableCopying, NSSecureCoding>
@property (readonly, copy) NSArray<UNNotificationAttachment *> *attachments;
@property (nullable, readonly, copy) NSNumber *badge;
@property (readonly, copy) NSString *body;
@property (readonly, copy) NSString *categoryIdentifier;
@property (nullable, readonly, copy) UNNotificationSound *sound;
@property (readonly, copy) NSString *subtitle;
@property (readonly, copy) NSString *threadIdentifier;
@property (readonly, copy) NSString *title;
@property (readonly, copy) NSDictionary *userInfo;
@end

@interface UNMutableNotificationContent : UNNotificationContent
@property (copy) NSArray<UNNotificationAttachment *> *attachments;
@property (nullable, copy) NSNumber *badge;
@property (copy) NSString *body;
@property (copy) NSString *categoryIdentifier;
@property (nullable, copy) UNNotificationSound *sound;
@property (copy) NSString *subtitle;
@property (copy) NSString *threadIdentifier;
@property (copy) NSString *title;
@property (copy) NSDictionary *userInfo;
@end

@interface UNNotificationTrigger : NSObject <NSCopying, NSSecureCoding>
@property (readonly) BOOL repeats;
@end

@interface UNPushNotificationTrigger : UNNotificationTrigger
@end

@interface UNTimeIntervalNotificationTrigger : UNNotificationTrigger
@property (readonly) NSTimeInterval timeInterval;
+ (instancetype)triggerWithTimeInterval:(NSTimeInterval)timeInterval repeats:(BOOL)repeats;
- (nullable NSDate *)nextTriggerDate;
@end

@interface UNCalendarNotificationTrigger : UNNotificationTrigger
@property (readonly, copy) NSDateComponents *dateComponents;
+ (instancetype)triggerWithDateMatchingComponents:(NSDateComponents *)dateComponents repeats:(BOOL)repeats;
- (nullable NSDate *)nextTriggerDate;
@end

@interface UNNotificationRequest : NSObject <NSCopying, NSSecureCoding>
@property (readonly, copy) NSString *identifier;
@property (readonly, copy) UNNotificationContent *content;
@property (nullable, readonly, copy) UNNotificationTrigger *trigger;
+ (instancetype)requestWithIdentifier:(NSString *)identifier content:(UNNotificationContent *)content
                              trigger:(nullable UNNotificationTrigger *)trigger;
@end

@interface UNNotification : NSObject <NSCopying, NSSecureCoding>
@property (readonly, copy) NSDate *date;
@property (readonly, copy) UNNotificationRequest *request;
@end

@interface UNNotificationResponse : NSObject <NSCopying, NSSecureCoding>
@property (readonly, copy) NSString *actionIdentifier;
@property (readonly, copy) UNNotification *notification;
@end

@interface UNNotificationAction : NSObject <NSCopying, NSSecureCoding>
@property (readonly, copy) NSString *identifier;
@property (readonly, copy) NSString *title;
+ (instancetype)actionWithIdentifier:(NSString *)identifier title:(NSString *)title options:(NSUInteger)options;
@end

@interface UNNotificationCategory : NSObject <NSCopying, NSSecureCoding>
@property (readonly, copy) NSString *identifier;
@property (readonly, copy) NSArray<UNNotificationAction *> *actions;
+ (instancetype)categoryWithIdentifier:(NSString *)identifier actions:(NSArray<UNNotificationAction *> *)actions
                     intentIdentifiers:(NSArray<NSString *> *)intentIdentifiers options:(NSUInteger)options;
@end

@interface UNNotificationSettings : NSObject <NSCopying, NSSecureCoding>
@property (readonly) UNAuthorizationStatus authorizationStatus;
@property (readonly) UNNotificationSetting soundSetting;
@property (readonly) UNNotificationSetting badgeSetting;
@property (readonly) UNNotificationSetting alertSetting;
@property (readonly) UNNotificationSetting notificationCenterSetting;
@property (readonly) UNNotificationSetting lockScreenSetting;
@property (readonly) UNAlertStyle alertStyle;
@end

@class UNUserNotificationCenter;

@protocol UNUserNotificationCenterDelegate <NSObject>
@optional
- (void)userNotificationCenter:(UNUserNotificationCenter *)center willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler;
- (void)userNotificationCenter:(UNUserNotificationCenter *)center didReceiveNotificationResponse:(UNNotificationResponse *)response
         withCompletionHandler:(void (^)(void))completionHandler;
- (void)userNotificationCenter:(UNUserNotificationCenter *)center openSettingsForNotification:(nullable UNNotification *)notification;
@end

@interface UNUserNotificationCenter : NSObject
@property (nullable, weak) id<UNUserNotificationCenterDelegate> delegate;
@property (readonly) BOOL supportsContentExtensions;
+ (UNUserNotificationCenter *)currentNotificationCenter;
- (void)requestAuthorizationWithOptions:(UNAuthorizationOptions)options
                      completionHandler:(void (^)(BOOL granted, NSError *_Nullable error))completionHandler;
- (void)setNotificationCategories:(NSSet<UNNotificationCategory *> *)categories;
- (void)getNotificationCategoriesWithCompletionHandler:(void (^)(NSSet<UNNotificationCategory *> *categories))completionHandler;
- (void)getNotificationSettingsWithCompletionHandler:(void (^)(UNNotificationSettings *settings))completionHandler;
- (void)addNotificationRequest:(UNNotificationRequest *)request withCompletionHandler:(nullable void (^)(NSError *_Nullable error))completionHandler;
- (void)getPendingNotificationRequestsWithCompletionHandler:(void (^)(NSArray<UNNotificationRequest *> *requests))completionHandler;
- (void)removePendingNotificationRequestsWithIdentifiers:(NSArray<NSString *> *)identifiers;
- (void)removeAllPendingNotificationRequests;
- (void)getDeliveredNotificationsWithCompletionHandler:(void (^)(NSArray<UNNotification *> *notifications))completionHandler;
- (void)removeDeliveredNotificationsWithIdentifiers:(NSArray<NSString *> *)identifiers;
- (void)removeAllDeliveredNotifications;
@end

@interface NSString (UNUserNotificationCenterSupport)
+ (NSString *)localizedUserNotificationStringForKey:(NSString *)key arguments:(nullable NSArray *)arguments;
@end

NS_ASSUME_NONNULL_END
