/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The notification center's settings (Atlas.md, 5.6, N1). Read from the
 * user's preferences again each time System Preferences posts
 * kNCSettingsChanged; the apps SystemUIServer has not seen are added, so
 * the pane can list them.
 */

#import "NotificationSettings.h"
#include <notify.h>
#include "NotificationCenterTypes.h"

NSNotificationName const NotificationSettingsDidChange = @"NotificationSettingsDidChange";

#define kDomain CFSTR(kNCSettingsDomain)

@implementation NotificationSettings {
    int _token;
}

+ (instancetype)shared
{
    static NotificationSettings *shared;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        shared = [[NotificationSettings alloc] init];
    });
    return shared;
}

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    CFPreferencesAppSynchronize(kDomain);
    notify_register_dispatch(kNCSettingsChanged, &_token, dispatch_get_main_queue(), ^(int token) {
        CFPreferencesAppSynchronize(kDomain);
        fprintf(stderr, "SystemUIServer: notification settings changed (do not disturb %s)\n",
                self.doNotDisturb ? "on" : "off");
        [[NSNotificationCenter defaultCenter] postNotificationName:NotificationSettingsDidChange object:self];
    });
    return self;
}

- (BOOL)isDoNotDisturb
{
    return CFPreferencesGetAppBooleanValue(CFSTR(kNCSettingDoNotDisturb), kDomain, NULL);
}

- (void)setDoNotDisturb:(BOOL)on
{
    CFPreferencesSetAppValue(CFSTR(kNCSettingDoNotDisturb), on ? kCFBooleanTrue : kCFBooleanFalse, kDomain);
    CFPreferencesAppSynchronize(kDomain);
    /* The pane, if it is open, and ourselves (the menu bar's icon). */
    notify_post(kNCSettingsChanged);
}

static NSString *appKey(NSDictionary *n)
{
    NSString *app = n[@kNCKeyBundleIdentifier];

    if (app.length == 0)
        app = n[@kNCKeyAppName];
    return app.length > 0 ? [@kNCSettingAppPrefix stringByAppendingString:app] : nil;
}

- (nullable NSDictionary *)appOf:(NSDictionary *)n
{
    NSString *key = appKey(n);
    CFPropertyListRef value = key != nil ? CFPreferencesCopyAppValue((__bridge CFStringRef)key, kDomain) : NULL;
    NSDictionary *app = nil;

    if (value != NULL && CFGetTypeID(value) == CFDictionaryGetTypeID())
        app = (__bridge NSDictionary *)value;
    if (value != NULL)
        CFRelease(value);
    return app;
}

- (void)noteAppOf:(NSDictionary *)n
{
    NSString *key = appKey(n);
    NSDictionary *app;

    if (key == nil)
        return;
    CFPreferencesAppSynchronize(kDomain);
    app = [self appOf:n];
    /* Its name and place kept up to date; its settings left alone. */
    NSMutableDictionary *updated = app != nil ? [app mutableCopy] : [@{
        @kNCAppAllowed : @YES,
        @kNCAppStyle : @"banner",
        @kNCAppSound : @YES,
    } mutableCopy];
    if (n[@kNCKeyAppName] != nil)
        updated[@kNCAppName] = n[@kNCKeyAppName];
    if (n[@kNCKeyAppPath] != nil)
        updated[@kNCAppPath] = n[@kNCKeyAppPath];
    if ([updated isEqual:app])
        return;
    CFPreferencesSetAppValue((__bridge CFStringRef)key, (__bridge CFDictionaryRef)updated, kDomain);
    CFPreferencesAppSynchronize(kDomain);
}

- (BOOL)allowsNotification:(NSDictionary *)n
{
    id allowed = [self appOf:n][@kNCAppAllowed];

    return allowed == nil || [allowed boolValue];
}

- (NotificationStyle)styleOfNotification:(NSDictionary *)n
{
    NSString *style = [self appOf:n][@kNCAppStyle];

    if ([style isEqual:@"none"])
        return NotificationStyleNone;
    if ([style isEqual:@"alert"])
        return NotificationStyleAlert;
    return NotificationStyleBanner;
}

- (BOOL)soundsNotification:(NSDictionary *)n
{
    id sound = [self appOf:n][@kNCAppSound];

    return sound == nil || [sound boolValue];
}

/* A setting that is on when absent. */
- (BOOL)app:(NSDictionary *)n flag:(NSString *)key
{
    id value = [self appOf:n][key];

    return value == nil || [value boolValue];
}

- (BOOL)listsNotification:(NSDictionary *)n
{
    return [self app:n flag:@kNCAppShowInList];
}

- (BOOL)showsNotificationWhileLocked:(NSDictionary *)n
{
    return [self app:n flag:@kNCAppLockScreen];
}

- (BOOL)isScreenLocked
{
    static int token = -1;
    uint64_t state = 0;

    if (token < 0 && notify_register_check(kNCScreenLocked, &token) != NOTIFY_STATUS_OK)
        return NO;
    notify_get_state(token, &state);
    return state != 0;
}

/* Always; when unlocked, unless the screen is locked; never. */
- (BOOL)previewsNotification:(NSDictionary *)n
{
    NSString *preview = [self appOf:n][@kNCAppPreview];

    if ([preview isEqual:@"never"])
        return NO;
    if ([preview isEqual:@"unlocked"])
        return !self.screenLocked;
    return YES;
}

- (BOOL)sortsByApp
{
    CFPropertyListRef value = CFPreferencesCopyAppValue(CFSTR(kNCSettingSortOrder), kDomain);
    BOOL byApp = value != NULL && CFGetTypeID(value) == CFStringGetTypeID() &&
        CFEqual(value, CFSTR("recentsByApp"));

    if (value != NULL)
        CFRelease(value);
    return byApp;
}

@end
