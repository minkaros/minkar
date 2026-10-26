/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

// NSHashTable, the class (macOS 10.5 and later), and the C functions over
// it. Minkar: this replaces a C-only hash table; the class was missing.
//
// Items live in chained buckets. Every table works through one set of
// callbacks: from NSPointerFunctionsOptions for tables made with options,
// or the caller's for NSCreateHashTable. Weak tables keep each item with
// objc_storeWeak in its node; an item whose object is gone reads as NULL
// and its node is dropped when found.

#import <Foundation/NSHashTable.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSString.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSException.h>
#import <Foundation/NSData.h>
#import <objc/runtime.h>

#include <stdlib.h>
#include <string.h>

// The ARC runtime's weak references, used by hand (this file is MRC).
OBJC_EXPORT id objc_storeWeak(id *location, id obj);
OBJC_EXPORT id objc_loadWeakRetained(id *location);
OBJC_EXPORT void objc_destroyWeak(id *location);

typedef struct NSHashBucket {
    struct NSHashBucket *next;
    void *item;
} NSHashBucket;

@interface NSHashTable () {
@public
    NSPointerFunctionsOptions _options;
    BOOL _fromOptions;                  // NO: C callbacks
    BOOL _weak;
    NSHashTableCallBacks _callBacks;
    NSUInteger _count;                  // nodes, including cleared weak ones
    NSUInteger _nBuckets;
    NSHashBucket **_buckets;
    unsigned long _mutations;
}
- (instancetype)_initWithCapacity:(NSUInteger)capacity;
@end

static NSUInteger pointerHash(const void *p) {
    uintptr_t v = (uintptr_t)p;
    return (NSUInteger)((v >> 4) ^ (v >> 16));
}

/* ---- Callbacks from NSPointerFunctionsOptions ---------------------------- */

#define PERSONALITY(o) ((o) & 0xff00)
#define MEMORY(o) ((o) & 0xff)

static NSUInteger optionsHash(NSHashTable *table, const void *item) {
    switch (PERSONALITY(table->_options)) {
    case NSPointerFunctionsObjectPersonality:
        return [(id)item hash];
    case NSPointerFunctionsCStringPersonality: {
        NSUInteger h = 5381;
        for (const unsigned char *c = item; *c; c++)
            h = h * 33 + *c;
        return h;
    }
    case NSPointerFunctionsIntegerPersonality:
        return (NSUInteger)(uintptr_t)item;
    default:
        return pointerHash(item);
    }
}

static BOOL optionsIsEqual(NSHashTable *table, const void *a, const void *b) {
    switch (PERSONALITY(table->_options)) {
    case NSPointerFunctionsObjectPersonality:
        return a == b || [(id)a isEqual:(id)b];
    case NSPointerFunctionsCStringPersonality:
        return strcmp(a, b) == 0;
    default:
        return a == b;
    }
}

static BOOL holdsObjects(NSPointerFunctionsOptions options) {
    NSUInteger p = PERSONALITY(options);
    return p == NSPointerFunctionsObjectPersonality || p == NSPointerFunctionsObjectPointerPersonality;
}

static void optionsRetain(NSHashTable *table, const void *item) {
    if (MEMORY(table->_options) == NSPointerFunctionsStrongMemory && holdsObjects(table->_options))
        [(id)item retain];
}

static void optionsRelease(NSHashTable *table, void *item) {
    switch (MEMORY(table->_options)) {
    case NSPointerFunctionsStrongMemory:
        if (holdsObjects(table->_options))
            [(id)item release];
        break;
    case NSPointerFunctionsMallocMemory:
        free(item);
        break;
    default:
        break;
    }
}

static NSString *optionsDescribe(NSHashTable *table, const void *item) {
    if (holdsObjects(table->_options))
        return [(id)item description];
    if (PERSONALITY(table->_options) == NSPointerFunctionsCStringPersonality)
        return [NSString stringWithUTF8String:item];
    return [NSString stringWithFormat:@"%p", item];
}

/* ---- The table ---------------------------------------------------------- */

static NSUInteger hashOf(NSHashTable *t, const void *item) {
    return t->_callBacks.hash ? t->_callBacks.hash(t, item) : pointerHash(item);
}

static BOOL equal(NSHashTable *t, const void *a, const void *b) {
    return t->_callBacks.isEqual ? t->_callBacks.isEqual(t, a, b) : a == b;
}

// The item in a node, retained and autoreleased for weak tables (NULL
// when its object is gone).
static void *nodeItem(NSHashTable *t, NSHashBucket *node) {
    if (!t->_weak)
        return node->item;
    return [objc_loadWeakRetained((id *)&node->item) autorelease];
}

static void setNodeItem(NSHashTable *t, NSHashBucket *node, const void *item) {
    if (t->_weak) {
        objc_storeWeak((id *)&node->item, (id)item);
    } else {
        if (t->_callBacks.retain)
            t->_callBacks.retain(t, item);
        node->item = (void *)(uintptr_t)item;     /* the table owns it now */
    }
}

static void freeNode(NSHashTable *t, NSHashBucket *node) {
    if (t->_weak)
        objc_destroyWeak((id *)&node->item);
    else if (t->_callBacks.release)
        t->_callBacks.release(t, node->item);
    free(node);
}

static void grow(NSHashTable *t) {
    NSUInteger     n = t->_nBuckets * 2;
    NSHashBucket **buckets = calloc(n, sizeof(NSHashBucket *));

    for (NSUInteger i = 0; i < t->_nBuckets; i++) {
        NSHashBucket *node = t->_buckets[i], *next;
        for (; node != NULL; node = next) {
            void *item = nodeItem(t, node);
            next = node->next;
            if (item == NULL) {             // a cleared weak entry
                freeNode(t, node);
                t->_count--;
                continue;
            }
            NSUInteger b = hashOf(t, item) % n;
            node->next = buckets[b];
            buckets[b] = node;
        }
    }
    free(t->_buckets);
    t->_buckets = buckets;
    t->_nBuckets = n;
}

// The node holding an item equal to item, or NULL; drops cleared weak
// nodes met on the way.
static NSHashBucket *findNode(NSHashTable *t, const void *item) {
    NSHashBucket **link = &t->_buckets[hashOf(t, item) % t->_nBuckets];

    while (*link != NULL) {
        NSHashBucket *node = *link;
        void *have = nodeItem(t, node);

        if (have == NULL) {
            *link = node->next;
            freeNode(t, node);
            t->_count--;
            continue;
        }
        if (equal(t, have, item))
            return node;
        link = &node->next;
    }
    return NULL;
}

static void insertNew(NSHashTable *t, const void *item) {
    NSHashBucket *node;
    NSUInteger    b;

    if (t->_count >= t->_nBuckets)
        grow(t);
    node = calloc(1, sizeof(NSHashBucket));
    setNodeItem(t, node, item);
    b = hashOf(t, item) % t->_nBuckets;
    node->next = t->_buckets[b];
    t->_buckets[b] = node;
    t->_count++;
    t->_mutations++;
}

static BOOL removeItem(NSHashTable *t, const void *item) {
    NSHashBucket **link = &t->_buckets[hashOf(t, item) % t->_nBuckets];

    while (*link != NULL) {
        NSHashBucket *node = *link;
        void *have = nodeItem(t, node);

        if (have == NULL || equal(t, have, item)) {
            *link = node->next;
            freeNode(t, node);
            t->_count--;
            t->_mutations++;
            if (have != NULL)
                return YES;
            continue;
        }
        link = &node->next;
    }
    return NO;
}

static void removeAll(NSHashTable *t) {
    for (NSUInteger i = 0; i < t->_nBuckets; i++) {
        NSHashBucket *node = t->_buckets[i], *next;
        for (; node != NULL; node = next) {
            next = node->next;
            freeNode(t, node);
        }
        t->_buckets[i] = NULL;
    }
    t->_count = 0;
    t->_mutations++;
}

// Every live item, retained by the array.
static NSArray *allItems(NSHashTable *t) {
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:t->_count];

    for (NSUInteger i = 0; i < t->_nBuckets; i++)
        for (NSHashBucket *node = t->_buckets[i]; node != NULL; node = node->next) {
            void *item = nodeItem(t, node);
            if (item != NULL)
                [result addObject:(id)item];
        }
    return result;
}

static NSUInteger liveCount(NSHashTable *t) {
    if (!t->_weak)
        return t->_count;
    NSUInteger n = 0;
    for (NSUInteger i = 0; i < t->_nBuckets; i++)
        for (NSHashBucket *node = t->_buckets[i]; node != NULL; node = node->next)
            if (nodeItem(t, node) != NULL)
                n++;
    return n;
}

@interface _NSHashTableEnumerator : NSEnumerator {
    NSArray *_items;
    NSUInteger _next;
}
- (instancetype)initWithItems:(NSArray *)items;
@end

@implementation _NSHashTableEnumerator
- (instancetype)initWithItems:(NSArray *)items {
    if ((self = [super init]) != nil)
        _items = [items retain];
    return self;
}
- (void)dealloc {
    [_items release];
    [super dealloc];
}
- (id)nextObject {
    return _next < [_items count] ? [_items objectAtIndex:_next++] : nil;
}
@end

@implementation NSHashTable

- (instancetype)_initWithCapacity:(NSUInteger)capacity {
    if ((self = [super init]) != nil) {
        _nBuckets = capacity < 8 ? 8 : capacity;
        _buckets = calloc(_nBuckets, sizeof(NSHashBucket *));
    }
    return self;
}

- (instancetype)initWithOptions:(NSPointerFunctionsOptions)options capacity:(NSUInteger)capacity {
    if ((self = [self _initWithCapacity:capacity]) != nil) {
        NSUInteger memory = MEMORY(options);

        _options = options;
        _fromOptions = YES;
        _weak = (memory == NSPointerFunctionsWeakMemory || memory == NSPointerFunctionsZeroingWeakMemory);
        _callBacks.hash = optionsHash;
        _callBacks.isEqual = optionsIsEqual;
        _callBacks.retain = optionsRetain;
        _callBacks.release = optionsRelease;
        _callBacks.describe = optionsDescribe;
    }
    return self;
}

- (instancetype)initWithPointerFunctions:(NSPointerFunctions *)functions capacity:(NSUInteger)capacity {
    NSPointerFunctionsOptions options = [functions usesWeakReadAndWriteBarriers]
        ? NSPointerFunctionsWeakMemory : NSPointerFunctionsStrongMemory;
    return [self initWithOptions:options capacity:capacity];
}

- (instancetype)init {
    return [self initWithOptions:NSPointerFunctionsStrongMemory capacity:0];
}

+ (NSHashTable *)hashTableWithOptions:(NSPointerFunctionsOptions)options {
    return [[[self alloc] initWithOptions:options capacity:0] autorelease];
}

+ (NSHashTable *)weakObjectsHashTable {
    return [self hashTableWithOptions:NSPointerFunctionsWeakMemory];
}

+ (id)hashTableWithWeakObjects {
    return [self weakObjectsHashTable];
}

- (void)dealloc {
    removeAll(self);
    free(_buckets);
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    NSHashTable *copy = [[NSHashTable allocWithZone:zone] _initWithCapacity:_count];

    copy->_options = _options;
    copy->_fromOptions = _fromOptions;
    copy->_weak = _weak;
    copy->_callBacks = _callBacks;
    // Node by node: the items need not be objects (C callbacks).
    for (NSUInteger i = 0; i < _nBuckets; i++)
        for (NSHashBucket *node = _buckets[i]; node != NULL; node = node->next) {
            void *item = nodeItem(self, node);
            if (item != NULL)
                insertNew(copy, item);
        }
    return copy;
}

- (NSPointerFunctions *)pointerFunctions {
    return [NSPointerFunctions pointerFunctionsWithOptions:_options];
}

- (NSUInteger)count {
    return liveCount(self);
}

- (id)member:(id)object {
    if (object == nil)
        return nil;
    NSHashBucket *node = findNode(self, object);
    return node ? (id)nodeItem(self, node) : nil;
}

- (BOOL)containsObject:(id)object {
    return [self member:object] != nil;
}

- (void)addObject:(id)object {
    NSHashBucket *node;

    if (object == nil)
        return;
    if (_fromOptions && (_options & NSPointerFunctionsCopyIn) && holdsObjects(_options))
        object = [[object copy] autorelease];
    if ((node = findNode(self, object)) != NULL) {
        // As NSHashInsert: the new item replaces the old one.
        void *old = node->item;
        if (_weak) {
            setNodeItem(self, node, object);
        } else {
            setNodeItem(self, node, object);
            if (_callBacks.release)
                _callBacks.release(self, old);
        }
        _mutations++;
        return;
    }
    insertNew(self, object);
}

- (void)removeObject:(id)object {
    if (object != nil)
        removeItem(self, object);
}

- (void)removeAllObjects {
    removeAll(self);
}

- (NSArray *)allObjects {
    return allItems(self);
}

- (id)anyObject {
    for (NSUInteger i = 0; i < _nBuckets; i++)
        for (NSHashBucket *node = _buckets[i]; node != NULL; node = node->next) {
            id item = nodeItem(self, node);
            if (item != nil)
                return item;
        }
    return nil;
}

- (NSEnumerator *)objectEnumerator {
    return [[[_NSHashTableEnumerator alloc] initWithItems:allItems(self)] autorelease];
}

- (NSSet *)setRepresentation {
    return [NSSet setWithArray:allItems(self)];
}

- (BOOL)intersectsHashTable:(NSHashTable *)other {
    for (id item in allItems(self))
        if ([other member:item] != nil)
            return YES;
    return NO;
}

- (BOOL)isSubsetOfHashTable:(NSHashTable *)other {
    for (id item in allItems(self))
        if ([other member:item] == nil)
            return NO;
    return YES;
}

- (BOOL)isEqualToHashTable:(NSHashTable *)other {
    return [self count] == [other count] && [self isSubsetOfHashTable:other];
}

- (BOOL)isEqual:(id)other {
    return self == other || ([other isKindOfClass:[NSHashTable class]] && [self isEqualToHashTable:other]);
}

- (NSUInteger)hash {
    return [self count];
}

- (void)intersectHashTable:(NSHashTable *)other {
    for (id item in allItems(self))
        if ([other member:item] == nil)
            removeItem(self, item);
}

- (void)unionHashTable:(NSHashTable *)other {
    for (id item in [other allObjects])
        [self addObject:item];
}

- (void)minusHashTable:(NSHashTable *)other {
    for (id item in [other allObjects])
        removeItem(self, item);
}

// Enumerates a snapshot of the live items; mutating the table during the
// enumeration raises, as for other collections.
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id *)buffer count:(NSUInteger)len {
    if (state->state == 0) {
        NSArray       *items = allItems(self);
        NSUInteger     n = [items count];
        NSMutableData *storage = [NSMutableData dataWithLength:(n + 1) * sizeof(id)];

        [items getObjects:(id *)[storage mutableBytes] range:NSMakeRange(0, n)];
        // items (autoreleased) keeps the objects alive for the loop.
        state->itemsPtr = (id *)[storage mutableBytes];
        state->mutationsPtr = &_mutations;
        state->state = 1;
        state->extra[0] = n;
        return n;
    }
    return 0;
}

- (NSString *)description {
    NSMutableString *s = [NSMutableString stringWithFormat:@"NSHashTable {\n"];

    for (NSUInteger i = 0; i < _nBuckets; i++)
        for (NSHashBucket *node = _buckets[i]; node != NULL; node = node->next) {
            void *item = nodeItem(self, node);
            if (item == NULL)
                continue;
            [s appendFormat:@"[%lu] %@\n", (unsigned long)i,
                _callBacks.describe ? _callBacks.describe(self, item) : [NSString stringWithFormat:@"%p", item]];
        }
    [s appendString:@"}\n"];
    return s;
}

@end

/* ---- The C functions ---------------------------------------------------- */

NSHashTable *NSCreateHashTableWithZone(NSHashTableCallBacks callBacks, NSUInteger capacity, NSZone *zone) {
    NSHashTable *t = [[NSHashTable allocWithZone:zone] _initWithCapacity:capacity];

    t->_callBacks = callBacks;
    return t;
}

NSHashTable *NSCreateHashTable(NSHashTableCallBacks callBacks, NSUInteger capacity) {
    return NSCreateHashTableWithZone(callBacks, capacity, NULL);
}

NSHashTable *NSCopyHashTableWithZone(NSHashTable *table, NSZone *zone) {
    return [table copyWithZone:zone];
}

void NSFreeHashTable(NSHashTable *table) {
    [table release];
}

void NSResetHashTable(NSHashTable *table) {
    removeAll(table);
}

BOOL NSCompareHashTables(NSHashTable *table1, NSHashTable *table2) {
    if (liveCount(table1) != liveCount(table2))
        return NO;
    for (NSUInteger i = 0; i < table1->_nBuckets; i++)
        for (NSHashBucket *node = table1->_buckets[i]; node != NULL; node = node->next) {
            void *item = nodeItem(table1, node);
            if (item != NULL && findNode(table2, item) == NULL)
                return NO;
        }
    return YES;
}

NSUInteger NSCountHashTable(NSHashTable *table) {
    return liveCount(table);
}

void *NSHashGet(NSHashTable *table, const void *pointer) {
    NSHashBucket *node = findNode(table, pointer);
    return node ? nodeItem(table, node) : NULL;
}

NSArray *NSAllHashTableObjects(NSHashTable *table) {
    return allItems(table);
}

NSHashEnumerator NSEnumerateHashTable(NSHashTable *table) {
    NSHashEnumerator e;

    e.table = table;
    e.i = 0;
    e.j = table->_nBuckets > 0 ? table->_buckets[0] : NULL;
    return e;
}

void *NSNextHashEnumeratorItem(NSHashEnumerator *e) {
    NSHashTable *t = e->table;

    while (e->i < t->_nBuckets) {
        while (e->j != NULL) {
            NSHashBucket *node = e->j;
            void *item = nodeItem(t, node);
            e->j = node->next;
            if (item != NULL)
                return item;
        }
        if (++e->i < t->_nBuckets)
            e->j = t->_buckets[e->i];
    }
    return NULL;
}

void NSEndHashTableEnumeration(NSHashEnumerator *e) {
    e->j = NULL;
    e->i = e->table->_nBuckets;
}

void NSHashInsert(NSHashTable *table, const void *pointer) {
    NSHashBucket *node;

    if (pointer == NULL)
        [NSException raise:NSInvalidArgumentException format:@"NSHashInsert: NULL item"];
    if ((node = findNode(table, pointer)) != NULL) {
        void *old = node->item;
        setNodeItem(table, node, pointer);
        if (!table->_weak && table->_callBacks.release)
            table->_callBacks.release(table, old);
        table->_mutations++;
        return;
    }
    insertNew(table, pointer);
}

void NSHashInsertKnownAbsent(NSHashTable *table, const void *pointer) {
    if (pointer == NULL)
        [NSException raise:NSInvalidArgumentException format:@"NSHashInsertKnownAbsent: NULL item"];
    if (findNode(table, pointer) != NULL)
        [NSException raise:NSInvalidArgumentException format:@"NSHashInsertKnownAbsent: item already in the table"];
    insertNew(table, pointer);
}

void *NSHashInsertIfAbsent(NSHashTable *table, const void *pointer) {
    NSHashBucket *node;

    if (pointer == NULL)
        [NSException raise:NSInvalidArgumentException format:@"NSHashInsertIfAbsent: NULL item"];
    if ((node = findNode(table, pointer)) != NULL)
        return nodeItem(table, node);
    insertNew(table, pointer);
    return NULL;
}

void NSHashRemove(NSHashTable *table, const void *pointer) {
    if (pointer != NULL)
        removeItem(table, pointer);
}

NSString *NSStringFromHashTable(NSHashTable *table) {
    return [table description];
}

/* ---- The standard C callbacks ------------------------------------------- */

static NSUInteger intHash(NSHashTable *t, const void *p) { return (NSUInteger)(uintptr_t)p; }
static NSUInteger ptrHash(NSHashTable *t, const void *p) { return pointerHash(p); }
static NSUInteger objHash(NSHashTable *t, const void *p) { return [(id)p hash]; }
static NSUInteger structHash(NSHashTable *t, const void *p) { return (NSUInteger)*(const int *)p; }
static BOOL ptrEqual(NSHashTable *t, const void *a, const void *b) { return a == b; }
static BOOL objEqual(NSHashTable *t, const void *a, const void *b) { return a == b || [(id)a isEqual:(id)b]; }
static BOOL structEqual(NSHashTable *t, const void *a, const void *b) { return *(const int *)a == *(const int *)b; }
static void objRetain(NSHashTable *t, const void *p) { [(id)p retain]; }
static void objRelease(NSHashTable *t, void *p) { [(id)p release]; }
static void ptrFree(NSHashTable *t, void *p) { free(p); }
static NSString *intDescribe(NSHashTable *t, const void *p) { return [NSString stringWithFormat:@"%ld", (long)(intptr_t)p]; }
static NSString *ptrDescribe(NSHashTable *t, const void *p) { return [NSString stringWithFormat:@"%p", p]; }
static NSString *objDescribe(NSHashTable *t, const void *p) { return [(id)p description]; }

const NSHashTableCallBacks NSIntHashCallBacks = { intHash, ptrEqual, NULL, NULL, intDescribe };
const NSHashTableCallBacks NSIntegerHashCallBacks = { intHash, ptrEqual, NULL, NULL, intDescribe };
const NSHashTableCallBacks NSNonOwnedPointerHashCallBacks = { ptrHash, ptrEqual, NULL, NULL, ptrDescribe };
const NSHashTableCallBacks NSNonRetainedObjectHashCallBacks = { objHash, objEqual, NULL, NULL, objDescribe };
const NSHashTableCallBacks NSObjectHashCallBacks = { objHash, objEqual, objRetain, objRelease, objDescribe };
const NSHashTableCallBacks NSOwnedObjectIdentityHashCallBacks = { ptrHash, ptrEqual, objRetain, objRelease, objDescribe };
const NSHashTableCallBacks NSOwnedPointerHashCallBacks = { ptrHash, ptrEqual, NULL, ptrFree, ptrDescribe };
const NSHashTableCallBacks NSPointerToStructHashCallBacks = { structHash, structEqual, NULL, ptrFree, ptrDescribe };
