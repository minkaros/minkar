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
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSPointerFunctions.h>

@class NSArray;

NS_ASSUME_NONNULL_BEGIN

// An ordered collection of pointers that may hold NULL; with weak memory,
// an item whose object goes away reads as NULL (compact removes those).
@interface NSPointerArray : NSObject <NSFastEnumeration, NSCopying>

- (instancetype)initWithOptions:(NSPointerFunctionsOptions)options;
- (instancetype)initWithPointerFunctions:(NSPointerFunctions *)functions;
+ (NSPointerArray *)pointerArrayWithOptions:(NSPointerFunctionsOptions)options;
+ (NSPointerArray *)pointerArrayWithPointerFunctions:(NSPointerFunctions *)functions;
+ (NSPointerArray *)strongObjectsPointerArray;
+ (NSPointerArray *)weakObjectsPointerArray;

@property (readonly, copy) NSPointerFunctions *pointerFunctions;
@property NSUInteger count;     // a larger count adds NULLs, a smaller one drops items
@property (readonly, copy) NSArray *allObjects;     // the non-NULL items

- (nullable void *)pointerAtIndex:(NSUInteger)index;
- (void)addPointer:(nullable void *)pointer;
- (void)removePointerAtIndex:(NSUInteger)index;
- (void)insertPointer:(nullable void *)item atIndex:(NSUInteger)index;
- (void)replacePointerAtIndex:(NSUInteger)index withPointer:(nullable void *)item;
- (void)compact;

@end

NS_ASSUME_NONNULL_END
