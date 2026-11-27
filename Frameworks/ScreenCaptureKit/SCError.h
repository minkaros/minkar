/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* ScreenCaptureKit's errors: their domain and codes. */

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT NSString *const SCStreamErrorDomain;

typedef NS_ENUM(NSInteger, SCStreamErrorCode) {
    SCStreamErrorUserDeclined = -3801,
    SCStreamErrorFailedToStart = -3802,
    SCStreamErrorMissingEntitlements = -3803,
    SCStreamErrorFailedApplicationConnectionInvalid = -3804,
    SCStreamErrorFailedApplicationConnectionInterrupted = -3805,
    SCStreamErrorFailedNoMatchingApplicationContext = -3806,
    SCStreamErrorAttemptToStartStreamState = -3807,
    SCStreamErrorAttemptToStopStreamState = -3808,
    SCStreamErrorAttemptToUpdateFilterState = -3809,
    SCStreamErrorAttemptToConfigState = -3810,
    SCStreamErrorInternalError = -3811,
    SCStreamErrorInvalidParameter = -3812,
    SCStreamErrorNoWindowList = -3813,
    SCStreamErrorNoDisplayList = -3814,
    SCStreamErrorNoCaptureSource = -3815,
    SCStreamErrorRemovingStream = -3816,
    SCStreamErrorUserStopped = -3817,
    SCStreamErrorFailedToStartAudioCapture = -3818,
    SCStreamErrorFailedToStopAudioCapture = -3819,
    SCStreamErrorFailedToStartMicrophoneCapture = -3820,
    SCStreamErrorSystemStoppedStream = -3821,
};
