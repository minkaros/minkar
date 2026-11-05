/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The changes that make one ordered collection into another: removals at
 * the old collection's indexes, insertions at the new one's.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSOrderedCollectionChange.h>
#import <Foundation/NSEnumerator.h>

@class NSIndexSet, NSArray<ObjectType>;

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, NSOrderedCollectionDifferenceCalculationOptions) {
    NSOrderedCollectionDifferenceCalculationOmitInsertedObjects = (1 << 0UL),
    NSOrderedCollectionDifferenceCalculationOmitRemovedObjects = (1 << 1UL),
    NSOrderedCollectionDifferenceCalculationInferMoves = (1 << 2UL)
};

@interface NSOrderedCollectionDifference<ObjectType> : NSObject <NSFastEnumeration> {
    NSArray *_insertions;
    NSArray *_removals;
}

- (instancetype)initWithChanges:(NSArray<NSOrderedCollectionChange<ObjectType> *> *)changes;
- (instancetype)initWithInsertIndexes:(NSIndexSet *)inserts insertedObjects:(nullable NSArray<ObjectType> *)insertedObjects removeIndexes:(NSIndexSet *)removes removedObjects:(nullable NSArray<ObjectType> *)removedObjects additionalChanges:(NSArray<NSOrderedCollectionChange<ObjectType> *> *)changes NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithInsertIndexes:(NSIndexSet *)inserts insertedObjects:(nullable NSArray<ObjectType> *)insertedObjects removeIndexes:(NSIndexSet *)removes removedObjects:(nullable NSArray<ObjectType> *)removedObjects;

@property (strong, readonly) NSArray<NSOrderedCollectionChange<ObjectType> *> *insertions;
@property (strong, readonly) NSArray<NSOrderedCollectionChange<ObjectType> *> *removals;
@property (assign, readonly) BOOL hasChanges;

- (NSOrderedCollectionDifference<id> *)differenceByTransformingChangesWithBlock:(NSOrderedCollectionChange<id> *(NS_NOESCAPE ^)(NSOrderedCollectionChange<ObjectType> *))block;
- (instancetype)inverseDifference;

@end

/* The difference's changes applied to an array; nil when they do not fit. */
FOUNDATION_EXPORT NSArray *_Nullable _NSArrayByApplyingDifference(NSArray *array, NSOrderedCollectionDifference *difference);
/* The difference from one list to another, by a common subsequence. */
FOUNDATION_EXPORT NSOrderedCollectionDifference *_NSOrderedDifference(NSArray *from, NSArray *to, NSOrderedCollectionDifferenceCalculationOptions options, BOOL (^_Nullable equal)(id, id));

NS_ASSUME_NONNULL_END
