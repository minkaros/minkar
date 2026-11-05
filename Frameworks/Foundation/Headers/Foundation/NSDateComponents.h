/* Copyright (c) 2007 Christopher J. W. Lloyd <cjwl@objc.net>

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSCalendar.h>

// Minkar: NSIntegerMax, as the API has it (it was 0x7fffffff, which
// programs built for the reference system do not compare with).
enum : NSInteger {
    NSDateComponentUndefined = NSIntegerMax,
    NSUndefinedDateComponent = NSDateComponentUndefined
};

@class NSCalendar, NSTimeZone, NSDate;

NS_ASSUME_NONNULL_BEGIN

@interface NSDateComponents : NSObject <NSCopying, NSSecureCoding> {
    NSCalendar *_calendar;
    NSTimeZone *_timeZone;
    NSInteger _era;
    NSInteger _year;
    NSInteger _quarter;
    NSInteger _month;
    NSInteger _week;
    NSInteger _weekday;
    NSInteger _weekdayOrdinal;
    NSInteger _day;
    NSInteger _hour;
    NSInteger _minute;
    NSInteger _second;
    NSInteger _weekOfMonth;
    NSInteger _weekOfYear;
    NSInteger _yearForWeekOfYear;
    NSInteger _nanosecond;
    NSInteger _leapMonth;	/* -1 unset, 0 no, 1 yes */
}

@property (nullable, copy) NSCalendar *calendar;
@property (nullable, copy) NSTimeZone *timeZone;
@property NSInteger era;
@property NSInteger year;
@property NSInteger month;
@property NSInteger day;
@property NSInteger hour;
@property NSInteger minute;
@property NSInteger second;
@property NSInteger nanosecond;
@property NSInteger weekday;
@property NSInteger weekdayOrdinal;
@property NSInteger quarter;
@property NSInteger weekOfMonth;
@property NSInteger weekOfYear;
@property NSInteger yearForWeekOfYear;
@property (getter=isLeapMonth) BOOL leapMonth;
@property (nullable, readonly, copy) NSDate *date;

- (NSInteger)week;
- (void)setWeek:(NSInteger)value;

- (void)setValue:(NSInteger)value forComponent:(NSCalendarUnit)unit;
- (NSInteger)valueForComponent:(NSCalendarUnit)unit;

@property (readonly, getter=isValidDate) BOOL validDate;
- (BOOL)isValidDateInCalendar:(NSCalendar *)calendar;

@end

NS_ASSUME_NONNULL_END
