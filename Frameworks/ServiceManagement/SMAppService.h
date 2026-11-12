/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SMAppService (API 22): an app's login items, agents and daemons as
 * launchd jobs. Ahead of Minkar's API 19 to 21, because the reference apps
 * use it (KeepingYouAwake).
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, SMAppServiceStatus) {
    SMAppServiceStatusNotRegistered = 0,
    SMAppServiceStatusEnabled = 1,
    SMAppServiceStatusRequiresApproval = 2,
    SMAppServiceStatusNotFound = 3,
} API_AVAILABLE(macos(13.0));

API_AVAILABLE(macos(13.0))
@interface SMAppService : NSObject

@property (class, readonly) SMAppService *mainAppService;
+ (SMAppService *)loginItemServiceWithIdentifier:(NSString *)identifier;
+ (SMAppService *)agentServiceWithPlistName:(NSString *)plistName;
+ (SMAppService *)daemonServiceWithPlistName:(NSString *)plistName;

- (BOOL)registerAndReturnError:(NSError **)error;
- (BOOL)unregisterAndReturnError:(NSError **)error;
- (void)unregisterWithCompletionHandler:(void (^)(NSError * _Nullable error))handler;

@property (readonly) SMAppServiceStatus status;
+ (SMAppServiceStatus)statusForLegacyURL:(NSURL *)url;
+ (void)openSystemSettingsLoginItems;

@end

NS_ASSUME_NONNULL_END
