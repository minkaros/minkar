/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

FOUNDATION_EXPORT NSString *const kCLErrorDomain;

typedef NS_ENUM(NSInteger, CLError) {
    kCLErrorLocationUnknown = 0,
    kCLErrorDenied = 1,
    kCLErrorNetwork = 2,
    kCLErrorHeadingFailure = 3,
};
