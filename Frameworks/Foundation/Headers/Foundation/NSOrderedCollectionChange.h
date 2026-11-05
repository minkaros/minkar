/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * One change of an ordered collection's difference: an object inserted or
 * removed at an index, and with moves the index it came from or went to.
 */

#import <Foundation/NSObject.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, NSCollectionChangeType) {
    NSCollectionChangeInsert,
    NSCollectionChangeRemove
};

@interface NSOrderedCollectionChange<ObjectType> : NSObject

+ (NSOrderedCollectionChange<ObjectType> *)changeWithObject:(nullable ObjectType)anObject type:(NSCollectionChangeType)type index:(NSUInteger)index;
+ (NSOrderedCollectionChange<ObjectType> *)changeWithObject:(nullable ObjectType)anObject type:(NSCollectionChangeType)type index:(NSUInteger)index associatedIndex:(NSUInteger)associatedIndex;

@property (readonly, strong, nullable) ObjectType object;
@property (readonly) NSCollectionChangeType changeType;
@property (readonly) NSUInteger index;
@property (readonly) NSUInteger associatedIndex;

- (id)init NS_UNAVAILABLE;
- (instancetype)initWithObject:(nullable ObjectType)anObject type:(NSCollectionChangeType)type index:(NSUInteger)index;
- (instancetype)initWithObject:(nullable ObjectType)anObject type:(NSCollectionChangeType)type index:(NSUInteger)index associatedIndex:(NSUInteger)associatedIndex NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END
