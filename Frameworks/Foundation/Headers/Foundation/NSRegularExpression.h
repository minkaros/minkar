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

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>
#import <Foundation/NSTextCheckingResult.h>

@class NSArray, NSError, NSMutableString, NSString;

NS_ASSUME_NONNULL_BEGIN

// Regular expressions with ICU's syntax (macOS 10.7). Immutable and safe
// to use from several threads at once.

typedef NS_OPTIONS(NSUInteger, NSRegularExpressionOptions) {
    NSRegularExpressionCaseInsensitive          = 1 << 0,
    NSRegularExpressionAllowCommentsAndWhitespace = 1 << 1,
    NSRegularExpressionIgnoreMetacharacters     = 1 << 2,
    NSRegularExpressionDotMatchesLineSeparators = 1 << 3,
    NSRegularExpressionAnchorsMatchLines        = 1 << 4,
    NSRegularExpressionUseUnixLineSeparators    = 1 << 5,
    NSRegularExpressionUseUnicodeWordBoundaries = 1 << 6,
};

typedef NS_OPTIONS(NSUInteger, NSMatchingOptions) {
    NSMatchingReportProgress         = 1 << 0,
    NSMatchingReportCompletion       = 1 << 1,
    NSMatchingAnchored               = 1 << 2,
    NSMatchingWithTransparentBounds  = 1 << 3,
    NSMatchingWithoutAnchoringBounds = 1 << 4,
};

typedef NS_OPTIONS(NSUInteger, NSMatchingFlags) {
    NSMatchingProgress      = 1 << 0,
    NSMatchingCompleted     = 1 << 1,
    NSMatchingHitEnd        = 1 << 2,
    NSMatchingRequiredEnd   = 1 << 3,
    NSMatchingInternalError = 1 << 4,
};

@interface NSRegularExpression : NSObject <NSCopying, NSSecureCoding>

+ (nullable NSRegularExpression *)regularExpressionWithPattern:(NSString *)pattern
                                                       options:(NSRegularExpressionOptions)options
                                                         error:(NSError **)error;

// nil, and *error set, when the pattern is not a valid expression.
- (nullable instancetype)initWithPattern:(NSString *)pattern
                                 options:(NSRegularExpressionOptions)options
                                   error:(NSError **)error NS_DESIGNATED_INITIALIZER;

- (nullable instancetype)initWithCoder:(NSCoder *)coder;

@property (readonly, copy) NSString *pattern;
@property (readonly) NSRegularExpressionOptions options;
@property (readonly) NSUInteger numberOfCaptureGroups;

// A pattern that matches the string literally.
+ (NSString *)escapedPatternForString:(NSString *)string;

// For each match in range, in order. The block may set *stop to end early.
- (void)enumerateMatchesInString:(NSString *)string
                         options:(NSMatchingOptions)options
                           range:(NSRange)range
                      usingBlock:(void (NS_NOESCAPE ^)(NSTextCheckingResult *_Nullable result,
                                                       NSMatchingFlags flags,
                                                       BOOL *stop))block;

- (NSArray *)matchesInString:(NSString *)string
                                            options:(NSMatchingOptions)options
                                              range:(NSRange)range;
- (NSUInteger)numberOfMatchesInString:(NSString *)string
                              options:(NSMatchingOptions)options
                                range:(NSRange)range;
- (nullable NSTextCheckingResult *)firstMatchInString:(NSString *)string
                                              options:(NSMatchingOptions)options
                                                range:(NSRange)range;
// {NSNotFound, 0} when nothing matches.
- (NSRange)rangeOfFirstMatchInString:(NSString *)string
                             options:(NSMatchingOptions)options
                               range:(NSRange)range;

@end

// Replacement templates: "$0" is the whole match, "$1"... the capture
// groups; a backslash takes the next character literally.
@interface NSRegularExpression (NSReplacement)

- (NSString *)stringByReplacingMatchesInString:(NSString *)string
                                       options:(NSMatchingOptions)options
                                         range:(NSRange)range
                                  withTemplate:(NSString *)templ;

// Returns the number of matches replaced.
- (NSUInteger)replaceMatchesInString:(NSMutableString *)string
                             options:(NSMatchingOptions)options
                               range:(NSRange)range
                        withTemplate:(NSString *)templ;

// The template filled in for one result; offset is how far the result's
// ranges have moved in string since it was found.
- (NSString *)replacementStringForResult:(NSTextCheckingResult *)result
                                inString:(NSString *)string
                                  offset:(NSInteger)offset
                                template:(NSString *)templ;

// A template that produces the string literally.
+ (NSString *)escapedTemplateForString:(NSString *)string;

@end

NS_ASSUME_NONNULL_END
