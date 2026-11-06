/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/NSObject.h>

@class NSString, NSArray<ObjectType>;

NS_ASSUME_NONNULL_BEGIN

// The results that have one value of a grouping attribute.
@interface NSMetadataQueryResultGroup : NSObject {
    NSString *_attribute;
    id _value;
    NSArray *_results;
}

@property (readonly, copy) NSString *attribute;
@property (readonly, retain) id value;
@property (nullable, readonly, copy) NSArray<NSMetadataQueryResultGroup *> *subgroups;
@property (readonly) NSUInteger resultCount;
- (id)resultAtIndex:(NSUInteger)idx;
@property (readonly, copy) NSArray *results;

@end

NS_ASSUME_NONNULL_END
