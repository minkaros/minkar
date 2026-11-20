// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>

@class NSString;

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const NSOperationNotSupportedForKeyException;

/* Key-value coding for scripting: the elements of a to-many property by
   index, name or unique ID, inserted, removed and replaced (through the
   KVC indexed accessors, or the property's array), and a value coerced to
   a property's type. */
@interface NSObject (NSScriptKeyValueCoding)
- (nullable id)valueAtIndex:(NSUInteger)index inPropertyWithKey:(NSString *)key;
- (nullable id)valueWithName:(NSString *)name inPropertyWithKey:(NSString *)key;
- (nullable id)valueWithUniqueID:(id)uniqueID inPropertyWithKey:(NSString *)key;
- (void)insertValue:(id)value atIndex:(NSUInteger)index inPropertyWithKey:(NSString *)key;
- (void)removeValueAtIndex:(NSUInteger)index fromPropertyWithKey:(NSString *)key;
- (void)replaceValueAtIndex:(NSUInteger)index inPropertyWithKey:(NSString *)key withValue:(id)value;
- (void)insertValue:(id)value inPropertyWithKey:(NSString *)key;
- (nullable id)coerceValue:(nullable id)value forKey:(NSString *)key;
@end

NS_ASSUME_NONNULL_END
