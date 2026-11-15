/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Values an object loads before it can give them without waiting.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AVKeyValueStatus) {
    AVKeyValueStatusUnknown,
    AVKeyValueStatusLoading,
    AVKeyValueStatusLoaded,
    AVKeyValueStatusFailed,
    AVKeyValueStatusCancelled,
};

@protocol AVAsynchronousKeyValueLoading
@required
- (AVKeyValueStatus)statusOfValueForKey:(NSString *)key error:(NSError *_Nullable *_Nullable)outError;
- (void)loadValuesAsynchronouslyForKeys:(NSArray<NSString *> *)keys
                      completionHandler:(nullable void (^)(void))handler;
@end

NS_ASSUME_NONNULL_END
