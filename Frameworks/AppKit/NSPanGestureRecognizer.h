/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Recognizes a drag with the given buttons, reporting how far and how
 * fast it went.
 */

#import <AppKit/NSGestureRecognizer.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSPanGestureRecognizer : NSGestureRecognizer
@property NSUInteger buttonMask;
@property NSInteger numberOfTouchesRequired;
- (NSPoint)translationInView:(nullable NSView *)view;
- (void)setTranslation:(NSPoint)translation inView:(nullable NSView *)view;
- (NSPoint)velocityInView:(nullable NSView *)view;
@end

NS_ASSUME_NONNULL_END
