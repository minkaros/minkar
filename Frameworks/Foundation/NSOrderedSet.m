/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Ordered sets (API 19): an array for the order and a set for membership,
 * kept together; the mutable one changes both. ARC.
 */

#import <Foundation/NSOrderedSet.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSMutableArray.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSMutableSet.h>
#import <Foundation/NSIndexSet.h>
#import <Foundation/NSMutableIndexSet.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#include <stdarg.h>

@implementation NSOrderedSet

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)init {
    return [self initWithObjects:NULL count:0];
}

- (instancetype)initWithObjects:(const id [])objects count:(NSUInteger)cnt {
    if ((self = [super init]) == nil)
        return nil;
    _array = [NSMutableArray arrayWithCapacity:cnt];
    _set = [NSMutableSet setWithCapacity:cnt];
    for (NSUInteger i = 0; i < cnt; i++)
        if (![_set containsObject:objects[i]]) {
            [_array addObject:objects[i]];
            [_set addObject:objects[i]];
        }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSArray *array = [coder allowsKeyedCoding] ? [coder decodeObjectForKey:@"NS.objects"] : [coder decodeObject];
    return [self initWithArray:array ?: @[]];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    if ([coder allowsKeyedCoding])
        [coder encodeObject:[_array copy] forKey:@"NS.objects"];
    else
        [coder encodeObject:[_array copy]];
}

- (id)copyWithZone:(NSZone *)zone {
    return [[NSOrderedSet allocWithZone:zone] initWithArray:_array];
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return [[NSMutableOrderedSet allocWithZone:zone] initWithArray:_array];
}

- (NSUInteger)count {
    return [_array count];
}

- (id)objectAtIndex:(NSUInteger)idx {
    return [_array objectAtIndex:idx];
}

- (NSUInteger)indexOfObject:(id)object {
    return [_set containsObject:object] ? [_array indexOfObject:object] : NSNotFound;
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(__unsafe_unretained id [])buffer count:(NSUInteger)len {
    return [_array countByEnumeratingWithState:state objects:(__autoreleasing id *)(void *)buffer count:len];
}

- (NSUInteger)hash {
    return [_array count];
}

- (BOOL)isEqual:(id)other {
    return other == self || ([other isKindOfClass:[NSOrderedSet class]] && [self isEqualToOrderedSet:other]);
}

@end

@implementation NSOrderedSet (NSExtendedOrderedSet)

- (void)getObjects:(id __unsafe_unretained [])objects range:(NSRange)range {
    [_array getObjects:(__autoreleasing id *)(void *)objects range:range];
}

- (NSArray *)objectsAtIndexes:(NSIndexSet *)indexes {
    return [_array objectsAtIndexes:indexes];
}

- (id)firstObject {
    return [_array firstObject];
}

- (id)lastObject {
    return [_array lastObject];
}

- (BOOL)isEqualToOrderedSet:(NSOrderedSet *)other {
    return [_array isEqualToArray:[other array]];
}

- (BOOL)containsObject:(id)object {
    return [_set containsObject:object];
}

- (BOOL)intersectsOrderedSet:(NSOrderedSet *)other {
    return [_set intersectsSet:[other set]];
}

- (BOOL)intersectsSet:(NSSet *)set {
    return [_set intersectsSet:set];
}

- (BOOL)isSubsetOfOrderedSet:(NSOrderedSet *)other {
    return [_set isSubsetOfSet:[other set]];
}

- (BOOL)isSubsetOfSet:(NSSet *)set {
    return [_set isSubsetOfSet:set];
}

- (id)objectAtIndexedSubscript:(NSUInteger)idx {
    return [_array objectAtIndex:idx];
}

- (NSEnumerator *)objectEnumerator {
    return [[_array copy] objectEnumerator];
}

- (NSEnumerator *)reverseObjectEnumerator {
    return [[_array copy] reverseObjectEnumerator];
}

- (NSOrderedSet *)reversedOrderedSet {
    return [NSOrderedSet orderedSetWithArray:[[_array reverseObjectEnumerator] allObjects]];
}

- (NSArray *)array {
    return [_array copy];
}

- (NSSet *)set {
    return [_set copy];
}

- (void)enumerateObjectsUsingBlock:(void (^)(id, NSUInteger, BOOL *))block {
    [[_array copy] enumerateObjectsUsingBlock:block];
}

- (void)enumerateObjectsWithOptions:(NSEnumerationOptions)opts usingBlock:(void (^)(id, NSUInteger, BOOL *))block {
    [[_array copy] enumerateObjectsWithOptions:opts usingBlock:block];
}

- (void)enumerateObjectsAtIndexes:(NSIndexSet *)s options:(NSEnumerationOptions)opts usingBlock:(void (^)(id, NSUInteger, BOOL *))block {
    [[_array copy] enumerateObjectsAtIndexes:s options:opts usingBlock:block];
}

- (NSUInteger)indexOfObjectPassingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    return [_array indexOfObjectPassingTest:predicate];
}

- (NSUInteger)indexOfObjectWithOptions:(NSEnumerationOptions)opts passingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    return [_array indexOfObjectWithOptions:opts passingTest:predicate];
}

- (NSUInteger)indexOfObjectAtIndexes:(NSIndexSet *)s options:(NSEnumerationOptions)opts passingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    return [_array indexOfObjectAtIndexes:s options:opts passingTest:predicate];
}

- (NSIndexSet *)indexesOfObjectsPassingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    return [_array indexesOfObjectsPassingTest:predicate];
}

- (NSIndexSet *)indexesOfObjectsWithOptions:(NSEnumerationOptions)opts passingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    return [_array indexesOfObjectsWithOptions:opts passingTest:predicate];
}

- (NSIndexSet *)indexesOfObjectsAtIndexes:(NSIndexSet *)s options:(NSEnumerationOptions)opts passingTest:(BOOL (^)(id, NSUInteger, BOOL *))predicate {
    return [_array indexesOfObjectsAtIndexes:s options:opts passingTest:predicate];
}

- (NSUInteger)indexOfObject:(id)object inSortedRange:(NSRange)range options:(NSBinarySearchingOptions)opts usingComparator:(NSComparator)cmp {
    return [_array indexOfObject:object inSortedRange:range options:opts usingComparator:cmp];
}

- (NSArray *)sortedArrayUsingComparator:(NSComparator)cmptr {
    return [_array sortedArrayUsingComparator:cmptr];
}

- (NSArray *)sortedArrayWithOptions:(NSSortOptions)opts usingComparator:(NSComparator)cmptr {
    return [_array sortedArrayWithOptions:opts usingComparator:cmptr];
}

- (NSString *)description {
    return [self descriptionWithLocale:nil indent:0];
}

- (NSString *)descriptionWithLocale:(id)locale {
    return [self descriptionWithLocale:locale indent:0];
}

- (NSString *)descriptionWithLocale:(id)locale indent:(NSUInteger)level {
    NSMutableString *s = [NSMutableString stringWithString:@"{("];
    BOOL first = YES;
    for (id object in _array) {
        [s appendFormat:@"%@\n    %@", first ? @"" : @",", object];
        first = NO;
    }
    [s appendString:@"\n)}"];
    return s;
}

@end

@implementation NSOrderedSet (NSOrderedSetCreation)

+ (instancetype)orderedSet {
    return [[self alloc] init];
}

+ (instancetype)orderedSetWithObject:(id)object {
    return [[self alloc] initWithObject:object];
}

+ (instancetype)orderedSetWithObjects:(const id [])objects count:(NSUInteger)cnt {
    return [[self alloc] initWithObjects:objects count:cnt];
}

+ (instancetype)orderedSetWithObjects:(id)firstObj, ... {
    NSMutableArray *objects = [NSMutableArray array];
    va_list args;
    va_start(args, firstObj);
    for (id object = firstObj; object != nil; object = va_arg(args, id))
        [objects addObject:object];
    va_end(args);
    return [[self alloc] initWithArray:objects];
}

+ (instancetype)orderedSetWithOrderedSet:(NSOrderedSet *)set {
    return [[self alloc] initWithOrderedSet:set];
}

+ (instancetype)orderedSetWithOrderedSet:(NSOrderedSet *)set range:(NSRange)range copyItems:(BOOL)flag {
    return [[self alloc] initWithOrderedSet:set range:range copyItems:flag];
}

+ (instancetype)orderedSetWithArray:(NSArray *)array {
    return [[self alloc] initWithArray:array];
}

+ (instancetype)orderedSetWithArray:(NSArray *)array range:(NSRange)range copyItems:(BOOL)flag {
    return [[self alloc] initWithArray:array range:range copyItems:flag];
}

+ (instancetype)orderedSetWithSet:(NSSet *)set {
    return [[self alloc] initWithSet:set];
}

+ (instancetype)orderedSetWithSet:(NSSet *)set copyItems:(BOOL)flag {
    return [[self alloc] initWithSet:set copyItems:flag];
}

- (instancetype)initWithObject:(id)object {
    return [self initWithObjects:&object count:1];
}

- (instancetype)initWithObjects:(id)firstObj, ... {
    NSMutableArray *objects = [NSMutableArray array];
    va_list args;
    va_start(args, firstObj);
    for (id object = firstObj; object != nil; object = va_arg(args, id))
        [objects addObject:object];
    va_end(args);
    return [self initWithArray:objects];
}

- (instancetype)initWithOrderedSet:(NSOrderedSet *)set {
    return [self initWithArray:[set array]];
}

- (instancetype)initWithOrderedSet:(NSOrderedSet *)set copyItems:(BOOL)flag {
    return [self initWithArray:[set array] copyItems:flag];
}

- (instancetype)initWithOrderedSet:(NSOrderedSet *)set range:(NSRange)range copyItems:(BOOL)flag {
    return [self initWithArray:[set array] range:range copyItems:flag];
}

- (instancetype)initWithArray:(NSArray *)array {
    return [self initWithArray:array range:NSMakeRange(0, [array count]) copyItems:NO];
}

- (instancetype)initWithArray:(NSArray *)array copyItems:(BOOL)flag {
    return [self initWithArray:array range:NSMakeRange(0, [array count]) copyItems:flag];
}

- (instancetype)initWithArray:(NSArray *)array range:(NSRange)range copyItems:(BOOL)flag {
    NSMutableArray *objects = [NSMutableArray arrayWithCapacity:range.length];
    for (NSUInteger i = range.location; i < NSMaxRange(range); i++) {
        id object = array[i];
        [objects addObject:flag ? [object copy] : object];
    }
    NSUInteger n = [objects count];
    __unsafe_unretained id *buffer = (__unsafe_unretained id *)calloc(n ? n : 1, sizeof(id));
    [objects getObjects:(__autoreleasing id *)(void *)buffer range:NSMakeRange(0, n)];
    self = [self initWithObjects:buffer count:n];
    free(buffer);
    return self;
}

- (instancetype)initWithSet:(NSSet *)set {
    return [self initWithArray:[set allObjects]];
}

- (instancetype)initWithSet:(NSSet *)set copyItems:(BOOL)flag {
    return [self initWithArray:[set allObjects] copyItems:flag];
}

@end

@implementation NSOrderedSet (NSOrderedSetDiffing)

- (NSOrderedCollectionDifference *)differenceFromOrderedSet:(NSOrderedSet *)other withOptions:(NSOrderedCollectionDifferenceCalculationOptions)options usingEquivalenceTest:(BOOL (^)(id, id))block {
    return _NSOrderedDifference([other array], _array, options, block);
}

- (NSOrderedCollectionDifference *)differenceFromOrderedSet:(NSOrderedSet *)other withOptions:(NSOrderedCollectionDifferenceCalculationOptions)options {
    return _NSOrderedDifference([other array], _array, options, nil);
}

- (NSOrderedCollectionDifference *)differenceFromOrderedSet:(NSOrderedSet *)other {
    return _NSOrderedDifference([other array], _array, 0, nil);
}

- (NSOrderedSet *)orderedSetByApplyingDifference:(NSOrderedCollectionDifference *)difference {
    NSArray *result = _NSArrayByApplyingDifference(_array, difference);
    return result != nil ? [NSOrderedSet orderedSetWithArray:result] : nil;
}

@end

@implementation NSMutableOrderedSet

- (instancetype)init {
    return [self initWithObjects:NULL count:0];
}

- (instancetype)initWithCapacity:(NSUInteger)numItems {
    return [self initWithObjects:NULL count:0];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super initWithCoder:coder];
}

- (void)insertObject:(id)object atIndex:(NSUInteger)idx {
    if (idx > [_array count])
        [NSException raise:NSRangeException format:@"index %lu beyond %lu", (unsigned long)idx, (unsigned long)[_array count]];
    if ([_set containsObject:object])
        return;
    [_array insertObject:object atIndex:idx];
    [_set addObject:object];
}

- (void)removeObjectAtIndex:(NSUInteger)idx {
    id object = [_array objectAtIndex:idx];
    [_array removeObjectAtIndex:idx];
    [_set removeObject:object];
}

/* Not when the object is elsewhere already: the set stays a set. */
- (void)replaceObjectAtIndex:(NSUInteger)idx withObject:(id)object {
    id old = [_array objectAtIndex:idx];
    if (old == object || [old isEqual:object]) {
        [_array replaceObjectAtIndex:idx withObject:object];
        return;
    }
    if ([_set containsObject:object])
        return;
    [_set removeObject:old];
    [_array replaceObjectAtIndex:idx withObject:object];
    [_set addObject:object];
}

@end

@implementation NSMutableOrderedSet (NSExtendedMutableOrderedSet)

- (void)addObject:(id)object {
    [self insertObject:object atIndex:[_array count]];
}

- (void)addObjects:(const id [])objects count:(NSUInteger)count {
    for (NSUInteger i = 0; i < count; i++)
        [self addObject:objects[i]];
}

- (void)addObjectsFromArray:(NSArray *)array {
    for (id object in array)
        [self addObject:object];
}

- (void)exchangeObjectAtIndex:(NSUInteger)idx1 withObjectAtIndex:(NSUInteger)idx2 {
    [_array exchangeObjectAtIndex:idx1 withObjectAtIndex:idx2];
}

- (void)moveObjectsAtIndexes:(NSIndexSet *)indexes toIndex:(NSUInteger)idx {
    NSArray *moving = [_array objectsAtIndexes:indexes];
    [_array removeObjectsAtIndexes:indexes];
    [_array insertObjects:moving atIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(idx, [moving count])]];
}

- (void)insertObjects:(NSArray *)objects atIndexes:(NSIndexSet *)indexes {
    __block NSUInteger n = 0;
    [indexes enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        [self insertObject:objects[n++] atIndex:idx];
    }];
}

- (void)setObject:(id)obj atIndex:(NSUInteger)idx {
    if (idx == [_array count])
        [self addObject:obj];
    else
        [self replaceObjectAtIndex:idx withObject:obj];
}

- (void)setObject:(id)obj atIndexedSubscript:(NSUInteger)idx {
    [self setObject:obj atIndex:idx];
}

- (void)replaceObjectsInRange:(NSRange)range withObjects:(const id [])objects count:(NSUInteger)count {
    [self removeObjectsInRange:range];
    for (NSUInteger i = 0; i < count; i++)
        [self insertObject:objects[i] atIndex:range.location + i];
}

- (void)replaceObjectsAtIndexes:(NSIndexSet *)indexes withObjects:(NSArray *)objects {
    __block NSUInteger n = 0;
    [indexes enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        [self replaceObjectAtIndex:idx withObject:objects[n++]];
    }];
}

- (void)removeObjectsInRange:(NSRange)range {
    for (NSUInteger i = NSMaxRange(range); i-- > range.location;)
        [self removeObjectAtIndex:i];
}

- (void)removeObjectsAtIndexes:(NSIndexSet *)indexes {
    [indexes enumerateIndexesWithOptions:NSEnumerationReverse usingBlock:^(NSUInteger idx, BOOL *stop) {
        [self removeObjectAtIndex:idx];
    }];
}

- (void)removeAllObjects {
    [_array removeAllObjects];
    [_set removeAllObjects];
}

- (void)removeObject:(id)object {
    NSUInteger idx = [self indexOfObject:object];
    if (idx != NSNotFound)
        [self removeObjectAtIndex:idx];
}

- (void)removeObjectsInArray:(NSArray *)array {
    for (id object in array)
        [self removeObject:object];
}

- (void)intersectOrderedSet:(NSOrderedSet *)other {
    [self intersectSet:[other set]];
}

- (void)minusOrderedSet:(NSOrderedSet *)other {
    [self minusSet:[other set]];
}

- (void)unionOrderedSet:(NSOrderedSet *)other {
    [self addObjectsFromArray:[other array]];
}

- (void)intersectSet:(NSSet *)other {
    for (NSUInteger i = [_array count]; i-- > 0;)
        if (![other containsObject:_array[i]])
            [self removeObjectAtIndex:i];
}

- (void)minusSet:(NSSet *)other {
    for (NSUInteger i = [_array count]; i-- > 0;)
        if ([other containsObject:_array[i]])
            [self removeObjectAtIndex:i];
}

- (void)unionSet:(NSSet *)other {
    [self addObjectsFromArray:[other allObjects]];
}

- (void)sortUsingComparator:(NSComparator)cmptr {
    [_array sortUsingComparator:cmptr];
}

- (void)sortWithOptions:(NSSortOptions)opts usingComparator:(NSComparator)cmptr {
    [_array sortWithOptions:opts usingComparator:cmptr];
}

- (void)sortRange:(NSRange)range options:(NSSortOptions)opts usingComparator:(NSComparator)cmptr {
    NSArray *sorted = [[_array subarrayWithRange:range] sortedArrayWithOptions:opts usingComparator:cmptr];
    [_array replaceObjectsInRange:range withObjectsFromArray:sorted];
}

@end

@implementation NSMutableOrderedSet (NSMutableOrderedSetCreation)

+ (instancetype)orderedSetWithCapacity:(NSUInteger)numItems {
    return [[self alloc] initWithCapacity:numItems];
}

@end

@implementation NSMutableOrderedSet (NSMutableOrderedSetDiffing)

- (void)applyDifference:(NSOrderedCollectionDifference *)difference {
    NSArray *result = _NSArrayByApplyingDifference(_array, difference);
    if (result == nil)
        [NSException raise:NSInvalidArgumentException format:@"the difference does not apply to this ordered set"];
    [self removeAllObjects];
    [self addObjectsFromArray:result];
}

@end
