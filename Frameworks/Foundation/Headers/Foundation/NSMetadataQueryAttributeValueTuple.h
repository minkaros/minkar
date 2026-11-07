/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/NSObject.h>

@class NSString;

NS_ASSUME_NONNULL_BEGIN

// A value an attribute has in a query's results, and how many have it.
@interface NSMetadataQueryAttributeValueTuple : NSObject {
    NSString *_attribute;
    id _value;
    NSUInteger _count;
}

@property (readonly, copy) NSString *attribute;
@property (nullable, readonly, retain) id value;
@property (readonly) NSUInteger count;

@end

NS_ASSUME_NONNULL_END
