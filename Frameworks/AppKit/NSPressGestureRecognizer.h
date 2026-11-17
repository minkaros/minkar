/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Recognizes a button held down without moving far, for a while.
 */

#import <AppKit/NSGestureRecognizer.h>
#import <Foundation/NSDate.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSPressGestureRecognizer : NSGestureRecognizer
@property NSUInteger buttonMask;
@property NSTimeInterval minimumPressDuration;
@property CGFloat allowableMovement;
@property NSInteger numberOfTouchesRequired;
@end

NS_ASSUME_NONNULL_END
