/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/NSObject.h>

@class NSString, NSArray<ObjectType>, NSDictionary<KeyType, ObjectType>, NSURL;

NS_ASSUME_NONNULL_BEGIN

// A file a metadata query found, and its attributes.
@interface NSMetadataItem : NSObject {
    NSURL *_URL;
    NSDictionary *_attributes;
}

- (nullable instancetype)initWithURL:(NSURL *)url;
- (nullable id)valueForAttribute:(NSString *)key;
- (nullable NSDictionary<NSString *, id> *)valuesForAttributes:(NSArray<NSString *> *)keys;
@property (readonly, copy) NSArray<NSString *> *attributes;

@end

NS_ASSUME_NONNULL_END
