/* Copyright (c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>
#import <CoreFoundation/CFCalendar.h>

@class NSDateComponents, NSTimeZone, NSLocale, NSDate;

enum {
    NSEraCalendarUnit = kCFCalendarUnitEra,
    NSYearCalendarUnit = kCFCalendarUnitYear,
    NSMonthCalendarUnit = kCFCalendarUnitMonth,
    NSDayCalendarUnit = kCFCalendarUnitDay,
    NSHourCalendarUnit = kCFCalendarUnitHour,
    NSMinuteCalendarUnit = kCFCalendarUnitMinute,
    NSSecondCalendarUnit = kCFCalendarUnitSecond,
    // kCFCalendarUnitWeek's value: the CF constant is deprecated, and
    // naming it here warned in every file that imports Foundation.
    NSWeekCalendarUnit = (1UL << 8),
    NSWeekdayCalendarUnit = kCFCalendarUnitWeekday,
    NSWeekdayOrdinalCalendarUnit = kCFCalendarUnitWeekdayOrdinal,
    //   NSQuarterCalendarUnit = kCFCalendarUnitQuarter,
};
typedef NSUInteger NSCalendarUnit;

enum {
    NSCalendarUnitEra = kCFCalendarUnitEra,
    NSCalendarUnitYear = kCFCalendarUnitYear,
    NSCalendarUnitMonth = kCFCalendarUnitMonth,
    NSCalendarUnitDay = kCFCalendarUnitDay,
    NSCalendarUnitHour = kCFCalendarUnitHour,
    NSCalendarUnitMinute = kCFCalendarUnitMinute,
    NSCalendarUnitSecond = kCFCalendarUnitSecond,
    NSCalendarUnitWeekday = kCFCalendarUnitWeekday,
    NSCalendarUnitWeekdayOrdinal = kCFCalendarUnitWeekdayOrdinal,
    NSCalendarUnitQuarter = (1UL << 11),
    NSCalendarUnitWeekOfMonth = (1UL << 12),
    NSCalendarUnitWeekOfYear = (1UL << 13),
    NSCalendarUnitYearForWeekOfYear = (1UL << 14),
    NSCalendarUnitNanosecond = (1UL << 15),
    NSCalendarUnitCalendar = (1UL << 20),
    NSCalendarUnitTimeZone = (1UL << 21),
};

/* Calendars by their CLDR identifier (10.6; a type of its own since 10.12). */
typedef NSString *NSCalendarIdentifier NS_TYPED_ENUM;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierGregorian;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierBuddhist;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierChinese;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierCoptic;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierEthiopicAmeteMihret;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierEthiopicAmeteAlem;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierHebrew;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierISO8601;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierIndian;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierIslamic;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierIslamicCivil;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierJapanese;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierPersian;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierRepublicOfChina;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierIslamicTabular;
FOUNDATION_EXPORT NSCalendarIdentifier const NSCalendarIdentifierIslamicUmmAlQura;

/* The older name of NSCalendarIdentifierGregorian. */
FOUNDATION_EXPORT NSString *const NSGregorianCalendar;

@interface NSCalendar : NSObject <NSCopying> {
    NSString *_identifier;
    NSUInteger _firstWeekday;
    NSUInteger _minimumDaysInFirstWeek;
    NSTimeZone *_timeZone;
    NSLocale *_locale;
}

+ currentCalendar;
/* The current calendar; it does not follow later changes of the user's
 * settings yet (no locale change notification). */
+ (NSCalendar *)autoupdatingCurrentCalendar;
+ (NSCalendar *)calendarWithIdentifier:(NSCalendarIdentifier)identifier;

- initWithCalendarIdentifier:(NSCalendarIdentifier)identifier;

- (NSCalendarIdentifier)calendarIdentifier;
- (NSUInteger)firstWeekday;
- (NSUInteger)minimumDaysInFirstWeek;
- (NSTimeZone *)timeZone;
- (NSLocale *)locale;

- (void)setFirstWeekday:(NSUInteger)weekday;
- (void)setMinimumDaysInFirstWeek:(NSUInteger)days;
- (void)setTimeZone:(NSTimeZone *)timeZone;
- (void)setLocale:(NSLocale *)locale;

- (NSRange)minimumRangeOfUnit:(NSCalendarUnit)unit;
- (NSRange)maximumRangeOfUnit:(NSCalendarUnit)unit;
- (NSRange)rangeOfUnit:(NSCalendarUnit)unit inUnit:(NSCalendarUnit)inUnit forDate:(NSDate *)date;
- (NSUInteger)ordinalityOfUnit:(NSCalendarUnit)unit inUnit:(NSCalendarUnit)inUnit forDate:(NSDate *)date;

- (NSDateComponents *)components:(NSUInteger)flags fromDate:(NSDate *)date;
- (NSDateComponents *)components:(NSUInteger)flags fromDate:(NSDate *)fromDate toDate:(NSDate *)toDate options:(NSUInteger)options;

- (NSDate *)dateByAddingComponents:(NSDateComponents *)components toDate:(NSDate *)date options:(NSUInteger)options;
- (NSDate *)dateFromComponents:(NSDateComponents *)components;

@end
