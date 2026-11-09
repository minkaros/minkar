/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSZone.h>

@class CAMediaTimingFunction;

/* Groups of animated changes: a stack of contexts per thread, each with
   its duration, timing function and completion handler. Changes made
   through animator proxies take effect at once; the completion runs on
   the main queue once the group's duration has passed. */
@interface NSAnimationContext : NSObject {
    NSTimeInterval _duration;
    CAMediaTimingFunction *_timingFunction;
    void (^_completionHandler)(void);
    BOOL _allowsImplicitAnimation;
}

+ (void)runAnimationGroup:(void (^)(NSAnimationContext *context))changes completionHandler:(void (^)(void))completionHandler;
+ (void)runAnimationGroup:(void (^)(NSAnimationContext *context))changes;
+ (void)beginGrouping;
+ (void)endGrouping;
+ (NSAnimationContext *)currentContext;

@property NSTimeInterval duration;
@property (retain) CAMediaTimingFunction *timingFunction;
@property (copy) void (^completionHandler)(void);
@property BOOL allowsImplicitAnimation;

@end
