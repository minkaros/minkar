/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSHapticFeedback.h>

@interface _NSHapticFeedbackPerformer : NSObject <NSHapticFeedbackPerformer>
@end

@implementation _NSHapticFeedbackPerformer
- (void)performFeedbackPattern:(NSHapticFeedbackPattern)pattern performanceTime:(NSHapticFeedbackPerformanceTime)performanceTime {
}
@end

@implementation NSHapticFeedbackManager

+ (id<NSHapticFeedbackPerformer>)defaultPerformer {
    static _NSHapticFeedbackPerformer *performer;
    if (performer == nil)
        performer = [[_NSHapticFeedbackPerformer alloc] init];
    return performer;
}

@end
