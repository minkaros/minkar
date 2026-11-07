/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSFormatter.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <CoreFoundation/CFDateFormatter.h>

typedef NS_ENUM(NSUInteger, NSDateFormatterBehavior) {
    NSDateFormatterBehaviorDefault = 0,
    NSDateFormatterBehavior10_0 = 1000,
    NSDateFormatterBehavior10_4 = 1040,
};

typedef NS_ENUM(NSUInteger, NSDateFormatterStyle) {
    NSDateFormatterNoStyle = kCFDateFormatterNoStyle,
    NSDateFormatterShortStyle = kCFDateFormatterShortStyle,
    NSDateFormatterMediumStyle = kCFDateFormatterMediumStyle,
    NSDateFormatterLongStyle = kCFDateFormatterLongStyle,
    NSDateFormatterFullStyle = kCFDateFormatterFullStyle
};

@class NSArray<ObjectType>, NSCalendar, NSLocale, NSTimeZone;

NS_ASSUME_NONNULL_BEGIN

@interface NSDateFormatter : NSFormatter {
    NSDateFormatterBehavior _behavior;
    NSDateFormatterStyle _dateStyle;
    NSDateFormatterStyle _timeStyle;
    NSString *_dateFormat10_0;
    NSString *_dateFormat;
    BOOL _allowsNaturalLanguage;
    NSDictionary *_locale;
    NSTimeZone *_tz;
    // Minkar: the 10.4 behavior, on CFDateFormatter.
    NSLocale *_nsLocale;
    NSCalendar *_calendar;
    void *_cf;
    NSMutableDictionary *_properties;
    BOOL _lenient;
    BOOL _relative;
    BOOL _generatesCalendarDates;
    NSFormattingContext _formattingContext;
}

+ (NSString *)localizedStringFromDate:(NSDate *)date dateStyle:(NSDateFormatterStyle)dstyle timeStyle:(NSDateFormatterStyle)tstyle;
+ (nullable NSString *)dateFormatFromTemplate:(NSString *)tmplate options:(NSUInteger)opts locale:(nullable NSLocale *)locale;
+ (NSDateFormatterBehavior)defaultFormatterBehavior;
+ (void)setDefaultFormatterBehavior:(NSDateFormatterBehavior)behavior;

- (instancetype)init;
- initWithDateFormat:(NSString *)format allowNaturalLanguage:(BOOL)flag;
// The backend of NSCalendarDate's initWithString:calendarFormat:locale:.
- initWithDateFormat:(NSString *)format allowNaturalLanguage:(BOOL)flag locale:(nullable NSDictionary *)locale;

- (BOOL)allowsNaturalLanguage;
- (void)setLocalizedDateFormatFromTemplate:(NSString *)dateFormatTemplate;

@property (null_resettable, copy) NSString *dateFormat;
@property NSDateFormatterStyle dateStyle;
@property NSDateFormatterStyle timeStyle;
@property (null_resettable, copy) NSLocale *locale;
@property BOOL generatesCalendarDates;
@property NSDateFormatterBehavior formatterBehavior;
@property (null_resettable, copy) NSTimeZone *timeZone;
@property (null_resettable, copy) NSCalendar *calendar;
@property (getter=isLenient) BOOL lenient;
@property (nullable, copy) NSDate *twoDigitStartDate;
@property (nullable, copy) NSDate *defaultDate;
@property (null_resettable, copy) NSArray<NSString *> *eraSymbols;
@property (null_resettable, copy) NSArray<NSString *> *monthSymbols;
@property (null_resettable, copy) NSArray<NSString *> *shortMonthSymbols;
@property (null_resettable, copy) NSArray<NSString *> *weekdaySymbols;
@property (null_resettable, copy) NSArray<NSString *> *shortWeekdaySymbols;
@property (null_resettable, copy) NSString *AMSymbol;
@property (null_resettable, copy) NSString *PMSymbol;
@property (null_resettable, copy) NSArray<NSString *> *longEraSymbols;
@property (null_resettable, copy) NSArray<NSString *> *veryShortMonthSymbols;
@property (null_resettable, copy) NSArray<NSString *> *standaloneMonthSymbols;
@property (null_resettable, copy) NSArray<NSString *> *shortStandaloneMonthSymbols;
@property (null_resettable, copy) NSArray<NSString *> *veryShortStandaloneMonthSymbols;
@property (null_resettable, copy) NSArray<NSString *> *veryShortWeekdaySymbols;
@property (null_resettable, copy) NSArray<NSString *> *standaloneWeekdaySymbols;
@property (null_resettable, copy) NSArray<NSString *> *shortStandaloneWeekdaySymbols;
@property (null_resettable, copy) NSArray<NSString *> *veryShortStandaloneWeekdaySymbols;
@property (null_resettable, copy) NSArray<NSString *> *quarterSymbols;
@property (null_resettable, copy) NSArray<NSString *> *shortQuarterSymbols;
@property (null_resettable, copy) NSArray<NSString *> *standaloneQuarterSymbols;
@property (null_resettable, copy) NSArray<NSString *> *shortStandaloneQuarterSymbols;
@property (nullable, copy) NSDate *gregorianStartDate;
@property BOOL doesRelativeDateFormatting;
@property NSFormattingContext formattingContext;

- (NSString *)stringFromDate:(NSDate *)date;
- (nullable NSDate *)dateFromString:(NSString *)string;
- (BOOL)getObjectValue:(out id _Nullable * _Nullable)obj forString:(NSString *)string range:(inout nullable NSRange *)rangep error:(out NSError **)error;

@end

NS_ASSUME_NONNULL_END

// internal use

NSTimeInterval NSMoveIntervalFromTimeZoneToGMT(NSTimeInterval interval, NSTimeZone *timeZone);
NSTimeInterval NSMoveIntervalFromGMTToTimeZone(NSTimeInterval interval, NSTimeZone *timeZone);

NSInteger NSNumberOfDaysInMonthOfYear(NSInteger month, NSInteger year);
// interval is not time zone adjusteed
NSTimeInterval NSTimeIntervalWithComponents(NSInteger year, NSInteger month, NSInteger day, NSInteger hour, NSInteger minute, NSInteger second, NSInteger milliseconds);

// interval has already been time zone adjusted
NSInteger NSDayOfCommonEraFromTimeInterval(NSTimeInterval interval);

NSInteger NSYearFromTimeInterval(NSTimeInterval interval);
NSInteger NSDayOfYearFromTimeInterval(NSTimeInterval interval); // 1-366

NSInteger NSMonthFromTimeInterval(NSTimeInterval interval); // 1-12
NSInteger NSDayOfMonthFromTimeInterval(NSTimeInterval interval); // 0-31

NSInteger NSWeekdayFromTimeInterval(NSTimeInterval interval); // 1-7

NSInteger NS24HourFromTimeInterval(NSTimeInterval interval); // 0-23
NSInteger NS12HourFromTimeInterval(NSTimeInterval interval); // 1-12
NSInteger NSAMPMFromTimeInterval(NSTimeInterval interval); // 0-1

NSInteger NSMinuteFromTimeInterval(NSTimeInterval interval); // 0-59

NSInteger NSSecondFromTimeInterval(NSTimeInterval interval); // 0-59

NSInteger NSMillisecondsFromTimeInterval(NSTimeInterval interval); // 0-999

// interval will be time-zone adjusted
NSString *NSStringWithDateFormatLocale(NSTimeInterval interval, NSString *format, NSDictionary *locale, NSTimeZone *timeZone);

NSDate *NSDateWithStringDateFormatLocale(NSString *string, NSString *format, NSDictionary *locale, NSTimeZone *timeZone);
