/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Names as text and back (API 19).
 */

#import <Foundation/NSFormatter.h>
#import <Foundation/NSPersonNameComponents.h>

@class NSAttributedString, NSString;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, NSPersonNameComponentsFormatterStyle) {
    NSPersonNameComponentsFormatterStyleDefault = 0,
    NSPersonNameComponentsFormatterStyleShort,
    NSPersonNameComponentsFormatterStyleMedium,
    NSPersonNameComponentsFormatterStyleLong,
    NSPersonNameComponentsFormatterStyleAbbreviated
};

typedef NS_OPTIONS(NSUInteger, NSPersonNameComponentsFormatterOptions) {
    NSPersonNameComponentsFormatterPhonetic = (1UL << 1)
};

@interface NSPersonNameComponentsFormatter : NSFormatter {
    NSPersonNameComponentsFormatterStyle _style;
    BOOL _phonetic;
}

@property NSPersonNameComponentsFormatterStyle style;
@property (getter=isPhonetic) BOOL phonetic;

+ (NSString *)localizedStringFromPersonNameComponents:(NSPersonNameComponents *)components style:(NSPersonNameComponentsFormatterStyle)nameFormatStyle options:(NSPersonNameComponentsFormatterOptions)nameOptions;
- (NSString *)stringFromPersonNameComponents:(NSPersonNameComponents *)components;
- (NSAttributedString *)annotatedStringFromPersonNameComponents:(NSPersonNameComponents *)components;
- (nullable NSPersonNameComponents *)personNameComponentsFromString:(NSString *)string;
- (BOOL)getObjectValue:(out id _Nullable * _Nullable)obj forString:(NSString *)string errorDescription:(out NSString * _Nullable * _Nullable)error;

@end

FOUNDATION_EXPORT NSString * const NSPersonNameComponentKey;
FOUNDATION_EXPORT NSString * const NSPersonNameComponentGivenName;
FOUNDATION_EXPORT NSString * const NSPersonNameComponentFamilyName;
FOUNDATION_EXPORT NSString * const NSPersonNameComponentMiddleName;
FOUNDATION_EXPORT NSString * const NSPersonNameComponentPrefix;
FOUNDATION_EXPORT NSString * const NSPersonNameComponentSuffix;
FOUNDATION_EXPORT NSString * const NSPersonNameComponentNickname;
FOUNDATION_EXPORT NSString * const NSPersonNameComponentDelimiter;

NS_ASSUME_NONNULL_END
