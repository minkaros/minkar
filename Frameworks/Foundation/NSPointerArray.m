/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

// NSPointerArray. Minkar: the class did not exist (its header was empty).
//
// Items live in a malloc'd array of slots. Strong object items are
// retained; weak ones are stored with objc_storeWeak, so each slot keeps a
// fixed address (the array of slots moves only by memmove of slots that
// are first moved out of the runtime's weak table). Opaque items are kept
// as they are.

#import <Foundation/NSPointerArray.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <objc/runtime.h>

#include <stdlib.h>
#include <string.h>

OBJC_EXPORT id objc_storeWeak(id *location, id obj);
OBJC_EXPORT id objc_loadWeakRetained(id *location);
OBJC_EXPORT void objc_destroyWeak(id *location);

@implementation NSPointerArray {
    NSPointerFunctionsOptions _options;
    BOOL _weak;
    BOOL _objects;              // strong object items: retained
    void **_slots;
    NSUInteger _count;
    NSUInteger _capacity;
    unsigned long _mutations;
}

- (instancetype)initWithOptions:(NSPointerFunctionsOptions)options {
    if ((self = [super init]) != nil) {
        NSUInteger memory = options & 0xff, personality = options & 0xff00;

        _options = options;
        _weak = (memory == NSPointerFunctionsWeakMemory || memory == NSPointerFunctionsZeroingWeakMemory);
        _objects = memory == NSPointerFunctionsStrongMemory &&
            (personality == NSPointerFunctionsObjectPersonality ||
             personality == NSPointerFunctionsObjectPointerPersonality);
    }
    return self;
}

- (instancetype)init {
    return [self initWithOptions:NSPointerFunctionsStrongMemory];
}

- (instancetype)initWithPointerFunctions:(NSPointerFunctions *)functions {
    return [self initWithOptions:[functions usesWeakReadAndWriteBarriers]
        ? NSPointerFunctionsWeakMemory : NSPointerFunctionsStrongMemory];
}

+ (NSPointerArray *)pointerArrayWithOptions:(NSPointerFunctionsOptions)options {
    return [[[self alloc] initWithOptions:options] autorelease];
}

+ (NSPointerArray *)pointerArrayWithPointerFunctions:(NSPointerFunctions *)functions {
    return [[[self alloc] initWithPointerFunctions:functions] autorelease];
}

+ (NSPointerArray *)strongObjectsPointerArray {
    return [self pointerArrayWithOptions:NSPointerFunctionsStrongMemory];
}

+ (NSPointerArray *)weakObjectsPointerArray {
    return [self pointerArrayWithOptions:NSPointerFunctionsWeakMemory];
}

/* ---- Slots --------------------------------------------------------------- */

static void *readSlot(NSPointerArray *self, NSUInteger i) {
    if (self->_weak)
        return [objc_loadWeakRetained((id *)&self->_slots[i]) autorelease];
    return self->_slots[i];
}

// Stores item into an empty slot.
static void fillSlot(NSPointerArray *self, NSUInteger i, void *item) {
    if (self->_weak) {
        self->_slots[i] = NULL;
        objc_storeWeak((id *)&self->_slots[i], (id)item);
    } else {
        if (self->_objects && item != NULL)
            [(id)item retain];
        self->_slots[i] = item;
    }
}

// Empties a slot.
static void clearSlot(NSPointerArray *self, NSUInteger i) {
    if (self->_weak)
        objc_destroyWeak((id *)&self->_slots[i]);
    else if (self->_objects && self->_slots[i] != NULL)
        [(id)self->_slots[i] release];
    self->_slots[i] = NULL;
}

static void reserve(NSPointerArray *self, NSUInteger count) {
    if (count <= self->_capacity)
        return;
    NSUInteger capacity = self->_capacity ? self->_capacity : 8;
    while (capacity < count)
        capacity *= 2;
    if (self->_weak) {
        // Weak slots are registered by address: move them one by one.
        void **slots = calloc(capacity, sizeof(void *));
        for (NSUInteger i = 0; i < self->_count; i++) {
            id item = [objc_loadWeakRetained((id *)&self->_slots[i]) autorelease];
            objc_destroyWeak((id *)&self->_slots[i]);
            objc_storeWeak((id *)&slots[i], item);
        }
        free(self->_slots);
        self->_slots = slots;
    } else {
        self->_slots = realloc(self->_slots, capacity * sizeof(void *));
        memset(self->_slots + self->_capacity, 0, (capacity - self->_capacity) * sizeof(void *));
    }
    self->_capacity = capacity;
}

static void checkIndex(NSPointerArray *self, NSUInteger index, NSUInteger limit, SEL cmd) {
    if (index >= limit)
        [NSException raise:NSRangeException format:@"-[NSPointerArray %s]: index %lu beyond count %lu",
            sel_getName(cmd), (unsigned long)index, (unsigned long)self->_count];
}

/* ---- API ----------------------------------------------------------------- */

- (void)dealloc {
    for (NSUInteger i = 0; i < _count; i++)
        clearSlot(self, i);
    free(_slots);
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    NSPointerArray *copy = [[NSPointerArray allocWithZone:zone] initWithOptions:_options];

    for (NSUInteger i = 0; i < _count; i++)
        [copy addPointer:readSlot(self, i)];
    return copy;
}

- (NSPointerFunctions *)pointerFunctions {
    return [NSPointerFunctions pointerFunctionsWithOptions:_options];
}

- (NSUInteger)count {
    return _count;
}

- (void)setCount:(NSUInteger)count {
    _mutations++;
    while (_count > count)
        clearSlot(self, --_count);
    reserve(self, count);
    while (_count < count)
        fillSlot(self, _count++, NULL);
}

- (void *)pointerAtIndex:(NSUInteger)index {
    checkIndex(self, index, _count, _cmd);
    return readSlot(self, index);
}

- (void)addPointer:(void *)pointer {
    _mutations++;
    reserve(self, _count + 1);
    fillSlot(self, _count++, pointer);
}

// Moves items [from, _count) by delta slots (weak slots one at a time).
static void shift(NSPointerArray *self, NSUInteger from, NSInteger delta) {
    if (delta > 0) {
        for (NSUInteger i = self->_count; i-- > from;) {
            void *item = readSlot(self, i);
            if (self->_weak) {
                fillSlot(self, i + (NSUInteger)delta, item);
                objc_destroyWeak((id *)&self->_slots[i]);
                self->_slots[i] = NULL;
            } else {
                self->_slots[i + (NSUInteger)delta] = self->_slots[i];
                self->_slots[i] = NULL;
            }
        }
    } else {
        for (NSUInteger i = from; i < self->_count; i++) {
            void *item = readSlot(self, i);
            NSUInteger to = i - (NSUInteger)(-delta);
            if (self->_weak) {
                fillSlot(self, to, item);
                objc_destroyWeak((id *)&self->_slots[i]);
                self->_slots[i] = NULL;
            } else {
                self->_slots[to] = self->_slots[i];
                self->_slots[i] = NULL;
            }
        }
    }
}

- (void)insertPointer:(void *)item atIndex:(NSUInteger)index {
    checkIndex(self, index, _count + 1, _cmd);
    _mutations++;
    reserve(self, _count + 1);
    shift(self, index, 1);
    _count++;
    fillSlot(self, index, item);
}

- (void)removePointerAtIndex:(NSUInteger)index {
    checkIndex(self, index, _count, _cmd);
    _mutations++;
    clearSlot(self, index);
    shift(self, index + 1, -1);
    _count--;
}

- (void)replacePointerAtIndex:(NSUInteger)index withPointer:(void *)item {
    checkIndex(self, index, _count, _cmd);
    _mutations++;
    if (_objects && item != NULL)
        [(id)item retain];          // before releasing the old one: it may be the same
    clearSlot(self, index);
    if (_weak) {
        fillSlot(self, index, item);
    } else {
        _slots[index] = item;
    }
}

- (void)compact {
    NSUInteger to = 0;

    _mutations++;
    for (NSUInteger i = 0; i < _count; i++) {
        void *item = readSlot(self, i);

        if (item == NULL) {
            clearSlot(self, i);
            continue;
        }
        if (i != to) {
            if (_weak) {
                fillSlot(self, to, item);
                objc_destroyWeak((id *)&_slots[i]);
                _slots[i] = NULL;
            } else {
                _slots[to] = _slots[i];
                _slots[i] = NULL;
            }
        }
        to++;
    }
    _count = to;
}

- (NSArray *)allObjects {
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:_count];

    for (NSUInteger i = 0; i < _count; i++) {
        void *item = readSlot(self, i);
        if (item != NULL)
            [result addObject:(id)item];
    }
    return result;
}

// Enumerates a snapshot of every item, NULLs included, as on macOS; a
// mutation during the loop raises.
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id *)buffer count:(NSUInteger)len {
    NSMutableData *storage;
    NSMutableArray *keep;
    id *items;

    if (state->state != 0)
        return 0;
    storage = [NSMutableData dataWithLength:(_count + 1) * sizeof(id)];
    items = (id *)[storage mutableBytes];
    keep = [NSMutableArray arrayWithCapacity:_count];   // keeps weak items alive
    for (NSUInteger i = 0; i < _count; i++) {
        items[i] = (id)readSlot(self, i);
        if (_weak && items[i] != nil)
            [keep addObject:items[i]];
    }
    state->itemsPtr = items;
    state->mutationsPtr = &_mutations;
    state->state = 1;
    state->extra[0] = (unsigned long)keep;
    return _count;
}

- (NSString *)description {
    NSMutableString *s = [NSMutableString stringWithString:@"("];

    for (NSUInteger i = 0; i < _count; i++) {
        void *item = readSlot(self, i);
        BOOL objects = (_options & 0xff00) == NSPointerFunctionsObjectPersonality ||
                       (_options & 0xff00) == NSPointerFunctionsObjectPointerPersonality;
        [s appendFormat:@"%@%@", i ? @", " : @"",
            item == NULL ? @"NULL" : objects ? [(id)item description] : [NSString stringWithFormat:@"%p", item]];
    }
    [s appendString:@")"];
    return s;
}

@end
