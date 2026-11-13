/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

typedef NSString *AVMediaType;

/* The media types (the same codes as CoreMedia's, as strings). */
FOUNDATION_EXPORT AVMediaType const AVMediaTypeVideo;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeAudio;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeText;
FOUNDATION_EXPORT AVMediaType const AVMediaTypeMuxed;

typedef NS_ENUM(NSInteger, AVAuthorizationStatus) {
    AVAuthorizationStatusNotDetermined = 0,
    AVAuthorizationStatusRestricted = 1,
    AVAuthorizationStatusDenied = 2,
    AVAuthorizationStatusAuthorized = 3,
};
