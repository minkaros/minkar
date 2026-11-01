/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSDateFormatter.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSUserDefaults.h>
#import <Foundation/NSAttributedString.h>
#import <Foundation/NSTimeZone.h>
#include <math.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSScanner.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSKeyedUnarchiver.h>
#import <Foundation/NSCalendar.h>
#import <Foundation/NSNumber.h>

// Minkar: two behaviors. 10.0 (initWithDateFormat:allowNaturalLanguage:,
// "%Y-%m-%d" formats, NSCalendarDate) is Cocotron's code below. 10.4,
// the default (init), uses CFDateFormatter on ICU: Unicode patterns
// ("yyyy-MM-dd"), date and time styles, locale, time zone, calendar,
// symbols, templates.

#import <Foundation/NSLocale.h>
#import <Foundation/NSError.h>
#import <CoreFoundation/CFLocale.h>
#import <CoreFoundation/CFTimeZone.h>
#import <CoreFoundation/CFNumber.h>
#import <CoreFoundation/CFDate.h>

static NSDateFormatterBehavior defaultBehavior = NSDateFormatterBehavior10_4;

// A CF string with the same characters: CF's string functions do not take
// Foundation's own string classes (only CF's strings), so every NSString
// handed to CF goes through here. Returned retained.
static CFStringRef copyCFString(NSString *string) {
   if(string==nil)
    return NULL;
   return CFStringCreateWithCString(NULL,[string UTF8String],kCFStringEncodingUTF8);
}

// The same for the values of properties (symbol arrays, symbols).
static CFTypeRef copyCFValue(id value) {
   if([value isKindOfClass:[NSString class]])
    return copyCFString(value);
   if([value isKindOfClass:[NSArray class]]){
    CFMutableArrayRef array=CFArrayCreateMutable(NULL,[value count],&kCFTypeArrayCallBacks);
    for(id item in value){
     CFTypeRef cf=copyCFValue(item);
     if(cf!=NULL){
      CFArrayAppendValue(array,cf);
      CFRelease(cf);
     }
    }
    return array;
   }
   return value?CFRetain((CFTypeRef)value):NULL;
}

@implementation NSDateFormatter

+(NSDateFormatterBehavior)defaultFormatterBehavior {
   return defaultBehavior;
}

+(void)setDefaultFormatterBehavior:(NSDateFormatterBehavior)behavior {
   defaultBehavior=(behavior==NSDateFormatterBehavior10_0)?NSDateFormatterBehavior10_0:NSDateFormatterBehavior10_4;
}

-initWithDateFormat:(NSString *)format allowNaturalLanguage:(BOOL)flag {
    return [self initWithDateFormat:format allowNaturalLanguage:flag locale:[[NSUserDefaults standardUserDefaults] dictionaryRepresentation]];
}

-initWithDateFormat:(NSString *)format allowNaturalLanguage:(BOOL)flag locale:(NSDictionary *)locale {
    [super init];
    _behavior=NSDateFormatterBehavior10_0;
    _dateFormat10_0 = [format copy];
    _dateFormat = [format copy];
    _allowsNaturalLanguage = flag;
    _locale = [locale retain];
    _tz = [[NSTimeZone defaultTimeZone] retain];
    _nsLocale = [[NSLocale currentLocale] retain];

    return self;
}

-init {
   if((self=[super init])==nil)
    return nil;
   _behavior=NSDateFormatterBehaviorDefault;
   _dateStyle=NSDateFormatterNoStyle;
   _timeStyle=NSDateFormatterNoStyle;
   _dateFormat=nil;
   _nsLocale=[[NSLocale currentLocale] retain];
   _tz=[[NSTimeZone defaultTimeZone] retain];
   _properties=[[NSMutableDictionary alloc] init];
   return self;
}

-(BOOL)_isModern {
   NSDateFormatterBehavior behavior=_behavior;
   if(behavior==NSDateFormatterBehaviorDefault)
    behavior=defaultBehavior;
   return behavior!=NSDateFormatterBehavior10_0;
}

-(void)_invalidate {
   if(_cf!=NULL){
    CFRelease((CFTypeRef)_cf);
    _cf=NULL;
   }
}

-(void)dealloc {
    [self _invalidate];
    [_dateFormat10_0 release];
    [_dateFormat release];
    [_locale release];
    [_tz release];
    [_nsLocale release];
    [_calendar release];
    [_properties release];

    [super dealloc];
}

-initWithCoder:(NSCoder*)coder {
   [super initWithCoder:coder];

   if([coder allowsKeyedCoding]){
    NSDictionary *attributes=[coder decodeObjectForKey:@"NS.attributes"];

    _dateFormat10_0=[[attributes objectForKey:@"dateFormat_10_0"] copy];
    _behavior=[[attributes objectForKey:@"formatterBehavior"] intValue];
    _dateStyle=[[attributes objectForKey:@"dateStyle"] intValue];
    _timeStyle=[[attributes objectForKey:@"timeStyle"] intValue];
    _dateFormat=[[coder decodeObjectForKey:@"NS.format"] retain];
    _allowsNaturalLanguage=[coder decodeBoolForKey:@"NS.natural"];
    _tz = [[coder decodeObjectForKey:@"timeZone"] retain];
   }
   if(_tz==nil)
    _tz=[[NSTimeZone defaultTimeZone] retain];
   _nsLocale=[[NSLocale currentLocale] retain];
   _properties=[[NSMutableDictionary alloc] init];

   return self;
}

-copyWithZone:(NSZone *)zone {
   NSDateFormatter *copy;

   if(![self _isModern])
    return [[NSDateFormatter allocWithZone:zone] initWithDateFormat:_dateFormat10_0 allowNaturalLanguage:_allowsNaturalLanguage locale:_locale];
   copy=[[NSDateFormatter allocWithZone:zone] init];
   [copy setFormatterBehavior:_behavior];
   [copy setLocale:_nsLocale];
   [copy setTimeZone:_tz];
   [copy setCalendar:_calendar];
   [copy setDateStyle:_dateStyle];
   [copy setTimeStyle:_timeStyle];
   if(_dateFormat!=nil)
    [copy setDateFormat:_dateFormat];
   [copy setLenient:_lenient];
   [copy->_properties addEntriesFromDictionary:_properties];
   return copy;
}

// The CFDateFormatter for the current settings, made again after a change.
-(CFDateFormatterRef)_cfFormatter {
   if(_cf==NULL){
    NSString        *identifier=[_nsLocale localeIdentifier];
    NSMutableString *full=[NSMutableString stringWithString:identifier?identifier:@"en_US"];
    CFLocaleRef      locale;
    CFDateFormatterRef formatter;

    if(_calendar!=nil && [_calendar calendarIdentifier]!=nil && [full rangeOfString:@"@calendar="].location==NSNotFound)
     [full appendFormat:@"@calendar=%@",[_calendar calendarIdentifier]];
    CFStringRef localeName=copyCFString(full);
    locale=CFLocaleCreate(NULL,localeName);
    CFRelease(localeName);
    formatter=CFDateFormatterCreate(NULL,locale,(CFDateFormatterStyle)_dateStyle,(CFDateFormatterStyle)_timeStyle);
    if(locale!=NULL)
     CFRelease(locale);
    if(formatter==NULL)
     return NULL;

    CFTimeZoneRef zone=NULL;
    if([_tz name]!=nil){
     CFStringRef zoneName=copyCFString([_tz name]);
     zone=CFTimeZoneCreateWithName(NULL,zoneName,true);
     CFRelease(zoneName);
    }
    if(zone==NULL)
     zone=CFTimeZoneCreateWithTimeIntervalFromGMT(NULL,[_tz secondsFromGMT]);
    if(zone!=NULL){
     CFDateFormatterSetProperty(formatter,kCFDateFormatterTimeZone,zone);
     CFRelease(zone);
    }
    CFDateFormatterSetProperty(formatter,kCFDateFormatterIsLenient,_lenient?kCFBooleanTrue:kCFBooleanFalse);
    for(NSString *key in _properties){
     CFTypeRef value=copyCFValue([_properties objectForKey:key]);
     CFDateFormatterSetProperty(formatter,(CFStringRef)key,value);
     if(value!=NULL)
      CFRelease(value);
    }
    if(_dateFormat!=nil){
     CFStringRef format=copyCFString(_dateFormat);
     CFDateFormatterSetFormat(formatter,format);
     CFRelease(format);
    }
    _cf=(void *)formatter;
   }
   return (CFDateFormatterRef)_cf;
}

-(NSString *)dateFormat {
   if(![self _isModern] || _dateFormat!=nil)
    return _dateFormat?_dateFormat:@"";
   CFDateFormatterRef formatter=[self _cfFormatter];
   return formatter?(NSString *)CFDateFormatterGetFormat(formatter):@"";
}

-(void)setDateFormat:(NSString *)format {
   format=[format copy];
   if(![self _isModern]){
    [_dateFormat10_0 release];
    _dateFormat10_0=[format retain];
   }
   [_dateFormat release];
   _dateFormat=format;
   if(_cf!=NULL && format!=nil){
    CFStringRef cfFormat=copyCFString(format);
    CFDateFormatterSetFormat((CFDateFormatterRef)_cf,cfFormat);
    CFRelease(cfFormat);
   }
   else
    [self _invalidate];
}

// A style sets the format again.
-(NSDateFormatterStyle)dateStyle {
   return _dateStyle;
}

-(void)setDateStyle:(NSDateFormatterStyle)style {
   _dateStyle=style;
   [_dateFormat release];
   _dateFormat=nil;
   [self _invalidate];
}

-(NSDateFormatterStyle)timeStyle {
   return _timeStyle;
}

-(void)setTimeStyle:(NSDateFormatterStyle)style {
   _timeStyle=style;
   [_dateFormat release];
   _dateFormat=nil;
   [self _invalidate];
}

-(void)setLocalizedDateFormatFromTemplate:(NSString *)dateFormatTemplate {
   NSString *format=[[self class] dateFormatFromTemplate:dateFormatTemplate options:0 locale:_nsLocale];
   if(format!=nil)
    [self setDateFormat:format];
}

-(BOOL)allowsNaturalLanguage {
   return _allowsNaturalLanguage;
}

-(NSDateFormatterBehavior)formatterBehavior {
   return _behavior;
}

-(void)setFormatterBehavior:(NSDateFormatterBehavior)value {
   _behavior=value;
   [self _invalidate];
}

-(NSLocale *)locale {
   return _nsLocale;
}

-(void)setLocale:(NSLocale *)locale {
   if(locale==nil)
    locale=[NSLocale currentLocale];
   [_nsLocale release];
   _nsLocale=[locale copy];
   [self _invalidate];
}

-(void)setTimeZone:(NSTimeZone*)tz {
    if(tz==nil)
     tz=[NSTimeZone defaultTimeZone];
    [_tz autorelease];
    _tz = [tz retain];
    [self _invalidate];
}

-(NSTimeZone*)timeZone {
    return _tz;
}

-(NSCalendar *)calendar {
   return _calendar;
}

-(void)setCalendar:(NSCalendar *)calendar {
   [_calendar release];
   _calendar=[calendar retain];
   [self _invalidate];
}

-(BOOL)isLenient {
   return _lenient;
}

-(void)setLenient:(BOOL)value {
   _lenient=value;
   if(_cf!=NULL)
    CFDateFormatterSetProperty((CFDateFormatterRef)_cf,kCFDateFormatterIsLenient,value?kCFBooleanTrue:kCFBooleanFalse);
}

-(BOOL)generatesCalendarDates {
   return _generatesCalendarDates;
}

-(void)setGeneratesCalendarDates:(BOOL)value {
   _generatesCalendarDates=value;
}

// Relative dates ("today") need what CFDateFormatter does not have: the
// setting is kept, the dates are formatted as usual.
-(BOOL)doesRelativeDateFormatting {
   return _relative;
}

-(void)setDoesRelativeDateFormatting:(BOOL)value {
   _relative=value;
}

-(NSFormattingContext)formattingContext {
   return _formattingContext;
}

-(void)setFormattingContext:(NSFormattingContext)context {
   _formattingContext=context;
}

#pragma mark Properties of the CFDateFormatter

-(id)_property:(CFStringRef)key {
   CFDateFormatterRef formatter=[self _cfFormatter];
   CFTypeRef value=formatter?CFDateFormatterCopyProperty(formatter,key):NULL;
   return [(id)value autorelease];
}

-(void)_setProperty:(CFStringRef)key value:(id)value {
   if(value==nil)
    [_properties removeObjectForKey:(NSString *)key];
   else
    [_properties setObject:value forKey:(NSString *)key];
   [self _invalidate];
}

-(NSDate *)_dateProperty:(CFStringRef)key {
   id value=[self _property:key];
   if(value==nil)
    return nil;
   return [NSDate dateWithTimeIntervalSinceReferenceDate:CFDateGetAbsoluteTime((CFDateRef)value)];
}

-(void)_setDateProperty:(CFStringRef)key value:(NSDate *)date {
   CFDateRef cfDate=date?CFDateCreate(NULL,[date timeIntervalSinceReferenceDate]):NULL;
   [self _setProperty:key value:(id)cfDate];
   if(cfDate!=NULL)
    CFRelease(cfDate);
}

-(NSDate *)twoDigitStartDate { return [self _dateProperty:kCFDateFormatterTwoDigitStartDate]; }
-(void)setTwoDigitStartDate:(NSDate *)date { [self _setDateProperty:kCFDateFormatterTwoDigitStartDate value:date]; }
-(NSDate *)defaultDate { return [self _dateProperty:kCFDateFormatterDefaultDate]; }
-(void)setDefaultDate:(NSDate *)date { [self _setDateProperty:kCFDateFormatterDefaultDate value:date]; }
-(NSDate *)gregorianStartDate { return [self _dateProperty:kCFDateFormatterGregorianStartDate]; }
-(void)setGregorianStartDate:(NSDate *)date { [self _setDateProperty:kCFDateFormatterGregorianStartDate value:date]; }

#define SYMBOLS(getter, setter, key, type) \
-(type)getter { return [self _property:key]; } \
-(void)setter:(type)value { [self _setProperty:key value:value]; }

SYMBOLS(eraSymbols, setEraSymbols, kCFDateFormatterEraSymbols, NSArray *)
SYMBOLS(longEraSymbols, setLongEraSymbols, kCFDateFormatterLongEraSymbols, NSArray *)
SYMBOLS(monthSymbols, setMonthSymbols, kCFDateFormatterMonthSymbols, NSArray *)
SYMBOLS(shortMonthSymbols, setShortMonthSymbols, kCFDateFormatterShortMonthSymbols, NSArray *)
SYMBOLS(veryShortMonthSymbols, setVeryShortMonthSymbols, kCFDateFormatterVeryShortMonthSymbols, NSArray *)
SYMBOLS(standaloneMonthSymbols, setStandaloneMonthSymbols, kCFDateFormatterStandaloneMonthSymbols, NSArray *)
SYMBOLS(shortStandaloneMonthSymbols, setShortStandaloneMonthSymbols, kCFDateFormatterShortStandaloneMonthSymbols, NSArray *)
SYMBOLS(veryShortStandaloneMonthSymbols, setVeryShortStandaloneMonthSymbols, kCFDateFormatterVeryShortStandaloneMonthSymbols, NSArray *)
SYMBOLS(weekdaySymbols, setWeekdaySymbols, kCFDateFormatterWeekdaySymbols, NSArray *)
SYMBOLS(shortWeekdaySymbols, setShortWeekdaySymbols, kCFDateFormatterShortWeekdaySymbols, NSArray *)
SYMBOLS(veryShortWeekdaySymbols, setVeryShortWeekdaySymbols, kCFDateFormatterVeryShortWeekdaySymbols, NSArray *)
SYMBOLS(standaloneWeekdaySymbols, setStandaloneWeekdaySymbols, kCFDateFormatterStandaloneWeekdaySymbols, NSArray *)
SYMBOLS(shortStandaloneWeekdaySymbols, setShortStandaloneWeekdaySymbols, kCFDateFormatterShortStandaloneWeekdaySymbols, NSArray *)
SYMBOLS(veryShortStandaloneWeekdaySymbols, setVeryShortStandaloneWeekdaySymbols, kCFDateFormatterVeryShortStandaloneWeekdaySymbols, NSArray *)
SYMBOLS(quarterSymbols, setQuarterSymbols, kCFDateFormatterQuarterSymbols, NSArray *)
SYMBOLS(shortQuarterSymbols, setShortQuarterSymbols, kCFDateFormatterShortQuarterSymbols, NSArray *)
SYMBOLS(standaloneQuarterSymbols, setStandaloneQuarterSymbols, kCFDateFormatterStandaloneQuarterSymbols, NSArray *)
SYMBOLS(shortStandaloneQuarterSymbols, setShortStandaloneQuarterSymbols, kCFDateFormatterShortStandaloneQuarterSymbols, NSArray *)
SYMBOLS(AMSymbol, setAMSymbol, kCFDateFormatterAMSymbol, NSString *)
SYMBOLS(PMSymbol, setPMSymbol, kCFDateFormatterPMSymbol, NSString *)

#undef SYMBOLS

#pragma mark Formatting

- (NSString *)stringFromDate:(NSDate *)date {
   if(date==nil)
    return nil;
   if(![self _isModern])
    return NSStringWithDateFormatLocale([date timeIntervalSinceReferenceDate], [self dateFormat], nil, _tz);

   CFDateFormatterRef formatter=[self _cfFormatter];
   CFStringRef string=formatter?CFDateFormatterCreateStringWithAbsoluteTime(NULL,formatter,[date timeIntervalSinceReferenceDate]):NULL;
   return [(NSString *)string autorelease];
}

- (NSDate*)dateFromString:(NSString*)string {
   NSDate *date=nil;

   if(string==nil)
    return nil;
   if(![self _isModern])
    return NSDateWithStringDateFormatLocale(string, [self dateFormat], nil, _tz);
   if([self getObjectValue:&date forString:string range:NULL error:NULL])
    return date;
   return nil;
}

// The whole string (or the range) must be a date.
-(BOOL)getObjectValue:(id *)object forString:(NSString *)string range:(NSRange *)rangep error:(NSError **)error {
   CFDateFormatterRef formatter=[self _cfFormatter];
   CFAbsoluteTime     at=0;
   CFRange            range=CFRangeMake(0,[string length]);
   BOOL               ok;

   if(rangep!=NULL)
    range=CFRangeMake(rangep->location,rangep->length);
   CFStringRef cfString=copyCFString(string);
   ok=formatter!=NULL && cfString!=NULL &&
      CFDateFormatterGetAbsoluteTimeFromString(formatter,cfString,&range,&at) &&
      (rangep!=NULL || range.length==(CFIndex)[string length]);
   if(cfString!=NULL)
    CFRelease(cfString);
   if(rangep!=NULL && ok)
    *rangep=NSMakeRange(range.location,range.length);
   if(!ok){
    if(object!=NULL)
     *object=nil;
    if(error!=NULL)
     *error=[NSError errorWithDomain:NSCocoaErrorDomain code:2048 userInfo:
             [NSDictionary dictionaryWithObject:[NSString stringWithFormat:@"The value “%@” is invalid.",string] forKey:NSLocalizedDescriptionKey]];
    return NO;
   }
   if(object!=NULL)
    *object=[NSDate dateWithTimeIntervalSinceReferenceDate:at];
   return YES;
}

+(NSString *)localizedStringFromDate:(NSDate *)date dateStyle:(NSDateFormatterStyle)dstyle timeStyle:(NSDateFormatterStyle)tstyle {
   NSDateFormatter *formatter=[[[NSDateFormatter alloc] init] autorelease];
   [formatter setDateStyle:dstyle];
   [formatter setTimeStyle:tstyle];
   return [formatter stringFromDate:date];
}

+(NSString *)dateFormatFromTemplate:(NSString *)tmplate options:(NSUInteger)opts locale:(NSLocale *)locale {
   NSString   *identifier=[(locale?locale:[NSLocale currentLocale]) localeIdentifier];
   CFStringRef name=copyCFString(identifier?identifier:@"en_US");
   CFLocaleRef cfLocale=CFLocaleCreate(NULL,name);
   CFStringRef cfTemplate=copyCFString(tmplate);
   CFStringRef format=cfTemplate?CFDateFormatterCreateDateFormatFromTemplate(NULL,cfTemplate,opts,cfLocale):NULL;

   CFRelease(name);
   if(cfTemplate!=NULL)
    CFRelease(cfTemplate);
   if(cfLocale!=NULL)
    CFRelease(cfLocale);
   return [(NSString *)format autorelease];
}

-(NSString *)stringForObjectValue:(id)object {
   if(![object isKindOfClass:[NSDate class]])
    return nil;
   if([self _isModern])
    return [self stringFromDate:object];
   if([object isKindOfClass:[NSCalendarDate class]])
    return NSStringWithDateFormatLocale([object timeIntervalSinceReferenceDate], _dateFormat10_0, _locale, [object timeZone]);
   return NSStringWithDateFormatLocale([object timeIntervalSinceReferenceDate], _dateFormat10_0, _locale, _tz);
}

-(NSAttributedString *)attributedStringForObjectValue:(id)object
   withDefaultAttributes:(NSDictionary *)attributes {
    NSString *string=[self stringForObjectValue:object];
    if(string==nil)
     return nil;
    return [[[NSAttributedString allocWithZone:NULL] initWithString:string attributes:attributes] autorelease];
}

-(NSString *)editingStringForObjectValue:(id)object {
    return [self stringForObjectValue:object];
}

-(BOOL)getObjectValue:(id *)object forString:(NSString *)string errorDescription:(NSString **)error {
    if([self _isModern]){
     NSError *e=nil;
     BOOL ok=[self getObjectValue:object forString:string range:NULL error:&e];
     if(!ok && error!=NULL)
      *error=[e localizedDescription];
     return ok;
    }
    *object = NSDateWithStringDateFormatLocale(string, _dateFormat10_0, _locale, _tz);
    if (*object == nil) {
// FIX localization
       if(error!=NULL)
        *error = @"Couldn't convert string to a valid NSCalendarDate object.";
        return NO;
    }

    return YES;
}

-(BOOL)isPartialStringValid:(NSString *)partialString
   newEditingString:(NSString **)newString errorDescription:(NSString **)error {
   return YES;
}

@end



@interface NSMutableString(NSDateFormatterExtensions)

-(void)__appendCharacter:(unichar)character;

-(void)__appendLocale:(NSDictionary *)locale key:(NSString *)key
              index:(NSInteger)index;

@end

@implementation NSMutableString(NSDateFormatterExtensions)

// blek, get rid of these
-(void)__appendCharacter:(unichar)character {
    [self appendString:[NSString stringWithCharacters:&character length:1]];
}

-(void)__appendLocale:(NSDictionary *)locale key:(NSString *)key
              index:(NSInteger)index {
    NSArray *array=[locale objectForKey:key];

    if(array!=nil)
        [self appendString:[array objectAtIndex:index]];
}

@end

NSTimeInterval NSMoveIntervalFromTimeZoneToGMT(NSTimeInterval interval,
                                                NSTimeZone *timeZone) {
    int offset = [timeZone secondsFromGMTForDate:[NSDate dateWithTimeIntervalSinceReferenceDate:interval]];
    return interval - (double)offset;
}

NSTimeInterval NSMoveIntervalFromGMTToTimeZone(NSTimeInterval interval, NSTimeZone *timeZone) {
	int offset = [timeZone secondsFromGMTForDate:[NSDate dateWithTimeIntervalSinceReferenceDate:interval]]; 
    return interval + (double)offset;
}

#define NSDaysOfCommonEraOfReferenceDate	730486

// thirty days hath september, april, june, and november.
// all the rest have thirty-one, except February, which is borked.
NSInteger NSNumberOfDaysInMonthOfYear(NSInteger month, NSInteger year) {
    switch (month) {
        case 2:
            if (((year % 4) == 0 && (year % 100) != 0) || (year % 400) == 0)
                return 29;
            else
                return 28;
        case 4:
        case 6:
        case 9:
        case 11:
            return 30;
        default:
            return 31;
    }
}

static inline NSInteger numberOfDaysInCommonEraOfDayMonthAndYear(NSInteger day, NSInteger month, NSInteger year) {
    NSInteger result = 0;

    for (month--; month > 0; month--)
        result += NSNumberOfDaysInMonthOfYear(month, year);

    result += 365 * (year-1);
    result += (year - 1)/4;
    result -= (year - 1)/100;
    result += (year - 1)/400;

    // wtf, i tried this using day as the result variable and it started from zero
    result += day;

    return result;
}

NSTimeInterval NSTimeIntervalWithComponents(NSInteger year, NSInteger month, NSInteger day, NSInteger hour, NSInteger minute, NSInteger second, NSInteger milliseconds) {
    NSInteger daysOfCommonEra;
    NSTimeInterval interval;

    daysOfCommonEra = numberOfDaysInCommonEraOfDayMonthAndYear(day, month, year);
    daysOfCommonEra -= NSDaysOfCommonEraOfReferenceDate;

    interval = (daysOfCommonEra * 86400.0) + (hour * 3600) + (minute * 60) + second;
    
    if (milliseconds) {
        interval += milliseconds/1000.0 + 0.0001;
    }

    return interval;
}

NSInteger NSDayOfCommonEraFromTimeInterval(NSTimeInterval interval) {
    return (interval/86400.0) + NSDaysOfCommonEraOfReferenceDate;
}

NSInteger NSYearFromTimeInterval(NSTimeInterval interval) {
    NSInteger days = NSDayOfCommonEraFromTimeInterval(interval);
    NSInteger year = days/366;

    while (days >= numberOfDaysInCommonEraOfDayMonthAndYear(1, 1, year+1))
        year++;

    return year;
}

NSInteger NSDayOfYearFromTimeInterval(NSTimeInterval interval){ // 1-366
    NSInteger year = NSYearFromTimeInterval(interval);
    NSInteger result;

    result = NSDayOfCommonEraFromTimeInterval(interval) - numberOfDaysInCommonEraOfDayMonthAndYear(1, 1, year) + 1;
    if(result == 0) { result = 366; };

    return result;
}

NSInteger NSMonthFromTimeInterval(NSTimeInterval interval){ // 1-12
    NSInteger year = NSYearFromTimeInterval(interval);
    NSInteger days = NSDayOfCommonEraFromTimeInterval(interval);
    NSInteger month = 1;

    while (days > numberOfDaysInCommonEraOfDayMonthAndYear(NSNumberOfDaysInMonthOfYear(month, year), month, year))
        month++;

    return month;
}

NSInteger NSDayOfMonthFromTimeInterval(NSTimeInterval interval){ // 1-31
    NSInteger dayOfCommonEra = NSDayOfCommonEraFromTimeInterval(interval);
    NSInteger year = NSYearFromTimeInterval(interval);
    NSInteger month = NSMonthFromTimeInterval(interval);

    dayOfCommonEra -= numberOfDaysInCommonEraOfDayMonthAndYear(1, month, year) - 1;

    return dayOfCommonEra;
}

NSInteger NSWeekdayFromTimeInterval(NSTimeInterval interval){ // 1-7
    NSInteger weekday = NSDayOfCommonEraFromTimeInterval(interval);

    weekday = weekday % 7;
    if (weekday < 0)
        weekday += 7;

    return weekday;
}

NSInteger NS24HourFromTimeInterval(NSTimeInterval interval){ // 0-23
    NSTimeInterval hour = NSDayOfCommonEraFromTimeInterval(interval);

    hour -= NSDaysOfCommonEraOfReferenceDate;
    hour *= 86400.0;
    hour -= interval;
    hour /= 3600;
    hour = fabs(hour);

    if (hour == 24)
        hour = 0;

    return hour;
}

NSInteger NS12HourFromTimeInterval(NSTimeInterval interval){ // 1-12
    NSInteger hour = NS24HourFromTimeInterval(interval)%12;
    if (hour == 0)
        hour = 12;
    return hour;
}

NSInteger NSAMPMFromTimeInterval(NSTimeInterval interval){ // 0-1
    NSInteger hour=NS24HourFromTimeInterval(interval);

    return (hour < 12) ? 0 : 1;
}

NSInteger NSMinuteFromTimeInterval(NSTimeInterval interval){ // 0-59
    NSTimeInterval startOfHour = NSTimeIntervalWithComponents(NSYearFromTimeInterval(interval),
                                                              NSMonthFromTimeInterval(interval),
                                                              NSDayOfMonthFromTimeInterval(interval),
                                                              NS24HourFromTimeInterval(interval), 0, 0, 0);

    return (NSInteger)(interval - startOfHour)/60;
}

NSInteger NSSecondFromTimeInterval(NSTimeInterval interval){ // 0-59
    NSInteger seconds = fmod(floor(interval),60);
    if (seconds < 0)
        seconds = (60 + seconds);

    return seconds;
}

NSInteger NSMillisecondsFromTimeInterval(NSTimeInterval interval){ // 0-999
    NSInteger milli = fmod(interval*1000,1000);
    if (milli < 0) {
        milli = (1000 + milli);
    }
    return milli;
}

NSString *NSStringWithDateFormatLocale(NSTimeInterval interval,NSString *format,NSDictionary *locale,NSTimeZone *timeZone) {
    NSUInteger         pos,fmtLength=[format length];
    unichar          fmtBuffer[fmtLength],unicode;
    NSMutableString *result=[NSMutableString stringWithCapacity:fmtLength];

    unichar fillChar='0';
    BOOL    suppressZero=NO;

    enum {
        STATE_SCANNING,
        STATE_PERCENT,
        STATE_CONVERSION
    } state=STATE_SCANNING;

#if defined(__MINKAR__) // our clock is UTC as is good and proper
    interval=NSMoveIntervalFromGMTToTimeZone(interval,timeZone);
#else
    interval=NSMoveIntervalFromTimeZoneToGMT(interval,timeZone);
#endif
    if (locale == nil)
        locale = [[NSUserDefaults standardUserDefaults] dictionaryRepresentation];

    [format getCharacters:fmtBuffer];

    for(pos=0;pos<fmtLength;pos++){
        unicode=fmtBuffer[pos];

        switch(state){

            case STATE_SCANNING:
                if(unicode!='%')
                    [result __appendCharacter:unicode];
                else{
                    fillChar='0';
                    suppressZero=NO;
                    state=STATE_PERCENT;
                }
                    break;

            case STATE_PERCENT:
                switch(unicode){

                    case '.': suppressZero=YES; break;
                    case '_': fillChar=' '; break;
                    case ' ': fillChar=' '; break;

                    default:
                        pos--;
                        state=STATE_CONVERSION;
                        break;
                }
                break;

            case STATE_CONVERSION:
                switch(unicode){

                    case '%':
                        [result __appendCharacter:'%'];
                        break;

                    case 'a':
                        [result __appendLocale:locale key:NSShortWeekDayNameArray
                                       index:NSWeekdayFromTimeInterval(interval)];
                        break;

                    case 'A':
                        [result __appendLocale:locale key:NSWeekDayNameArray
                                       index:NSWeekdayFromTimeInterval(interval)];
                        break;

                    case 'b':
                        [result __appendLocale:locale key:NSShortMonthNameArray
                                       index:NSMonthFromTimeInterval(interval)-1];
                        break;

                    case 'B':
                        [result __appendLocale:locale key:NSMonthNameArray
                                       index:NSMonthFromTimeInterval(interval)-1];
                        break;

                    case 'c':
                        [result appendFormat:@"%@", NSStringWithDateFormatLocale(
                            NSMoveIntervalFromTimeZoneToGMT(interval,timeZone),
                            [locale objectForKey:NSTimeDateFormatString],locale,timeZone)];
                        break;

                    case 'd':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%2d":@"%02d");

                        [result appendFormat:fmt,NSDayOfMonthFromTimeInterval(interval)];
                    }
                        break;

                    case 'D':
                        [result appendFormat:@"%@", NSStringWithDateFormatLocale(
                            NSMoveIntervalFromTimeZoneToGMT(interval,timeZone),
                            @"%m/%d/%y",locale,timeZone)];
                        break;
                    case 'e':{
                        id fmt=@"%d";
                        [result appendFormat:fmt,NSDayOfMonthFromTimeInterval(interval)];
                    }
                        break;

                   case 'F':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%3d":@"%03d");

                        [result appendFormat:fmt,NSMillisecondsFromTimeInterval(interval)];
                    }
                        break;

                    case 'H':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%2d":@"%02d");

                        [result appendFormat:fmt,NS24HourFromTimeInterval(interval)];
                    }
                        break;

                    case 'I':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%2d":@"%02d");

                        [result appendFormat:fmt,NS12HourFromTimeInterval(interval)];
                    }
                        break;

                    case 'j':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%3d":@"%03d");

                        [result appendFormat:fmt,NSDayOfYearFromTimeInterval(interval)];
                    }
                        break;

                    case 'm':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%2d":@"%02d");

                        [result appendFormat:fmt,NSMonthFromTimeInterval(interval)];
                    }
                        break;

                    case 'M':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%2d":@"%02d");

                        [result appendFormat:fmt,NSMinuteFromTimeInterval(interval)];
                    }
                        break;

                    case 'p':
                        [result __appendLocale:locale key:NSAMPMDesignation
                                       index:NSAMPMFromTimeInterval(interval)];
                        break;
                    case 'r':
                        [result appendFormat:@"%@", NSStringWithDateFormatLocale(
                            NSMoveIntervalFromTimeZoneToGMT(interval,timeZone),
                            @"%I:%M:%S %p",locale,timeZone)];
                        break;
                    case 'R':
                        [result appendFormat:@"%@", NSStringWithDateFormatLocale(
                            NSMoveIntervalFromTimeZoneToGMT(interval,timeZone)
                            ,@"%H:%M",locale,timeZone)];
                        break;

                    case 'S':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%2d":@"%02d");

                        [result appendFormat:fmt,NSSecondFromTimeInterval(interval)];
                    }
                        break;
                    case 'T':
                        [result appendFormat:@"%@", NSStringWithDateFormatLocale(
                            NSMoveIntervalFromTimeZoneToGMT(interval,timeZone),
                            @"%H:%M:%S",locale,timeZone)];
                        break;

                    case 'w':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%1d":@"%01d");

                        [result appendFormat:fmt,NSWeekdayFromTimeInterval(interval)];
                    }
                        break;

                    case 'x':
                        [result appendFormat:@"%@", NSStringWithDateFormatLocale(
                            NSMoveIntervalFromTimeZoneToGMT(interval,timeZone),
                            [locale objectForKey:NSDateFormatString],locale,timeZone)];
                        break;

                    case 'X':
                        [result appendFormat:@"%@", NSStringWithDateFormatLocale(
                            NSMoveIntervalFromTimeZoneToGMT(interval,timeZone),
                            [locale objectForKey:NSTimeFormatString],locale,timeZone)];
                        break;

                    case 'y':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%2d":@"%02d");

                        [result appendFormat:fmt,NSYearFromTimeInterval(interval)%100];
                    }
                        break;

                    case 'Y':{
                        id fmt=(suppressZero)?@"%d":((fillChar==' ')?@"%4d":@"%04d");

                        [result appendFormat:fmt,NSYearFromTimeInterval(interval)];
                    }
                        break;

                    case 'Z':
                        [result appendString:[timeZone name]];
                        break;

                    case 'z': {
                        NSInteger  secondsFromGMT = [timeZone secondsFromGMTForDate:[NSDate dateWithTimeIntervalSinceReferenceDate:interval]];
                        [result appendString:[[[NSString allocWithZone:NULL] initWithFormat:@"%@%02d%02d",
                                                                          (secondsFromGMT >= 0 ? @"+" : @""),
                                                                          secondsFromGMT/3600,
                                                                          (secondsFromGMT % 3600)/60] autorelease]];
                    }
                        break;
                }
                state=STATE_SCANNING;
                break;
        }

    }

    return result;
}

NSString *NSReadStringInString(NSString *aString, NSCharacterSet *characterSet, NSUInteger position, NSUInteger maxLength, NSUInteger *endPosition)
{
    NSMutableString *resultString = [NSMutableString stringWithCapacity:maxLength];

    for(NSUInteger i = 0; i < maxLength; i++) {
        if(position+i >= [aString length]) {
            break;
        }

        unichar c = [aString characterAtIndex:position+i];
        if([characterSet characterIsMember:c] == NO) {
            break;
        }
        [resultString appendFormat:@"%c", c];
    }
    *endPosition = position + [resultString length];
    return resultString;
}

NSInteger NSReadIntegerInString(NSString *aString, NSCharacterSet *characterSet, NSUInteger position, NSUInteger maxLength, NSUInteger *endPosition)
{
    NSString    *str;
    unichar     firstChar = [aString characterAtIndex:position];
    BOOL        negate = NO;
    NSInteger   i;

    if (firstChar == '+') {
        position++;
    }
    else  if (firstChar == '-') {
        position++;
        negate = YES;
    }

    str = NSReadStringInString(aString, characterSet, position, maxLength, endPosition);
    i = [str integerValue];

    if (negate == YES) {
        i *= -1;
    }

    return i;
}


// might as well use the same code since they're the exact same formatting specifiers
// ok. we need at minimum the year. everything else is optional.
// weekday information is useless.
NSDate *NSDateWithStringDateFormatLocale(NSString *string, NSString *format, NSDictionary *locale, NSTimeZone *timeZone) {
    NSScanner       *scanner = [NSScanner scannerWithString:string];
    NSUInteger         currentPosition,fmtLength=[format length];
    unichar          fmtBuffer[fmtLength],unicode;
    NSInteger		     years = NSNotFound, months = NSNotFound, days = NSNotFound, hours = NSNotFound, minutes = NSNotFound, seconds = NSNotFound, milliseconds = NSNotFound;
    NSInteger		     AMPMMultiplier = 0;
    NSTimeInterval   adjustment = 0;
    NSArray	    *monthNames, *shortMonthNames, *AMPMDesignations;
    NSTimeInterval   timeInterval;
    NSDate  *date;
    
    [scanner setCharactersToBeSkipped:nil];

    enum {
        STATE_SCANNING,
        STATE_PERCENT,
        STATE_CONVERSION
    } state = STATE_SCANNING;

    if ([string length] == 0) {
        return nil;
    }

    if (locale == nil) {
        locale = [[NSUserDefaults standardUserDefaults] dictionaryRepresentation];
    }

    monthNames = [locale objectForKey:NSMonthNameArray];
    shortMonthNames = [locale objectForKey:NSShortMonthNameArray];
    AMPMDesignations = [locale objectForKey:NSAMPMDesignation];

    // although we don't use the weekday arrays for anything, the spec
    // says to check them anyway.
    if ([monthNames count] > 12 || [shortMonthNames count] > 12 ||
            [[locale objectForKey:NSShortWeekDayNameArray] count] > 7 ||
            [[locale objectForKey:NSWeekDayNameArray] count] > 7) {
        return nil;
    }

    [format getCharacters:fmtBuffer];

    for (currentPosition = 0; currentPosition < fmtLength; currentPosition++) {
        unicode = fmtBuffer[currentPosition];

        switch (state) {
            case STATE_SCANNING:
                if (unicode == '%') {
                    state = STATE_PERCENT;
                } else {
                    currentPosition++;
                }
                break;

            case STATE_PERCENT:
                switch (unicode) {
                    case '.':
                    case ' ':
                    default:
                        currentPosition--;
                        state = STATE_CONVERSION;
                        break;
                }
                break;

            case STATE_CONVERSION:
                switch (unicode) {
                    case '%':
                        currentPosition++;
                        break;

                        // can't really do anything with the day of the week, but we have to skip it.
                    case 'a':
                        NSReadStringInString(string, [NSCharacterSet letterCharacterSet], currentPosition, 255, &currentPosition);
                        break;
                    case 'A':
                        NSReadStringInString(string, [NSCharacterSet letterCharacterSet], currentPosition, 255, &currentPosition);
                        break;

                        // month or its abbreviation. look it up in the arrays..
                    case 'b': {
                        NSString *temp;
                        NSEnumerator *enumerator = [shortMonthNames objectEnumerator];
                        NSString *shortMonthName;
                        months = NSNotFound;
                        int month = 1;

                        temp = NSReadStringInString(string, [NSCharacterSet letterCharacterSet], currentPosition, 255, &currentPosition);

                        while ((shortMonthName = [enumerator nextObject]) != nil) {
                            if ([shortMonthName caseInsensitiveCompare:temp] == NSOrderedSame) {
                                months = month;
                                break;
                            } else {
                                month++;
                            }
                        }

                        //month not found
                        if (months == NSNotFound) {
                            return nil;
                        }
                        break;
                    }

                    case 'B': {
                        NSString *temp;
                        NSEnumerator *enumerator = [monthNames objectEnumerator];
                        NSString *monthName;
                        months = NSNotFound;
                        int month = 1;

                        temp = NSReadStringInString(string, [NSCharacterSet letterCharacterSet], currentPosition, 255, &currentPosition);

                        while ((monthName = [enumerator nextObject]) != nil) {
                            if ([monthName caseInsensitiveCompare:temp] == NSOrderedSame) {
                                months = month;
                                break;
                            } else {
                                month++;
                            }
                        }

                        //month not found
                        if (months == NSNotFound) {
                            return nil;
                        }
                        break;
                    }

                    case 'c':
                        return NSDateWithStringDateFormatLocale(string, [locale objectForKey:NSTimeDateFormatString], locale, timeZone);

                    case 'd':
                        days = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 2, &currentPosition);
                        break;

                    case 'F':
                        milliseconds = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 3, &currentPosition);
                        break;

                    case 'H':
                        hours = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 2, &currentPosition);
                        break;

                    case 'I':
                        hours = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 2, &currentPosition);

                        AMPMMultiplier = 1;
                        break;

                    // grr
                    case 'j': {
                        NSInteger numberOfDays = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 3, &currentPosition);

                        adjustment += numberOfDays * 86400.0;
                        break;
                    }

                    case 'm':
                        months = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 2, &currentPosition);

                        break;

                    case 'M':
                        minutes = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 2, &currentPosition);
                        break;

                    case 'p': {
                        NSString *temp;

                        temp = NSReadStringInString(string, [NSCharacterSet letterCharacterSet], currentPosition, 255, &currentPosition);

                        AMPMMultiplier = [AMPMDesignations indexOfObject:temp];
                        if (AMPMMultiplier == NSNotFound) {
                            return nil;
                        }
                        AMPMMultiplier++; // e.g. 0 = 1, 1 = 2...
                        break;
                    }

                    case 'S':
                        seconds = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 2, &currentPosition);
                        break;

                    // again, weekdays are useless
                    case 'w': {
                        NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 1, &currentPosition);
                        break;
                    }

                    case 'x':
                        return NSDateWithStringDateFormatLocale(string,[locale objectForKey:NSDateFormatString],locale, timeZone);

                    case 'X':
                        return NSDateWithStringDateFormatLocale(string,[locale objectForKey:NSTimeFormatString],locale, timeZone);

                    case 'y':
                        years = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 2, &currentPosition);

// FIX QUESTIONABLE
// 1900 or 2000??, YB does 2000, for some? all?
                        years += 2000;
                        break;

                    case 'Y':
                        years = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 4, &currentPosition);
                        /*if (![scanner scanInteger:&years])
                            return nil;*/
                        break;

                    case 'Z': {
                        NSString *temp;

                        temp = NSReadStringInString(string, [NSCharacterSet letterCharacterSet], currentPosition, 255, &currentPosition);

                        timeZone = [NSTimeZone timeZoneWithName:temp];
                        break;
                    }

                    case 'z': {
                        NSInteger hoursMinutes, tzHours, tzMinutes;
                        hoursMinutes = NSReadIntegerInString(string, [NSCharacterSet decimalDigitCharacterSet], currentPosition, 4, &currentPosition);

                        tzHours = hoursMinutes / 100;
                        tzMinutes = hoursMinutes % 100;
                        timeZone = [NSTimeZone timeZoneForSecondsFromGMT:(tzHours * 3600) + (tzMinutes * 60)];
                        break;
                    }
                }
                state = STATE_SCANNING;
                break;
        }
    }

    // now that we've got whatever information we can get from the string,
    // try to make an NSCalendarDate of it.
    if (AMPMMultiplier != 0 && hours != NSNotFound) {
        hours *= AMPMMultiplier;
    }

    // maybe we've been given the number of days in the year but not the month/day
    if (months == NSNotFound && days == NSNotFound) {
        months = 1;
        days = 1;
    }

    // if no year, then this year
    if (years == NSNotFound) {
        years = [[NSCalendarDate date] yearOfCommonEra];
    }

    if (hours == NSNotFound) {
        hours = 0;
    }
    if (minutes == NSNotFound) {
        minutes = 0;
    }
    if (seconds == NSNotFound) {
        seconds = 0;
    }
    if (milliseconds == NSNotFound) {
        milliseconds = 0;
    }

    if (timeZone == nil) {
        timeZone = [NSTimeZone defaultTimeZone];
    }

    timeInterval = NSTimeIntervalWithComponents(years, months, days, hours, minutes, seconds, milliseconds);
    timeInterval += adjustment;

    timeInterval = timeInterval - [timeZone secondsFromGMTForDate:[NSDate dateWithTimeIntervalSinceReferenceDate:timeInterval]];

    date = [[[NSDate allocWithZone:NULL] initWithTimeIntervalSinceReferenceDate:timeInterval] autorelease];
    return date;
}
