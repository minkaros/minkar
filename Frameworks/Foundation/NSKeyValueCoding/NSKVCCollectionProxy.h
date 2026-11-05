/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/NSProxy.h>

@class NSString;

// What -mutableSetValueForKey: and -mutableOrderedSetValueForKey: return:
// a set (or ordered set) whose changes go to the object's property.
@interface NSKVCCollectionProxy : NSProxy {
    id _object;
    NSString *_key;
    BOOL _ordered;
}

- (id)initWithKey:(NSString *)key forProxyObject:(id)object ordered:(BOOL)ordered;

@end
