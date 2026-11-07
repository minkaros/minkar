/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>

@class NSArray<ObjectType>, NSDictionary<KeyType, ObjectType>, NSString, NSCharacterSet;

typedef NSString *NSLocaleKey NS_TYPED_ENUM;

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSLocaleKey const NSLocaleIdentifier;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleLanguageCode;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleCountryCode;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleScriptCode;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleVariantCode;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleExemplarCharacterSet;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleCalendar;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleCollationIdentifier;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleUsesMetricSystem;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleMeasurementSystem;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleDecimalSeparator;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleGroupingSeparator;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleCurrencySymbol;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleCurrencyCode;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleCollatorIdentifier;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleQuotationBeginDelimiterKey;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleQuotationEndDelimiterKey;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleAlternateQuotationBeginDelimiterKey;
FOUNDATION_EXPORT NSLocaleKey const NSLocaleAlternateQuotationEndDelimiterKey;

FOUNDATION_EXPORT NSString *const NSCurrentLocaleDidChangeNotification;

typedef NS_ENUM(NSUInteger, NSLocaleLanguageDirection) {
    NSLocaleLanguageDirectionUnknown = 0,
    NSLocaleLanguageDirectionLeftToRight = 1,
    NSLocaleLanguageDirectionRightToLeft = 2,
    NSLocaleLanguageDirectionTopToBottom = 3,
    NSLocaleLanguageDirectionBottomToTop = 4,
};

/* A locale: CoreFoundation's (CFLocale, on ICU) under the Cocoa API. */
@interface NSLocale : NSObject <NSCopying, NSSecureCoding> {
    void *_cf;
    NSString *_identifier;
}

- (nullable id)objectForKey:(NSLocaleKey)key;
- (nullable NSString *)displayNameForKey:(NSLocaleKey)key value:(id)value;

- (instancetype)initWithLocaleIdentifier:(NSString *)string NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
+ (instancetype)localeWithLocaleIdentifier:(NSString *)ident;
- (instancetype)init NS_UNAVAILABLE;

@property (class, readonly, strong) NSLocale *autoupdatingCurrentLocale;
@property (class, readonly, copy) NSLocale *currentLocale;
@property (class, readonly, copy) NSLocale *systemLocale;

@property (readonly, copy) NSString *localeIdentifier;
- (nullable NSString *)localizedStringForLocaleIdentifier:(NSString *)localeIdentifier;
@property (nullable, readonly, copy) NSString *languageCode;
- (nullable NSString *)localizedStringForLanguageCode:(NSString *)languageCode;
@property (nullable, readonly, copy) NSString *countryCode;
- (nullable NSString *)localizedStringForCountryCode:(NSString *)countryCode;
@property (nullable, readonly, copy) NSString *scriptCode;
- (nullable NSString *)localizedStringForScriptCode:(NSString *)scriptCode;
@property (nullable, readonly, copy) NSString *variantCode;
- (nullable NSString *)localizedStringForVariantCode:(NSString *)variantCode;
@property (readonly, copy) NSCharacterSet *exemplarCharacterSet;
@property (readonly, copy) NSString *calendarIdentifier;
- (nullable NSString *)localizedStringForCalendarIdentifier:(NSString *)calendarIdentifier;
@property (nullable, readonly, copy) NSString *collationIdentifier;
- (nullable NSString *)localizedStringForCollationIdentifier:(NSString *)collationIdentifier;
@property (readonly) BOOL usesMetricSystem;
@property (readonly, copy) NSString *decimalSeparator;
@property (readonly, copy) NSString *groupingSeparator;
@property (readonly, copy) NSString *currencySymbol;
@property (nullable, readonly, copy) NSString *currencyCode;
- (nullable NSString *)localizedStringForCurrencyCode:(NSString *)currencyCode;
@property (readonly, copy) NSString *collatorIdentifier;
- (nullable NSString *)localizedStringForCollatorIdentifier:(NSString *)collatorIdentifier;
@property (readonly, copy) NSString *quotationBeginDelimiter;
@property (readonly, copy) NSString *quotationEndDelimiter;
@property (readonly, copy) NSString *alternateQuotationBeginDelimiter;
@property (readonly, copy) NSString *alternateQuotationEndDelimiter;

@property (class, readonly, copy) NSArray<NSString *> *availableLocaleIdentifiers;
@property (class, readonly, copy) NSArray<NSString *> *ISOLanguageCodes;
@property (class, readonly, copy) NSArray<NSString *> *ISOCountryCodes;
@property (class, readonly, copy) NSArray<NSString *> *ISOCurrencyCodes;
@property (class, readonly, copy) NSArray<NSString *> *commonISOCurrencyCodes;
@property (class, readonly, copy) NSArray<NSString *> *preferredLanguages;

+ (NSDictionary<NSString *, NSString *> *)componentsFromLocaleIdentifier:(NSString *)string;
+ (NSString *)localeIdentifierFromComponents:(NSDictionary<NSString *, NSString *> *)dict;
+ (NSString *)canonicalLocaleIdentifierFromString:(NSString *)string;
+ (NSString *)canonicalLanguageIdentifierFromString:(NSString *)string;
+ (nullable NSString *)localeIdentifierFromWindowsLocaleCode:(uint32_t)lcid;
+ (uint32_t)windowsLocaleCodeFromLocaleIdentifier:(NSString *)localeIdentifier;
+ (NSLocaleLanguageDirection)characterDirectionForLanguage:(NSString *)isoLangCode;
+ (NSLocaleLanguageDirection)lineDirectionForLanguage:(NSString *)isoLangCode;

@end

NS_ASSUME_NONNULL_END
