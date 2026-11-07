/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <CoreData/NSPropertyDescription.h>

@class NSMutableArray;

@interface NSPropertyDescription (private)
- (void)_setEntity:(NSEntityDescription *)entity;
- (void)_checkMutable;
- (void)_copyPropertiesFrom:(NSPropertyDescription *)other;
- (void)_appendVersionHashFields:(NSMutableArray *)fields;
@end

// SHA-256 of the fields joined: the version hash of a model's parts.
NSData *_NSCoreDataVersionHash(NSArray *fields);
