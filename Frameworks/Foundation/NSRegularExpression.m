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

// NSRegularExpression on ICU's regular expressions (uregex, libicucore),
// and the regular expression results of NSTextCheckingResult. Built with
// ARC.
//
// NSString and ICU both count UTF-16 code units, so ranges pass between
// them unchanged. The compiled expression is never used directly: each
// operation works on a clone, so one NSRegularExpression can be used from
// several threads at once.

#import <Foundation/NSRegularExpression.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <Foundation/FoundationErrors.h>
#include <stdlib.h>
#include <string.h>
#include <unicode/uregex.h>

// The archive keys, those of macOS's keyed archives, so that archives move
// between the systems.
static NSString *const NSRegularExpressionPatternKey = @"NSPattern";
static NSString *const NSRegularExpressionOptionsKey = @"NSOptions";

#pragma mark - Results

// NSTextCheckingResult's initializer (NSTextCheckingResult.m).
@interface NSTextCheckingResult (NSRegularExpressionPrivate)
- (id)initWithResultType:(NSTextCheckingType)resultType
                   range:(NSRange)range
              properties:(NSDictionary *)properties;
@end

@interface NSRegularExpressionCheckingResult : NSTextCheckingResult
- (instancetype)initWithRanges:(const NSRange *)ranges
                         count:(NSUInteger)count
             regularExpression:(NSRegularExpression *)regularExpression;
@end

@implementation NSRegularExpressionCheckingResult {
    NSRange *_ranges;
    NSUInteger _count;
    NSRegularExpression *_expression;
}

- (instancetype)initWithRanges:(const NSRange *)ranges
                         count:(NSUInteger)count
             regularExpression:(NSRegularExpression *)regularExpression
{
    NSRange whole = count > 0 ? ranges[0] : NSMakeRange(NSNotFound, 0);
    self = [super initWithResultType:NSTextCheckingTypeRegularExpression
                               range:whole
                          properties:nil];
    if (self == nil)
        return nil;
    _ranges = calloc(count > 0 ? count : 1, sizeof(NSRange));
    if (_ranges == NULL)
        return nil;
    if (count > 0)
        memcpy(_ranges, ranges, count * sizeof(NSRange));
    _count = count;
    _expression = regularExpression;
    return self;
}

- (void)dealloc
{
    free(_ranges);
}

- (NSUInteger)numberOfRanges
{
    return _count;
}

- (NSRange)rangeAtIndex:(NSUInteger)index
{
    if (index >= _count) {
        [NSException raise:NSRangeException
                    format:@"-[%@ %@]: index %lu beyond bounds [0 .. %lu]", [self class],
                           NSStringFromSelector(_cmd), (unsigned long)index,
                           (unsigned long)(_count > 0 ? _count - 1 : 0)];
    }
    return _ranges[index];
}

- (NSRegularExpression *)regularExpression
{
    return _expression;
}

- (NSTextCheckingResult *)resultByAdjustingRangesWithOffset:(NSInteger)offset
{
    NSRange *moved = calloc(_count > 0 ? _count : 1, sizeof(NSRange));
    if (moved == NULL)
        return self;
    for (NSUInteger i = 0; i < _count; i++) {
        moved[i] = _ranges[i];
        if (moved[i].location != NSNotFound)
            moved[i].location += offset;
    }
    NSTextCheckingResult *result = [[NSRegularExpressionCheckingResult alloc]
        initWithRanges:moved count:_count regularExpression:_expression];
    free(moved);
    return result;
}

@end

@implementation NSTextCheckingResult (NSRegularExpression)

+ (NSTextCheckingResult *)regularExpressionCheckingResultWithRanges:(NSRangePointer)ranges
                                                              count:(NSUInteger)count
                                                  regularExpression:(NSRegularExpression *)regularExpression
{
    return [[NSRegularExpressionCheckingResult alloc] initWithRanges:ranges
                                                               count:count
                                                   regularExpression:regularExpression];
}

// Other kinds of result have one range, their own.
- (NSUInteger)numberOfRanges
{
    return 1;
}

- (NSRange)rangeAtIndex:(NSUInteger)index
{
    if (index != 0) {
        [NSException raise:NSRangeException
                    format:@"-[%@ %@]: index %lu beyond bounds [0 .. 0]", [self class],
                           NSStringFromSelector(_cmd), (unsigned long)index];
    }
    return [self range];
}

- (NSRegularExpression *)regularExpression
{
    return nil;
}

- (NSTextCheckingResult *)resultByAdjustingRangesWithOffset:(NSInteger)offset
{
    if (offset == 0)
        return self;
    NSRange range = [self range];
    if (range.location != NSNotFound)
        range.location += offset;
    return [[NSTextCheckingResult alloc] initWithResultType:[self resultType]
                                                      range:range
                                                 properties:nil];
}

@end

#pragma mark - Expressions

// The string's UTF-16 code units, for ICU. Free with free().
static UChar *copyCharacters(NSString *string, NSUInteger *length)
{
    NSUInteger n = [string length];
    UChar *buffer = malloc((n > 0 ? n : 1) * sizeof(UChar));
    if (buffer != NULL && n > 0)
        [string getCharacters:(unichar *)buffer range:NSMakeRange(0, n)];
    *length = n;
    return buffer;
}

static uint32_t icuFlags(NSRegularExpressionOptions options)
{
    uint32_t flags = 0;
    if (options & NSRegularExpressionCaseInsensitive)
        flags |= UREGEX_CASE_INSENSITIVE;
    if (options & NSRegularExpressionAllowCommentsAndWhitespace)
        flags |= UREGEX_COMMENTS;
    if (options & NSRegularExpressionIgnoreMetacharacters)
        flags |= UREGEX_LITERAL;
    if (options & NSRegularExpressionDotMatchesLineSeparators)
        flags |= UREGEX_DOTALL;
    if (options & NSRegularExpressionAnchorsMatchLines)
        flags |= UREGEX_MULTILINE;
    if (options & NSRegularExpressionUseUnixLineSeparators)
        flags |= UREGEX_UNIX_LINES;
    if (options & NSRegularExpressionUseUnicodeWordBoundaries)
        flags |= UREGEX_UWORD;
    return flags;
}

@implementation NSRegularExpression {
    NSString *_pattern;
    NSRegularExpressionOptions _options;
    URegularExpression *_regex;     // compiled once; only ever cloned
    NSUInteger _groups;
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

+ (nullable NSRegularExpression *)regularExpressionWithPattern:(NSString *)pattern
                                                       options:(NSRegularExpressionOptions)options
                                                         error:(NSError **)error
{
    return [[self alloc] initWithPattern:pattern options:options error:error];
}

- (instancetype)init
{
    return [self initWithPattern:@"" options:0 error:NULL];
}

- (nullable instancetype)initWithPattern:(NSString *)pattern
                                 options:(NSRegularExpressionOptions)options
                                   error:(NSError **)error
{
    self = [super init];
    if (self == nil)
        return nil;
    if (pattern == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"-[%@ %@]: nil pattern", [self class], NSStringFromSelector(_cmd)];
    }

    NSUInteger length = 0;
    UChar *characters = copyCharacters(pattern, &length);
    UParseError parseError;
    UErrorCode status = U_ZERO_ERROR;
    if (characters != NULL) {
        // ICU keeps its own copy of the pattern.
        _regex = uregex_open(characters, (int32_t)length, icuFlags(options),
                             &parseError, &status);
        free(characters);
    }
    if (_regex == NULL || U_FAILURE(status)) {
        if (_regex != NULL)
            uregex_close(_regex);
        _regex = NULL;
        if (error != NULL) {
            NSString *reason = [NSString stringWithFormat:
                @"The value \"%@\" is invalid (%s).", pattern, u_errorName(status)];
            *error = [NSError errorWithDomain:NSCocoaErrorDomain
                                         code:NSFormattingError
                                     userInfo:@{ @"NSInvalidValue": pattern,
                                                 NSLocalizedDescriptionKey: reason }];
        }
        return nil;
    }
    _groups = (NSUInteger)uregex_groupCount(_regex, &status);
    _pattern = [pattern copy];
    _options = options;
    return self;
}

- (nullable instancetype)initWithCoder:(NSCoder *)coder
{
    NSString *pattern = nil;
    NSRegularExpressionOptions options = 0;
    if ([coder allowsKeyedCoding]) {
        pattern = [coder decodeObjectForKey:NSRegularExpressionPatternKey];
        options = (NSRegularExpressionOptions)[coder decodeIntegerForKey:NSRegularExpressionOptionsKey];
    } else {
        pattern = [coder decodeObject];
        [coder decodeValueOfObjCType:@encode(NSUInteger) at:&options];
    }
    if (![pattern isKindOfClass:[NSString class]])
        return nil;
    return [self initWithPattern:pattern options:options error:NULL];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    if ([coder allowsKeyedCoding]) {
        [coder encodeObject:_pattern forKey:NSRegularExpressionPatternKey];
        [coder encodeInteger:(NSInteger)_options forKey:NSRegularExpressionOptionsKey];
    } else {
        [coder encodeObject:_pattern];
        [coder encodeValueOfObjCType:@encode(NSUInteger) at:&_options];
    }
}

- (void)dealloc
{
    if (_regex != NULL)
        uregex_close(_regex);
}

- (id)copyWithZone:(NSZone *)zone
{
    (void)zone;
    return self;    // immutable
}

- (NSString *)pattern
{
    return _pattern;
}

- (NSRegularExpressionOptions)options
{
    return _options;
}

- (NSUInteger)numberOfCaptureGroups
{
    return _groups;
}

- (BOOL)isEqual:(id)object
{
    if (object == self)
        return YES;
    if (![object isKindOfClass:[NSRegularExpression class]])
        return NO;
    NSRegularExpression *other = object;
    return _options == [other options] && [_pattern isEqualToString:[other pattern]];
}

- (NSUInteger)hash
{
    return [_pattern hash] ^ _options;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %p> %@ 0x%lx", [self class], self, _pattern,
                                      (unsigned long)_options];
}

+ (NSString *)escapedPatternForString:(NSString *)string
{
    static NSString *const special = @"\\*?+[](){}^$|./";
    NSMutableString *escaped = [NSMutableString stringWithCapacity:[string length]];
    NSUInteger length = [string length];
    for (NSUInteger i = 0; i < length; i++) {
        unichar c = [string characterAtIndex:i];
        if ([special rangeOfString:[NSString stringWithCharacters:&c length:1]].location != NSNotFound)
            [escaped appendString:@"\\"];
        [escaped appendFormat:@"%C", c];
    }
    return [escaped copy];
}

#pragma mark Matching

- (void)enumerateMatchesInString:(NSString *)string
                         options:(NSMatchingOptions)options
                           range:(NSRange)range
                      usingBlock:(void (NS_NOESCAPE ^)(NSTextCheckingResult *_Nullable result,
                                                       NSMatchingFlags flags,
                                                       BOOL *stop))block
{
    if (string == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"-[%@ %@]: nil string", [self class], NSStringFromSelector(_cmd)];
    }
    NSUInteger length = [string length];
    if (range.location > length || range.length > length - range.location) {
        [NSException raise:NSRangeException
                    format:@"-[%@ %@]: range {%lu, %lu} out of bounds; string length %lu",
                           [self class], NSStringFromSelector(_cmd),
                           (unsigned long)range.location, (unsigned long)range.length,
                           (unsigned long)length];
    }

    UErrorCode status = U_ZERO_ERROR;
    URegularExpression *re = uregex_clone(_regex, &status);
    UChar *text = copyCharacters(string, &length);
    BOOL stop = NO;
    if (re == NULL || text == NULL || U_FAILURE(status)) {
        block(nil, NSMatchingInternalError, &stop);
        free(text);
        if (re != NULL)
            uregex_close(re);
        return;
    }

    uregex_setText(re, text, (int32_t)length, &status);
    uregex_setRegion64(re, (int64_t)range.location, (int64_t)NSMaxRange(range), &status);
    uregex_useTransparentBounds(re, (options & NSMatchingWithTransparentBounds) != 0, &status);
    uregex_useAnchoringBounds(re, (options & NSMatchingWithoutAnchoringBounds) == 0, &status);

    NSUInteger count = _groups + 1;
    NSRange *ranges = calloc(count, sizeof(NSRange));
    NSUInteger anchor = range.location;     // where an anchored match must start
    BOOL failed = (ranges == NULL || U_FAILURE(status));

    while (!failed && !stop && uregex_findNext(re, &status)) {
        if (U_FAILURE(status)) {
            failed = YES;
            break;
        }
        for (NSUInteger i = 0; i < count; i++) {
            int64_t start = uregex_start64(re, (int32_t)i, &status);
            int64_t end = uregex_end64(re, (int32_t)i, &status);
            ranges[i] = start < 0 ? NSMakeRange(NSNotFound, 0)
                                  : NSMakeRange((NSUInteger)start, (NSUInteger)(end - start));
        }
        if ((options & NSMatchingAnchored) && ranges[0].location != anchor)
            break;
        anchor = NSMaxRange(ranges[0]);
        NSTextCheckingResult *result =
            [[NSRegularExpressionCheckingResult alloc] initWithRanges:ranges
                                                                count:count
                                                    regularExpression:self];
        block(result, 0, &stop);
        if (!stop && (options & NSMatchingReportProgress))
            block(nil, NSMatchingProgress, &stop);
    }
    if (U_FAILURE(status))
        failed = YES;

    if (failed && !stop) {
        block(nil, NSMatchingInternalError, &stop);
    } else if (!stop && (options & NSMatchingReportCompletion)) {
        UErrorCode s = U_ZERO_ERROR;
        NSMatchingFlags flags = NSMatchingCompleted;
        if (uregex_hitEnd(re, &s))
            flags |= NSMatchingHitEnd;
        if (uregex_requireEnd(re, &s))
            flags |= NSMatchingRequiredEnd;
        block(nil, flags, &stop);
    }
    free(ranges);
    free(text);
    uregex_close(re);
}

- (NSArray *)matchesInString:(NSString *)string
                                            options:(NSMatchingOptions)options
                                              range:(NSRange)range
{
    NSMutableArray *matches = [NSMutableArray array];
    options &= ~(NSMatchingReportProgress | NSMatchingReportCompletion);
    [self enumerateMatchesInString:string options:options range:range
                        usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
        (void)flags;
        (void)stop;
        if (result != nil)
            [matches addObject:result];
    }];
    return [matches copy];
}

- (NSUInteger)numberOfMatchesInString:(NSString *)string
                              options:(NSMatchingOptions)options
                                range:(NSRange)range
{
    __block NSUInteger count = 0;
    options &= ~(NSMatchingReportProgress | NSMatchingReportCompletion);
    [self enumerateMatchesInString:string options:options range:range
                        usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
        (void)flags;
        (void)stop;
        if (result != nil)
            count++;
    }];
    return count;
}

- (nullable NSTextCheckingResult *)firstMatchInString:(NSString *)string
                                              options:(NSMatchingOptions)options
                                                range:(NSRange)range
{
    __block NSTextCheckingResult *first = nil;
    options &= ~(NSMatchingReportProgress | NSMatchingReportCompletion);
    [self enumerateMatchesInString:string options:options range:range
                        usingBlock:^(NSTextCheckingResult *result, NSMatchingFlags flags, BOOL *stop) {
        (void)flags;
        if (result != nil) {
            first = result;
            *stop = YES;
        }
    }];
    return first;
}

- (NSRange)rangeOfFirstMatchInString:(NSString *)string
                             options:(NSMatchingOptions)options
                               range:(NSRange)range
{
    NSTextCheckingResult *first = [self firstMatchInString:string options:options range:range];
    return first != nil ? [first range] : NSMakeRange(NSNotFound, 0);
}

@end

#pragma mark - Replacement

@implementation NSRegularExpression (NSReplacement)

+ (NSString *)escapedTemplateForString:(NSString *)string
{
    NSMutableString *escaped = [NSMutableString stringWithCapacity:[string length]];
    NSUInteger length = [string length];
    for (NSUInteger i = 0; i < length; i++) {
        unichar c = [string characterAtIndex:i];
        if (c == '\\' || c == '$')
            [escaped appendString:@"\\"];
        [escaped appendFormat:@"%C", c];
    }
    return [escaped copy];
}

- (NSString *)replacementStringForResult:(NSTextCheckingResult *)result
                                inString:(NSString *)string
                                  offset:(NSInteger)offset
                                template:(NSString *)templ
{
    NSMutableString *out = [NSMutableString string];
    NSUInteger groups = [result numberOfRanges];
    NSUInteger length = [templ length];
    NSUInteger stringLength = [string length];
    for (NSUInteger i = 0; i < length; i++) {
        unichar c = [templ characterAtIndex:i];
        if (c == '\\') {
            // The next character, literally.
            if (i + 1 < length) {
                unichar next = [templ characterAtIndex:++i];
                [out appendFormat:@"%C", next];
            }
            continue;
        }
        if (c != '$' || i + 1 >= length) {
            [out appendFormat:@"%C", c];
            continue;
        }
        // $ and the longest run of digits that names a group.
        NSUInteger j = i + 1, group = 0;
        BOOL any = NO;
        while (j < length) {
            unichar d = [templ characterAtIndex:j];
            if (d < '0' || d > '9')
                break;
            NSUInteger next = group * 10 + (NSUInteger)(d - '0');
            if (any && next >= groups)
                break;
            group = next;
            any = YES;
            j++;
        }
        if (!any) {
            [out appendString:@"$"];
            continue;
        }
        i = j - 1;
        if (group >= groups)
            continue;       // no such group: nothing
        NSRange r = [result rangeAtIndex:group];
        if (r.location == NSNotFound)
            continue;
        NSInteger location = (NSInteger)r.location + offset;
        if (location < 0 || (NSUInteger)location + r.length > stringLength)
            continue;
        [out appendString:[string substringWithRange:NSMakeRange((NSUInteger)location, r.length)]];
    }
    return [out copy];
}

- (NSString *)stringByReplacingMatchesInString:(NSString *)string
                                       options:(NSMatchingOptions)options
                                         range:(NSRange)range
                                  withTemplate:(NSString *)templ
{
    NSArray *matches = [self matchesInString:string options:options range:range];
    if ([matches count] == 0)
        return [string copy];
    NSMutableString *out = [NSMutableString stringWithCapacity:[string length]];
    NSUInteger last = 0;
    for (NSTextCheckingResult *match in matches) {
        NSRange r = [match range];
        [out appendString:[string substringWithRange:NSMakeRange(last, r.location - last)]];
        [out appendString:[self replacementStringForResult:match inString:string offset:0 template:templ]];
        last = NSMaxRange(r);
    }
    [out appendString:[string substringFromIndex:last]];
    return [out copy];
}

- (NSUInteger)replaceMatchesInString:(NSMutableString *)string
                             options:(NSMatchingOptions)options
                               range:(NSRange)range
                        withTemplate:(NSString *)templ
{
    NSArray *matches = [self matchesInString:string options:options range:range];
    NSInteger offset = 0;
    for (NSTextCheckingResult *match in matches) {
        NSString *replacement = [self replacementStringForResult:match
                                                        inString:string
                                                          offset:offset
                                                        template:templ];
        NSRange r = [match range];
        r.location = (NSUInteger)((NSInteger)r.location + offset);
        [string replaceCharactersInRange:r withString:replacement];
        offset += (NSInteger)[replacement length] - (NSInteger)[match range].length;
    }
    return [matches count];
}

@end
