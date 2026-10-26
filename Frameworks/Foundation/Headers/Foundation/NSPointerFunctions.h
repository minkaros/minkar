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

NS_ASSUME_NONNULL_BEGIN

// How a collection (NSHashTable, NSMapTable) holds its items: a memory
// option (low byte), a personality (second byte) and NSPointerFunctionsCopyIn.
typedef NS_OPTIONS(NSUInteger, NSPointerFunctionsOptions) {
    NSPointerFunctionsStrongMemory = (0UL << 0),
    NSPointerFunctionsZeroingWeakMemory = (1UL << 0),   // deprecated: weak
    NSPointerFunctionsOpaqueMemory = (2UL << 0),
    NSPointerFunctionsMallocMemory = (3UL << 0),
    NSPointerFunctionsMachVirtualMemory = (4UL << 0),
    NSPointerFunctionsWeakMemory = (5UL << 0),

    NSPointerFunctionsObjectPersonality = (0UL << 8),
    NSPointerFunctionsOpaquePersonality = (1UL << 8),
    NSPointerFunctionsObjectPointerPersonality = (2UL << 8),
    NSPointerFunctionsCStringPersonality = (3UL << 8),
    NSPointerFunctionsStructPersonality = (4UL << 8),
    NSPointerFunctionsIntegerPersonality = (5UL << 8),

    NSPointerFunctionsCopyIn = (1UL << 16),
};

@interface NSPointerFunctions : NSObject <NSCopying>

- (instancetype)initWithOptions:(NSPointerFunctionsOptions)options;
+ (NSPointerFunctions *)pointerFunctionsWithOptions:(NSPointerFunctionsOptions)options;

@property (nullable) NSUInteger (*hashFunction)(const void *item,
    NSUInteger (*_Nullable size)(const void *item));
@property (nullable) BOOL (*isEqualFunction)(const void *item1, const void *item2,
    NSUInteger (*_Nullable size)(const void *item));
@property (nullable) NSUInteger (*sizeFunction)(const void *item);
@property (nullable) NSString *_Nullable (*descriptionFunction)(const void *item);
@property (nullable) void (*relinquishFunction)(const void *item,
    NSUInteger (*_Nullable size)(const void *item));
@property (nullable) void *_Nonnull (*acquireFunction)(const void *src,
    NSUInteger (*_Nullable size)(const void *item), BOOL shouldCopy);

@property BOOL usesStrongWriteBarrier;          // garbage collection: no effect
@property BOOL usesWeakReadAndWriteBarriers;    // YES for weak memory

@end

NS_ASSUME_NONNULL_END
