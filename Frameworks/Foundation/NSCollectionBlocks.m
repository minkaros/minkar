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

// The block methods of the collections: enumeration, tests, sorting with
// comparators and binary search. NSEnumerationConcurrent and
// NSSortConcurrent are hints: the work runs on the calling thread, in
// order. Sorting is always stable, so NSSortStable holds too.

#import <Foundation/NSArray.h>
#import <Foundation/NSMutableArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSMutableSet.h>
#import <Foundation/NSIndexSet.h>
#import <Foundation/NSMutableIndexSet.h>
#import <Foundation/NSException.h>
#include <stdlib.h>

// Stable merge sort of objects[0..count) with cmp. scratch holds count
// pointers. Neither buffer retains: the objects belong to a collection that
// outlives the sort.
static void mergeSort(void **objects, void **scratch, NSUInteger count, NSComparator cmp)
{
    if (count < 2)
        return;
    NSUInteger half = count / 2;
    mergeSort(objects, scratch, half, cmp);
    mergeSort(objects + half, scratch, count - half, cmp);
    // Already in order: nothing to merge.
    if (cmp((__bridge id)objects[half - 1], (__bridge id)objects[half]) != NSOrderedDescending)
        return;
    NSUInteger i = 0, j = half, k = 0;
    while (i < half && j < count) {
        // Ties take the left element first: that keeps the sort stable.
        if (cmp((__bridge id)objects[j], (__bridge id)objects[i]) == NSOrderedAscending)
            scratch[k++] = objects[j++];
        else
            scratch[k++] = objects[i++];
    }
    while (i < half)
        scratch[k++] = objects[i++];
    while (j < count)
        scratch[k++] = objects[j++];
    for (k = 0; k < count; k++)
        objects[k] = scratch[k];
}

static NSArray *sortedObjects(NSArray *array, NSComparator cmp)
{
    NSUInteger count = [array count];
    if (count == 0)
        return [NSArray array];
    void **objects = calloc(count, sizeof(void *));
    void **scratch = calloc(count, sizeof(void *));
    if (objects == NULL || scratch == NULL) {
        free(objects);
        free(scratch);
        [NSException raise:NSMallocException format:@"no memory to sort %lu objects",
                                                     (unsigned long)count];
    }
    for (NSUInteger i = 0; i < count; i++)
        objects[i] = (__bridge void *)[array objectAtIndex:i];
    mergeSort(objects, scratch, count, cmp);
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:count];
    for (NSUInteger i = 0; i < count; i++)
        [result addObject:(__bridge id)objects[i]];
    free(objects);
    free(scratch);
    return result;
}

static void requireBlock(id block, SEL sel, id receiver)
{
    if (block == nil)
        [NSException raise:NSInvalidArgumentException format:@"-[%@ %@]: block cannot be nil",
                                                              [receiver class], NSStringFromSelector(sel)];
}

@implementation NSArray (NSBlocks)

- (void)enumerateObjectsUsingBlock:(void (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))block
{
    [self enumerateObjectsWithOptions:0 usingBlock:block];
}

- (void)enumerateObjectsWithOptions:(NSEnumerationOptions)opts
                         usingBlock:(void (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))block
{
    requireBlock(block, _cmd, self);
    NSUInteger count = [self count];
    BOOL stop = NO;
    for (NSUInteger n = 0; n < count && !stop; n++) {
        NSUInteger idx = (opts & NSEnumerationReverse) ? count - 1 - n : n;
        block([self objectAtIndex:idx], idx, &stop);
    }
}

- (void)enumerateObjectsAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)opts
                       usingBlock:(void (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))block
{
    requireBlock(block, _cmd, self);
    if (indexes != nil && [indexes count] > 0 && [indexes lastIndex] >= [self count])
        [NSException raise:NSRangeException format:@"-[%@ %@]: index %lu beyond bounds [0 .. %ld]",
                           [self class], NSStringFromSelector(_cmd),
                           (unsigned long)[indexes lastIndex], (long)[self count] - 1];
    [indexes enumerateIndexesWithOptions:opts usingBlock:^(NSUInteger idx, BOOL *stop) {
        block([self objectAtIndex:idx], idx, stop);
    }];
}

- (NSUInteger)indexOfObjectPassingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate
{
    return [self indexOfObjectWithOptions:0 passingTest:predicate];
}

- (NSUInteger)indexOfObjectWithOptions:(NSEnumerationOptions)opts
                           passingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate
{
    requireBlock(predicate, _cmd, self);
    __block NSUInteger found = NSNotFound;
    [self enumerateObjectsWithOptions:opts usingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        if (predicate(obj, idx, stop)) {
            found = idx;
            *stop = YES;
        }
    }];
    return found;
}

- (NSUInteger)indexOfObjectAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)opts
                         passingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate
{
    requireBlock(predicate, _cmd, self);
    __block NSUInteger found = NSNotFound;
    [self enumerateObjectsAtIndexes:indexes options:opts usingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        if (predicate(obj, idx, stop)) {
            found = idx;
            *stop = YES;
        }
    }];
    return found;
}

- (NSIndexSet *)indexesOfObjectsPassingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate
{
    return [self indexesOfObjectsWithOptions:0 passingTest:predicate];
}

- (NSIndexSet *)indexesOfObjectsWithOptions:(NSEnumerationOptions)opts
                                passingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate
{
    requireBlock(predicate, _cmd, self);
    NSMutableIndexSet *result = [NSMutableIndexSet indexSet];
    [self enumerateObjectsWithOptions:opts usingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        if (predicate(obj, idx, stop))
            [result addIndex:idx];
    }];
    return result;
}

- (NSIndexSet *)indexesOfObjectsAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)opts
                              passingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate
{
    requireBlock(predicate, _cmd, self);
    NSMutableIndexSet *result = [NSMutableIndexSet indexSet];
    [self enumerateObjectsAtIndexes:indexes options:opts usingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        if (predicate(obj, idx, stop))
            [result addIndex:idx];
    }];
    return result;
}

- (NSArray *)sortedArrayUsingComparator:(NSComparator NS_NOESCAPE)cmptr
{
    return [self sortedArrayWithOptions:0 usingComparator:cmptr];
}

- (NSArray *)sortedArrayWithOptions:(NSSortOptions)opts usingComparator:(NSComparator NS_NOESCAPE)cmptr
{
    (void)opts;
    requireBlock(cmptr, _cmd, self);
    return sortedObjects(self, cmptr);
}

- (NSUInteger)indexOfObject:(id)obj inSortedRange:(NSRange)r options:(NSBinarySearchingOptions)opts
            usingComparator:(NSComparator NS_NOESCAPE)cmp
{
    requireBlock(cmp, _cmd, self);
    BOOL first = (opts & NSBinarySearchingFirstEqual) != 0;
    BOOL last = (opts & NSBinarySearchingLastEqual) != 0;
    BOOL insertion = (opts & NSBinarySearchingInsertionIndex) != 0;
    if (first && last)
        [NSException raise:NSInvalidArgumentException
                    format:@"-[%@ %@]: both NSBinarySearchingFirstEqual and NSBinarySearchingLastEqual",
                           [self class], NSStringFromSelector(_cmd)];
    if (r.location > [self count] || r.length > [self count] - r.location)
        [NSException raise:NSRangeException format:@"-[%@ %@]: range {%lu, %lu} beyond bounds [0 .. %ld]",
                           [self class], NSStringFromSelector(_cmd), (unsigned long)r.location,
                           (unsigned long)r.length, (long)[self count] - 1];
    if (obj == nil)
        return NSNotFound;

    // Lower bound: first index whose element is not less than obj.
    NSUInteger lo = r.location, hi = r.location + r.length;
    while (lo < hi) {
        NSUInteger mid = lo + (hi - lo) / 2;
        if (cmp([self objectAtIndex:mid], obj) == NSOrderedAscending)
            lo = mid + 1;
        else
            hi = mid;
    }
    NSUInteger lower = lo;
    // Upper bound: first index whose element is greater than obj.
    hi = r.location + r.length;
    while (lo < hi) {
        NSUInteger mid = lo + (hi - lo) / 2;
        if (cmp([self objectAtIndex:mid], obj) == NSOrderedDescending)
            hi = mid;
        else
            lo = mid + 1;
    }
    NSUInteger upper = lo;

    if (insertion)
        return first ? lower : upper;
    if (lower == upper)
        return NSNotFound;
    return last ? upper - 1 : lower;
}

@end

@implementation NSMutableArray (NSBlocks)

- (void)sortUsingComparator:(NSComparator NS_NOESCAPE)cmptr
{
    [self sortWithOptions:0 usingComparator:cmptr];
}

- (void)sortWithOptions:(NSSortOptions)opts usingComparator:(NSComparator NS_NOESCAPE)cmptr
{
    (void)opts;
    requireBlock(cmptr, _cmd, self);
    [self setArray:sortedObjects(self, cmptr)];
}

@end

@implementation NSDictionary (NSBlocks)

- (void)enumerateKeysAndObjectsUsingBlock:(void (NS_NOESCAPE ^)(id key, id obj, BOOL *stop))block
{
    [self enumerateKeysAndObjectsWithOptions:0 usingBlock:block];
}

- (void)enumerateKeysAndObjectsWithOptions:(NSEnumerationOptions)opts
                                usingBlock:(void (NS_NOESCAPE ^)(id key, id obj, BOOL *stop))block
{
    requireBlock(block, _cmd, self);
    // The keys as they are now, so the block sees each key once.
    [[self allKeys] enumerateObjectsWithOptions:opts usingBlock:^(id key, NSUInteger idx, BOOL *stop) {
        (void)idx;
        block(key, [self objectForKey:key], stop);
    }];
}

- (NSSet *)keysOfEntriesPassingTest:(BOOL (NS_NOESCAPE ^)(id key, id obj, BOOL *stop))predicate
{
    return [self keysOfEntriesWithOptions:0 passingTest:predicate];
}

- (NSSet *)keysOfEntriesWithOptions:(NSEnumerationOptions)opts
                        passingTest:(BOOL (NS_NOESCAPE ^)(id key, id obj, BOOL *stop))predicate
{
    requireBlock(predicate, _cmd, self);
    NSMutableSet *result = [NSMutableSet set];
    [self enumerateKeysAndObjectsWithOptions:opts usingBlock:^(id key, id obj, BOOL *stop) {
        if (predicate(key, obj, stop))
            [result addObject:key];
    }];
    return result;
}

- (NSArray *)keysSortedByValueUsingComparator:(NSComparator NS_NOESCAPE)cmptr
{
    return [self keysSortedByValueWithOptions:0 usingComparator:cmptr];
}

- (NSArray *)keysSortedByValueWithOptions:(NSSortOptions)opts usingComparator:(NSComparator NS_NOESCAPE)cmptr
{
    requireBlock(cmptr, _cmd, self);
    return [[self allKeys] sortedArrayWithOptions:opts usingComparator:^NSComparisonResult(id k1, id k2) {
        return cmptr([self objectForKey:k1], [self objectForKey:k2]);
    }];
}

@end

@implementation NSSet (NSBlocks)

- (void)enumerateObjectsUsingBlock:(void (NS_NOESCAPE ^)(id obj, BOOL *stop))block
{
    [self enumerateObjectsWithOptions:0 usingBlock:block];
}

- (void)enumerateObjectsWithOptions:(NSEnumerationOptions)opts
                         usingBlock:(void (NS_NOESCAPE ^)(id obj, BOOL *stop))block
{
    requireBlock(block, _cmd, self);
    [[self allObjects] enumerateObjectsWithOptions:opts usingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
        (void)idx;
        block(obj, stop);
    }];
}

- (NSSet *)objectsPassingTest:(BOOL (NS_NOESCAPE ^)(id obj, BOOL *stop))predicate
{
    return [self objectsWithOptions:0 passingTest:predicate];
}

- (NSSet *)objectsWithOptions:(NSEnumerationOptions)opts
                  passingTest:(BOOL (NS_NOESCAPE ^)(id obj, BOOL *stop))predicate
{
    requireBlock(predicate, _cmd, self);
    NSMutableSet *result = [NSMutableSet set];
    [self enumerateObjectsWithOptions:opts usingBlock:^(id obj, BOOL *stop) {
        if (predicate(obj, stop))
            [result addObject:obj];
    }];
    return result;
}

@end

@implementation NSIndexSet (NSBlocks)

- (void)enumerateIndexesUsingBlock:(void (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))block
{
    [self enumerateIndexesWithOptions:0 usingBlock:block];
}

- (void)enumerateIndexesWithOptions:(NSEnumerationOptions)opts
                         usingBlock:(void (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))block
{
    requireBlock(block, _cmd, self);
    BOOL stop = NO;
    if (opts & NSEnumerationReverse) {
        for (NSUInteger idx = [self lastIndex]; idx != NSNotFound && !stop;
             idx = [self indexLessThanIndex:idx])
            block(idx, &stop);
    } else {
        for (NSUInteger idx = [self firstIndex]; idx != NSNotFound && !stop;
             idx = [self indexGreaterThanIndex:idx])
            block(idx, &stop);
    }
}

- (void)enumerateRangesUsingBlock:(void (NS_NOESCAPE ^)(NSRange range, BOOL *stop))block
{
    requireBlock(block, _cmd, self);
    BOOL stop = NO;
    NSUInteger idx = [self firstIndex];
    while (idx != NSNotFound && !stop) {
        NSRange range = NSMakeRange(idx, 1);
        NSUInteger next = [self indexGreaterThanIndex:idx];
        while (next != NSNotFound && next == NSMaxRange(range)) {
            range.length++;
            next = [self indexGreaterThanIndex:next];
        }
        block(range, &stop);
        idx = next;
    }
}

- (NSUInteger)indexPassingTest:(BOOL (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))predicate
{
    requireBlock(predicate, _cmd, self);
    __block NSUInteger found = NSNotFound;
    [self enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        if (predicate(idx, stop)) {
            found = idx;
            *stop = YES;
        }
    }];
    return found;
}

- (NSIndexSet *)indexesPassingTest:(BOOL (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))predicate
{
    requireBlock(predicate, _cmd, self);
    NSMutableIndexSet *result = [NSMutableIndexSet indexSet];
    [self enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        if (predicate(idx, stop))
            [result addIndex:idx];
    }];
    return result;
}

@end
