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

@class NSData, NSError, NSInputStream, NSOutputStream;

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, NSJSONReadingOptions) {
    NSJSONReadingMutableContainers = (1UL << 0),
    NSJSONReadingMutableLeaves = (1UL << 1),
    NSJSONReadingFragmentsAllowed = (1UL << 2),
    NSJSONReadingJSON5Allowed = (1UL << 3),
    NSJSONReadingTopLevelDictionaryAssumed = (1UL << 4),
    NSJSONReadingAllowFragments = NSJSONReadingFragmentsAllowed,
};

typedef NS_OPTIONS(NSUInteger, NSJSONWritingOptions) {
    NSJSONWritingPrettyPrinted = (1UL << 0),
    NSJSONWritingSortedKeys = (1UL << 1),
    NSJSONWritingFragmentsAllowed = (1UL << 2),
    NSJSONWritingWithoutEscapingSlashes = (1UL << 3),
};

@interface NSJSONSerialization : NSObject

+ (BOOL)isValidJSONObject:(id)obj;

+ (nullable NSData *)dataWithJSONObject:(id)obj
                                options:(NSJSONWritingOptions)opt
                                  error:(NSError **)error;

+ (nullable id)JSONObjectWithData:(NSData *)data
                          options:(NSJSONReadingOptions)opt
                            error:(NSError **)error;

+ (NSInteger)writeJSONObject:(id)obj
                    toStream:(NSOutputStream *)stream
                     options:(NSJSONWritingOptions)opt
                       error:(NSError **)error;

+ (nullable id)JSONObjectWithStream:(NSInputStream *)stream
                            options:(NSJSONReadingOptions)opt
                              error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
