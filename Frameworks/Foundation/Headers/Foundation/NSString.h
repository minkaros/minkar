/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <sys/types.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>
#import <Foundation/NSZone.h>

@class NSArray<ObjectType>, NSData, NSDictionary<KeyType, ObjectType>, NSCharacterSet, NSError, NSLocale, NSURL;

typedef unsigned short unichar;

typedef NSUInteger NSStringEncoding;
enum : NSStringEncoding {
    NSASCIIStringEncoding = 1,
    NSNEXTSTEPStringEncoding = 2,
    NSJapaneseEUCStringEncoding = 3,
    NSUTF8StringEncoding = 4,
    NSISOLatin1StringEncoding = 5,
    NSSymbolStringEncoding = 6,
    NSNonLossyASCIIStringEncoding = 7,
    NSShiftJISStringEncoding = 8,
    NSISOLatin2StringEncoding = 9,
    NSUnicodeStringEncoding = 10,
    NSUTF16StringEncoding = NSUnicodeStringEncoding,
    NSWindowsCP1251StringEncoding = 11,
    NSWindowsCP1252StringEncoding = 12,
    NSWindowsCP1253StringEncoding = 13,
    NSWindowsCP1254StringEncoding = 14,
    NSWindowsCP1250StringEncoding = 15,
    NSISO2022JPStringEncoding = 21,
    NSMacOSRomanStringEncoding = 30,
    NSProprietaryStringEncoding = 0x00010000,
    NSUTF16BigEndianStringEncoding = 0x90000100,
    NSUTF16LittleEndianStringEncoding = 0x94000100,
    NSUTF32StringEncoding = 0x8c000100,
    NSUTF32BigEndianStringEncoding = 0x98000100,
    NSUTF32LittleEndianStringEncoding = 0x9c000100,
};

typedef NS_OPTIONS(NSUInteger, NSStringCompareOptions) {
    NSCaseInsensitiveSearch = 0x01,
    NSLiteralSearch = 0x02,
    NSBackwardsSearch = 0x04,
    NSAnchoredSearch = 0x08,
    NSNumericSearch = 0x40,
    NSDiacriticInsensitiveSearch = 0x80,
    NSWidthInsensitiveSearch = 0x100,
    NSForcedOrderingSearch = 0x200,
    NSRegularExpressionSearch = 0x400,
};

typedef NS_OPTIONS(NSUInteger, NSStringEncodingConversionOptions) {
    NSStringEncodingConversionAllowLossy = 1,
    NSStringEncodingConversionExternalRepresentation = 2
};

FOUNDATION_EXPORT const NSUInteger NSMaximumStringLength;

typedef NS_OPTIONS(NSUInteger, NSStringEnumerationOptions) {
    NSStringEnumerationByLines = 0,
    NSStringEnumerationByParagraphs = 1,
    NSStringEnumerationByComposedCharacterSequences = 2,
    NSStringEnumerationByWords = 3,
    NSStringEnumerationBySentences = 4,
    NSStringEnumerationReverse = 1UL << 8,
    NSStringEnumerationSubstringNotRequired = 1UL << 9,
    NSStringEnumerationLocalized = 1UL << 10,
};

typedef NSString *NSStringTransform NS_TYPED_EXTENSIBLE_ENUM;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformLatinToKatakana;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformLatinToHiragana;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformLatinToHangul;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformLatinToArabic;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformLatinToHebrew;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformLatinToThai;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformLatinToCyrillic;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformLatinToGreek;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformToLatin;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformMandarinToLatin;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformHiraganaToKatakana;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformFullwidthToHalfwidth;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformToXMLHex;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformToUnicodeName;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformStripCombiningMarks;
FOUNDATION_EXPORT NSStringTransform const NSStringTransformStripDiacritics;

NS_ASSUME_NONNULL_BEGIN

@interface NSString : NSObject <NSCopying, NSMutableCopying, NSSecureCoding>

@property (class, readonly) const NSStringEncoding *availableStringEncodings;
+ (NSString *)localizedNameOfStringEncoding:(NSStringEncoding)encoding;

+ (instancetype)stringWithCharacters:(const unichar *)unicode length:(NSUInteger)length;
+ (instancetype)string;
+ (nullable instancetype)stringWithCString:(const char *)cString length:(NSUInteger)length;
+ (nullable instancetype)stringWithCString:(const char *)cString;
+ (instancetype)stringWithString:(NSString *)string;
+ (instancetype)stringWithFormat:(NSString *)format, ...;
+ (nullable instancetype)stringWithContentsOfFile:(NSString *)path;
+ (nullable instancetype)stringWithContentsOfFile:(NSString *)path encoding:(NSStringEncoding)encoding error:(NSError **)error;
+ (nullable instancetype)stringWithContentsOfFile:(NSString *)path usedEncoding:(nullable NSStringEncoding *)encoding error:(NSError **)error;
+ (nullable instancetype)stringWithContentsOfURL:(NSURL *)url encoding:(NSStringEncoding)encoding error:(NSError **)error;
+ (nullable instancetype)stringWithContentsOfURL:(NSURL *)url usedEncoding:(nullable NSStringEncoding *)encoding error:(NSError **)error;
+ (nullable instancetype)stringWithCString:(const char *)cString encoding:(NSStringEncoding)encoding;
+ (nullable instancetype)stringWithUTF8String:(const char *)utf8;

+ (instancetype)localizedStringWithFormat:(NSString *)format, ...;

- (instancetype)initWithCharactersNoCopy:(unichar *)unicode length:(NSUInteger)length freeWhenDone:(BOOL)freeWhenDone;
- (instancetype)initWithCharacters:(const unichar *)unicode length:(NSUInteger)length;
- (nonnull instancetype)init;

- initWithCStringNoCopy:(char *)cString length:(NSUInteger)length
             freeWhenDone:(BOOL)freeWhenDone;
- (nullable instancetype)initWithCString:(const char *)cString length:(NSUInteger)length;
- (nullable instancetype)initWithCString:(const char *)cString;
- (nullable instancetype)initWithCString:(const char *)cString encoding:(NSStringEncoding)encoding;

- (instancetype)initWithString:(NSString *)string;

- (instancetype)initWithFormat:(NSString *)format locale:(nullable id)locale arguments:(va_list)arguments;
- (instancetype)initWithFormat:(NSString *)format locale:(nullable id)locale, ...;
- (instancetype)initWithFormat:(NSString *)format arguments:(va_list)arguments;
- (instancetype)initWithFormat:(NSString *)format, ...;

- (nullable instancetype)initWithData:(NSData *)data encoding:(NSStringEncoding)encoding;
- (nullable instancetype)initWithUTF8String:(const char *)utf8;
- (nullable instancetype)initWithBytes:(const void *)bytes length:(NSUInteger)length encoding:(NSStringEncoding)encoding;
- (nullable instancetype)initWithBytesNoCopy:(void *)bytes length:(NSUInteger)length encoding:(NSStringEncoding)encoding freeWhenDone:(BOOL)freeWhenDone;

- (nullable instancetype)initWithContentsOfFile:(NSString *)path;
- (nullable instancetype)initWithContentsOfFile:(NSString *)path encoding:(NSStringEncoding)encoding error:(NSError **)error;
- (nullable instancetype)initWithContentsOfFile:(NSString *)path usedEncoding:(nullable NSStringEncoding *)encoding error:(NSError **)error;
- (nullable instancetype)initWithContentsOfURL:(NSURL *)url encoding:(NSStringEncoding)encoding error:(NSError **)error;
- (nullable instancetype)initWithContentsOfURL:(NSURL *)url usedEncoding:(nullable NSStringEncoding *)encoding error:(NSError **)error;

- (unichar)characterAtIndex:(NSUInteger)location;
@property (readonly) NSUInteger length;

- (void)getCharacters:(unichar *)buffer range:(NSRange)range;
- (void)getCharacters:(unichar *)buffer;

- (NSComparisonResult)compare:(NSString *)other options:(NSStringCompareOptions)options range:(NSRange)range locale:(nullable id)locale;
- (NSComparisonResult)compare:(NSString *)other options:(NSStringCompareOptions)options range:(NSRange)range;
- (NSComparisonResult)compare:(NSString *)other options:(NSStringCompareOptions)options;
- (NSComparisonResult)compare:(NSString *)other;
- (NSComparisonResult)caseInsensitiveCompare:(NSString *)other;
- (NSComparisonResult)localizedCompare:(NSString *)other;
- (NSComparisonResult)localizedCaseInsensitiveCompare:(NSString *)other;

- (BOOL)isEqualToString:(NSString *)string;

- (BOOL)hasPrefix:(NSString *)string;
- (BOOL)hasSuffix:(NSString *)string;
- (NSRange)rangeOfString:(NSString *)string options:(NSStringCompareOptions)options range:(NSRange)range locale:(nullable NSLocale *)locale;
- (NSRange)rangeOfString:(NSString *)string options:(NSStringCompareOptions)options range:(NSRange)range;
- (NSRange)rangeOfString:(NSString *)string options:(NSStringCompareOptions)options;
- (NSRange)rangeOfString:(NSString *)string;

- (NSRange)rangeOfCharacterFromSet:(NSCharacterSet *)set options:(NSStringCompareOptions)options range:(NSRange)range;
- (NSRange)rangeOfCharacterFromSet:(NSCharacterSet *)set options:(NSStringCompareOptions)options;
- (NSRange)rangeOfCharacterFromSet:(NSCharacterSet *)set;

- (void)getLineStart:(nullable NSUInteger *)startp end:(nullable NSUInteger *)endp contentsEnd:(nullable NSUInteger *)contentsEndp forRange:(NSRange)range;
- (NSRange)lineRangeForRange:(NSRange)range;

- (void)getParagraphStart:(nullable NSUInteger *)startp end:(nullable NSUInteger *)endp contentsEnd:(nullable NSUInteger *)contentsEndp forRange:(NSRange)range;
- (NSRange)paragraphRangeForRange:(NSRange)range;

- (NSString *)substringWithRange:(NSRange)range;
- (NSString *)substringFromIndex:(NSUInteger)location;
- (NSString *)substringToIndex:(NSUInteger)location;

@property (readonly) BOOL boolValue;
@property (readonly) int intValue;
@property (readonly) NSInteger integerValue;
@property (readonly) long long longLongValue;
@property (readonly) float floatValue;
@property (readonly) double doubleValue;

@property (readonly, copy) NSString *lowercaseString;
@property (readonly, copy) NSString *uppercaseString;
@property (readonly, copy) NSString *capitalizedString;

- (NSString *)stringByAppendingFormat:(NSString *)format, ...;
- (NSString *)stringByAppendingString:(NSString *)string;

- (NSArray<NSString *> *)componentsSeparatedByString:(NSString *)separator;
- (NSArray<NSString *> *)componentsSeparatedByCharactersInSet:(NSCharacterSet *)set;

- (NSString *)commonPrefixWithString:(NSString *)other options:(NSStringCompareOptions)options;
- (NSString *)stringByPaddingToLength:(NSUInteger)length withString:(NSString *)padding startingAtIndex:(NSUInteger)index;
- (NSString *)stringByReplacingCharactersInRange:(NSRange)range withString:(NSString *)substitute;
- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)original withString:(NSString *)substitute;
- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)original withString:(NSString *)substitute options:(NSStringCompareOptions)options range:(NSRange)range;

- (NSString *)stringByFoldingWithOptions:(NSStringCompareOptions)options locale:(nullable NSLocale *)locale;

- (NSRange)rangeOfComposedCharacterSequenceAtIndex:(NSUInteger)index;
- (NSRange)rangeOfComposedCharacterSequencesForRange:(NSRange)range;

@property (readonly, copy) NSString *precomposedStringWithCanonicalMapping;
@property (readonly, copy) NSString *decomposedStringWithCanonicalMapping;
@property (readonly, copy) NSString *precomposedStringWithCompatibilityMapping;
@property (readonly, copy) NSString *decomposedStringWithCompatibilityMapping;

- (id)propertyList;
- (nullable NSDictionary *)propertyListFromStringsFileFormat;

- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically;
- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically encoding:(NSStringEncoding)encoding error:(NSError **)error;
- (BOOL)writeToURL:(NSURL *)url atomically:(BOOL)atomically encoding:(NSStringEncoding)encoding error:(NSError **)error;

@property (readonly) NSStringEncoding fastestEncoding;
@property (readonly) NSStringEncoding smallestEncoding;

- (BOOL)canBeConvertedToEncoding:(NSStringEncoding)encoding;
- (NSUInteger)lengthOfBytesUsingEncoding:(NSStringEncoding)encoding;
- (NSUInteger)maximumLengthOfBytesUsingEncoding:(NSStringEncoding)encoding;

- (nullable NSData *)dataUsingEncoding:(NSStringEncoding)encoding
         allowLossyConversion:(BOOL)lossy;
- (nullable NSData *)dataUsingEncoding:(NSStringEncoding)encoding;

- (BOOL)getBytes:(nullable void *)buffer maxLength:(NSUInteger)maxLength usedLength:(nullable NSUInteger *)usedLength encoding:(NSStringEncoding)encoding options:(NSStringEncodingConversionOptions)options range:(NSRange)range remainingRange:(nullable NSRangePointer)remainingRange;

@property (nullable, readonly) const char *UTF8String NS_RETURNS_INNER_POINTER;

- (nullable NSString *)stringByReplacingPercentEscapesUsingEncoding:(NSStringEncoding)encoding;
- (nullable NSString *)stringByAddingPercentEscapesUsingEncoding:(NSStringEncoding)encoding;

- (NSString *)stringByTrimmingCharactersInSet:(NSCharacterSet *)set;

- (nullable const char *)cStringUsingEncoding:(NSStringEncoding)encoding;
- (BOOL)getCString:(char *)cString maxLength:(NSUInteger)maxLength encoding:(NSStringEncoding)encoding;

@property (readonly, copy) NSString *description;

@property (class, readonly) NSStringEncoding defaultCStringEncoding;

- (void)getCString:(char *)buffer maxLength:(NSUInteger)maxLength
             range:(NSRange)range
    remainingRange:(NSRange *)remainingRange;
- (void)getCString:(char *)buffer maxLength:(NSUInteger)maxLength;
- (void)getCString:(char *)buffer;

- (NSUInteger)cStringLength;
- (nullable const char *)cString;
- (nullable const char *)lossyCString;

@end

NS_ASSUME_NONNULL_END

@interface NSConstantString : NSString {
	const char *_bytes;
	unsigned long _length;
}
@end

@interface __builtin_NSString : NSString {
	const char *str;
	unsigned int length;
}
@end

@interface NSConstantString_tiny : NSString
@end

@class NSCharacterSet, NSLocale;

NS_ASSUME_NONNULL_BEGIN

// Methods of macOS 10.6 to 10.11 (NSStringModern.m).
@interface NSString (NSStringModern)

- (BOOL)containsString:(NSString *)str;
- (BOOL)localizedCaseInsensitiveContainsString:(NSString *)str;
- (BOOL)localizedStandardContainsString:(NSString *)str;
- (NSRange)localizedStandardRangeOfString:(NSString *)str;
- (NSComparisonResult)localizedStandardCompare:(NSString *)string;

- (void)enumerateLinesUsingBlock:(void (NS_NOESCAPE ^)(NSString *line, BOOL *stop))block;
- (void)enumerateSubstringsInRange:(NSRange)range options:(NSStringEnumerationOptions)opts
                        usingBlock:(void (NS_NOESCAPE ^)(NSString * _Nullable substring, NSRange substringRange,
                                             NSRange enclosingRange, BOOL *stop))block;

- (nullable NSString *)stringByAddingPercentEncodingWithAllowedCharacters:(NSCharacterSet *)allowedCharacters;
@property (nullable, readonly, copy) NSString *stringByRemovingPercentEncoding;

- (NSString *)uppercaseStringWithLocale:(nullable NSLocale *)locale;
- (NSString *)lowercaseStringWithLocale:(nullable NSLocale *)locale;
- (NSString *)capitalizedStringWithLocale:(nullable NSLocale *)locale;
@property (readonly, copy) NSString *localizedUppercaseString;
@property (readonly, copy) NSString *localizedLowercaseString;
@property (readonly, copy) NSString *localizedCapitalizedString;

- (nullable NSString *)stringByApplyingTransform:(NSStringTransform)transform reverse:(BOOL)reverse;

@end

NS_ASSUME_NONNULL_END


#import <Foundation/NSMutableString.h>
