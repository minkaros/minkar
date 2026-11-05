/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * ISO 8601 dates (API 19): the Gregorian calendar in the formatter's time
 * zone (GMT unless set); the options pick the fields and separators. ARC.
 */

#import <Foundation/NSISO8601DateFormatter.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSTimeZone.h>
#import <Foundation/NSString.h>
#import <Foundation/NSCalendar.h>
#import <Foundation/NSDateComponents.h>
#import <Foundation/NSCoder.h>
#include <math.h>

@implementation NSISO8601DateFormatter

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)init {
    if ((self = [super init]) == nil)
        return nil;
    _timeZone = [NSTimeZone timeZoneForSecondsFromGMT:0];
    _formatOptions = NSISO8601DateFormatWithInternetDateTime;
    return self;
}

- (NSTimeZone *)timeZone {
    return _timeZone;
}

- (void)setTimeZone:(NSTimeZone *)timeZone {
    _timeZone = timeZone != nil ? [timeZone copy] : [NSTimeZone timeZoneForSecondsFromGMT:0];
}

- (NSISO8601DateFormatOptions)formatOptions {
    return _formatOptions;
}

- (void)setFormatOptions:(NSISO8601DateFormatOptions)formatOptions {
    _formatOptions = formatOptions;
}

/* Gregorian with ISO 8601's weeks: Monday first, four days in week one. */
static NSCalendar *isoCalendar(NSTimeZone *zone) {
    NSCalendar *calendar = [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
    [calendar setFirstWeekday:2];
    [calendar setMinimumDaysInFirstWeek:4];
    [calendar setTimeZone:zone];
    return calendar;
}

- (NSCalendar *)_calendar {
    return isoCalendar(_timeZone);
}

+ (NSString *)stringFromDate:(NSDate *)date timeZone:(NSTimeZone *)timeZone formatOptions:(NSISO8601DateFormatOptions)formatOptions {
    NSISO8601DateFormatter *f = [[self alloc] init];
    f.timeZone = timeZone;
    f.formatOptions = formatOptions;
    return [f stringFromDate:date];
}

- (NSString *)stringFromDate:(NSDate *)date {
    NSISO8601DateFormatOptions o = _formatOptions;
    NSCalendar *calendar = [self _calendar];
    NSDateComponents *c = [calendar components:NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay |
        NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond | NSCalendarUnitWeekOfYear |
        NSCalendarUnitYearForWeekOfYear | NSCalendarUnitWeekday fromDate:date];
    NSMutableString *s = [NSMutableString string];
    NSString *dash = (o & NSISO8601DateFormatWithDashSeparatorInDate) ? @"-" : @"";
    BOOL week = (o & NSISO8601DateFormatWithWeekOfYear) != 0;

    if (o & NSISO8601DateFormatWithYear)
        [s appendFormat:@"%04ld", (long)(week ? c.yearForWeekOfYear : c.year)];
    if (week) {
        [s appendFormat:@"%@W%02ld", [s length] ? dash : @"", (long)c.weekOfYear];
        if (o & NSISO8601DateFormatWithDay)
            [s appendFormat:@"%@%02ld", dash, (long)((c.weekday + 5) % 7 + 1)];
    } else {
        if (o & NSISO8601DateFormatWithMonth)
            [s appendFormat:@"%@%02ld", [s length] ? dash : @"", (long)c.month];
        if (o & NSISO8601DateFormatWithDay) {
            if (o & NSISO8601DateFormatWithMonth)
                [s appendFormat:@"%@%02ld", dash, (long)c.day];
            else
                [s appendFormat:@"%@%03lu", [s length] ? dash : @"",
                    (unsigned long)[calendar ordinalityOfUnit:NSCalendarUnitDay inUnit:NSCalendarUnitYear forDate:date]];
        }
    }
    if (o & NSISO8601DateFormatWithTime) {
        NSString *colon = (o & NSISO8601DateFormatWithColonSeparatorInTime) ? @":" : @"";
        if ([s length])
            [s appendString:(o & NSISO8601DateFormatWithSpaceBetweenDateAndTime) ? @" " : @"T"];
        [s appendFormat:@"%02ld%@%02ld%@%02ld", (long)c.hour, colon, (long)c.minute, colon, (long)c.second];
        if (o & NSISO8601DateFormatWithFractionalSeconds) {
            double t = [date timeIntervalSinceReferenceDate];
            long ms = lround((t - floor(t)) * 1000) % 1000;
            [s appendFormat:@".%03ld", ms];
        }
    }
    if (o & NSISO8601DateFormatWithTimeZone) {
        NSInteger offset = [_timeZone secondsFromGMTForDate:date];
        if (offset == 0)
            [s appendString:@"Z"];
        else {
            NSInteger a = labs(offset) / 60;
            [s appendFormat:@"%c%02ld%@%02ld", offset < 0 ? '-' : '+', (long)(a / 60),
                (o & NSISO8601DateFormatWithColonSeparatorInTimeZone) ? @":" : @"", (long)(a % 60)];
        }
    }
    return s;
}

/* Digits at *i, at most max of them (exactly n when n > 0). */
static BOOL digits(NSString *s, NSUInteger *i, int n, long *value) {
    long v = 0;
    int count = 0;
    while (*i < [s length] && count < n) {
        unichar c = [s characterAtIndex:*i];
        if (c < '0' || c > '9')
            break;
        v = v * 10 + (c - '0');
        (*i)++;
        count++;
    }
    *value = v;
    return count == n;
}

static BOOL skip(NSString *s, NSUInteger *i, unichar c) {
    if (*i < [s length] && [s characterAtIndex:*i] == c) {
        (*i)++;
        return YES;
    }
    return NO;
}

/* The fields the options name, in their order; separators where they may
 * be. Not a date when something is missing or left over. */
- (NSDate *)dateFromString:(NSString *)string {
    NSISO8601DateFormatOptions o = _formatOptions;
    NSUInteger i = 0;
    long year = 1, month = 1, day = 1, hour = 0, minute = 0, second = 0, week = 1, weekday = 1, ordinal = 0;
    double fraction = 0;
    NSTimeZone *zone = _timeZone;
    BOOL isWeek = (o & NSISO8601DateFormatWithWeekOfYear) != 0;
    BOOL any = NO;

    if (o & NSISO8601DateFormatWithYear) {
        if (!digits(string, &i, 4, &year)) return nil;
        any = YES;
    }
    if (isWeek) {
        if (any) skip(string, &i, '-');
        if (!skip(string, &i, 'W') || !digits(string, &i, 2, &week)) return nil;
        if (o & NSISO8601DateFormatWithDay) {
            skip(string, &i, '-');
            if (!digits(string, &i, 1, &weekday)) return nil;
        }
        any = YES;
    } else {
        if (o & NSISO8601DateFormatWithMonth) {
            if (any) skip(string, &i, '-');
            if (!digits(string, &i, 2, &month)) return nil;
            any = YES;
        }
        if (o & NSISO8601DateFormatWithDay) {
            if (any) skip(string, &i, '-');
            if (o & NSISO8601DateFormatWithMonth) {
                if (!digits(string, &i, 2, &day)) return nil;
            } else if (!digits(string, &i, 3, &ordinal))
                return nil;
            any = YES;
        }
    }
    if (o & NSISO8601DateFormatWithTime) {
        if (any && !skip(string, &i, 'T') && !skip(string, &i, ' ') && !skip(string, &i, 't'))
            return nil;
        if (!digits(string, &i, 2, &hour)) return nil;
        skip(string, &i, ':');
        if (!digits(string, &i, 2, &minute)) return nil;
        skip(string, &i, ':');
        if (!digits(string, &i, 2, &second)) return nil;
        if (skip(string, &i, '.') || skip(string, &i, ',')) {
            double scale = 0.1;
            while (i < [string length]) {
                unichar c = [string characterAtIndex:i];
                if (c < '0' || c > '9')
                    break;
                fraction += (c - '0') * scale;
                scale /= 10;
                i++;
            }
        }
    }
    if (o & NSISO8601DateFormatWithTimeZone) {
        if (skip(string, &i, 'Z') || skip(string, &i, 'z'))
            zone = [NSTimeZone timeZoneForSecondsFromGMT:0];
        else {
            int sign = skip(string, &i, '+') ? 1 : skip(string, &i, '-') ? -1 : 0;
            long zh = 0, zm = 0;
            if (sign == 0 || !digits(string, &i, 2, &zh)) return nil;
            skip(string, &i, ':');
            digits(string, &i, 2, &zm);
            zone = [NSTimeZone timeZoneForSecondsFromGMT:sign * (zh * 3600 + zm * 60)];
        }
    }
    if (i != [string length])
        return nil;
    NSCalendar *calendar = isoCalendar(zone);
    NSDateComponents *c = [[NSDateComponents alloc] init];
    if (isWeek) {
        c.yearForWeekOfYear = year;
        c.weekOfYear = week;
        c.weekday = weekday % 7 + 1;
    } else {
        c.year = year;
        c.month = ordinal ? 1 : month;
        c.day = ordinal ? ordinal : day;
    }
    c.hour = hour;
    c.minute = minute;
    c.second = second;
    NSDate *date = [calendar dateFromComponents:c];
    return fraction > 0 ? [date dateByAddingTimeInterval:fraction] : date;
}

- (NSString *)stringForObjectValue:(id)obj {
    return [obj isKindOfClass:[NSDate class]] ? [self stringFromDate:obj] : nil;
}

- (BOOL)getObjectValue:(id *)obj forString:(NSString *)string errorDescription:(NSString **)error {
    NSDate *date = [self dateFromString:string];
    if (obj != NULL)
        *obj = date;
    if (date == nil && error != NULL)
        *error = @"Not an ISO 8601 date";
    return date != nil;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_timeZone forKey:@"NS.timeZone"];
    [coder encodeInteger:(NSInteger)_formatOptions forKey:@"NS.formatOptions"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [self init]) == nil)
        return nil;
    NSTimeZone *zone = [coder decodeObjectOfClass:[NSTimeZone class] forKey:@"NS.timeZone"];
    if (zone != nil)
        _timeZone = zone;
    _formatOptions = (NSISO8601DateFormatOptions)[coder decodeIntegerForKey:@"NS.formatOptions"];
    return self;
}

@end
