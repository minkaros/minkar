/* Copyright (c) 2007 Christopher J. W. Lloyd <cjwl@objc.net>
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSDateComponents.h>
#import <Foundation/NSCalendar.h>
#import <Foundation/NSTimeZone.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSString.h>

/* Components with no value hold NSDateComponentUndefined; the leap month
 * is unset (-1) until set. */
@implementation NSDateComponents

+ (BOOL)supportsSecureCoding {
    return YES;
}

- init {
    if ((self = [super init]) == nil)
        return nil;
    _era = _year = _quarter = _month = _week = _weekday = _weekdayOrdinal = _day = NSDateComponentUndefined;
    _hour = _minute = _second = _weekOfMonth = _weekOfYear = _yearForWeekOfYear = NSDateComponentUndefined;
    _nanosecond = NSDateComponentUndefined;
    _leapMonth = -1;
    return self;
}

- (void)dealloc {
    [_calendar release];
    [_timeZone release];
    [super dealloc];
}

- copyWithZone:(NSZone *)zone {
    NSDateComponents *copy = [[[self class] allocWithZone:zone] init];

    copy->_calendar = [_calendar copy];
    copy->_timeZone = [_timeZone copy];
    copy->_era = _era; copy->_year = _year; copy->_quarter = _quarter; copy->_month = _month;
    copy->_week = _week; copy->_weekday = _weekday; copy->_weekdayOrdinal = _weekdayOrdinal;
    copy->_day = _day; copy->_hour = _hour; copy->_minute = _minute; copy->_second = _second;
    copy->_weekOfMonth = _weekOfMonth; copy->_weekOfYear = _weekOfYear;
    copy->_yearForWeekOfYear = _yearForWeekOfYear; copy->_nanosecond = _nanosecond;
    copy->_leapMonth = _leapMonth;
    return copy;
}

static NSString *const keys[] = { @"era", @"year", @"quarter", @"month", @"week", @"weekday",
    @"weekdayOrdinal", @"day", @"hour", @"minute", @"second", @"weekOfMonth", @"weekOfYear",
    @"yearForWeekOfYear", @"nanosecond" };

- (NSInteger *)_fields {
    return &_era;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    NSInteger *fields = [self _fields];

    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (fields[i] != NSDateComponentUndefined)
            [coder encodeInteger:fields[i] forKey:keys[i]];
    if (_leapMonth >= 0)
        [coder encodeBool:_leapMonth == 1 forKey:@"leapMonth"];
    if (_calendar != nil)
        [coder encodeObject:_calendar forKey:@"calendar"];
    if (_timeZone != nil)
        [coder encodeObject:_timeZone forKey:@"timeZone"];
}

- initWithCoder:(NSCoder *)coder {
    if ((self = [self init]) == nil)
        return nil;
    NSInteger *fields = [self _fields];
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if ([coder containsValueForKey:keys[i]])
            fields[i] = [coder decodeIntegerForKey:keys[i]];
    if ([coder containsValueForKey:@"leapMonth"])
        _leapMonth = [coder decodeBoolForKey:@"leapMonth"] ? 1 : 0;
    _calendar = [[coder decodeObjectOfClass:[NSCalendar class] forKey:@"calendar"] retain];
    _timeZone = [[coder decodeObjectOfClass:[NSTimeZone class] forKey:@"timeZone"] retain];
    return self;
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSDateComponents class]])
        return NO;
    NSDateComponents *o = other;
    NSInteger *a = [self _fields], *b = [o _fields];
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (a[i] != b[i])
            return NO;
    if (_leapMonth != o->_leapMonth)
        return NO;
    if ((_calendar != o->_calendar) && ![_calendar isEqual:o->_calendar])
        return NO;
    if ((_timeZone != o->_timeZone) && ![_timeZone isEqual:o->_timeZone])
        return NO;
    return YES;
}

- (NSUInteger)hash {
    NSInteger *fields = [self _fields];
    NSUInteger h = 0;
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        h = h * 31 + (NSUInteger)fields[i];
    return h;
}

- (NSString *)description {
    NSMutableString *s = [NSMutableString stringWithFormat:@"<%@ %p>", [self class], self];
    NSInteger *fields = [self _fields];

    if (_calendar != nil)
        [s appendFormat:@" Calendar: %@", [_calendar calendarIdentifier]];
    if (_timeZone != nil)
        [s appendFormat:@" TimeZone: %@", [_timeZone name]];
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (fields[i] != NSDateComponentUndefined)
            [s appendFormat:@" %@: %ld", keys[i], (long)fields[i]];
    if (_leapMonth >= 0)
        [s appendFormat:@" isLeapMonth: %d", (int)_leapMonth];
    return s;
}

- (NSCalendar *)calendar { return _calendar; }
- (void)setCalendar:(NSCalendar *)calendar {
    calendar = [calendar copy];
    [_calendar release];
    _calendar = calendar;
}
- (NSTimeZone *)timeZone { return _timeZone; }
- (void)setTimeZone:(NSTimeZone *)timeZone {
    timeZone = [timeZone copy];
    [_timeZone release];
    _timeZone = timeZone;
}

#define FIELD(get, set, ivar) \
    - (NSInteger)get { return ivar; } \
    - (void)set:(NSInteger)value { ivar = value; }
FIELD(era, setEra, _era)
FIELD(year, setYear, _year)
FIELD(quarter, setQuarter, _quarter)
FIELD(month, setMonth, _month)
FIELD(week, setWeek, _week)
FIELD(weekday, setWeekday, _weekday)
FIELD(weekdayOrdinal, setWeekdayOrdinal, _weekdayOrdinal)
FIELD(day, setDay, _day)
FIELD(hour, setHour, _hour)
FIELD(minute, setMinute, _minute)
FIELD(second, setSecond, _second)
FIELD(weekOfMonth, setWeekOfMonth, _weekOfMonth)
FIELD(weekOfYear, setWeekOfYear, _weekOfYear)
FIELD(yearForWeekOfYear, setYearForWeekOfYear, _yearForWeekOfYear)
FIELD(nanosecond, setNanosecond, _nanosecond)
#undef FIELD

- (BOOL)isLeapMonth { return _leapMonth == 1; }
- (void)setLeapMonth:(BOOL)value { _leapMonth = value ? 1 : 0; }
- (BOOL)_leapMonthSet { return _leapMonth >= 0; }

- (NSInteger *)_fieldForUnit:(NSCalendarUnit)unit {
    switch (unit) {
    case NSCalendarUnitEra: return &_era;
    case NSCalendarUnitYear: return &_year;
    case NSCalendarUnitMonth: return &_month;
    case NSCalendarUnitDay: return &_day;
    case NSCalendarUnitHour: return &_hour;
    case NSCalendarUnitMinute: return &_minute;
    case NSCalendarUnitSecond: return &_second;
    case NSCalendarUnitWeekday: return &_weekday;
    case NSCalendarUnitWeekdayOrdinal: return &_weekdayOrdinal;
    case NSCalendarUnitQuarter: return &_quarter;
    case NSCalendarUnitWeekOfMonth: return &_weekOfMonth;
    case NSCalendarUnitWeekOfYear: return &_weekOfYear;
    case NSCalendarUnitYearForWeekOfYear: return &_yearForWeekOfYear;
    case NSCalendarUnitNanosecond: return &_nanosecond;
    default: return NULL;
    }
}

- (void)setValue:(NSInteger)value forComponent:(NSCalendarUnit)unit {
    NSInteger *field = [self _fieldForUnit:unit];
    if (field != NULL)
        *field = value;
}

- (NSInteger)valueForComponent:(NSCalendarUnit)unit {
    NSInteger *field = [self _fieldForUnit:unit];
    return field != NULL ? *field : NSDateComponentUndefined;
}

- (NSDate *)date {
    NSCalendar *calendar = _calendar;
    if (calendar == nil)
        return nil;
    if (_timeZone != nil) {
        calendar = [[calendar copy] autorelease];
        [calendar setTimeZone:_timeZone];
    }
    return [calendar dateFromComponents:self];
}

- (BOOL)isValidDate {
    return _calendar != nil && [self isValidDateInCalendar:_calendar];
}

/* Valid when the date the components make has the same components again. */
- (BOOL)isValidDateInCalendar:(NSCalendar *)calendar {
    if (_timeZone != nil) {
        calendar = [[calendar copy] autorelease];
        [calendar setTimeZone:_timeZone];
    }
    NSDate *date = [calendar dateFromComponents:self];
    if (date == nil)
        return NO;
    NSCalendarUnit units = 0;
    static const NSCalendarUnit all[] = { NSCalendarUnitEra, NSCalendarUnitYear, NSCalendarUnitMonth,
        NSCalendarUnitDay, NSCalendarUnitHour, NSCalendarUnitMinute, NSCalendarUnitSecond,
        NSCalendarUnitWeekday, NSCalendarUnitWeekdayOrdinal, NSCalendarUnitQuarter,
        NSCalendarUnitWeekOfMonth, NSCalendarUnitWeekOfYear, NSCalendarUnitYearForWeekOfYear,
        NSCalendarUnitNanosecond };
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); i++)
        if ([self valueForComponent:all[i]] != NSDateComponentUndefined)
            units |= all[i];
    NSDateComponents *again = [calendar components:units fromDate:date];
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        NSInteger v = [self valueForComponent:all[i]];
        if (v == NSDateComponentUndefined)
            continue;
        if (all[i] == NSCalendarUnitNanosecond) {
            if (llabs((long long)([again nanosecond] - v)) > 1000)
                return NO;
        } else if ([again valueForComponent:all[i]] != v)
            return NO;
    }
    return YES;
}

@end
