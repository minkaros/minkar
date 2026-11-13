/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT NSString *const LAErrorDomain;

typedef NS_ENUM(NSInteger, LAError) {
    LAErrorAuthenticationFailed = -1,
    LAErrorUserCancel = -2,
    LAErrorUserFallback = -3,
    LAErrorSystemCancel = -4,
    LAErrorPasscodeNotSet = -5,
    LAErrorBiometryNotAvailable = -6,
    LAErrorBiometryNotEnrolled = -7,
    LAErrorBiometryLockout = -8,
    LAErrorAppCancel = -9,
    LAErrorInvalidContext = -10,
    LAErrorWatchNotAvailable = -11,
    LAErrorBiometryNotPaired = -12,
    LAErrorBiometryDisconnected = -13,
    LAErrorInvalidDimensions = -14,
    LAErrorNotInteractive = -1004,
};
