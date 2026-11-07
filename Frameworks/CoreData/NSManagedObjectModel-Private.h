/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <CoreData/NSManagedObjectModel.h>

@interface NSManagedObjectModel (private)
- (void)_checkMutable;
- (void)_setImmutable;
- (NSEntityDescription *)_entityForStoreName:(NSString *)name;
@end
