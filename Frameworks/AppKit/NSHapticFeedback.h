/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Haptic feedback: a tap felt on a trackpad that can give one. Minkar
 * drives no such trackpad, so the default performer gives none, as on a
 * machine without one.
 */

#import <Foundation/NSObject.h>

typedef NS_ENUM(NSInteger, NSHapticFeedbackPattern) {
    NSHapticFeedbackPatternGeneric = 0,
    NSHapticFeedbackPatternAlignment,
    NSHapticFeedbackPatternLevelChange
};

typedef NS_ENUM(NSUInteger, NSHapticFeedbackPerformanceTime) {
    NSHapticFeedbackPerformanceTimeDefault = 0,
    NSHapticFeedbackPerformanceTimeNow,
    NSHapticFeedbackPerformanceTimeDrawCompleted
};

@protocol NSHapticFeedbackPerformer <NSObject>
- (void)performFeedbackPattern:(NSHapticFeedbackPattern)pattern performanceTime:(NSHapticFeedbackPerformanceTime)performanceTime;
@end

@interface NSHapticFeedbackManager : NSObject
@property (class, readonly, strong) id<NSHapticFeedbackPerformer> defaultPerformer;
@end
