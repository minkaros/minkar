/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Recognizes a number of clicks of the given buttons in one place.
 */

#import <AppKit/NSGestureRecognizer.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSClickGestureRecognizer : NSGestureRecognizer
@property NSUInteger buttonMask;
@property NSInteger numberOfClicksRequired;
@property NSInteger numberOfTouchesRequired;
@end

NS_ASSUME_NONNULL_END
