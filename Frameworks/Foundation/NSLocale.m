/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSDictionary.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSString.h>
#import <Foundation/NSData.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSCalendar.h>
#import <CoreFoundation/CFLocale.h>
#import <CoreFoundation/CFCharacterSet.h>
#import <CoreFoundation/CFCalendar.h>

/* The keys are CoreFoundation's own strings, as on the reference
 * platform (NSLocaleCountryCode is kCFLocaleCountryCode). */
NSLocaleKey const NSLocaleIdentifier = @"kCFLocaleIdentifierKey";
NSLocaleKey const NSLocaleLanguageCode = @"kCFLocaleLanguageCodeKey";
NSLocaleKey const NSLocaleCountryCode = @"kCFLocaleCountryCodeKey";
NSLocaleKey const NSLocaleScriptCode = @"kCFLocaleScriptCodeKey";
NSLocaleKey const NSLocaleVariantCode = @"kCFLocaleVariantCodeKey";
NSLocaleKey const NSLocaleExemplarCharacterSet = @"kCFLocaleExemplarCharacterSetKey";
NSLocaleKey const NSLocaleCalendar = @"kCFLocaleCalendarKey";
NSLocaleKey const NSLocaleCollationIdentifier = @"collation";
NSLocaleKey const NSLocaleUsesMetricSystem = @"kCFLocaleUsesMetricSystemKey";
NSLocaleKey const NSLocaleMeasurementSystem = @"kCFLocaleMeasurementSystemKey";
NSLocaleKey const NSLocaleDecimalSeparator = @"kCFLocaleDecimalSeparatorKey";
NSLocaleKey const NSLocaleGroupingSeparator = @"kCFLocaleGroupingSeparatorKey";
NSLocaleKey const NSLocaleCurrencySymbol = @"kCFLocaleCurrencySymbolKey";
NSLocaleKey const NSLocaleCurrencyCode = @"currency";
NSLocaleKey const NSLocaleCollatorIdentifier = @"kCFLocaleCollatorIdentifierKey";
NSLocaleKey const NSLocaleQuotationBeginDelimiterKey = @"kCFLocaleQuotationBeginDelimiterKey";
NSLocaleKey const NSLocaleQuotationEndDelimiterKey = @"kCFLocaleQuotationEndDelimiterKey";
NSLocaleKey const NSLocaleAlternateQuotationBeginDelimiterKey = @"kCFLocaleAlternateQuotationBeginDelimiterKey";
NSLocaleKey const NSLocaleAlternateQuotationEndDelimiterKey = @"kCFLocaleAlternateQuotationEndDelimiterKey";

NSString * const NSCurrentLocaleDidChangeNotification = @"kCFLocaleCurrentLocaleDidChangeNotification";

#define CF(l) ((CFLocaleRef)(l)->_cf)

/* CF takes its own strings only. */
static CFStringRef copyCFString(NSString *string) {
    return string == nil ? NULL : CFStringCreateWithCString(NULL, [string UTF8String], kCFStringEncodingUTF8);
}

/* A CF string as a Foundation string of its own. */
static NSString *fromCFString(CFStringRef string) {
    if (string == NULL)
        return nil;
    CFIndex length = CFStringGetLength(string);
    unichar *buffer = malloc(sizeof(unichar) * (length + 1));
    CFStringGetCharacters(string, CFRangeMake(0, length), buffer);
    NSString *result = [NSString stringWithCharacters:buffer length:(NSUInteger)length];
    free(buffer);
    return result;
}

static NSArray *fromCFArrayOfStrings(CFArrayRef array) {
    NSMutableArray *result = [NSMutableArray array];
    if (array == NULL)
        return result;
    for (CFIndex i = 0; i < CFArrayGetCount(array); i++)
        [result addObject:fromCFString(CFArrayGetValueAtIndex(array, i))];
    return result;
}

/* The CF key a Foundation key stands for. */
static CFStringRef cfKey(NSString *key) {
    static const struct { NSString *const *ns; const CFStringRef *cf; } keys[] = {
        { &NSLocaleIdentifier, &kCFLocaleIdentifier }, { &NSLocaleLanguageCode, &kCFLocaleLanguageCode },
        { &NSLocaleCountryCode, &kCFLocaleCountryCode }, { &NSLocaleScriptCode, &kCFLocaleScriptCode },
        { &NSLocaleVariantCode, &kCFLocaleVariantCode }, { &NSLocaleExemplarCharacterSet, &kCFLocaleExemplarCharacterSet },
        { &NSLocaleCalendar, &kCFLocaleCalendar }, { &NSLocaleCollationIdentifier, &kCFLocaleCollationIdentifier },
        { &NSLocaleUsesMetricSystem, &kCFLocaleUsesMetricSystem }, { &NSLocaleMeasurementSystem, &kCFLocaleMeasurementSystem },
        { &NSLocaleDecimalSeparator, &kCFLocaleDecimalSeparator }, { &NSLocaleGroupingSeparator, &kCFLocaleGroupingSeparator },
        { &NSLocaleCurrencySymbol, &kCFLocaleCurrencySymbol }, { &NSLocaleCurrencyCode, &kCFLocaleCurrencyCode },
        { &NSLocaleCollatorIdentifier, &kCFLocaleCollatorIdentifier },
        { &NSLocaleQuotationBeginDelimiterKey, &kCFLocaleQuotationBeginDelimiterKey },
        { &NSLocaleQuotationEndDelimiterKey, &kCFLocaleQuotationEndDelimiterKey },
        { &NSLocaleAlternateQuotationBeginDelimiterKey, &kCFLocaleAlternateQuotationBeginDelimiterKey },
        { &NSLocaleAlternateQuotationEndDelimiterKey, &kCFLocaleAlternateQuotationEndDelimiterKey },
    };
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if ([key isEqualToString:*keys[i].ns])
            return *keys[i].cf;
    return NULL;
}

@implementation NSLocale

static NSLocale *systemLocale = nil;
static NSLocale *currentLocale = nil;

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (NSLocale *)systemLocale {
    @synchronized(self) {
        if (systemLocale == nil)
            systemLocale = [[NSLocale alloc] initWithLocaleIdentifier:@""];
    }
    return systemLocale;
}

+ (NSLocale *)currentLocale {
    @synchronized(self) {
        if (currentLocale == nil) {
            NSString *identifier = @"en_US";
            if ([self respondsToSelector:@selector(_platformCurrentLocaleIdentifier)])
                identifier = [self performSelector:@selector(_platformCurrentLocaleIdentifier)] ?: identifier;
            currentLocale = [[NSLocale alloc] initWithLocaleIdentifier:identifier];
        }
    }
    return currentLocale;
}

/* The current locale; the user's settings are read once (no change
 * notification yet). */
+ (NSLocale *)autoupdatingCurrentLocale {
    return [self currentLocale];
}

+ (instancetype)localeWithLocaleIdentifier:(NSString *)identifier {
    return [[[self alloc] initWithLocaleIdentifier:identifier] autorelease];
}

- (instancetype)init {
    return [self initWithLocaleIdentifier:@""];
}

- (instancetype)initWithLocaleIdentifier:(NSString *)identifier {
    if ((self = [super init]) == nil)
        return nil;
    if (identifier == nil)
        identifier = @"";
    CFStringRef cfIdentifier = copyCFString(identifier);
    _cf = (void *)CFLocaleCreate(NULL, cfIdentifier);
    CFRelease(cfIdentifier);
    if (_cf == NULL) {
        CFStringRef root = copyCFString(@"");
        _cf = (void *)CFLocaleCreate(NULL, root);
        CFRelease(root);
    }
    _identifier = [identifier copy];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSString *identifier = nil;
    if ([coder allowsKeyedCoding])
        identifier = [coder decodeObjectOfClass:[NSString class] forKey:@"NS.identifier"];
    return [self initWithLocaleIdentifier:identifier ?: @""];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    if ([coder allowsKeyedCoding])
        [coder encodeObject:_identifier forKey:@"NS.identifier"];
}

- (void)dealloc {
    if (_cf != NULL)
        CFRelease((CFTypeRef)_cf);
    [_identifier release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (BOOL)isEqual:(id)other {
    return other == self || ([other isKindOfClass:[NSLocale class]] && [_identifier isEqualToString:[other localeIdentifier]]);
}

- (NSUInteger)hash {
    return [_identifier hash];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ (fixed)", _identifier];
}

- (NSString *)localeIdentifier {
    return _identifier;
}

/* A value of the locale's, as a Foundation object. */
- (id)objectForKey:(NSLocaleKey)key {
    CFStringRef cf = cfKey(key);
    if (cf == NULL)
        return nil;
    CFTypeRef value = CFLocaleGetValue(CF(self), cf);
    if (value == NULL)
        return nil;
    CFTypeID type = CFGetTypeID(value);
    if (type == CFStringGetTypeID())
        return fromCFString(value);
    if (type == CFBooleanGetTypeID())
        return [NSNumber numberWithBool:CFBooleanGetValue(value)];
    if (type == CFCharacterSetGetTypeID()) {
        CFDataRef bitmap = CFCharacterSetCreateBitmapRepresentation(NULL, value);
        NSData *data = [NSData dataWithBytes:CFDataGetBytePtr(bitmap) length:(NSUInteger)CFDataGetLength(bitmap)];
        CFRelease(bitmap);
        return [NSCharacterSet characterSetWithBitmapRepresentation:data];
    }
    if (type == CFCalendarGetTypeID()) {
        NSString *identifier = fromCFString(CFCalendarGetIdentifier(value));
        NSCalendar *calendar = [NSCalendar calendarWithIdentifier:identifier];
        [calendar setLocale:self];
        return calendar;
    }
    return nil;
}

- (NSString *)displayNameForKey:(NSLocaleKey)key value:(id)value {
    CFStringRef cf = cfKey(key);
    if (cf == NULL || ![value isKindOfClass:[NSString class]])
        return nil;
    CFStringRef cfValue = copyCFString(value);
    CFStringRef name = CFLocaleCopyDisplayNameForPropertyValue(CF(self), cf, cfValue);
    CFRelease(cfValue);
    NSString *result = fromCFString(name);
    if (name != NULL)
        CFRelease(name);
    return result;
}

#define STRING(name, key) - (NSString *)name { return [self objectForKey:key]; }
STRING(languageCode, NSLocaleLanguageCode)
STRING(countryCode, NSLocaleCountryCode)
STRING(scriptCode, NSLocaleScriptCode)
STRING(variantCode, NSLocaleVariantCode)
STRING(collationIdentifier, NSLocaleCollationIdentifier)
STRING(currencyCode, NSLocaleCurrencyCode)
#undef STRING
#define STRING(name, key, fallback) - (NSString *)name { return [self objectForKey:key] ?: fallback; }
STRING(decimalSeparator, NSLocaleDecimalSeparator, @".")
STRING(groupingSeparator, NSLocaleGroupingSeparator, @",")
STRING(currencySymbol, NSLocaleCurrencySymbol, @"¤")
STRING(collatorIdentifier, NSLocaleCollatorIdentifier, _identifier)
STRING(quotationBeginDelimiter, NSLocaleQuotationBeginDelimiterKey, @"“")
STRING(quotationEndDelimiter, NSLocaleQuotationEndDelimiterKey, @"”")
STRING(alternateQuotationBeginDelimiter, NSLocaleAlternateQuotationBeginDelimiterKey, @"‘")
STRING(alternateQuotationEndDelimiter, NSLocaleAlternateQuotationEndDelimiterKey, @"’")
#undef STRING

- (NSCharacterSet *)exemplarCharacterSet {
    return [self objectForKey:NSLocaleExemplarCharacterSet] ?: [NSCharacterSet characterSetWithRange:NSMakeRange(0x20, 0x5f)];
}

- (NSString *)calendarIdentifier {
    return [[self objectForKey:NSLocaleCalendar] calendarIdentifier] ?: NSCalendarIdentifierGregorian;
}

- (BOOL)usesMetricSystem {
    return [[self objectForKey:NSLocaleUsesMetricSystem] boolValue];
}

#define LOCALIZED(name, key) - (NSString *)name:(NSString *)value { return [self displayNameForKey:key value:value]; }
LOCALIZED(localizedStringForLocaleIdentifier, NSLocaleIdentifier)
LOCALIZED(localizedStringForLanguageCode, NSLocaleLanguageCode)
LOCALIZED(localizedStringForCountryCode, NSLocaleCountryCode)
LOCALIZED(localizedStringForScriptCode, NSLocaleScriptCode)
LOCALIZED(localizedStringForVariantCode, NSLocaleVariantCode)
LOCALIZED(localizedStringForCalendarIdentifier, NSLocaleCalendar)
LOCALIZED(localizedStringForCollationIdentifier, NSLocaleCollationIdentifier)
LOCALIZED(localizedStringForCurrencyCode, NSLocaleCurrencyCode)
LOCALIZED(localizedStringForCollatorIdentifier, NSLocaleCollatorIdentifier)
#undef LOCALIZED

/* -- The class's lists and identifiers -- */

#define LIST(name, function) + (NSArray *)name { \
    CFArrayRef array = function(); \
    NSArray *result = fromCFArrayOfStrings(array); \
    if (array != NULL) CFRelease(array); \
    return result; }
LIST(availableLocaleIdentifiers, CFLocaleCopyAvailableLocaleIdentifiers)
LIST(ISOLanguageCodes, CFLocaleCopyISOLanguageCodes)
LIST(ISOCountryCodes, CFLocaleCopyISOCountryCodes)
LIST(ISOCurrencyCodes, CFLocaleCopyISOCurrencyCodes)
LIST(commonISOCurrencyCodes, CFLocaleCopyCommonISOCurrencyCodes)
#undef LIST

/* The user's languages: the current locale's first, then English. */
+ (NSArray *)preferredLanguages {
    NSString *language = [[self currentLocale] languageCode];
    NSString *country = [[self currentLocale] countryCode];
    NSMutableArray *result = [NSMutableArray array];
    if (language != nil)
        [result addObject:country != nil ? [NSString stringWithFormat:@"%@-%@", language, country] : language];
    if (![language isEqualToString:@"en"])
        [result addObject:@"en"];
    return result;
}

+ (NSDictionary *)componentsFromLocaleIdentifier:(NSString *)string {
    CFStringRef cf = copyCFString(string);
    CFDictionaryRef components = CFLocaleCreateComponentsFromLocaleIdentifier(NULL, cf);
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    CFRelease(cf);
    if (components == NULL)
        return result;
    CFIndex count = CFDictionaryGetCount(components);
    const void **keys = malloc(sizeof(void *) * count), **values = malloc(sizeof(void *) * count);
    CFDictionaryGetKeysAndValues(components, keys, values);
    for (CFIndex i = 0; i < count; i++)
        [result setObject:fromCFString(values[i]) forKey:fromCFString(keys[i])];
    free(keys);
    free(values);
    CFRelease(components);
    return result;
}

+ (NSString *)localeIdentifierFromComponents:(NSDictionary *)dict {
    CFMutableDictionaryRef components = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (NSString *key in dict) {
        CFStringRef k = copyCFString(key), v = copyCFString([[dict objectForKey:key] description]);
        CFDictionarySetValue(components, k, v);
        CFRelease(k);
        CFRelease(v);
    }
    CFStringRef identifier = CFLocaleCreateLocaleIdentifierFromComponents(NULL, components);
    CFRelease(components);
    NSString *result = fromCFString(identifier);
    if (identifier != NULL)
        CFRelease(identifier);
    return result ?: @"";
}

#define CANONICAL(name, function) + (NSString *)name:(NSString *)string { \
    CFStringRef cf = copyCFString(string); \
    CFStringRef canonical = function(NULL, cf); \
    NSString *result = fromCFString(canonical); \
    CFRelease(cf); \
    if (canonical != NULL) CFRelease(canonical); \
    return result ?: string; }
CANONICAL(canonicalLocaleIdentifierFromString, CFLocaleCreateCanonicalLocaleIdentifierFromString)
CANONICAL(canonicalLanguageIdentifierFromString, CFLocaleCreateCanonicalLanguageIdentifierFromString)
#undef CANONICAL

+ (NSString *)localeIdentifierFromWindowsLocaleCode:(uint32_t)lcid {
    CFStringRef identifier = CFLocaleCreateLocaleIdentifierFromWindowsLocaleCode(NULL, lcid);
    NSString *result = fromCFString(identifier);
    if (identifier != NULL)
        CFRelease(identifier);
    return result;
}

+ (uint32_t)windowsLocaleCodeFromLocaleIdentifier:(NSString *)localeIdentifier {
    CFStringRef cf = copyCFString(localeIdentifier);
    uint32_t code = CFLocaleGetWindowsLocaleCodeFromLocaleIdentifier(cf);
    CFRelease(cf);
    return code;
}

+ (NSLocaleLanguageDirection)characterDirectionForLanguage:(NSString *)isoLangCode {
    CFStringRef cf = copyCFString(isoLangCode);
    CFLocaleLanguageDirection direction = CFLocaleGetLanguageCharacterDirection(cf);
    CFRelease(cf);
    return (NSLocaleLanguageDirection)direction;
}

+ (NSLocaleLanguageDirection)lineDirectionForLanguage:(NSString *)isoLangCode {
    CFStringRef cf = copyCFString(isoLangCode);
    CFLocaleLanguageDirection direction = CFLocaleGetLanguageLineDirection(cf);
    CFRelease(cf);
    return (NSLocaleLanguageDirection)direction;
}

@end
