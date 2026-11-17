/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Recognizes a rotation gesture on a trackpad.
 */

#import <AppKit/NSGestureRecognizer.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSRotationGestureRecognizer : NSGestureRecognizer
@property CGFloat rotation;
@property CGFloat rotationInDegrees;
@end

NS_ASSUME_NONNULL_END
