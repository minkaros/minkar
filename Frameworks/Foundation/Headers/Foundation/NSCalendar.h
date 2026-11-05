/* Copyright (c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>
#import <CoreFoundation/CFCalendar.h>

@class NSDateComponents, NSTimeZone, NSLocale, NSDate, NSArray<ObjectType>, NSString;
#import <Foundation/NSNotification.h>
#import <Foundation/NSDate.h>

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
typedef NS_OPTIONS(NSUInteger, NSCalendarUnit) {
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

typedef NS_OPTIONS(NSUInteger, NSCalendarOptions) {
    NSCalendarWrapComponents = (1UL << 0),
    NSCalendarMatchStrictly = (1UL << 1),
    NSCalendarSearchBackwards = (1UL << 2),
    NSCalendarMatchPreviousTimePreservingSmallerUnits = (1UL << 8),
    NSCalendarMatchNextTimePreservingSmallerUnits = (1UL << 9),
    NSCalendarMatchNextTime = (1UL << 10),
    NSCalendarMatchFirst = (1UL << 12),
    NSCalendarMatchLast = (1UL << 13),
};

enum {
    NSWrapCalendarComponents = NSCalendarWrapComponents,
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

FOUNDATION_EXPORT NSNotificationName const NSCalendarDayChangedNotification;

NS_ASSUME_NONNULL_BEGIN

/* A calendar: CoreFoundation's (CFCalendar, on ICU) under the Cocoa API. */
@interface NSCalendar : NSObject <NSCopying, NSSecureCoding> {
    CFCalendarRef _cf;
    NSString *_identifier;
    NSTimeZone *_timeZone;
    NSLocale *_locale;
    BOOL _autoupdating;
}

@property (class, readonly, copy) NSCalendar *currentCalendar;
/* The current calendar; it follows the user's settings when asked again. */
@property (class, readonly, strong) NSCalendar *autoupdatingCurrentCalendar;
+ (nullable NSCalendar *)calendarWithIdentifier:(NSCalendarIdentifier)identifier;

- (instancetype)init NS_UNAVAILABLE;
- (nullable id)initWithCalendarIdentifier:(NSCalendarIdentifier)identifier NS_DESIGNATED_INITIALIZER;

@property (readonly, copy) NSCalendarIdentifier calendarIdentifier;
@property (nullable, copy) NSLocale *locale;
@property (copy) NSTimeZone *timeZone;
@property NSUInteger firstWeekday;
@property NSUInteger minimumDaysInFirstWeek;

@property (readonly, copy) NSArray<NSString *> *eraSymbols;
@property (readonly, copy) NSArray<NSString *> *longEraSymbols;
@property (readonly, copy) NSArray<NSString *> *monthSymbols;
@property (readonly, copy) NSArray<NSString *> *shortMonthSymbols;
@property (readonly, copy) NSArray<NSString *> *veryShortMonthSymbols;
@property (readonly, copy) NSArray<NSString *> *standaloneMonthSymbols;
@property (readonly, copy) NSArray<NSString *> *shortStandaloneMonthSymbols;
@property (readonly, copy) NSArray<NSString *> *veryShortStandaloneMonthSymbols;
@property (readonly, copy) NSArray<NSString *> *weekdaySymbols;
@property (readonly, copy) NSArray<NSString *> *shortWeekdaySymbols;
@property (readonly, copy) NSArray<NSString *> *veryShortWeekdaySymbols;
@property (readonly, copy) NSArray<NSString *> *standaloneWeekdaySymbols;
@property (readonly, copy) NSArray<NSString *> *shortStandaloneWeekdaySymbols;
@property (readonly, copy) NSArray<NSString *> *veryShortStandaloneWeekdaySymbols;
@property (readonly, copy) NSArray<NSString *> *quarterSymbols;
@property (readonly, copy) NSArray<NSString *> *shortQuarterSymbols;
@property (readonly, copy) NSArray<NSString *> *standaloneQuarterSymbols;
@property (readonly, copy) NSArray<NSString *> *shortStandaloneQuarterSymbols;
@property (readonly, copy) NSString *AMSymbol;
@property (readonly, copy) NSString *PMSymbol;

- (NSRange)minimumRangeOfUnit:(NSCalendarUnit)unit;
- (NSRange)maximumRangeOfUnit:(NSCalendarUnit)unit;
- (NSRange)rangeOfUnit:(NSCalendarUnit)smaller inUnit:(NSCalendarUnit)larger forDate:(NSDate *)date;
- (NSUInteger)ordinalityOfUnit:(NSCalendarUnit)smaller inUnit:(NSCalendarUnit)larger forDate:(NSDate *)date;
- (BOOL)rangeOfUnit:(NSCalendarUnit)unit startDate:(NSDate * _Nullable * _Nullable)datep interval:(nullable NSTimeInterval *)tip forDate:(NSDate *)date;

- (nullable NSDate *)dateFromComponents:(NSDateComponents *)comps;
- (NSDateComponents *)components:(NSCalendarUnit)unitFlags fromDate:(NSDate *)date;
- (nullable NSDate *)dateByAddingComponents:(NSDateComponents *)comps toDate:(NSDate *)date options:(NSCalendarOptions)opts;
- (NSDateComponents *)components:(NSCalendarUnit)unitFlags fromDate:(NSDate *)startingDate toDate:(NSDate *)resultDate options:(NSCalendarOptions)opts;

- (void)getEra:(nullable out NSInteger *)eraValuePointer year:(nullable out NSInteger *)yearValuePointer month:(nullable out NSInteger *)monthValuePointer day:(nullable out NSInteger *)dayValuePointer fromDate:(NSDate *)date;
- (void)getEra:(nullable out NSInteger *)eraValuePointer yearForWeekOfYear:(nullable out NSInteger *)yearValuePointer weekOfYear:(nullable out NSInteger *)weekValuePointer weekday:(nullable out NSInteger *)weekdayValuePointer fromDate:(NSDate *)date;
- (void)getHour:(nullable out NSInteger *)hourValuePointer minute:(nullable out NSInteger *)minuteValuePointer second:(nullable out NSInteger *)secondValuePointer nanosecond:(nullable out NSInteger *)nanosecondValuePointer fromDate:(NSDate *)date;
- (NSInteger)component:(NSCalendarUnit)unit fromDate:(NSDate *)date;

- (nullable NSDate *)dateWithEra:(NSInteger)eraValue year:(NSInteger)yearValue month:(NSInteger)monthValue day:(NSInteger)dayValue hour:(NSInteger)hourValue minute:(NSInteger)minuteValue second:(NSInteger)secondValue nanosecond:(NSInteger)nanosecondValue;
- (nullable NSDate *)dateWithEra:(NSInteger)eraValue yearForWeekOfYear:(NSInteger)yearValue weekOfYear:(NSInteger)weekValue weekday:(NSInteger)weekdayValue hour:(NSInteger)hourValue minute:(NSInteger)minuteValue second:(NSInteger)secondValue nanosecond:(NSInteger)nanosecondValue;

- (NSDate *)startOfDayForDate:(NSDate *)date;
- (NSDateComponents *)componentsInTimeZone:(NSTimeZone *)timezone fromDate:(NSDate *)date;
- (NSComparisonResult)compareDate:(NSDate *)date1 toDate:(NSDate *)date2 toUnitGranularity:(NSCalendarUnit)unit;
- (BOOL)isDate:(NSDate *)date1 equalToDate:(NSDate *)date2 toUnitGranularity:(NSCalendarUnit)unit;
- (BOOL)isDate:(NSDate *)date1 inSameDayAsDate:(NSDate *)date2;
- (BOOL)isDateInToday:(NSDate *)date;
- (BOOL)isDateInYesterday:(NSDate *)date;
- (BOOL)isDateInTomorrow:(NSDate *)date;
- (BOOL)isDateInWeekend:(NSDate *)date;
- (BOOL)rangeOfWeekendStartDate:(out NSDate * _Nullable * _Nullable)datep interval:(out nullable NSTimeInterval *)tip containingDate:(NSDate *)date;
- (BOOL)nextWeekendStartDate:(out NSDate * _Nullable * _Nullable)datep interval:(out nullable NSTimeInterval *)tip options:(NSCalendarOptions)options afterDate:(NSDate *)date;

- (NSDateComponents *)components:(NSCalendarUnit)unitFlags fromDateComponents:(NSDateComponents *)startingDateComp toDateComponents:(NSDateComponents *)resultDateComp options:(NSCalendarOptions)options;
- (nullable NSDate *)dateByAddingUnit:(NSCalendarUnit)unit value:(NSInteger)value toDate:(NSDate *)date options:(NSCalendarOptions)options;

- (void)enumerateDatesStartingAfterDate:(NSDate *)start matchingComponents:(NSDateComponents *)comps options:(NSCalendarOptions)opts usingBlock:(void (NS_NOESCAPE ^)(NSDate * _Nullable date, BOOL exactMatch, BOOL *stop))block;
- (nullable NSDate *)nextDateAfterDate:(NSDate *)date matchingComponents:(NSDateComponents *)comps options:(NSCalendarOptions)options;
- (nullable NSDate *)nextDateAfterDate:(NSDate *)date matchingUnit:(NSCalendarUnit)unit value:(NSInteger)value options:(NSCalendarOptions)options;
- (nullable NSDate *)nextDateAfterDate:(NSDate *)date matchingHour:(NSInteger)hourValue minute:(NSInteger)minuteValue second:(NSInteger)secondValue options:(NSCalendarOptions)options;
- (nullable NSDate *)dateBySettingUnit:(NSCalendarUnit)unit value:(NSInteger)v ofDate:(NSDate *)date options:(NSCalendarOptions)opts;
- (nullable NSDate *)dateBySettingHour:(NSInteger)h minute:(NSInteger)m second:(NSInteger)s ofDate:(NSDate *)date options:(NSCalendarOptions)opts;
- (BOOL)date:(NSDate *)date matchesComponents:(NSDateComponents *)components;

@end

NS_ASSUME_NONNULL_END
