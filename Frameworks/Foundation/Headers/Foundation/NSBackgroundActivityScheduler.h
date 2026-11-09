/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSBackgroundActivityScheduler (API 19): work to do now and then in the
 * background, at an interval with some tolerance.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSOperation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, NSBackgroundActivityResult) {
    NSBackgroundActivityResultFinished = 1,
    NSBackgroundActivityResultDeferred = 2,
};

typedef void (^NSBackgroundActivityCompletionHandler)(NSBackgroundActivityResult result);

@interface NSBackgroundActivityScheduler : NSObject {
    NSString *_identifier;
    NSQualityOfService _qualityOfService;
    BOOL _repeats;
    NSTimeInterval _interval;
    NSTimeInterval _tolerance;
    id _timer;
    BOOL _running;
}

- (instancetype)initWithIdentifier:(NSString *)identifier;

@property (readonly, copy) NSString *identifier;
@property NSQualityOfService qualityOfService;
@property BOOL repeats;
@property NSTimeInterval interval;
@property NSTimeInterval tolerance;

/* Runs block after the interval (now, without one), on a background
   queue; the block calls its handler when done or to be deferred. */
- (void)scheduleWithBlock:(void (^)(NSBackgroundActivityCompletionHandler completionHandler))block;
- (void)invalidate;
/* Whether work should stop and be done later: no, here. */
@property (readonly) BOOL shouldDefer;

@end

NS_ASSUME_NONNULL_END
