/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Ordered collection differences (API 19): changes, the difference of two
 * lists by their longest common subsequence, and applying one. ARC.
 */

#import <Foundation/NSOrderedCollectionDifference.h>
#import <Foundation/NSOrderedCollectionChange.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSMutableArray.h>
#import <Foundation/NSIndexSet.h>
#import <Foundation/NSMutableIndexSet.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSMutableDictionary.h>
#include <stdlib.h>

@implementation NSOrderedCollectionChange

+ (NSOrderedCollectionChange *)changeWithObject:(id)anObject type:(NSCollectionChangeType)type index:(NSUInteger)index {
    return [[self alloc] initWithObject:anObject type:type index:index];
}

+ (NSOrderedCollectionChange *)changeWithObject:(id)anObject type:(NSCollectionChangeType)type index:(NSUInteger)index associatedIndex:(NSUInteger)associatedIndex {
    return [[self alloc] initWithObject:anObject type:type index:index associatedIndex:associatedIndex];
}

- (instancetype)initWithObject:(id)anObject type:(NSCollectionChangeType)type index:(NSUInteger)index {
    return [self initWithObject:anObject type:type index:index associatedIndex:NSNotFound];
}

- (instancetype)initWithObject:(id)anObject type:(NSCollectionChangeType)type index:(NSUInteger)index associatedIndex:(NSUInteger)associatedIndex {
    if ((self = [super init]) == nil)
        return nil;
    _object = anObject;
    _changeType = type;
    _index = index;
    _associatedIndex = associatedIndex;
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p> %@ %@ at %lu%@", [self class], (void *)self,
        _changeType == NSCollectionChangeInsert ? @"insert" : @"remove", _object, (unsigned long)_index,
        _associatedIndex == NSNotFound ? @"" : [NSString stringWithFormat:@" (moved, %lu)", (unsigned long)_associatedIndex]];
}

@end

@implementation NSOrderedCollectionDifference

/* Removals by descending index, insertions by ascending one. */
static NSArray *sorted(NSArray *changes, BOOL ascending) {
    return [changes sortedArrayUsingComparator:^NSComparisonResult(NSOrderedCollectionChange *a, NSOrderedCollectionChange *b) {
        if (a.index == b.index)
            return NSOrderedSame;
        return ((a.index < b.index) == ascending) ? NSOrderedAscending : NSOrderedDescending;
    }];
}

- (instancetype)initWithChanges:(NSArray<NSOrderedCollectionChange *> *)changes {
    return [self initWithInsertIndexes:[NSIndexSet indexSet] insertedObjects:nil removeIndexes:[NSIndexSet indexSet]
        removedObjects:nil additionalChanges:changes];
}

- (instancetype)initWithInsertIndexes:(NSIndexSet *)inserts insertedObjects:(NSArray *)insertedObjects removeIndexes:(NSIndexSet *)removes removedObjects:(NSArray *)removedObjects {
    return [self initWithInsertIndexes:inserts insertedObjects:insertedObjects removeIndexes:removes
        removedObjects:removedObjects additionalChanges:@[]];
}

- (instancetype)initWithInsertIndexes:(NSIndexSet *)inserts insertedObjects:(NSArray *)insertedObjects removeIndexes:(NSIndexSet *)removes removedObjects:(NSArray *)removedObjects additionalChanges:(NSArray<NSOrderedCollectionChange *> *)changes {
    if ((self = [super init]) == nil)
        return nil;
    if ((insertedObjects != nil && [insertedObjects count] != [inserts count]) ||
        (removedObjects != nil && [removedObjects count] != [removes count]))
        [NSException raise:NSInvalidArgumentException format:@"the objects and the indexes are not as many"];
    NSMutableArray *insertions = [NSMutableArray array], *removals = [NSMutableArray array];
    __block NSUInteger n = 0;
    [inserts enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        [insertions addObject:[NSOrderedCollectionChange changeWithObject:insertedObjects ? insertedObjects[n] : nil
            type:NSCollectionChangeInsert index:idx]];
        n++;
    }];
    n = 0;
    [removes enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        [removals addObject:[NSOrderedCollectionChange changeWithObject:removedObjects ? removedObjects[n] : nil
            type:NSCollectionChangeRemove index:idx]];
        n++;
    }];
    for (NSOrderedCollectionChange *change in changes)
        [change.changeType == NSCollectionChangeInsert ? insertions : removals addObject:change];
    _insertions = sorted(insertions, YES);
    _removals = sorted(removals, NO);
    return self;
}

- (NSArray *)insertions {
    return _insertions;
}

- (NSArray *)removals {
    return _removals;
}

- (BOOL)hasChanges {
    return [_insertions count] + [_removals count] > 0;
}

- (NSOrderedCollectionDifference *)differenceByTransformingChangesWithBlock:(NSOrderedCollectionChange *(^)(NSOrderedCollectionChange *))block {
    NSMutableArray *changes = [NSMutableArray array];
    for (NSOrderedCollectionChange *change in _removals)
        [changes addObject:block(change)];
    for (NSOrderedCollectionChange *change in _insertions)
        [changes addObject:block(change)];
    return [[NSOrderedCollectionDifference alloc] initWithChanges:changes];
}

/* Insertions become removals and the other way round. */
- (instancetype)inverseDifference {
    return [self differenceByTransformingChangesWithBlock:^NSOrderedCollectionChange *(NSOrderedCollectionChange *c) {
        return [NSOrderedCollectionChange changeWithObject:c.object
            type:c.changeType == NSCollectionChangeInsert ? NSCollectionChangeRemove : NSCollectionChangeInsert
            index:c.index associatedIndex:c.associatedIndex];
    }];
}

/* Removals first (descending), then insertions (ascending), as they are
 * applied. */
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(__unsafe_unretained id [])buffer count:(NSUInteger)len {
    NSUInteger total = [_removals count] + [_insertions count], i = state->state, n = 0;
    state->mutationsPtr = &state->extra[0];
    state->itemsPtr = (__typeof__(state->itemsPtr))(void *)buffer;
    while (i < total && n < len) {
        buffer[n++] = i < [_removals count] ? _removals[i] : _insertions[i - [_removals count]];
        i++;
    }
    state->state = i;
    return n;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p> removals %@ insertions %@", [self class], (void *)self, _removals, _insertions];
}

@end

/* The longest common subsequence of from and to (by the equivalence test,
 * else isEqual:); what is not in it is removed from from and inserted in
 * to. Moves pair a removal with an insertion of an equal object. */
NSOrderedCollectionDifference *_NSOrderedDifference(NSArray *from, NSArray *to, NSOrderedCollectionDifferenceCalculationOptions options, BOOL (^equal)(id, id)) {
    NSUInteger n = [from count], m = [to count];
    BOOL (^same)(id, id) = equal ?: ^BOOL(id a, id b) { return a == b || [a isEqual:b]; };
    NSMutableIndexSet *keptFrom = [NSMutableIndexSet indexSet], *keptTo = [NSMutableIndexSet indexSet];

    /* Common head and tail first: the table is for what is between. */
    NSUInteger head = 0;
    while (head < n && head < m && same(from[head], to[head])) {
        [keptFrom addIndex:head];
        [keptTo addIndex:head];
        head++;
    }
    NSUInteger tail = 0;
    while (tail < n - head && tail < m - head && same(from[n - 1 - tail], to[m - 1 - tail])) {
        [keptFrom addIndex:n - 1 - tail];
        [keptTo addIndex:m - 1 - tail];
        tail++;
    }
    NSUInteger rows = n - head - tail, cols = m - head - tail;
    if (rows > 0 && cols > 0 && (unsigned long long)(rows + 1) * (cols + 1) <= 64ULL * 1024 * 1024) {
        uint32_t *table = calloc((rows + 1) * (cols + 1), sizeof(uint32_t));
#define T(i, j) table[(i) * (cols + 1) + (j)]
        for (NSUInteger i = rows; i-- > 0;)
            for (NSUInteger j = cols; j-- > 0;)
                T(i, j) = same(from[head + i], to[head + j]) ? T(i + 1, j + 1) + 1 :
                    (T(i + 1, j) >= T(i, j + 1) ? T(i + 1, j) : T(i, j + 1));
        NSUInteger i = 0, j = 0;
        while (i < rows && j < cols) {
            if (same(from[head + i], to[head + j])) {
                [keptFrom addIndex:head + i];
                [keptTo addIndex:head + j];
                i++;
                j++;
            } else if (T(i + 1, j) >= T(i, j + 1))
                i++;
            else
                j++;
        }
#undef T
        free(table);
    }

    NSMutableArray *removals = [NSMutableArray array], *insertions = [NSMutableArray array];
    for (NSUInteger i = 0; i < n; i++)
        if (![keptFrom containsIndex:i])
            [removals addObject:@(i)];
    for (NSUInteger j = 0; j < m; j++)
        if (![keptTo containsIndex:j])
            [insertions addObject:@(j)];

    /* Moves: a removed object and an inserted equal one, each once. */
    NSMutableDictionary *movedTo = [NSMutableDictionary dictionary], *movedFrom = [NSMutableDictionary dictionary];
    if (options & NSOrderedCollectionDifferenceCalculationInferMoves)
        for (NSNumber *r in removals)
            for (NSNumber *ins in insertions)
                if (movedFrom[ins] == nil && same(from[[r unsignedIntegerValue]], to[[ins unsignedIntegerValue]])) {
                    movedTo[r] = ins;
                    movedFrom[ins] = r;
                    break;
                }

    NSMutableArray *changes = [NSMutableArray array];
    BOOL omitRemoved = (options & NSOrderedCollectionDifferenceCalculationOmitRemovedObjects) != 0;
    BOOL omitInserted = (options & NSOrderedCollectionDifferenceCalculationOmitInsertedObjects) != 0;
    for (NSNumber *r in removals)
        [changes addObject:[NSOrderedCollectionChange changeWithObject:omitRemoved ? nil : from[[r unsignedIntegerValue]]
            type:NSCollectionChangeRemove index:[r unsignedIntegerValue]
            associatedIndex:movedTo[r] ? [movedTo[r] unsignedIntegerValue] : NSNotFound]];
    for (NSNumber *ins in insertions)
        [changes addObject:[NSOrderedCollectionChange changeWithObject:omitInserted ? nil : to[[ins unsignedIntegerValue]]
            type:NSCollectionChangeInsert index:[ins unsignedIntegerValue]
            associatedIndex:movedFrom[ins] ? [movedFrom[ins] unsignedIntegerValue] : NSNotFound]];
    return [[NSOrderedCollectionDifference alloc] initWithChanges:changes];
}

/* Removals (descending) from the array, then insertions (ascending). An
 * insertion without its object takes the one its move removed. */
NSArray *_NSArrayByApplyingDifference(NSArray *array, NSOrderedCollectionDifference *difference) {
    NSMutableArray *result = [array mutableCopy];
    NSMutableDictionary *removed = [NSMutableDictionary dictionary];
    for (NSOrderedCollectionChange *change in difference.removals) {
        if (change.index >= [result count])
            return nil;
        removed[@(change.index)] = result[change.index];
        [result removeObjectAtIndex:change.index];
    }
    for (NSOrderedCollectionChange *change in difference.insertions) {
        id object = change.object;
        if (object == nil && change.associatedIndex != NSNotFound)
            object = removed[@(change.associatedIndex)];
        if (object == nil || change.index > [result count])
            return nil;
        [result insertObject:object atIndex:change.index];
    }
    return result;
}

@implementation NSArray (NSArrayDiffing)

- (NSOrderedCollectionDifference *)differenceFromArray:(NSArray *)other withOptions:(NSOrderedCollectionDifferenceCalculationOptions)options usingEquivalenceTest:(BOOL (^)(id, id))block {
    return _NSOrderedDifference(other, self, options, block);
}

- (NSOrderedCollectionDifference *)differenceFromArray:(NSArray *)other withOptions:(NSOrderedCollectionDifferenceCalculationOptions)options {
    return _NSOrderedDifference(other, self, options, nil);
}

- (NSOrderedCollectionDifference *)differenceFromArray:(NSArray *)other {
    return _NSOrderedDifference(other, self, 0, nil);
}

- (NSArray *)arrayByApplyingDifference:(NSOrderedCollectionDifference *)difference {
    return _NSArrayByApplyingDifference(self, difference);
}

@end

@implementation NSMutableArray (NSMutableArrayDiffing)

- (void)applyDifference:(NSOrderedCollectionDifference *)difference {
    NSArray *result = _NSArrayByApplyingDifference(self, difference);
    if (result == nil)
        [NSException raise:NSInvalidArgumentException format:@"the difference does not apply to this array"];
    [self setArray:result];
}

@end
