/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// NSString methods of macOS 10.6 to 10.11 that were missing. Built with
// ARC. Word and sentence boundaries and transforms come from ICU (ubrk,
// utrans), which Foundation already links; the rest from NSString's own
// search and CoreFoundation's case mapping.

#import <Foundation/NSString.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSData.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSException.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSValue.h>
#import <CoreFoundation/CoreFoundation.h>

#include <unicode/ubrk.h>
#include <unicode/utrans.h>

// The transform names are ICU transform IDs; any other ICU ID works too.
NSStringTransform const NSStringTransformLatinToKatakana = @"Latin-Katakana";
NSStringTransform const NSStringTransformLatinToHiragana = @"Latin-Hiragana";
NSStringTransform const NSStringTransformLatinToHangul = @"Latin-Hangul";
NSStringTransform const NSStringTransformLatinToArabic = @"Latin-Arabic";
NSStringTransform const NSStringTransformLatinToHebrew = @"Latin-Hebrew";
NSStringTransform const NSStringTransformLatinToThai = @"Latin-Thai";
NSStringTransform const NSStringTransformLatinToCyrillic = @"Latin-Cyrillic";
NSStringTransform const NSStringTransformLatinToGreek = @"Latin-Greek";
NSStringTransform const NSStringTransformToLatin = @"Any-Latin";
NSStringTransform const NSStringTransformMandarinToLatin = @"Han-Latin";
NSStringTransform const NSStringTransformHiraganaToKatakana = @"Hiragana-Katakana";
NSStringTransform const NSStringTransformFullwidthToHalfwidth = @"Fullwidth-Halfwidth";
NSStringTransform const NSStringTransformToXMLHex = @"Any-Hex/XML";
NSStringTransform const NSStringTransformToUnicodeName = @"Any-Name";
NSStringTransform const NSStringTransformStripCombiningMarks = @"NFD; [:Mn:] Remove; NFC";
NSStringTransform const NSStringTransformStripDiacritics = @"NFD; [:Nonspacing Mark:] Remove; NFC";

static NSString *fromCF(CFStringRef s) {
    return (__bridge_transfer NSString *)s;
}

@implementation NSString (NSStringModern)

/* ---- Searching ----------------------------------------------------------- */

- (BOOL)containsString:(NSString *)str {
    return [self rangeOfString:str].location != NSNotFound;
}

- (BOOL)localizedCaseInsensitiveContainsString:(NSString *)str {
    return [self rangeOfString:str options:NSCaseInsensitiveSearch range:NSMakeRange(0, [self length])
                        locale:[NSLocale currentLocale]].location != NSNotFound;
}

- (NSRange)localizedStandardRangeOfString:(NSString *)str {
    return [self rangeOfString:str options:NSCaseInsensitiveSearch | NSDiacriticInsensitiveSearch
                         range:NSMakeRange(0, [self length]) locale:[NSLocale currentLocale]];
}

- (BOOL)localizedStandardContainsString:(NSString *)str {
    return [self localizedStandardRangeOfString:str].location != NSNotFound;
}

// As the Finder sorts: case-insensitive, numbers by value.
- (NSComparisonResult)localizedStandardCompare:(NSString *)string {
    return [self compare:string
                 options:NSCaseInsensitiveSearch | NSNumericSearch | NSWidthInsensitiveSearch | NSForcedOrderingSearch
                   range:NSMakeRange(0, [self length]) locale:[NSLocale currentLocale]];
}

/* ---- Enumerating --------------------------------------------------------- */

static BOOL isLineBreak(unichar c) {
    return c == '\n' || c == '\r' || c == 0x85 || c == 0x2028 || c == 0x2029;
}

static BOOL isParagraphBreak(unichar c) {
    return c == '\n' || c == '\r' || c == 0x85 || c == 0x2029;
}

// The ranges of the lines (or paragraphs) in range: each line without and
// with its terminator ("\r\n" is one).
static void splitLines(NSString *self, NSRange range, BOOL paragraphs, NSMutableArray *out) {
    NSUInteger end = NSMaxRange(range), start = range.location, i = start;

    while (i < end) {
        unichar c = [self characterAtIndex:i];
        if (paragraphs ? isParagraphBreak(c) : isLineBreak(c)) {
            NSUInteger after = i + 1;
            if (c == '\r' && after < end && [self characterAtIndex:after] == '\n')
                after++;
            [out addObject:[NSValue valueWithRange:NSMakeRange(start, i - start)]];
            [out addObject:[NSValue valueWithRange:NSMakeRange(start, after - start)]];
            start = i = after;
            continue;
        }
        i++;
    }
    if (start < end) {
        [out addObject:[NSValue valueWithRange:NSMakeRange(start, end - start)]];
        [out addObject:[NSValue valueWithRange:NSMakeRange(start, end - start)]];
    }
}

// Word or sentence ranges from ICU: substring and enclosing range (which
// takes the space up to the next unit).
static void splitICU(NSString *self, NSRange range, UBreakIteratorType type, NSMutableArray *out) {
    NSUInteger length = range.length;
    unichar   *chars = malloc((length + 1) * sizeof(unichar));
    UErrorCode status = U_ZERO_ERROR;
    NSString  *localeID = [[NSLocale currentLocale] localeIdentifier];
    UBreakIterator *bi;

    [self getCharacters:chars range:range];
    bi = ubrk_open(type, [localeID UTF8String], chars, (int32_t)length, &status);
    if (U_FAILURE(status) || bi == NULL) {
        free(chars);
        return;
    }
    int32_t start = ubrk_first(bi), next;
    NSMutableArray *units = [NSMutableArray array];
    while ((next = ubrk_next(bi)) != UBRK_DONE) {
        BOOL keep = YES;
        if (type == UBRK_WORD)
            keep = ubrk_getRuleStatus(bi) != UBRK_WORD_NONE;
        if (keep)
            [units addObject:[NSValue valueWithRange:NSMakeRange(range.location + (NSUInteger)start,
                                                                 (NSUInteger)(next - start))]];
        start = next;
    }
    ubrk_close(bi);
    free(chars);

    for (NSUInteger u = 0; u < [units count]; u++) {
        NSRange sub = [[units objectAtIndex:u] rangeValue];
        NSUInteger encEnd = (u + 1 < [units count]) ? [[units objectAtIndex:u + 1] rangeValue].location
                                                     : NSMaxRange(range);
        NSUInteger encStart = (u == 0) ? range.location : sub.location;
        if (type == UBRK_SENTENCE) {
            // A sentence's trailing spaces belong to it; the substring
            // leaves them out.
            NSUInteger subEnd = NSMaxRange(sub);
            while (subEnd > sub.location &&
                   [[NSCharacterSet whitespaceAndNewlineCharacterSet] characterIsMember:[self characterAtIndex:subEnd - 1]])
                subEnd--;
            encEnd = NSMaxRange(sub);
            sub.length = subEnd - sub.location;
        }
        [out addObject:[NSValue valueWithRange:sub]];
        [out addObject:[NSValue valueWithRange:NSMakeRange(encStart, encEnd - encStart)]];
    }
}

- (void)enumerateSubstringsInRange:(NSRange)range options:(NSStringEnumerationOptions)opts
                        usingBlock:(void (^)(NSString *, NSRange, NSRange, BOOL *))block {
    NSMutableArray *pairs = [NSMutableArray array];
    NSUInteger      kind = opts & 0xff;
    BOOL            stop = NO;

    if (NSMaxRange(range) > [self length])
        [NSException raise:NSRangeException format:@"-[NSString enumerateSubstringsInRange:...]: range beyond the string"];
    switch (kind) {
    case NSStringEnumerationByLines:
        splitLines(self, range, NO, pairs);
        break;
    case NSStringEnumerationByParagraphs:
        splitLines(self, range, YES, pairs);
        break;
    case NSStringEnumerationByComposedCharacterSequences:
        for (NSUInteger i = range.location; i < NSMaxRange(range);) {
            NSRange r = [self rangeOfComposedCharacterSequenceAtIndex:i];
            if (NSMaxRange(r) > NSMaxRange(range))
                r.length = NSMaxRange(range) - r.location;
            [pairs addObject:[NSValue valueWithRange:r]];
            [pairs addObject:[NSValue valueWithRange:r]];
            i = NSMaxRange(r);
        }
        break;
    case NSStringEnumerationByWords:
        splitICU(self, range, UBRK_WORD, pairs);
        break;
    case NSStringEnumerationBySentences:
        splitICU(self, range, UBRK_SENTENCE, pairs);
        break;
    default:
        return;
    }

    NSUInteger n = [pairs count] / 2;
    for (NSUInteger k = 0; k < n && !stop; k++) {
        NSUInteger i = (opts & NSStringEnumerationReverse) ? n - 1 - k : k;
        NSRange sub = [[pairs objectAtIndex:2 * i] rangeValue];
        NSRange enc = [[pairs objectAtIndex:2 * i + 1] rangeValue];
        NSString *text = (opts & NSStringEnumerationSubstringNotRequired) ? nil : [self substringWithRange:sub];
        block(text, sub, enc, &stop);
    }
}

- (void)enumerateLinesUsingBlock:(void (^)(NSString *, BOOL *))block {
    [self enumerateSubstringsInRange:NSMakeRange(0, [self length]) options:NSStringEnumerationByLines
                          usingBlock:^(NSString *line, NSRange r, NSRange e, BOOL *stop) {
        block(line, stop);
    }];
}

/* ---- Percent encoding ---------------------------------------------------- */

- (NSString *)stringByAddingPercentEncodingWithAllowedCharacters:(NSCharacterSet *)allowed {
    NSData          *utf8 = [self dataUsingEncoding:NSUTF8StringEncoding];
    const uint8_t   *b = [utf8 bytes];
    NSMutableString *out = [NSMutableString stringWithCapacity:[utf8 length]];

    if (utf8 == nil)
        return nil;
    for (NSUInteger i = 0; i < [utf8 length]; i++) {
        if (b[i] < 0x80 && [allowed characterIsMember:b[i]])
            [out appendFormat:@"%c", b[i]];
        else
            [out appendFormat:@"%%%02X", b[i]];
    }
    return [out copy];
}

static int hexDigit(unichar c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// nil when an escape is malformed or the bytes are not UTF-8, as on macOS.
- (NSString *)stringByRemovingPercentEncoding {
    NSUInteger     length = [self length];
    NSMutableData *bytes = [NSMutableData dataWithCapacity:length];
    NSData        *utf8 = [self dataUsingEncoding:NSUTF8StringEncoding];
    const uint8_t *s = [utf8 bytes];
    NSUInteger     n = [utf8 length];

    for (NSUInteger i = 0; i < n; i++) {
        if (s[i] == '%') {
            int hi = (i + 2 < n) ? hexDigit(s[i + 1]) : -1, lo = (i + 2 < n) ? hexDigit(s[i + 2]) : -1;
            if (hi < 0 || lo < 0)
                return nil;
            uint8_t v = (uint8_t)(hi * 16 + lo);
            [bytes appendBytes:&v length:1];
            i += 2;
        } else {
            [bytes appendBytes:&s[i] length:1];
        }
    }
    return [[NSString alloc] initWithData:bytes encoding:NSUTF8StringEncoding];
}

/* ---- Case mapping -------------------------------------------------------- */

typedef void (*CaseFunction)(CFMutableStringRef, CFLocaleRef);

static NSString *mapCase(NSString *self, NSLocale *locale, CaseFunction f) {
    NSUInteger         length = [self length];
    unichar           *chars = malloc((length + 1) * sizeof(unichar));
    CFMutableStringRef m;
    CFLocaleRef        cfLocale = NULL;

    [self getCharacters:chars range:NSMakeRange(0, length)];
    m = CFStringCreateMutable(NULL, 0);
    CFStringAppendCharacters(m, chars, (CFIndex)length);
    free(chars);
    if (locale != nil)
        cfLocale = CFLocaleCreate(NULL, (__bridge CFStringRef)[locale localeIdentifier]);
    f(m, cfLocale);
    if (cfLocale != NULL)
        CFRelease(cfLocale);
    return fromCF(m);
}

static void capitalize(CFMutableStringRef s, CFLocaleRef l) { CFStringCapitalize(s, l); }
static void upper(CFMutableStringRef s, CFLocaleRef l) { CFStringUppercase(s, l); }
static void lower(CFMutableStringRef s, CFLocaleRef l) { CFStringLowercase(s, l); }

- (NSString *)uppercaseStringWithLocale:(NSLocale *)locale { return mapCase(self, locale, upper); }
- (NSString *)lowercaseStringWithLocale:(NSLocale *)locale { return mapCase(self, locale, lower); }
- (NSString *)capitalizedStringWithLocale:(NSLocale *)locale { return mapCase(self, locale, capitalize); }
- (NSString *)localizedUppercaseString { return mapCase(self, [NSLocale currentLocale], upper); }
- (NSString *)localizedLowercaseString { return mapCase(self, [NSLocale currentLocale], lower); }
- (NSString *)localizedCapitalizedString { return mapCase(self, [NSLocale currentLocale], capitalize); }

/* ---- Transforms ---------------------------------------------------------- */

// nil when ICU does not know the transform.
- (NSString *)stringByApplyingTransform:(NSStringTransform)transform reverse:(BOOL)reverse {
    NSUInteger     idLength = [transform length], length = [self length];
    unichar       *idChars = malloc((idLength + 1) * sizeof(unichar));
    UErrorCode     status = U_ZERO_ERROR;
    UTransliterator *t;
    NSString      *result = nil;

    [transform getCharacters:idChars range:NSMakeRange(0, idLength)];
    t = utrans_openU(idChars, (int32_t)idLength, reverse ? UTRANS_REVERSE : UTRANS_FORWARD,
                     NULL, 0, NULL, &status);
    free(idChars);
    if (U_FAILURE(status) || t == NULL)
        return nil;

    int32_t capacity = (int32_t)(length * 4 + 16), textLength = (int32_t)length, limit = (int32_t)length;
    UChar  *text = malloc((size_t)capacity * sizeof(UChar));
    [self getCharacters:text range:NSMakeRange(0, length)];
    utrans_transUChars(t, text, &textLength, capacity, 0, &limit, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        free(text);
        capacity = textLength + 1;
        text = malloc((size_t)capacity * sizeof(UChar));
        [self getCharacters:text range:NSMakeRange(0, length)];
        textLength = limit = (int32_t)length;
        status = U_ZERO_ERROR;
        utrans_transUChars(t, text, &textLength, capacity, 0, &limit, &status);
    }
    if (U_SUCCESS(status))
        result = [NSString stringWithCharacters:text length:(NSUInteger)textLength];
    free(text);
    utrans_close(t);
    return result;
}

@end
