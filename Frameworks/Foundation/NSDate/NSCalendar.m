/* Copyright (c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSCalendar.h>
#import <Foundation/NSDateComponents.h>
#import <Foundation/NSTimeZone.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSException.h>
#import <Foundation/NSAutoreleasePool.h>
#import <CoreFoundation/CFCalendar.h>
#import <CoreFoundation/CFDateFormatter.h>
#import <CoreFoundation/CFTimeZone.h>
#import <CoreFoundation/CFLocale.h>
#include <math.h>

NSString * const NSCalendarIdentifierGregorian=@"gregorian";
NSString * const NSCalendarIdentifierBuddhist=@"buddhist";
NSString * const NSCalendarIdentifierChinese=@"chinese";
NSString * const NSCalendarIdentifierCoptic=@"coptic";
NSString * const NSCalendarIdentifierEthiopicAmeteMihret=@"ethiopic";
NSString * const NSCalendarIdentifierEthiopicAmeteAlem=@"ethiopic-amete-alem";
NSString * const NSCalendarIdentifierHebrew=@"hebrew";
NSString * const NSCalendarIdentifierISO8601=@"iso8601";
NSString * const NSCalendarIdentifierIndian=@"indian";
NSString * const NSCalendarIdentifierIslamic=@"islamic";
NSString * const NSCalendarIdentifierIslamicCivil=@"islamic-civil";
NSString * const NSCalendarIdentifierJapanese=@"japanese";
NSString * const NSCalendarIdentifierPersian=@"persian";
NSString * const NSCalendarIdentifierRepublicOfChina=@"roc";
NSString * const NSCalendarIdentifierIslamicTabular=@"islamic-tbla";
NSString * const NSCalendarIdentifierIslamicUmmAlQura=@"islamic-umalqura";
NSString * const NSGregorianCalendar=@"gregorian";

NSNotificationName const NSCalendarDayChangedNotification = @"NSCalendarDayChangedNotification";

/* CF takes its own strings only: every NSString handed to it is copied. */
static CFStringRef copyCFString(NSString *string) {
    return string == nil ? NULL : CFStringCreateWithCString(NULL, [string UTF8String], kCFStringEncodingUTF8);
}

static CFTimeZoneRef copyCFTimeZone(NSTimeZone *zone) {
    CFStringRef name = copyCFString([zone name]);
    CFTimeZoneRef cf = name != NULL ? CFTimeZoneCreateWithName(NULL, name, true) : NULL;
    if (name != NULL)
        CFRelease(name);
    if (cf == NULL)
        cf = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, [zone secondsFromGMT]);
    return cf;
}

static CFLocaleRef copyCFLocale(NSLocale *locale) {
    CFStringRef name = copyCFString([locale localeIdentifier] ?: @"en_US_POSIX");
    CFLocaleRef cf = CFLocaleCreate(NULL, name);
    CFRelease(name);
    return cf;
}

/* The units CFCalendar's format strings name, in the order components are
 * handled; quarter and nanosecond are worked out apart. */
static const struct { NSCalendarUnit unit; char code; } unitCodes[] = {
    { NSCalendarUnitEra, 'G' }, { NSCalendarUnitYear, 'y' }, { NSCalendarUnitMonth, 'M' },
    { NSCalendarUnitDay, 'd' }, { NSCalendarUnitHour, 'H' }, { NSCalendarUnitMinute, 'm' },
    { NSCalendarUnitSecond, 's' }, { NSCalendarUnitWeekday, 'E' },
    { NSCalendarUnitWeekdayOrdinal, 'F' }, { NSCalendarUnitWeekOfMonth, 'W' },
    { NSCalendarUnitWeekOfYear, 'w' }, { NSCalendarUnitYearForWeekOfYear, 'Y' },
};
#define NUNITS (sizeof(unitCodes) / sizeof(unitCodes[0]))

static BOOL decompose(CFCalendarRef cal, CFAbsoluteTime at, const char *codes, int *v) {
    switch (strlen(codes)) {
    case 0: return YES;
    case 1: return CFCalendarDecomposeAbsoluteTime(cal, at, codes, &v[0]);
    default: break;
    }
    /* One at a time: CF takes the values as varargs. */
    char one[2] = { 0, 0 };
    for (size_t i = 0; codes[i] != 0; i++) {
        one[0] = codes[i];
        if (!CFCalendarDecomposeAbsoluteTime(cal, at, one, &v[i]))
            return NO;
    }
    return YES;
}

static BOOL composeOne(CFCalendarRef cal, CFAbsoluteTime *at, const char *codes, const int *v, int n) {
    switch (n) {
    case 0: return CFCalendarComposeAbsoluteTime(cal, at, "");
    case 1: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0]);
    case 2: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1]);
    case 3: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2]);
    case 4: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2], v[3]);
    case 5: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2], v[3], v[4]);
    case 6: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2], v[3], v[4], v[5]);
    case 7: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
    case 8: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
    case 9: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]);
    case 10: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]);
    case 11: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10]);
    default: return CFCalendarComposeAbsoluteTime(cal, at, codes, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11]);
    }
}

static BOOL addOne(CFCalendarRef cal, CFAbsoluteTime *at, CFOptionFlags options, char code, int amount) {
    char one[2] = { code, 0 };
    return amount == 0 || CFCalendarAddComponents(cal, at, options, one, amount);
}

static NSCalendarUnit const allUnits = NSCalendarUnitEra | NSCalendarUnitYear | NSCalendarUnitMonth |
    NSCalendarUnitDay | NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond |
    NSCalendarUnitWeekday | NSCalendarUnitWeekdayOrdinal | NSCalendarUnitQuarter |
    NSCalendarUnitWeekOfMonth | NSCalendarUnitWeekOfYear | NSCalendarUnitYearForWeekOfYear |
    NSCalendarUnitNanosecond;

@implementation NSCalendar

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (NSCalendar *)currentCalendar {
    return [[[self alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian] autorelease];
}

+ (NSCalendar *)autoupdatingCurrentCalendar {
    NSCalendar *calendar = [self currentCalendar];
    calendar->_autoupdating = YES;
    return calendar;
}

+ (NSCalendar *)calendarWithIdentifier:(NSCalendarIdentifier)identifier {
    return [[[self alloc] initWithCalendarIdentifier:identifier] autorelease];
}

- (instancetype)init {
    return [self initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
}

- (id)initWithCalendarIdentifier:(NSCalendarIdentifier)identifier {
    if ((self = [super init]) == nil)
        return nil;
    if ([identifier isEqualToString:@"gregorian"] == NO && [identifier length] == 0) {
        [self release];
        return nil;
    }
    CFStringRef cfIdentifier = copyCFString(identifier);
    _cf = (CFCalendarRef)CFCalendarCreateWithIdentifier(NULL, cfIdentifier);
    CFRelease(cfIdentifier);
    if (_cf == NULL) {
        [self release];
        return nil;
    }
    _identifier = [identifier copy];
    [self setLocale:[NSLocale currentLocale]];
    [self setTimeZone:[NSTimeZone defaultTimeZone]];
    return self;
}

- (void)dealloc {
    if (_cf != NULL)
        CFRelease(_cf);
    [_identifier release];
    [_timeZone release];
    [_locale release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    NSCalendar *copy = [[NSCalendar allocWithZone:zone] initWithCalendarIdentifier:_identifier];

    [copy setLocale:_locale];
    [copy setTimeZone:_timeZone];
    [copy setFirstWeekday:[self firstWeekday]];
    [copy setMinimumDaysInFirstWeek:[self minimumDaysInFirstWeek]];
    return copy;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_identifier forKey:@"NS.identifier"];
    [coder encodeObject:_timeZone forKey:@"NS.timezone"];
    [coder encodeObject:_locale forKey:@"NS.locale"];
    [coder encodeInteger:[self firstWeekday] forKey:@"NS.firstwkdy"];
    [coder encodeInteger:[self minimumDaysInFirstWeek] forKey:@"NS.mindays"];
}

- (id)initWithCoder:(NSCoder *)coder {
    NSString *identifier = [coder decodeObjectOfClass:[NSString class] forKey:@"NS.identifier"];

    if ((self = [self initWithCalendarIdentifier:identifier ?: NSCalendarIdentifierGregorian]) == nil)
        return nil;
    NSTimeZone *zone = [coder decodeObjectOfClass:[NSTimeZone class] forKey:@"NS.timezone"];
    NSLocale *locale = [coder decodeObjectOfClass:[NSLocale class] forKey:@"NS.locale"];
    if (zone != nil)
        [self setTimeZone:zone];
    if (locale != nil)
        [self setLocale:locale];
    if ([coder containsValueForKey:@"NS.firstwkdy"])
        [self setFirstWeekday:[coder decodeIntegerForKey:@"NS.firstwkdy"]];
    if ([coder containsValueForKey:@"NS.mindays"])
        [self setMinimumDaysInFirstWeek:[coder decodeIntegerForKey:@"NS.mindays"]];
    return self;
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSCalendar class]])
        return NO;
    NSCalendar *o = other;
    return [_identifier isEqualToString:o->_identifier] && [[_timeZone name] isEqualToString:[o->_timeZone name]] &&
        [[_locale localeIdentifier] isEqualToString:[o->_locale localeIdentifier]] &&
        [self firstWeekday] == [o firstWeekday] && [self minimumDaysInFirstWeek] == [o minimumDaysInFirstWeek];
}

- (NSUInteger)hash {
    return [_identifier hash];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ (%@)", _identifier, _autoupdating ? @"autoupdatingCurrent" : @"fixed"];
}

- (NSCalendarIdentifier)calendarIdentifier {
    return _identifier;
}

- (NSLocale *)locale {
    return _locale;
}

- (void)setLocale:(NSLocale *)locale {
    locale = [locale copy];
    [_locale release];
    _locale = locale;
    CFLocaleRef cf = copyCFLocale(_locale ?: [NSLocale currentLocale]);
    if (cf != NULL) {
        CFCalendarSetLocale(_cf, cf);
        CFRelease(cf);
    }
}

- (NSTimeZone *)timeZone {
    return _timeZone;
}

- (void)setTimeZone:(NSTimeZone *)timeZone {
    if (timeZone == nil)
        timeZone = [NSTimeZone defaultTimeZone];
    timeZone = [timeZone copy];
    [_timeZone release];
    _timeZone = timeZone;
    CFTimeZoneRef cf = copyCFTimeZone(_timeZone);
    if (cf != NULL) {
        CFCalendarSetTimeZone(_cf, cf);
        CFRelease(cf);
    }
}

- (NSUInteger)firstWeekday {
    return (NSUInteger)CFCalendarGetFirstWeekday(_cf);
}

- (void)setFirstWeekday:(NSUInteger)weekday {
    CFCalendarSetFirstWeekday(_cf, (CFIndex)weekday);
}

- (NSUInteger)minimumDaysInFirstWeek {
    return (NSUInteger)CFCalendarGetMinimumDaysInFirstWeek(_cf);
}

- (void)setMinimumDaysInFirstWeek:(NSUInteger)days {
    CFCalendarSetMinimumDaysInFirstWeek(_cf, (CFIndex)days);
}

/* -- Symbols, from a date formatter of this calendar and locale -- */

- (id)_formatterProperty:(CFStringRef)key {
    CFLocaleRef locale = copyCFLocale(_locale ?: [NSLocale currentLocale]);
    CFDateFormatterRef formatter = CFDateFormatterCreate(NULL, locale, kCFDateFormatterNoStyle, kCFDateFormatterNoStyle);
    id value = nil;

    if (formatter != NULL) {
        CFDateFormatterSetProperty(formatter, kCFDateFormatterCalendar, _cf);
        value = [(id)CFDateFormatterCopyProperty(formatter, key) autorelease];
        CFRelease(formatter);
    }
    if (locale != NULL)
        CFRelease(locale);
    return value;
}

#define SYMBOLS(name, key) \
    - (NSArray *)name { return [self _formatterProperty:key] ?: [NSArray array]; }
SYMBOLS(eraSymbols, kCFDateFormatterEraSymbols)
SYMBOLS(longEraSymbols, kCFDateFormatterLongEraSymbols)
SYMBOLS(monthSymbols, kCFDateFormatterMonthSymbols)
SYMBOLS(shortMonthSymbols, kCFDateFormatterShortMonthSymbols)
SYMBOLS(veryShortMonthSymbols, kCFDateFormatterVeryShortMonthSymbols)
SYMBOLS(standaloneMonthSymbols, kCFDateFormatterStandaloneMonthSymbols)
SYMBOLS(shortStandaloneMonthSymbols, kCFDateFormatterShortStandaloneMonthSymbols)
SYMBOLS(veryShortStandaloneMonthSymbols, kCFDateFormatterVeryShortStandaloneMonthSymbols)
SYMBOLS(weekdaySymbols, kCFDateFormatterWeekdaySymbols)
SYMBOLS(shortWeekdaySymbols, kCFDateFormatterShortWeekdaySymbols)
SYMBOLS(veryShortWeekdaySymbols, kCFDateFormatterVeryShortWeekdaySymbols)
SYMBOLS(standaloneWeekdaySymbols, kCFDateFormatterStandaloneWeekdaySymbols)
SYMBOLS(shortStandaloneWeekdaySymbols, kCFDateFormatterShortStandaloneWeekdaySymbols)
SYMBOLS(veryShortStandaloneWeekdaySymbols, kCFDateFormatterVeryShortStandaloneWeekdaySymbols)
SYMBOLS(quarterSymbols, kCFDateFormatterQuarterSymbols)
SYMBOLS(shortQuarterSymbols, kCFDateFormatterShortQuarterSymbols)
SYMBOLS(standaloneQuarterSymbols, kCFDateFormatterStandaloneQuarterSymbols)
SYMBOLS(shortStandaloneQuarterSymbols, kCFDateFormatterShortStandaloneQuarterSymbols)
#undef SYMBOLS

- (NSString *)AMSymbol {
    return [self _formatterProperty:kCFDateFormatterAMSymbol] ?: @"AM";
}

- (NSString *)PMSymbol {
    return [self _formatterProperty:kCFDateFormatterPMSymbol] ?: @"PM";
}

/* -- Ranges -- */

static NSRange fromCF(CFRange r) {
    if (r.location == kCFNotFound)
        return NSMakeRange(NSNotFound, NSNotFound);
    return NSMakeRange((NSUInteger)r.location, (NSUInteger)r.length);
}

- (NSRange)minimumRangeOfUnit:(NSCalendarUnit)unit {
    if (unit == NSCalendarUnitQuarter)
        return NSMakeRange(1, 4);
    if (unit == NSCalendarUnitNanosecond)
        return NSMakeRange(0, 1000000000);
    return fromCF(CFCalendarGetMinimumRangeOfUnit(_cf, (CFCalendarUnit)unit));
}

- (NSRange)maximumRangeOfUnit:(NSCalendarUnit)unit {
    if (unit == NSCalendarUnitQuarter)
        return NSMakeRange(1, 4);
    if (unit == NSCalendarUnitNanosecond)
        return NSMakeRange(0, 1000000000);
    return fromCF(CFCalendarGetMaximumRangeOfUnit(_cf, (CFCalendarUnit)unit));
}

- (NSRange)rangeOfUnit:(NSCalendarUnit)smaller inUnit:(NSCalendarUnit)larger forDate:(NSDate *)date {
    if (smaller == NSCalendarUnitQuarter && larger == NSCalendarUnitYear)
        return NSMakeRange(1, 4);
    return fromCF(CFCalendarGetRangeOfUnit(_cf, (CFCalendarUnit)smaller, (CFCalendarUnit)larger,
        [date timeIntervalSinceReferenceDate]));
}

- (NSUInteger)ordinalityOfUnit:(NSCalendarUnit)smaller inUnit:(NSCalendarUnit)larger forDate:(NSDate *)date {
    if (smaller == NSCalendarUnitQuarter && larger == NSCalendarUnitYear)
        return (NSUInteger)[self component:NSCalendarUnitQuarter fromDate:date];
    CFIndex o = CFCalendarGetOrdinalityOfUnit(_cf, (CFCalendarUnit)smaller, (CFCalendarUnit)larger,
        [date timeIntervalSinceReferenceDate]);
    return o == kCFNotFound ? NSNotFound : (NSUInteger)o;
}

- (BOOL)rangeOfUnit:(NSCalendarUnit)unit startDate:(NSDate **)datep interval:(NSTimeInterval *)tip forDate:(NSDate *)date {
    CFAbsoluteTime start = 0;
    CFTimeInterval interval = 0;

    if (unit == NSCalendarUnitQuarter) {
        /* The quarter's first month, from its first day. */
        NSInteger q = [self component:NSCalendarUnitQuarter fromDate:date];
        NSDateComponents *c = [self components:NSCalendarUnitEra | NSCalendarUnitYear fromDate:date];
        [c setMonth:(q - 1) * 3 + 1];
        [c setDay:1];
        NSDate *first = [self dateFromComponents:c];
        NSDate *next = [self dateByAddingUnit:NSCalendarUnitMonth value:3 toDate:first options:0];
        if (first == nil || next == nil)
            return NO;
        if (datep != NULL)
            *datep = first;
        if (tip != NULL)
            *tip = [next timeIntervalSinceDate:first];
        return YES;
    }
    if (unit == NSCalendarUnitNanosecond) {
        if (datep != NULL)
            *datep = date;
        if (tip != NULL)
            *tip = 1e-9;
        return YES;
    }
    if (!CFCalendarGetTimeRangeOfUnit(_cf, (CFCalendarUnit)unit, [date timeIntervalSinceReferenceDate], &start, &interval))
        return NO;
    if (datep != NULL)
        *datep = [NSDate dateWithTimeIntervalSinceReferenceDate:start];
    if (tip != NULL)
        *tip = interval;
    return YES;
}

/* -- Components and dates -- */

- (NSDateComponents *)components:(NSCalendarUnit)unitFlags fromDate:(NSDate *)date {
    NSDateComponents *result = [[[NSDateComponents alloc] init] autorelease];
    CFAbsoluteTime at = [date timeIntervalSinceReferenceDate];
    char codes[NUNITS + 1];
    int values[NUNITS];
    NSCalendarUnit units[NUNITS];
    unsigned n = 0;

    if (date == nil)
        return result;
    for (unsigned i = 0; i < NUNITS; i++)
        if ((unitFlags & unitCodes[i].unit) || ((unitFlags & NSCalendarUnitQuarter) && unitCodes[i].unit == NSCalendarUnitMonth)) {
            codes[n] = unitCodes[i].code;
            units[n] = unitCodes[i].unit;
            n++;
        }
    codes[n] = 0;
    if (!decompose(_cf, at, codes, values))
        return result;
    NSInteger month = NSDateComponentUndefined;
    for (unsigned i = 0; i < n; i++) {
        if (units[i] == NSCalendarUnitMonth)
            month = values[i];
        if (unitFlags & units[i])
            [result setValue:values[i] forComponent:units[i]];
    }
    if ((unitFlags & NSCalendarUnitQuarter) && month != NSDateComponentUndefined)
        [result setQuarter:(month - 1) / 3 + 1];
    if (unitFlags & NSCalendarUnitMonth)
        [result setLeapMonth:NO];
    if (unitFlags & NSCalendarUnitNanosecond) {
        double fraction = at - floor(at);
        [result setNanosecond:(NSInteger)llround(fraction * 1e9) % 1000000000];
    }
    if (unitFlags & NSCalendarUnitCalendar)
        [result setCalendar:self];
    if (unitFlags & NSCalendarUnitTimeZone)
        [result setTimeZone:_timeZone];
    return result;
}

- (NSDate *)dateFromComponents:(NSDateComponents *)comps {
    NSTimeZone *saved = nil;
    char codes[NUNITS + 2];
    int values[NUNITS + 1];
    int n = 0;
    BOOL weekBased = [comps yearForWeekOfYear] != NSDateComponentUndefined && [comps year] == NSDateComponentUndefined;

    if ([comps timeZone] != nil && ![[comps timeZone] isEqual:_timeZone]) {
        saved = [[_timeZone retain] autorelease];
        [self setTimeZone:[comps timeZone]];
    }
    for (unsigned i = 0; i < NUNITS; i++) {
        NSInteger v = [comps valueForComponent:unitCodes[i].unit];
        if (v == NSDateComponentUndefined)
            continue;
        /* Without a weekday, a weekday ordinal or week means nothing to ICU. */
        codes[n] = unitCodes[i].code;
        values[n] = (int)v;
        n++;
    }
    /* Unset fields are the lowest values, as on the reference platform:
     * year 1, not ICU's 1970. */
    if (!weekBased && [comps year] == NSDateComponentUndefined) {
        codes[n] = 'y';
        values[n] = 1;
        n++;
    }
    if ([comps month] == NSDateComponentUndefined && [comps quarter] != NSDateComponentUndefined &&
        [comps weekOfYear] == NSDateComponentUndefined) {
        codes[n] = 'M';
        values[n] = (int)(([comps quarter] - 1) * 3 + 1);
        n++;
    }
    codes[n] = 0;
    CFAbsoluteTime at = 0;
    BOOL ok = composeOne(_cf, &at, codes, values, n);
    if (saved != nil)
        [self setTimeZone:saved];
    if (!ok)
        return nil;
    if ([comps nanosecond] != NSDateComponentUndefined)
        at += (double)[comps nanosecond] / 1e9;
    return [NSDate dateWithTimeIntervalSinceReferenceDate:at];
}

- (NSDate *)dateByAddingComponents:(NSDateComponents *)comps toDate:(NSDate *)date options:(NSCalendarOptions)opts {
    CFAbsoluteTime at = [date timeIntervalSinceReferenceDate];
    CFOptionFlags options = (opts & NSCalendarWrapComponents) ? kCFCalendarComponentsWrap : 0;
    static const struct { NSCalendarUnit unit; char code; int factor; } adds[] = {
        { NSCalendarUnitEra, 'G', 1 }, { NSCalendarUnitYear, 'y', 1 }, { NSCalendarUnitQuarter, 'M', 3 },
        { NSCalendarUnitMonth, 'M', 1 }, { NSCalendarUnitWeekOfYear, 'w', 1 },
        { NSCalendarUnitWeekOfMonth, 'W', 1 }, { NSCalendarUnitYearForWeekOfYear, 'Y', 1 },
        { NSCalendarUnitDay, 'd', 1 }, { NSCalendarUnitWeekday, 'd', 1 },
        { NSCalendarUnitWeekdayOrdinal, 'F', 1 }, { NSCalendarUnitHour, 'H', 1 },
        { NSCalendarUnitMinute, 'm', 1 }, { NSCalendarUnitSecond, 's', 1 },
    };

    if (date == nil)
        return nil;
    for (unsigned i = 0; i < sizeof(adds) / sizeof(adds[0]); i++) {
        NSInteger v = [comps valueForComponent:adds[i].unit];
        if (v == NSDateComponentUndefined || v == 0)
            continue;
        if (!addOne(_cf, &at, options, adds[i].code, (int)(v * adds[i].factor)))
            return nil;
    }
    if ([comps week] != NSDateComponentUndefined && [comps week] != 0)
        addOne(_cf, &at, options, 'w', (int)[comps week]);
    if ([comps nanosecond] != NSDateComponentUndefined)
        at += (double)[comps nanosecond] / 1e9;
    return [NSDate dateWithTimeIntervalSinceReferenceDate:at];
}

- (NSDateComponents *)components:(NSCalendarUnit)unitFlags fromDate:(NSDate *)startingDate toDate:(NSDate *)resultDate options:(NSCalendarOptions)opts {
    NSDateComponents *result = [[[NSDateComponents alloc] init] autorelease];
    CFAbsoluteTime start = [startingDate timeIntervalSinceReferenceDate];
    CFAbsoluteTime end = [resultDate timeIntervalSinceReferenceDate];
    /* Largest first, each difference added before the next is taken. */
    static const struct { NSCalendarUnit unit; char code; } order[] = {
        { NSCalendarUnitEra, 'G' }, { NSCalendarUnitYear, 'y' }, { NSCalendarUnitYearForWeekOfYear, 'Y' },
        { NSCalendarUnitQuarter, 0 }, { NSCalendarUnitMonth, 'M' }, { NSCalendarUnitWeekOfYear, 'w' },
        { NSCalendarUnitWeekOfMonth, 'W' }, { NSCalendarUnitDay, 'd' }, { NSCalendarUnitWeekday, 'd' },
        { NSCalendarUnitWeekdayOrdinal, 'F' }, { NSCalendarUnitHour, 'H' }, { NSCalendarUnitMinute, 'm' },
        { NSCalendarUnitSecond, 's' },
    };

    for (unsigned i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        if (!(unitFlags & order[i].unit))
            continue;
        int v = 0;
        if (order[i].unit == NSCalendarUnitQuarter) {
            char one[2] = { 'M', 0 };
            if (CFCalendarGetComponentDifference(_cf, start, end, 0, one, &v)) {
                v /= 3;
                addOne(_cf, &start, 0, 'M', v * 3);
            }
        } else {
            char one[2] = { order[i].code, 0 };
            if (CFCalendarGetComponentDifference(_cf, start, end, 0, one, &v))
                addOne(_cf, &start, 0, order[i].code, v);
        }
        [result setValue:v forComponent:order[i].unit];
    }
    if (unitFlags & NSCalendarUnitNanosecond) {
        double rest = end - start;
        [result setNanosecond:(NSInteger)llround(rest * 1e9)];
    }
    if (unitFlags & NSCalendarUnitCalendar)
        [result setCalendar:self];
    if (unitFlags & NSCalendarUnitTimeZone)
        [result setTimeZone:_timeZone];
    return result;
}

- (void)getEra:(NSInteger *)eraValuePointer year:(NSInteger *)yearValuePointer month:(NSInteger *)monthValuePointer day:(NSInteger *)dayValuePointer fromDate:(NSDate *)date {
    NSDateComponents *c = [self components:NSCalendarUnitEra | NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay fromDate:date];
    if (eraValuePointer) *eraValuePointer = [c era];
    if (yearValuePointer) *yearValuePointer = [c year];
    if (monthValuePointer) *monthValuePointer = [c month];
    if (dayValuePointer) *dayValuePointer = [c day];
}

- (void)getEra:(NSInteger *)eraValuePointer yearForWeekOfYear:(NSInteger *)yearValuePointer weekOfYear:(NSInteger *)weekValuePointer weekday:(NSInteger *)weekdayValuePointer fromDate:(NSDate *)date {
    NSDateComponents *c = [self components:NSCalendarUnitEra | NSCalendarUnitYearForWeekOfYear | NSCalendarUnitWeekOfYear | NSCalendarUnitWeekday fromDate:date];
    if (eraValuePointer) *eraValuePointer = [c era];
    if (yearValuePointer) *yearValuePointer = [c yearForWeekOfYear];
    if (weekValuePointer) *weekValuePointer = [c weekOfYear];
    if (weekdayValuePointer) *weekdayValuePointer = [c weekday];
}

- (void)getHour:(NSInteger *)hourValuePointer minute:(NSInteger *)minuteValuePointer second:(NSInteger *)secondValuePointer nanosecond:(NSInteger *)nanosecondValuePointer fromDate:(NSDate *)date {
    NSDateComponents *c = [self components:NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond | NSCalendarUnitNanosecond fromDate:date];
    if (hourValuePointer) *hourValuePointer = [c hour];
    if (minuteValuePointer) *minuteValuePointer = [c minute];
    if (secondValuePointer) *secondValuePointer = [c second];
    if (nanosecondValuePointer) *nanosecondValuePointer = [c nanosecond];
}

- (NSInteger)component:(NSCalendarUnit)unit fromDate:(NSDate *)date {
    return [[self components:unit fromDate:date] valueForComponent:unit];
}

- (NSDate *)dateWithEra:(NSInteger)eraValue year:(NSInteger)yearValue month:(NSInteger)monthValue day:(NSInteger)dayValue hour:(NSInteger)hourValue minute:(NSInteger)minuteValue second:(NSInteger)secondValue nanosecond:(NSInteger)nanosecondValue {
    NSDateComponents *c = [[[NSDateComponents alloc] init] autorelease];
    [c setEra:eraValue]; [c setYear:yearValue]; [c setMonth:monthValue]; [c setDay:dayValue];
    [c setHour:hourValue]; [c setMinute:minuteValue]; [c setSecond:secondValue]; [c setNanosecond:nanosecondValue];
    return [self dateFromComponents:c];
}

- (NSDate *)dateWithEra:(NSInteger)eraValue yearForWeekOfYear:(NSInteger)yearValue weekOfYear:(NSInteger)weekValue weekday:(NSInteger)weekdayValue hour:(NSInteger)hourValue minute:(NSInteger)minuteValue second:(NSInteger)secondValue nanosecond:(NSInteger)nanosecondValue {
    NSDateComponents *c = [[[NSDateComponents alloc] init] autorelease];
    [c setEra:eraValue]; [c setYearForWeekOfYear:yearValue]; [c setWeekOfYear:weekValue]; [c setWeekday:weekdayValue];
    [c setHour:hourValue]; [c setMinute:minuteValue]; [c setSecond:secondValue]; [c setNanosecond:nanosecondValue];
    return [self dateFromComponents:c];
}

/* -- Days, comparisons, weekends -- */

- (NSDate *)startOfDayForDate:(NSDate *)date {
    NSDate *start = nil;
    if ([self rangeOfUnit:NSCalendarUnitDay startDate:&start interval:NULL forDate:date])
        return start;
    return date;
}

- (NSDateComponents *)componentsInTimeZone:(NSTimeZone *)timezone fromDate:(NSDate *)date {
    NSCalendar *calendar = [[self copy] autorelease];
    [calendar setTimeZone:timezone];
    return [calendar components:allUnits | NSCalendarUnitCalendar | NSCalendarUnitTimeZone fromDate:date];
}

- (NSComparisonResult)compareDate:(NSDate *)date1 toDate:(NSDate *)date2 toUnitGranularity:(NSCalendarUnit)unit {
    NSDate *start1 = nil, *start2 = nil;

    if (unit == NSCalendarUnitNanosecond || ![self rangeOfUnit:unit startDate:&start1 interval:NULL forDate:date1] ||
        ![self rangeOfUnit:unit startDate:&start2 interval:NULL forDate:date2])
        return [date1 compare:date2];
    return [start1 compare:start2];
}

- (BOOL)isDate:(NSDate *)date1 equalToDate:(NSDate *)date2 toUnitGranularity:(NSCalendarUnit)unit {
    return [self compareDate:date1 toDate:date2 toUnitGranularity:unit] == NSOrderedSame;
}

- (BOOL)isDate:(NSDate *)date1 inSameDayAsDate:(NSDate *)date2 {
    return [self isDate:date1 equalToDate:date2 toUnitGranularity:NSCalendarUnitDay];
}

- (BOOL)isDateInToday:(NSDate *)date {
    return [self isDate:date inSameDayAsDate:[NSDate date]];
}

- (BOOL)isDateInYesterday:(NSDate *)date {
    NSDate *yesterday = [self dateByAddingUnit:NSCalendarUnitDay value:-1 toDate:[NSDate date] options:0];
    return yesterday != nil && [self isDate:date inSameDayAsDate:yesterday];
}

- (BOOL)isDateInTomorrow:(NSDate *)date {
    NSDate *tomorrow = [self dateByAddingUnit:NSCalendarUnitDay value:1 toDate:[NSDate date] options:0];
    return tomorrow != nil && [self isDate:date inSameDayAsDate:tomorrow];
}

/* The weekend: Saturday and Sunday (weekdays 7 and 1). */
static BOOL isWeekendDay(NSInteger weekday) {
    return weekday == 7 || weekday == 1;
}

- (BOOL)isDateInWeekend:(NSDate *)date {
    return isWeekendDay([self component:NSCalendarUnitWeekday fromDate:date]);
}

- (BOOL)rangeOfWeekendStartDate:(NSDate **)datep interval:(NSTimeInterval *)tip containingDate:(NSDate *)date {
    if (![self isDateInWeekend:date])
        return NO;
    NSDate *day = [self startOfDayForDate:date];
    if ([self component:NSCalendarUnitWeekday fromDate:day] == 1)
        day = [self dateByAddingUnit:NSCalendarUnitDay value:-1 toDate:day options:0];
    NSDate *end = [self dateByAddingUnit:NSCalendarUnitDay value:2 toDate:day options:0];
    if (datep != NULL)
        *datep = day;
    if (tip != NULL)
        *tip = [end timeIntervalSinceDate:day];
    return YES;
}

- (BOOL)nextWeekendStartDate:(NSDate **)datep interval:(NSTimeInterval *)tip options:(NSCalendarOptions)options afterDate:(NSDate *)date {
    BOOL backwards = (options & NSCalendarSearchBackwards) != 0;
    NSDate *day = [self startOfDayForDate:date];

    for (int i = 0; i < 14; i++) {
        day = [self dateByAddingUnit:NSCalendarUnitDay value:backwards ? -1 : 1 toDate:day options:0];
        if ([self component:NSCalendarUnitWeekday fromDate:day] == 7)
            return [self rangeOfWeekendStartDate:datep interval:tip containingDate:day];
    }
    return NO;
}

- (NSDateComponents *)components:(NSCalendarUnit)unitFlags fromDateComponents:(NSDateComponents *)startingDateComp toDateComponents:(NSDateComponents *)resultDateComp options:(NSCalendarOptions)options {
    NSDate *start = [self dateFromComponents:startingDateComp];
    NSDate *end = [self dateFromComponents:resultDateComp];
    if (start == nil || end == nil)
        return [[[NSDateComponents alloc] init] autorelease];
    return [self components:unitFlags fromDate:start toDate:end options:options];
}

- (NSDate *)dateByAddingUnit:(NSCalendarUnit)unit value:(NSInteger)value toDate:(NSDate *)date options:(NSCalendarOptions)options {
    NSDateComponents *c = [[[NSDateComponents alloc] init] autorelease];
    [c setValue:value forComponent:unit];
    return [self dateByAddingComponents:c toDate:date options:options];
}

/* -- Matching -- */

- (BOOL)date:(NSDate *)date matchesComponents:(NSDateComponents *)components {
    NSCalendarUnit units = 0;
    for (unsigned i = 0; i < NUNITS; i++)
        if ([components valueForComponent:unitCodes[i].unit] != NSDateComponentUndefined)
            units |= unitCodes[i].unit;
    if ([components quarter] != NSDateComponentUndefined)
        units |= NSCalendarUnitQuarter;
    NSDateComponents *have = [self components:units fromDate:date];
    for (unsigned i = 0; i < NUNITS; i++) {
        NSInteger want = [components valueForComponent:unitCodes[i].unit];
        if (want != NSDateComponentUndefined && [have valueForComponent:unitCodes[i].unit] != want)
            return NO;
    }
    if ([components quarter] != NSDateComponentUndefined && [have quarter] != [components quarter])
        return NO;
    return YES;
}

/* The smallest unit the components name: the search steps by it. */
static NSCalendarUnit smallestUnit(NSDateComponents *c) {
    static const NSCalendarUnit order[] = { NSCalendarUnitSecond, NSCalendarUnitMinute, NSCalendarUnitHour,
        NSCalendarUnitDay, NSCalendarUnitWeekday, NSCalendarUnitWeekdayOrdinal, NSCalendarUnitWeekOfMonth,
        NSCalendarUnitWeekOfYear, NSCalendarUnitMonth, NSCalendarUnitQuarter, NSCalendarUnitYear,
        NSCalendarUnitYearForWeekOfYear, NSCalendarUnitEra };
    for (unsigned i = 0; i < sizeof(order) / sizeof(order[0]); i++)
        if ([c valueForComponent:order[i]] != NSDateComponentUndefined)
            return order[i];
    return 0;
}

/* The unit the search steps by, and the one whose start a candidate is. */
static NSCalendarUnit stepUnit(NSCalendarUnit smallest) {
    switch (smallest) {
    case NSCalendarUnitWeekday: case NSCalendarUnitWeekdayOrdinal: return NSCalendarUnitDay;
    case NSCalendarUnitWeekOfMonth: case NSCalendarUnitWeekOfYear: return NSCalendarUnitWeekOfYear;
    case NSCalendarUnitQuarter: return NSCalendarUnitMonth;
    case NSCalendarUnitYearForWeekOfYear: return NSCalendarUnitYear;
    default: return smallest;
    }
}

/* The next (or previous) date the components match, at the start of the
 * smallest unit they name: stepped by that unit, so a match that cannot be
 * (the 30th of February) is passed over, as MatchStrictly does. */
- (NSDate *)nextDateAfterDate:(NSDate *)date matchingComponents:(NSDateComponents *)comps options:(NSCalendarOptions)options {
    BOOL backwards = (options & NSCalendarSearchBackwards) != 0;
    NSCalendarUnit smallest = smallestUnit(comps);
    NSDate *candidate = nil;

    if (smallest == 0) {
        if ([comps nanosecond] == NSDateComponentUndefined)
            return nil;
        smallest = NSCalendarUnitSecond;
    }
    NSCalendarUnit step = stepUnit(smallest);
    if (![self rangeOfUnit:step startDate:&candidate interval:NULL forDate:date])
        return nil;
    for (int i = 0; i < 200000; i++) {
        if ((backwards ? [candidate compare:date] == NSOrderedAscending : [candidate compare:date] == NSOrderedDescending) &&
            [self date:candidate matchesComponents:comps]) {
            if ([comps nanosecond] != NSDateComponentUndefined)
                candidate = [candidate dateByAddingTimeInterval:(double)[comps nanosecond] / 1e9];
            return candidate;
        }
        NSDate *next = [self dateByAddingUnit:step value:backwards ? -1 : 1 toDate:candidate options:0];
        if (next == nil || [next isEqual:candidate])
            return nil;
        candidate = next;
    }
    return nil;
}

- (void)enumerateDatesStartingAfterDate:(NSDate *)start matchingComponents:(NSDateComponents *)comps options:(NSCalendarOptions)opts usingBlock:(void (^)(NSDate *date, BOOL exactMatch, BOOL *stop))block {
    NSDate *date = [start retain];
    BOOL stop = NO;

    while (!stop) {
        NSAutoreleasePool *pool = [NSAutoreleasePool new];
        NSDate *next = [self nextDateAfterDate:date matchingComponents:comps options:opts];
        if (next == nil) {
            [pool release];
            break;
        }
        block(next, YES, &stop);
        [date release];
        date = [next retain];
        [pool release];
    }
    [date release];
}

- (NSDate *)nextDateAfterDate:(NSDate *)date matchingUnit:(NSCalendarUnit)unit value:(NSInteger)value options:(NSCalendarOptions)options {
    NSDateComponents *c = [[[NSDateComponents alloc] init] autorelease];
    [c setValue:value forComponent:unit];
    return [self nextDateAfterDate:date matchingComponents:c options:options];
}

- (NSDate *)nextDateAfterDate:(NSDate *)date matchingHour:(NSInteger)hourValue minute:(NSInteger)minuteValue second:(NSInteger)secondValue options:(NSCalendarOptions)options {
    NSDateComponents *c = [[[NSDateComponents alloc] init] autorelease];
    [c setHour:hourValue];
    [c setMinute:minuteValue];
    [c setSecond:secondValue];
    return [self nextDateAfterDate:date matchingComponents:c options:options];
}

/* The date with one unit changed, the larger ones kept; else the next date
 * that has it. */
- (NSDate *)dateBySettingUnit:(NSCalendarUnit)unit value:(NSInteger)v ofDate:(NSDate *)date options:(NSCalendarOptions)opts {
    if ([self component:unit fromDate:date] == v)
        return date;
    NSDateComponents *c = [self components:allUnits & ~(NSCalendarUnitWeekday | NSCalendarUnitWeekdayOrdinal |
        NSCalendarUnitQuarter | NSCalendarUnitWeekOfMonth | NSCalendarUnitWeekOfYear | NSCalendarUnitYearForWeekOfYear)
        fromDate:date];
    [c setValue:v forComponent:unit];
    NSDate *result = [self dateFromComponents:c];
    if (result != nil && [self component:unit fromDate:result] == v)
        return result;
    return [self nextDateAfterDate:date matchingUnit:unit value:v options:opts];
}

- (NSDate *)dateBySettingHour:(NSInteger)h minute:(NSInteger)m second:(NSInteger)s ofDate:(NSDate *)date options:(NSCalendarOptions)opts {
    NSDateComponents *c = [self components:NSCalendarUnitEra | NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay fromDate:date];
    [c setHour:h];
    [c setMinute:m];
    [c setSecond:s];
    NSDate *result = [self dateFromComponents:c];
    if (result != nil && [self component:NSCalendarUnitHour fromDate:result] == h)
        return result;
    return [self nextDateAfterDate:date matchingHour:h minute:m second:s options:opts];
}

@end
