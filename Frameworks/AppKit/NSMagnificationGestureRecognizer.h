/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Recognizes a magnification gesture on a trackpad.
 */

#import <AppKit/NSGestureRecognizer.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSMagnificationGestureRecognizer : NSGestureRecognizer
@property CGFloat magnification;
@end

NS_ASSUME_NONNULL_END
