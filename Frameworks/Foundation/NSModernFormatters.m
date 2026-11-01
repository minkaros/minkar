/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * The formatters of API 19 that NSFormatter lacked.
 *
 * On ICU (by locale): NSRelativeDateTimeFormatter ("3 days ago"),
 * NSDateIntervalFormatter ("Nov 14 – 16, 2023"), NSListFormatter
 * ("a, b, and c"), and +[NSNumberFormatter localizedStringFromNumber:
 * numberStyle:] (CFNumberFormatter). NSByteCountFormatter and
 * NSDateComponentsFormatter follow the API's rules with English unit
 * names, numbers formatted for the locale: ICU has no C interface for
 * units and durations.
 */

#import <Foundation/NSByteCountFormatter.h>
#import <Foundation/NSDateComponentsFormatter.h>
#import <Foundation/NSDateIntervalFormatter.h>
#import <Foundation/NSListFormatter.h>
#import <Foundation/NSRelativeDateTimeFormatter.h>
#import <Foundation/NSNumberFormatter.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDateComponents.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSString.h>
#import <Foundation/NSTimeZone.h>
#import <CoreFoundation/CFNumberFormatter.h>
#include <math.h>
#include <stdlib.h>
#include <unicode/ureldatefmt.h>
#include <unicode/udateintervalformat.h>
#include <unicode/ulistformatter.h>

static const char *localeID(NSLocale *locale) {
    NSString *identifier = [(locale ?: [NSLocale currentLocale]) localeIdentifier];
    return [identifier length] ? [identifier UTF8String] : "en_US";
}

static NSString *fromUChars(const UChar *chars, int32_t length) {
    return [NSString stringWithCharacters:(const unichar *)chars length:(NSUInteger)length];
}

// A CF string copy (CF takes only its own strings in some places).
static CFStringRef copyCF(NSString *s) {
    return CFStringCreateWithCString(NULL, [s UTF8String], kCFStringEncodingUTF8);
}

// A number in the locale's form, with these fraction digits.
static NSString *formatNumber(double value, NSInteger minFraction, NSInteger maxFraction, NSLocale *locale) {
    CFStringRef name = copyCF([(locale ?: [NSLocale currentLocale]) localeIdentifier] ?: @"en_US");
    CFLocaleRef cfLocale = CFLocaleCreate(NULL, name);
    CFNumberFormatterRef formatter = CFNumberFormatterCreate(NULL, cfLocale, kCFNumberFormatterDecimalStyle);
    CFNumberRef min = CFNumberCreate(NULL, kCFNumberNSIntegerType, &minFraction);
    CFNumberRef max = CFNumberCreate(NULL, kCFNumberNSIntegerType, &maxFraction);
    CFNumberFormatterSetProperty(formatter, kCFNumberFormatterMinFractionDigits, min);
    CFNumberFormatterSetProperty(formatter, kCFNumberFormatterMaxFractionDigits, max);
    CFStringRef string = CFNumberFormatterCreateStringWithValue(NULL, formatter, kCFNumberDoubleType, &value);
    CFRelease(min);
    CFRelease(max);
    CFRelease(formatter);
    if (cfLocale) CFRelease(cfLocale);
    CFRelease(name);
    return (__bridge_transfer NSString *)string;
}

#pragma mark - NSNumberFormatter

@implementation NSNumberFormatter (NSLocalizedNumber)

+ (NSString *)localizedStringFromNumber:(NSNumber *)number numberStyle:(NSNumberFormatterStyle)style {
    CFStringRef name = copyCF([[NSLocale currentLocale] localeIdentifier] ?: @"en_US");
    CFLocaleRef cfLocale = CFLocaleCreate(NULL, name);
    CFNumberFormatterRef formatter = CFNumberFormatterCreate(NULL, cfLocale, (CFNumberFormatterStyle)style);
    double value = [number doubleValue];
    long long integer = [number longLongValue];
    CFStringRef string = (value == (double)integer)
        ? CFNumberFormatterCreateStringWithValue(NULL, formatter, kCFNumberLongLongType, &integer)
        : CFNumberFormatterCreateStringWithValue(NULL, formatter, kCFNumberDoubleType, &value);
    CFRelease(formatter);
    if (cfLocale) CFRelease(cfLocale);
    CFRelease(name);
    return (__bridge_transfer NSString *)string;
}

@end

#pragma mark - NSByteCountFormatter

@implementation NSByteCountFormatter

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _allowsNonnumericFormatting = YES;
        _includesUnit = YES;
        _includesCount = YES;
        _adaptive = YES;
    }
    return self;
}

+ (NSString *)stringFromByteCount:(long long)byteCount countStyle:(NSByteCountFormatterCountStyle)countStyle {
    NSByteCountFormatter *formatter = [[NSByteCountFormatter alloc] init];
    [formatter setCountStyle:countStyle];
    return [formatter stringFromByteCount:byteCount];
}

- (NSString *)stringForObjectValue:(id)object {
    return [object isKindOfClass:[NSNumber class]] ? [self stringFromByteCount:[object longLongValue]] : nil;
}

// File and decimal counts are in thousands, memory and binary in 1024s;
// the largest allowed unit the count reaches is used, with as many
// fraction digits as its size calls for (none for KB, one for MB, two
// beyond) unless the formatter is not adaptive.
- (NSString *)stringFromByteCount:(long long)byteCount {
    static NSString *units[] = { @"bytes", @"KB", @"MB", @"GB", @"TB", @"PB", @"EB", @"ZB", @"YB" };
    double base = (_countStyle == NSByteCountFormatterCountStyleMemory ||
                   _countStyle == NSByteCountFormatterCountStyleBinary) ? 1024 : 1000;
    NSByteCountFormatterUnits allowed = _allowedUnits == NSByteCountFormatterUseDefault ? NSByteCountFormatterUseAll : _allowedUnits;
    double magnitude = fabs((double)byteCount);

    int unit = -1;
    for (int i = 8; i >= 0; i--) {
        NSUInteger bit = i == 8 ? NSByteCountFormatterUseYBOrHigher : (1UL << i);
        if ((allowed & bit) && (magnitude >= pow(base, i) || unit < 0)) {
            if (magnitude >= pow(base, i) || i == 0) {
                unit = i;
                break;
            }
            unit = i;      // the smallest allowed, if nothing else fits
        }
    }
    if (unit < 0)
        unit = 0;

    if (byteCount == 0 && _allowsNonnumericFormatting && _includesCount && _includesUnit)
        return @"Zero KB";

    double value = byteCount / pow(base, unit);
    NSInteger fraction = !_adaptive ? (unit == 0 ? 0 : 1) : (unit <= 1 ? 0 : unit == 2 ? 1 : 2);
    NSString *count = formatNumber(value, _zeroPadsFractionDigits ? fraction : 0, fraction, nil);
    NSString *name = unit == 0 ? (llabs(byteCount) == 1 ? @"byte" : @"bytes") : units[unit];

    NSString *result;
    if (_includesCount && _includesUnit)
        result = [NSString stringWithFormat:@"%@ %@", count, name];
    else if (_includesCount)
        result = count;
    else
        result = name;
    if (_includesActualByteCount && unit > 0)
        result = [result stringByAppendingFormat:@" (%@ bytes)", formatNumber((double)byteCount, 0, 0, nil)];
    return result;
}

@end

#pragma mark - NSDateComponentsFormatter

// The units, largest first, with their length in seconds and names
// (abbreviated, brief, short singular/plural, full singular/plural).
typedef struct {
    NSCalendarUnit unit;
    double seconds;
    const char *abbreviated, *brief, *shortOne, *shortMany, *full;
} DurationUnit;

static const DurationUnit durationUnits[] = {
    { NSCalendarUnitYear, 365.2425 * 86400, "y", "yr", "yr", "yrs", "year" },
    { NSCalendarUnitMonth, 30.436875 * 86400, "mo", "mth", "mth", "mths", "month" },
    { NSCalendarUnitWeekOfMonth, 7 * 86400, "w", "wk", "wk", "wks", "week" },
    { NSCalendarUnitDay, 86400, "d", "day", "day", "days", "day" },
    { NSCalendarUnitHour, 3600, "h", "hr", "hr", "hr", "hour" },
    { NSCalendarUnitMinute, 60, "m", "min", "min", "min", "minute" },
    { NSCalendarUnitSecond, 1, "s", "sec", "sec", "sec", "second" },
};
#define DURATION_UNITS (sizeof(durationUnits) / sizeof(durationUnits[0]))

@implementation NSDateComponentsFormatter

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _zeroFormattingBehavior = NSDateComponentsFormatterZeroFormattingBehaviorDefault;
        _unitsStyle = NSDateComponentsFormatterUnitsStylePositional;
    }
    return self;
}

+ (NSString *)localizedStringFromDateComponents:(NSDateComponents *)components
                                     unitsStyle:(NSDateComponentsFormatterUnitsStyle)unitsStyle {
    NSDateComponentsFormatter *formatter = [[NSDateComponentsFormatter alloc] init];
    [formatter setUnitsStyle:unitsStyle];
    return [formatter stringFromDateComponents:components];
}

- (NSString *)stringForObjectValue:(id)object {
    if ([object isKindOfClass:[NSNumber class]])
        return [self stringFromTimeInterval:[object doubleValue]];
    if ([object isKindOfClass:[NSDateComponents class]])
        return [self stringFromDateComponents:object];
    return nil;
}

- (NSString *)stringFromDate:(NSDate *)startDate toDate:(NSDate *)endDate {
    if (startDate == nil || endDate == nil)
        return nil;
    return [self stringFromTimeInterval:[endDate timeIntervalSinceDate:startDate]];
}

- (NSString *)stringFromDateComponents:(NSDateComponents *)components {
    NSTimeInterval total = 0;
    NSInteger values[] = { [components year], [components month], [components weekOfMonth], [components day],
                           [components hour], [components minute], [components second] };
    for (NSUInteger i = 0; i < DURATION_UNITS; i++)
        if (values[i] != NSNotFound && values[i] != NSDateComponentUndefined)
            total += values[i] * durationUnits[i].seconds;
    return [self stringFromTimeInterval:total];
}

static NSString *spellOut(long long n) {
    CFLocaleRef locale = CFLocaleCreate(NULL, CFSTR("en_US"));
    CFNumberFormatterRef formatter = CFNumberFormatterCreate(NULL, locale, kCFNumberFormatterSpellOutStyle);
    CFStringRef string = CFNumberFormatterCreateStringWithValue(NULL, formatter, kCFNumberLongLongType, &n);
    CFRelease(formatter);
    CFRelease(locale);
    return (__bridge_transfer NSString *)string;
}

- (NSString *)stringFromTimeInterval:(NSTimeInterval)interval {
    if (!isfinite(interval))
        return nil;
    NSCalendarUnit allowed = _allowedUnits;
    if (allowed == 0)
        allowed = _unitsStyle == NSDateComponentsFormatterUnitsStylePositional
            ? (NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond)
            : (NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitWeekOfMonth | NSCalendarUnitDay |
               NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond);

    BOOL negative = interval < 0;
    double rest = floor(fabs(interval) + 0.5);
    long long amounts[DURATION_UNITS];
    BOOL used[DURATION_UNITS];
    NSInteger last = -1;
    for (NSUInteger i = 0; i < DURATION_UNITS; i++) {
        used[i] = (allowed & durationUnits[i].unit) != 0;
        amounts[i] = 0;
        if (used[i]) {
            amounts[i] = (long long)floor(rest / durationUnits[i].seconds);
            rest -= amounts[i] * durationUnits[i].seconds;
            last = (NSInteger)i;
        }
    }
    // What is left below the smallest unit is rounded into it.
    if (last >= 0 && rest >= durationUnits[last].seconds / 2 && durationUnits[last].seconds > 1)
        amounts[last]++;

    NSDateComponentsFormatterZeroFormattingBehavior zeros = _zeroFormattingBehavior;
    BOOL positional = _unitsStyle == NSDateComponentsFormatterUnitsStylePositional;
    if (zeros & NSDateComponentsFormatterZeroFormattingBehaviorDefault)
        zeros = positional ? NSDateComponentsFormatterZeroFormattingBehaviorDropLeading
                           : NSDateComponentsFormatterZeroFormattingBehaviorDropAll;

    // The units shown.
    NSMutableArray *shown = [NSMutableArray array];
    NSInteger firstNonZero = -1, lastNonZero = -1;
    for (NSUInteger i = 0; i < DURATION_UNITS; i++)
        if (used[i] && amounts[i] != 0) {
            if (firstNonZero < 0) firstNonZero = (NSInteger)i;
            lastNonZero = (NSInteger)i;
        }
    for (NSUInteger i = 0; i < DURATION_UNITS; i++) {
        if (!used[i])
            continue;
        BOOL zero = amounts[i] == 0;
        if (zero && !(zeros & NSDateComponentsFormatterZeroFormattingBehaviorPad)) {
            BOOL leading = firstNonZero < 0 || (NSInteger)i < firstNonZero;
            BOOL trailing = firstNonZero >= 0 && (NSInteger)i > lastNonZero;
            BOOL middle = !leading && !trailing;
            if ((leading && (zeros & NSDateComponentsFormatterZeroFormattingBehaviorDropLeading)) ||
                (middle && (zeros & NSDateComponentsFormatterZeroFormattingBehaviorDropMiddle)) ||
                (trailing && (zeros & NSDateComponentsFormatterZeroFormattingBehaviorDropTrailing)))
                continue;
        }
        [shown addObject:@(i)];
    }
    // Positional styles show at least minutes and seconds.
    if (positional && [shown count] < 2 && last >= 0) {
        [shown removeAllObjects];
        for (NSInteger i = last; i >= 0 && [shown count] < 2; i--)
            if (used[i])
                [shown insertObject:@(i) atIndex:0];
    }
    if ([shown count] == 0 && last >= 0)
        [shown addObject:@(last)];
    if (_maximumUnitCount > 0 && (NSInteger)[shown count] > _maximumUnitCount)
        [shown removeObjectsInRange:NSMakeRange((NSUInteger)_maximumUnitCount, [shown count] - (NSUInteger)_maximumUnitCount)];

    NSMutableArray *parts = [NSMutableArray array];
    for (NSUInteger k = 0; k < [shown count]; k++) {
        NSUInteger i = [shown[k] unsignedIntegerValue];
        long long n = amounts[i];
        const DurationUnit *u = &durationUnits[i];
        switch (_unitsStyle) {
        case NSDateComponentsFormatterUnitsStylePositional:
            [parts addObject:k == 0 ? [NSString stringWithFormat:@"%lld", n] : [NSString stringWithFormat:@"%02lld", n]];
            break;
        case NSDateComponentsFormatterUnitsStyleAbbreviated:
            [parts addObject:[NSString stringWithFormat:@"%lld%s", n, u->abbreviated]];
            break;
        case NSDateComponentsFormatterUnitsStyleBrief:
            [parts addObject:[NSString stringWithFormat:@"%lld%s", n, u->brief]];
            break;
        case NSDateComponentsFormatterUnitsStyleShort:
            [parts addObject:[NSString stringWithFormat:@"%lld %s", n, n == 1 ? u->shortOne : u->shortMany]];
            break;
        case NSDateComponentsFormatterUnitsStyleSpellOut:
            [parts addObject:[NSString stringWithFormat:@"%@ %s%s", spellOut(n), u->full, n == 1 ? "" : "s"]];
            break;
        default:
            [parts addObject:[NSString stringWithFormat:@"%lld %s%s", n, u->full, n == 1 ? "" : "s"]];
            break;
        }
    }
    NSString *separator = positional ? @":"
        : (_unitsStyle == NSDateComponentsFormatterUnitsStyleAbbreviated || _unitsStyle == NSDateComponentsFormatterUnitsStyleBrief) ? @" "
        : @", ";
    NSString *result = [parts componentsJoinedByString:separator];
    if (negative)
        result = [@"-" stringByAppendingString:result];
    if (_includesApproximationPhrase)
        result = [@"About " stringByAppendingString:result];
    if (_includesTimeRemainingPhrase)
        result = [result stringByAppendingString:@" remaining"];
    return result;
}

@end

#pragma mark - NSRelativeDateTimeFormatter

@implementation NSRelativeDateTimeFormatter

- (NSString *)stringForObjectValue:(id)object {
    return [object isKindOfClass:[NSDate class]] ? [self localizedStringForDate:object relativeToDate:[NSDate date]] : nil;
}

- (NSString *)localizedStringForDate:(NSDate *)date relativeToDate:(NSDate *)referenceDate {
    return [self localizedStringFromTimeInterval:[date timeIntervalSinceDate:referenceDate]];
}

- (NSString *)localizedStringFromDateComponents:(NSDateComponents *)components {
    struct { NSInteger value; URelativeDateTimeUnit unit; } fields[7] = {
        { [components year], UDAT_REL_UNIT_YEAR }, { [components month], UDAT_REL_UNIT_MONTH },
        { [components weekOfMonth], UDAT_REL_UNIT_WEEK }, { [components day], UDAT_REL_UNIT_DAY },
        { [components hour], UDAT_REL_UNIT_HOUR }, { [components minute], UDAT_REL_UNIT_MINUTE },
        { [components second], UDAT_REL_UNIT_SECOND },
    };
    for (size_t i = 0; i < 7; i++)
        if (fields[i].value != NSNotFound && fields[i].value != NSDateComponentUndefined && fields[i].value != 0)
            return [self _format:(double)fields[i].value unit:fields[i].unit];
    return [self _format:0 unit:UDAT_REL_UNIT_SECOND];
}

// The largest unit the interval reaches, as a whole number of it.
- (NSString *)localizedStringFromTimeInterval:(NSTimeInterval)interval {
    static const struct { double seconds; URelativeDateTimeUnit unit; } units[] = {
        { 365.2425 * 86400, UDAT_REL_UNIT_YEAR }, { 30.436875 * 86400, UDAT_REL_UNIT_MONTH },
        { 7 * 86400, UDAT_REL_UNIT_WEEK }, { 86400, UDAT_REL_UNIT_DAY }, { 3600, UDAT_REL_UNIT_HOUR },
        { 60, UDAT_REL_UNIT_MINUTE }, { 1, UDAT_REL_UNIT_SECOND },
    };
    double magnitude = fabs(interval);
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); i++)
        if (magnitude >= units[i].seconds || i + 1 == sizeof(units) / sizeof(units[0])) {
            double amount = trunc(interval / units[i].seconds);
            return [self _format:amount unit:units[i].unit];
        }
    return nil;
}

- (NSString *)_format:(double)amount unit:(URelativeDateTimeUnit)unit {
    UErrorCode status = U_ZERO_ERROR;
    UDateRelativeDateTimeFormatterStyle width =
        _unitsStyle == NSRelativeDateTimeFormatterUnitsStyleShort ? UDAT_STYLE_SHORT
        : _unitsStyle == NSRelativeDateTimeFormatterUnitsStyleAbbreviated ? UDAT_STYLE_NARROW : UDAT_STYLE_LONG;
    UNumberFormat *numbers = NULL;
    if (_unitsStyle == NSRelativeDateTimeFormatterUnitsStyleSpellOut)
        numbers = unum_open(UNUM_SPELLOUT, NULL, 0, localeID(_locale), NULL, &status);
    URelativeDateTimeFormatter *formatter = ureldatefmt_open(localeID(_locale), numbers, width,
                                                             UDISPCTX_CAPITALIZATION_NONE, &status);
    if (U_FAILURE(status) || formatter == NULL)
        return nil;
    UChar buffer[256];
    int32_t length = _dateTimeStyle == NSRelativeDateTimeFormatterStyleNamed
        ? ureldatefmt_format(formatter, amount, unit, buffer, 256, &status)
        : ureldatefmt_formatNumeric(formatter, amount, unit, buffer, 256, &status);
    ureldatefmt_close(formatter);
    return U_SUCCESS(status) ? fromUChars(buffer, length) : nil;
}

@end

#pragma mark - NSDateIntervalFormatter

@implementation NSDateIntervalFormatter

- (instancetype)init {
    if ((self = [super init]) != nil) {
        _dateStyle = NSDateFormatterMediumStyle;
        _timeStyle = NSDateFormatterMediumStyle;
    }
    return self;
}

// An ICU skeleton for the styles (or the template).
- (NSString *)_skeleton {
    if ([_dateTemplate length] > 0)
        return _dateTemplate;
    static NSString *dates[] = { @"", @"yMd", @"yMMMd", @"yMMMMd", @"yMMMMEEEEd" };
    static NSString *times[] = { @"", @"jm", @"jms", @"jmsz", @"jmsz" };
    NSUInteger d = _dateStyle <= NSDateFormatterFullStyle ? _dateStyle : 0;
    NSUInteger t = _timeStyle <= NSDateFormatterFullStyle ? _timeStyle : 0;
    return [dates[d] stringByAppendingString:times[t]];
}

- (NSString *)stringFromDate:(NSDate *)fromDate toDate:(NSDate *)toDate {
    if (fromDate == nil || toDate == nil)
        return nil;
    NSString *skeleton = [self _skeleton];
    if ([skeleton length] == 0)
        return @"";
    NSString *zone = [(_timeZone ?: [NSTimeZone defaultTimeZone]) name] ?: @"GMT";
    NSUInteger skeletonLength = [skeleton length], zoneLength = [zone length];
    unichar skeletonChars[skeletonLength], zoneChars[zoneLength];
    [skeleton getCharacters:skeletonChars range:NSMakeRange(0, skeletonLength)];
    [zone getCharacters:zoneChars range:NSMakeRange(0, zoneLength)];
    UErrorCode status = U_ZERO_ERROR;
    UDateIntervalFormat *formatter = udtitvfmt_open(localeID(_locale), (const UChar *)skeletonChars, (int32_t)skeletonLength,
                                                    (const UChar *)zoneChars, (int32_t)zoneLength, &status);
    if (U_FAILURE(status) || formatter == NULL)
        return nil;
    UChar buffer[512];
    int32_t length = udtitvfmt_format(formatter, [fromDate timeIntervalSince1970] * 1000.0,
                                      [toDate timeIntervalSince1970] * 1000.0, buffer, 512, NULL, &status);
    udtitvfmt_close(formatter);
    return U_SUCCESS(status) ? fromUChars(buffer, length) : nil;
}

- (NSString *)stringFromDateInterval:(NSDateInterval *)dateInterval {
    return [self stringFromDate:[dateInterval startDate] toDate:[dateInterval endDate]];
}

- (NSString *)stringForObjectValue:(id)object {
    return [object isKindOfClass:[NSDateInterval class]] ? [self stringFromDateInterval:object] : nil;
}

@end

#pragma mark - NSListFormatter

@implementation NSListFormatter

+ (NSString *)localizedStringByJoiningStrings:(NSArray *)strings {
    return [[[self alloc] init] stringFromItems:strings];
}

- (NSString *)stringForObjectValue:(id)object {
    return [object isKindOfClass:[NSArray class]] ? [self stringFromItems:object] : nil;
}

- (NSString *)stringFromItems:(NSArray *)items {
    NSUInteger count = [items count];
    NSMutableArray *strings = [NSMutableArray arrayWithCapacity:count];
    for (id item in items) {
        NSString *string = _itemFormatter ? [_itemFormatter stringForObjectValue:item] : [item description];
        [strings addObject:string ?: @""];
    }
    UErrorCode status = U_ZERO_ERROR;
    UListFormatter *formatter = ulistfmt_open(localeID(_locale), &status);
    if (U_FAILURE(status) || formatter == NULL)
        return [strings componentsJoinedByString:@", "];
    const UChar **chars = calloc(count ? count : 1, sizeof(UChar *));
    int32_t *lengths = calloc(count ? count : 1, sizeof(int32_t));
    for (NSUInteger i = 0; i < count; i++) {
        NSUInteger n = [strings[i] length];
        UChar *buffer = malloc(sizeof(UChar) * (n ? n : 1));
        [strings[i] getCharacters:(unichar *)buffer range:NSMakeRange(0, n)];
        chars[i] = buffer;
        lengths[i] = (int32_t)n;
    }
    int32_t needed = ulistfmt_format(formatter, chars, lengths, (int32_t)count, NULL, 0, &status);
    NSString *result = nil;
    if (status == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(status)) {
        status = U_ZERO_ERROR;
        UChar *out = malloc(sizeof(UChar) * (needed + 1));
        int32_t length = ulistfmt_format(formatter, chars, lengths, (int32_t)count, out, needed + 1, &status);
        if (U_SUCCESS(status))
            result = fromUChars(out, length);
        free(out);
    }
    for (NSUInteger i = 0; i < count; i++)
        free((void *)chars[i]);
    free(chars);
    free(lengths);
    ulistfmt_close(formatter);
    return result;
}

@end
