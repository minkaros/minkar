/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LAError.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, LAPolicy) {
    LAPolicyDeviceOwnerAuthenticationWithBiometrics = 1,
    LAPolicyDeviceOwnerAuthentication = 2,
    LAPolicyDeviceOwnerAuthenticationWithWatch = 3,
    LAPolicyDeviceOwnerAuthenticationWithBiometricsOrWatch = 4,
    LAPolicyDeviceOwnerAuthenticationWithWristDetection = 5,
};

typedef NS_ENUM(NSInteger, LABiometryType) {
    LABiometryTypeNone = 0,
    LABiometryTypeTouchID = 1,
    LABiometryTypeFaceID = 2,
    LABiometryTypeOpticID = 4,
};

@interface LAContext : NSObject

- (BOOL)canEvaluatePolicy:(LAPolicy)policy error:(NSError * __autoreleasing *)error;
- (void)evaluatePolicy:(LAPolicy)policy localizedReason:(NSString *)localizedReason
                 reply:(void (^)(BOOL success, NSError * _Nullable error))reply;
- (void)invalidate;

@property (nullable, copy) NSString *localizedFallbackTitle;
@property (nullable, copy) NSString *localizedCancelTitle;
@property (nullable, copy) NSString *localizedReason;
@property BOOL interactionNotAllowed;
@property NSTimeInterval touchIDAuthenticationAllowableReuseDuration;
@property (readonly) LABiometryType biometryType;
@property (nullable, readonly) NSData *evaluatedPolicyDomainState;

@end

NS_ASSUME_NONNULL_END
