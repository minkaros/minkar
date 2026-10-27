/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSZone.h>
#import <Foundation/NSPointerFunctions.h>

@class NSArray, NSMapTable;
@class NSDictionary<KeyType, ObjectType>;

typedef struct {
    NSMapTable *table;
    NSInteger i;
    struct _NSMapNode *j;
} NSMapEnumerator;

typedef struct {
    NSUInteger (*hash)(NSMapTable *table, const void *);
    BOOL (*isEqual)(NSMapTable *table, const void *, const void *);
    void (*retain)(NSMapTable *table, const void *);
    void (*release)(NSMapTable *table, void *);
    NSString *(*describe)(NSMapTable *table, const void *);
    const void *notAKeyMarker;
} NSMapTableKeyCallBacks;

typedef struct {
    void (*retain)(NSMapTable *table, const void *);
    void (*release)(NSMapTable *table, void *);
    NSString *(*describe)(NSMapTable *table, const void *);
} NSMapTableValueCallBacks;

// Minkar: weak keys and values are zeroing weak references, as on macOS
// (they were only unretained, so a freed object left a dangling pointer);
// an entry goes away with its weak key or value.
enum {
    NSMapTableStrongMemory = NSPointerFunctionsStrongMemory,
    NSMapTableZeroingWeakMemory = NSPointerFunctionsZeroingWeakMemory,
    NSMapTableCopyIn = NSPointerFunctionsCopyIn,
    NSMapTableObjectPointerPersonality = NSPointerFunctionsObjectPointerPersonality,
    NSMapTableWeakMemory = NSPointerFunctionsWeakMemory,
};
typedef NSUInteger NSMapTableOptions;

@interface NSMapTable<KeyType, ObjectType> : NSObject <NSCopying, NSFastEnumeration> {
    NSMapTableKeyCallBacks *keyCallBacks;
    NSMapTableValueCallBacks *valueCallBacks;
    NSUInteger count;
    NSUInteger nBuckets;
    struct _NSMapNode **buckets;
    NSPointerFunctionsOptions keyOptions;
    NSPointerFunctionsOptions valueOptions;
    BOOL weakKeys;
    BOOL weakValues;
    unsigned long mutations;
}

- (instancetype)initWithKeyOptions:(NSPointerFunctionsOptions)keyOptions
                      valueOptions:(NSPointerFunctionsOptions)valueOptions
                          capacity:(NSUInteger)initialCapacity;
+ (NSMapTable<KeyType, ObjectType> *)mapTableWithKeyOptions:(NSPointerFunctionsOptions)keyOptions
                                               valueOptions:(NSPointerFunctionsOptions)valueOptions;

+ (id)mapTableWithStrongToStrongObjects;
+ (id)mapTableWithStrongToWeakObjects;
+ (id)mapTableWithWeakToStrongObjects;
+ (id)mapTableWithWeakToWeakObjects;

+ (NSMapTable<KeyType, ObjectType> *)strongToStrongObjectsMapTable;
+ (NSMapTable<KeyType, ObjectType> *)strongToWeakObjectsMapTable;
+ (NSMapTable<KeyType, ObjectType> *)weakToStrongObjectsMapTable;
+ (NSMapTable<KeyType, ObjectType> *)weakToWeakObjectsMapTable;

@property (readonly, copy) NSPointerFunctions *keyPointerFunctions;
@property (readonly, copy) NSPointerFunctions *valuePointerFunctions;

- (ObjectType)objectForKey:(KeyType)key;
- (void)removeObjectForKey:(KeyType)key;
- (void)setObject:(ObjectType)object forKey:(KeyType)key;

@property (readonly) NSUInteger count;

- (NSEnumerator<KeyType> *)keyEnumerator;
- (NSEnumerator<ObjectType> *)objectEnumerator;
- (void)removeAllObjects;
- (NSDictionary<KeyType, ObjectType> *)dictionaryRepresentation;

@end

FOUNDATION_EXPORT const void *NSNotAnIntMapKey;
FOUNDATION_EXPORT const void *NSNotAPointerMapKey;

FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSIntMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSIntMapValueCallBacks;

FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSIntegerMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSIntegerMapValueCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSNonOwnedPointerMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSNonOwnedPointerMapValueCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSNonOwnedPointerOrNullMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSNonRetainedObjectMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSNonRetainedObjectMapValueCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSObjectMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSObjectMapValueCallBacks;
FOUNDATION_EXPORT const NSMapTableKeyCallBacks NSOwnedPointerMapKeyCallBacks;
FOUNDATION_EXPORT const NSMapTableValueCallBacks NSOwnedPointerMapValueCallBacks;

FOUNDATION_EXPORT NSMapTable *NSCreateMapTable(NSMapTableKeyCallBacks keyCallBacks,
    NSMapTableValueCallBacks valueCallBacks, NSUInteger capacity);
FOUNDATION_EXPORT NSMapTable *NSCreateMapTableWithZone(NSMapTableKeyCallBacks keyCallBacks,
    NSMapTableValueCallBacks valueCallBacks, NSUInteger capacity, NSZone *zone);
FOUNDATION_EXPORT NSMapTable *NSCopyMapTableWithZone(NSMapTable *table, NSZone *zone);
FOUNDATION_EXPORT void NSFreeMapTable(NSMapTable *table);
FOUNDATION_EXPORT void NSResetMapTable(NSMapTable *table);
FOUNDATION_EXPORT BOOL NSCompareMapTables(NSMapTable *table1, NSMapTable *table2);
FOUNDATION_EXPORT NSUInteger NSCountMapTable(NSMapTable *table);
FOUNDATION_EXPORT BOOL NSMapMember(NSMapTable *table, const void *key, void **originalKey,
    void **value);
FOUNDATION_EXPORT void *NSMapGet(NSMapTable *table, const void *key);
FOUNDATION_EXPORT NSMapEnumerator NSEnumerateMapTable(NSMapTable *table);
FOUNDATION_EXPORT BOOL NSNextMapEnumeratorPair(NSMapEnumerator *enumerator, void **key,
    void **value);
FOUNDATION_EXPORT void NSEndMapTableEnumeration(NSMapEnumerator *enumerator);
FOUNDATION_EXPORT NSArray *NSAllMapTableKeys(NSMapTable *table);
FOUNDATION_EXPORT NSArray *NSAllMapTableValues(NSMapTable *table);
FOUNDATION_EXPORT void NSMapInsert(NSMapTable *table, const void *key, const void *value);
FOUNDATION_EXPORT void *NSMapInsertIfAbsent(NSMapTable *table, const void *key, const void *value);
FOUNDATION_EXPORT void NSMapInsertKnownAbsent(NSMapTable *table, const void *key,
    const void *value);
FOUNDATION_EXPORT void NSMapRemove(NSMapTable *table, const void *key);
FOUNDATION_EXPORT NSString *NSStringFromMapTable(NSMapTable *table);
