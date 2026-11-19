/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreData/NSAtomicStore.h>

@class NSMutableDictionary, NSMutableSet;

/* NSSQLiteStoreType: a SQLite database in the reference's layout (a table
   an entity hierarchy, Z_PK/Z_ENT/Z_OPT, Z_PRIMARYKEY, Z_METADATA), read
   whole as the store loads and written by its changes as it saves. */
@interface NSSQLitePersistentStore : NSAtomicStore {
    void *_db;                                  /* sqlite3 * */
    NSMutableDictionary *_referenceToCacheNode; /* "Entity/pN" -> node */
    NSMutableDictionary *_entityNumbers;        /* root entity name -> Z_ENT */
    NSMutableDictionary *_maxKeys;              /* root entity name -> Z_MAX */
    NSMutableSet *_inserted, *_updated, *_deleted; /* nodes to write */
}

@end
