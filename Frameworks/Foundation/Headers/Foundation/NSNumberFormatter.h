/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSFormatter.h>
#import <CoreFoundation/CFNumberFormatter.h>

@class NSLocale, NSDecimalNumberHandler, NSAttributedString, NSNumber, NSError;
@class NSDictionary<KeyType, ObjectType>;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, NSNumberFormatterBehavior) {
    NSNumberFormatterBehaviorDefault = 0,
    NSNumberFormatterBehavior10_0 = 1000,
    NSNumberFormatterBehavior10_4 = 1040,
};

typedef NS_ENUM(NSUInteger, NSNumberFormatterStyle) {
    NSNumberFormatterNoStyle = kCFNumberFormatterNoStyle,
    NSNumberFormatterDecimalStyle = kCFNumberFormatterDecimalStyle,
    NSNumberFormatterCurrencyStyle = kCFNumberFormatterCurrencyStyle,
    NSNumberFormatterPercentStyle = kCFNumberFormatterPercentStyle,
    NSNumberFormatterScientificStyle = kCFNumberFormatterScientificStyle,
    NSNumberFormatterSpellOutStyle = kCFNumberFormatterSpellOutStyle,
    NSNumberFormatterOrdinalStyle = 6,
    NSNumberFormatterCurrencyISOCodeStyle = 8,
    NSNumberFormatterCurrencyPluralStyle = 9,
    NSNumberFormatterCurrencyAccountingStyle = 10,
};

typedef NS_ENUM(NSUInteger, NSNumberFormatterPadPosition) {
    NSNumberFormatterPadBeforePrefix = kCFNumberFormatterPadBeforePrefix,
    NSNumberFormatterPadAfterPrefix = kCFNumberFormatterPadAfterPrefix,
    NSNumberFormatterPadBeforeSuffix = kCFNumberFormatterPadBeforeSuffix,
    NSNumberFormatterPadAfterSuffix = kCFNumberFormatterPadAfterSuffix,
};

typedef NS_ENUM(NSUInteger, NSNumberFormatterRoundingMode) {
    NSNumberFormatterRoundCeiling = kCFNumberFormatterRoundCeiling,
    NSNumberFormatterRoundFloor = kCFNumberFormatterRoundFloor,
    NSNumberFormatterRoundDown = kCFNumberFormatterRoundDown,
    NSNumberFormatterRoundUp = kCFNumberFormatterRoundUp,
    NSNumberFormatterRoundHalfEven = kCFNumberFormatterRoundHalfEven,
    NSNumberFormatterRoundHalfDown = kCFNumberFormatterRoundHalfDown,
    NSNumberFormatterRoundHalfUp = kCFNumberFormatterRoundHalfUp,
};

@interface NSNumberFormatter : NSFormatter {
    NSNumberFormatterBehavior _behavior;
    // 10.4
    NSNumberFormatterStyle _numberStyle;
    NSUInteger _formatWidth;
    NSLocale *_locale;
    NSNumber *_multiplier;

    BOOL _allowsFloats;
    BOOL _alwaysShowsDecimalSeparator;
    BOOL _isLenient;
    BOOL _isPartialStringValidationEnabled;
    BOOL _generatesDecimalNumbers;
    BOOL _usesGroupingSeparator;
    BOOL _usesSignificantDigits;

    NSUInteger _minimumIntegerDigits;
    NSUInteger _minimumFractionDigits;
    NSUInteger _minimumSignificantDigits;
    NSUInteger _maximumIntegerDigits;
    BOOL _customMaximumFractionDigits;
    NSUInteger _maximumFractionDigits;
    NSUInteger _maximumSignificantDigits;

    NSNumber *_minimum;
    NSNumber *_maximum;

    NSString *_nilSymbol;
    NSString *_notANumberSymbol;
    NSString *_zeroSymbol;
    NSString *_plusSign;
    NSString *_minusSign;
    NSString *_negativePrefix;
    NSString *_negativeSuffix;
    NSString *_positivePrefix;
    NSString *_positiveSuffix;
    NSString *_negativeInfinitySymbol;
    NSString *_positiveInfinitySymbol;

    NSString *_decimalSeparator;
    NSString *_exponentSymbol;
    NSString *_currencyCode;
    NSString *_currencySymbol;
    NSString *_internationalCurrencySymbol;
    NSString *_currencyDecimalSeparator;
    NSString *_currencyGroupingSeparator;
    NSString *_groupingSeparator;
    NSUInteger _groupingSize;
    NSUInteger _secondaryGroupingSize;
    NSString *_paddingCharacter;
    NSNumberFormatterPadPosition _paddingPosition;
    NSString *_percentSymbol;
    NSString *_perMillSymbol;
    NSNumber *_roundingIncrement;
    NSNumberFormatterRoundingMode _roundingMode;
    NSString *_positiveFormat;
    NSString *_negativeFormat;
    NSDictionary *_textAttributesForPositiveValues;
    NSDictionary *_textAttributesForNegativeValues;
    NSDictionary *_textAttributesForNegativeInfinity;
    NSDictionary *_textAttributesForNil;
    NSDictionary *_textAttributesForNotANumber;
    NSDictionary *_textAttributesForPositiveInfinity;
    NSDictionary *_textAttributesForZero;

    // 10.0
    NSAttributedString *_attributedStringForNil;
    NSAttributedString *_attributedStringForNotANumber;
    NSAttributedString *_attributedStringForZero;
    NSDecimalNumberHandler *_roundingBehavior;
    NSString *_thousandSeparator;

    BOOL _hasThousandSeparators;
    BOOL _localizesFormat;
}

+ (NSNumberFormatterBehavior)defaultFormatterBehavior;
+ (void)setDefaultFormatterBehavior:(NSNumberFormatterBehavior)value;

+ (NSString *)localizedStringFromNumber:(NSNumber *)num numberStyle:(NSNumberFormatterStyle)nstyle;

- (nullable NSString *)stringFromNumber:(NSNumber *)number;
- (nullable NSNumber *)numberFromString:(NSString *)string;
- (BOOL)getObjectValue:(out id _Nullable * _Nullable)valuep forString:(NSString *)string range:(inout nullable NSRange *)rangep error:(out NSError **)errorp;

@property NSNumberFormatterBehavior formatterBehavior;
@property NSNumberFormatterStyle numberStyle;
@property (null_resettable, copy) NSLocale *locale;
@property BOOL generatesDecimalNumbers;

@property (null_resettable, copy) NSString *negativeFormat;
@property (nullable, copy) NSDictionary<NSString *, id> *textAttributesForNegativeValues;
@property (null_resettable, copy) NSString *positiveFormat;
@property (nullable, copy) NSDictionary<NSString *, id> *textAttributesForPositiveValues;
@property BOOL allowsFloats;
@property (null_resettable, copy) NSString *decimalSeparator;
@property BOOL alwaysShowsDecimalSeparator;
@property (null_resettable, copy) NSString *currencyDecimalSeparator;
@property BOOL usesGroupingSeparator;
@property (null_resettable, copy) NSString *groupingSeparator;

@property (nullable, copy) NSString *zeroSymbol;
@property (nullable, copy) NSDictionary<NSString *, id> *textAttributesForZero;
@property (copy) NSString *nilSymbol;
@property (nullable, copy) NSDictionary<NSString *, id> *textAttributesForNil;
@property (null_resettable, copy) NSString *notANumberSymbol;
@property (nullable, copy) NSDictionary<NSString *, id> *textAttributesForNotANumber;
@property (copy) NSString *positiveInfinitySymbol;
@property (nullable, copy) NSDictionary<NSString *, id> *textAttributesForPositiveInfinity;
@property (copy) NSString *negativeInfinitySymbol;
@property (nullable, copy) NSDictionary<NSString *, id> *textAttributesForNegativeInfinity;

@property (null_resettable, copy) NSString *positivePrefix;
@property (null_resettable, copy) NSString *positiveSuffix;
@property (null_resettable, copy) NSString *negativePrefix;
@property (null_resettable, copy) NSString *negativeSuffix;
@property (null_resettable, copy) NSString *currencyCode;
@property (null_resettable, copy) NSString *currencySymbol;
@property (null_resettable, copy) NSString *internationalCurrencySymbol;
@property (null_resettable, copy) NSString *percentSymbol;
@property (null_resettable, copy) NSString *perMillSymbol;
@property (null_resettable, copy) NSString *minusSign;
@property (null_resettable, copy) NSString *plusSign;
@property (null_resettable, copy) NSString *exponentSymbol;

@property NSUInteger groupingSize;
@property NSUInteger secondaryGroupingSize;
@property (nullable, copy) NSNumber *multiplier;
@property NSUInteger formatWidth;
@property (null_resettable, copy) NSString *paddingCharacter;
@property NSNumberFormatterPadPosition paddingPosition;
@property NSNumberFormatterRoundingMode roundingMode;
@property (null_resettable, copy) NSNumber *roundingIncrement;
@property NSUInteger minimumIntegerDigits;
@property NSUInteger maximumIntegerDigits;
@property NSUInteger minimumFractionDigits;
@property NSUInteger maximumFractionDigits;
@property (nullable, copy) NSNumber *minimum;
@property (nullable, copy) NSNumber *maximum;
@property (null_resettable, copy) NSString *currencyGroupingSeparator;
@property (getter=isLenient) BOOL lenient;
@property BOOL usesSignificantDigits;
@property NSUInteger minimumSignificantDigits;
@property NSUInteger maximumSignificantDigits;
@property (getter=isPartialStringValidationEnabled) BOOL partialStringValidationEnabled;

// 10.0 behavior
@property BOOL hasThousandSeparators;
@property (null_resettable, copy) NSString *thousandSeparator;
@property BOOL localizesFormat;
@property (copy) NSString *format;
@property (copy) NSAttributedString *attributedStringForZero;
@property (copy) NSAttributedString *attributedStringForNil;
@property (copy) NSAttributedString *attributedStringForNotANumber;
@property (strong) NSDecimalNumberHandler *roundingBehavior;

@end

NS_ASSUME_NONNULL_END

