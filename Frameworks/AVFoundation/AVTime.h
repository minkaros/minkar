/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CoreMedia's times in values and archives.
 */

#import <Foundation/Foundation.h>
#import <CoreMedia/CMTime.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSValue (NSValueCMTimeExtensions)
+ (NSValue *)valueWithCMTime:(CMTime)time;
@property (readonly) CMTime CMTimeValue;
+ (NSValue *)valueWithCMTimeRange:(CMTimeRange)timeRange;
@property (readonly) CMTimeRange CMTimeRangeValue;
+ (NSValue *)valueWithCMTimeMapping:(CMTimeMapping)timeMapping;
@property (readonly) CMTimeMapping CMTimeMappingValue;
@end

@interface NSCoder (AVTimeCoding)
- (void)encodeCMTime:(CMTime)time forKey:(NSString *)key;
- (CMTime)decodeCMTimeForKey:(NSString *)key;
- (void)encodeCMTimeRange:(CMTimeRange)timeRange forKey:(NSString *)key;
- (CMTimeRange)decodeCMTimeRangeForKey:(NSString *)key;
- (void)encodeCMTimeMapping:(CMTimeMapping)timeMapping forKey:(NSString *)key;
- (CMTimeMapping)decodeCMTimeMappingForKey:(NSString *)key;
@end

NS_ASSUME_NONNULL_END
