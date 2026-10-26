/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSZone.h>
#import <Foundation/NSPointerFunctions.h>
#import <Foundation/NSEnumerator.h>

@class NSArray<ObjectType>, NSSet<ObjectType>, NSEnumerator<ObjectType>, NSString;

NS_ASSUME_NONNULL_BEGIN

// Minkar: NSHashTable is a class, as on macOS 10.5 and later (it was a C
// struct); the C functions below take instances of it.

enum {
    NSHashTableStrongMemory = NSPointerFunctionsStrongMemory,
    NSHashTableZeroingWeakMemory = NSPointerFunctionsZeroingWeakMemory,
    NSHashTableCopyIn = NSPointerFunctionsCopyIn,
    NSHashTableObjectPointerPersonality = NSPointerFunctionsObjectPointerPersonality,
    NSHashTableWeakMemory = NSPointerFunctionsWeakMemory,
};
typedef NSUInteger NSHashTableOptions;

@interface NSHashTable<ObjectType> : NSObject <NSCopying, NSFastEnumeration>

- (instancetype)initWithOptions:(NSPointerFunctionsOptions)options capacity:(NSUInteger)initialCapacity;
- (instancetype)initWithPointerFunctions:(NSPointerFunctions *)functions capacity:(NSUInteger)initialCapacity;

+ (NSHashTable<ObjectType> *)hashTableWithOptions:(NSPointerFunctionsOptions)options;
+ (NSHashTable<ObjectType> *)weakObjectsHashTable;

@property (readonly, copy) NSPointerFunctions *pointerFunctions;
@property (readonly) NSUInteger count;

- (nullable ObjectType)member:(nullable ObjectType)object;
- (NSEnumerator<ObjectType> *)objectEnumerator;
- (void)addObject:(nullable ObjectType)object;
- (void)removeObject:(nullable ObjectType)object;
- (void)removeAllObjects;

@property (readonly, copy) NSArray<ObjectType> *allObjects;
@property (nullable, nonatomic, readonly) ObjectType anyObject;
- (BOOL)containsObject:(nullable ObjectType)anObject;

- (BOOL)intersectsHashTable:(NSHashTable<ObjectType> *)other;
- (BOOL)isEqualToHashTable:(NSHashTable<ObjectType> *)other;
- (BOOL)isSubsetOfHashTable:(NSHashTable<ObjectType> *)other;
- (void)intersectHashTable:(NSHashTable<ObjectType> *)other;
- (void)unionHashTable:(NSHashTable<ObjectType> *)other;
- (void)minusHashTable:(NSHashTable<ObjectType> *)other;

@property (readonly, copy) NSSet<ObjectType> *setRepresentation;

@end

// The C interface.

typedef struct {
    NSHashTable *table;
    NSUInteger i;
    struct NSHashBucket *j;
} NSHashEnumerator;

typedef struct {
    NSUInteger (*_Nullable hash)(NSHashTable *table, const void *);
    BOOL (*_Nullable isEqual)(NSHashTable *table, const void *, const void *);
    void (*_Nullable retain)(NSHashTable *table, const void *);
    void (*_Nullable release)(NSHashTable *table, void *);
    NSString *_Nonnull (*_Nullable describe)(NSHashTable *table, const void *);
} NSHashTableCallBacks;

FOUNDATION_EXPORT const NSHashTableCallBacks NSIntHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSIntegerHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSNonOwnedPointerHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSNonRetainedObjectHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSObjectHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSOwnedObjectIdentityHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSOwnedPointerHashCallBacks;
FOUNDATION_EXPORT const NSHashTableCallBacks NSPointerToStructHashCallBacks;

FOUNDATION_EXPORT NSHashTable *NSCreateHashTable(NSHashTableCallBacks callBacks, NSUInteger capacity);
FOUNDATION_EXPORT NSHashTable *NSCreateHashTableWithZone(NSHashTableCallBacks callBacks,
    NSUInteger capacity, NSZone *_Nullable zone);
FOUNDATION_EXPORT NSHashTable *NSCopyHashTableWithZone(NSHashTable *table, NSZone *_Nullable zone);
FOUNDATION_EXPORT void NSFreeHashTable(NSHashTable *table);
FOUNDATION_EXPORT void NSResetHashTable(NSHashTable *table);
FOUNDATION_EXPORT BOOL NSCompareHashTables(NSHashTable *table1, NSHashTable *table2);
FOUNDATION_EXPORT NSUInteger NSCountHashTable(NSHashTable *table);
FOUNDATION_EXPORT void *_Nullable NSHashGet(NSHashTable *table, const void *_Nullable pointer);
FOUNDATION_EXPORT NSArray *NSAllHashTableObjects(NSHashTable *table);
FOUNDATION_EXPORT NSHashEnumerator NSEnumerateHashTable(NSHashTable *table);
FOUNDATION_EXPORT void *_Nullable NSNextHashEnumeratorItem(NSHashEnumerator *enumerator);
FOUNDATION_EXPORT void NSEndHashTableEnumeration(NSHashEnumerator *enumerator);
FOUNDATION_EXPORT void NSHashInsert(NSHashTable *table, const void *_Nullable pointer);
FOUNDATION_EXPORT void NSHashInsertKnownAbsent(NSHashTable *table, const void *_Nullable pointer);
FOUNDATION_EXPORT void *_Nullable NSHashInsertIfAbsent(NSHashTable *table, const void *_Nullable pointer);
FOUNDATION_EXPORT void NSHashRemove(NSHashTable *table, const void *_Nullable pointer);
FOUNDATION_EXPORT NSString *NSStringFromHashTable(NSHashTable *table);

NS_ASSUME_NONNULL_END
