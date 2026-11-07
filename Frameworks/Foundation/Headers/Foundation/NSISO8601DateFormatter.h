/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Dates in ISO 8601's forms (API 19).
 */

#import <Foundation/NSFormatter.h>

@class NSDate, NSTimeZone, NSString;

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, NSISO8601DateFormatOptions) {
    NSISO8601DateFormatWithYear = 1UL << 0,
    NSISO8601DateFormatWithMonth = 1UL << 1,
    NSISO8601DateFormatWithWeekOfYear = 1UL << 2,
    NSISO8601DateFormatWithDay = 1UL << 4,
    NSISO8601DateFormatWithTime = 1UL << 5,
    NSISO8601DateFormatWithTimeZone = 1UL << 6,
    NSISO8601DateFormatWithSpaceBetweenDateAndTime = 1UL << 7,
    NSISO8601DateFormatWithDashSeparatorInDate = 1UL << 8,
    NSISO8601DateFormatWithColonSeparatorInTime = 1UL << 9,
    NSISO8601DateFormatWithColonSeparatorInTimeZone = 1UL << 10,
    NSISO8601DateFormatWithFractionalSeconds = 1UL << 11,
    NSISO8601DateFormatWithFullDate = NSISO8601DateFormatWithYear | NSISO8601DateFormatWithMonth | NSISO8601DateFormatWithDay | NSISO8601DateFormatWithDashSeparatorInDate,
    NSISO8601DateFormatWithFullTime = NSISO8601DateFormatWithTime | NSISO8601DateFormatWithColonSeparatorInTime | NSISO8601DateFormatWithTimeZone | NSISO8601DateFormatWithColonSeparatorInTimeZone,
    NSISO8601DateFormatWithInternetDateTime = NSISO8601DateFormatWithFullDate | NSISO8601DateFormatWithFullTime,
};

@interface NSISO8601DateFormatter : NSFormatter <NSSecureCoding> {
    NSTimeZone *_timeZone;
    NSISO8601DateFormatOptions _formatOptions;
}

@property (null_resettable, copy) NSTimeZone *timeZone;
@property NSISO8601DateFormatOptions formatOptions;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (NSString *)stringFromDate:(NSDate *)date;
- (nullable NSDate *)dateFromString:(NSString *)string;
+ (NSString *)stringFromDate:(NSDate *)date timeZone:(NSTimeZone *)timeZone formatOptions:(NSISO8601DateFormatOptions)formatOptions;

@end

NS_ASSUME_NONNULL_END
