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

/*
 * Toll-free bridging of arrays and dictionaries.
 *
 * __NSCFArray and __NSCFDictionary are the classes of CF's arrays and
 * dictionaries once Foundation is loaded (CFSetupFoundationBridging): the
 * object is CF's, with CF's layout and memory, and each Objective-C method
 * calls the CF function. In the other direction CF's functions send
 * Foundation's own collections the messages listed in the categories
 * below, which NSArray and NSDictionary do not have.
 */

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSEnumerator.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <CoreFoundation/CFArray.h>
#import <CoreFoundation/CFDictionary.h>
#include <stdlib.h>

CF_EXPORT Boolean _CFArrayIsMutable(CFArrayRef array);
CF_EXPORT Boolean _CFDictionaryIsMutable(CFDictionaryRef dictionary);

@interface __NSCFArray : NSMutableArray
@end

@interface __NSCFDictionary : NSMutableDictionary
@end

static void raiseImmutable(id self, SEL cmd) {
    [NSException raise:NSInternalInconsistencyException
                format:@"-[%@ %@]: mutating method sent to immutable object",
                       NSStringFromClass([self class]), NSStringFromSelector(cmd)];
}

static void raiseNil(id self, SEL cmd, NSString *what) {
    [NSException raise:NSInvalidArgumentException
                format:@"-[%@ %@]: %@ cannot be nil",
                       NSStringFromClass([self class]), NSStringFromSelector(cmd), what];
}

static void raiseIndex(id self, SEL cmd, NSUInteger index, NSUInteger count) {
    [NSException raise:NSRangeException
                format:@"-[%@ %@]: index %lu beyond bounds [0 .. %ld]",
                       NSStringFromClass([self class]), NSStringFromSelector(cmd),
                       (unsigned long)index, (long)count - 1];
}

@implementation __NSCFArray

#define CFSELF ((CFMutableArrayRef)self)

static inline void checkMutable(__NSCFArray *self, SEL cmd) {
    if (!_CFArrayIsMutable((CFArrayRef)self))
        raiseImmutable(self, cmd);
}

// The memory is CF's: CF frees the array when its count reaches zero.
- (id)retain {
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release {
    CFRelease((CFTypeRef)self);
}

- (NSUInteger)retainCount {
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}

- (CFTypeID)_cfTypeID {
    return CFArrayGetTypeID();
}

- (NSUInteger)count {
    return (NSUInteger)CFArrayGetCount(CFSELF);
}

- (id)objectAtIndex:(NSUInteger)index {
    NSUInteger count = (NSUInteger)CFArrayGetCount(CFSELF);
    if (index >= count)
        raiseIndex(self, _cmd, index, count);
    return (id)CFArrayGetValueAtIndex(CFSELF, (CFIndex)index);
}

- (void)getObjects:(id *)objects range:(NSRange)range {
    NSUInteger count = (NSUInteger)CFArrayGetCount(CFSELF);
    if (range.location > count || range.length > count - range.location)
        raiseIndex(self, _cmd, range.location + range.length - 1, count);
    CFArrayGetValues(CFSELF, CFRangeMake((CFIndex)range.location, (CFIndex)range.length),
                     (const void **)objects);
}

- (void)addObject:(id)object {
    checkMutable(self, _cmd);
    if (object == nil)
        raiseNil(self, _cmd, @"object");
    CFArrayAppendValue(CFSELF, object);
}

- (void)insertObject:(id)object atIndex:(NSUInteger)index {
    checkMutable(self, _cmd);
    if (object == nil)
        raiseNil(self, _cmd, @"object");
    NSUInteger count = (NSUInteger)CFArrayGetCount(CFSELF);
    if (index > count)
        raiseIndex(self, _cmd, index, count + 1);
    CFArrayInsertValueAtIndex(CFSELF, (CFIndex)index, object);
}

- (void)removeObjectAtIndex:(NSUInteger)index {
    checkMutable(self, _cmd);
    NSUInteger count = (NSUInteger)CFArrayGetCount(CFSELF);
    if (index >= count)
        raiseIndex(self, _cmd, index, count);
    CFArrayRemoveValueAtIndex(CFSELF, (CFIndex)index);
}

- (void)removeLastObject {
    checkMutable(self, _cmd);
    CFIndex count = CFArrayGetCount(CFSELF);
    if (count == 0)
        raiseIndex(self, _cmd, 0, 0);
    CFArrayRemoveValueAtIndex(CFSELF, count - 1);
}

- (void)removeAllObjects {
    checkMutable(self, _cmd);
    CFArrayRemoveAllValues(CFSELF);
}

- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(id)object {
    checkMutable(self, _cmd);
    if (object == nil)
        raiseNil(self, _cmd, @"object");
    NSUInteger count = (NSUInteger)CFArrayGetCount(CFSELF);
    if (index >= count)
        raiseIndex(self, _cmd, index, count);
    CFArraySetValueAtIndex(CFSELF, (CFIndex)index, object);
}

- (void)exchangeObjectAtIndex:(NSUInteger)index withObjectAtIndex:(NSUInteger)other {
    checkMutable(self, _cmd);
    NSUInteger count = (NSUInteger)CFArrayGetCount(CFSELF);
    if (index >= count || other >= count)
        raiseIndex(self, _cmd, index >= count ? index : other, count);
    CFArrayExchangeValuesAtIndices(CFSELF, (CFIndex)index, (CFIndex)other);
}

- (id)copyWithZone:(NSZone *)zone {
    return (id)CFArrayCreateCopy(kCFAllocatorDefault, CFSELF);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, CFSELF);
}

- (Class)classForCoder {
    return _CFArrayIsMutable(CFSELF) ? [NSMutableArray class] : [NSArray class];
}

#undef CFSELF
@end

@implementation __NSCFDictionary

#define CFSELF ((CFMutableDictionaryRef)self)

static inline void checkMutableDictionary(__NSCFDictionary *self, SEL cmd) {
    if (!_CFDictionaryIsMutable((CFDictionaryRef)self))
        raiseImmutable(self, cmd);
}

- (id)retain {
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release {
    CFRelease((CFTypeRef)self);
}

- (NSUInteger)retainCount {
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}

- (CFTypeID)_cfTypeID {
    return CFDictionaryGetTypeID();
}

- (NSUInteger)count {
    return (NSUInteger)CFDictionaryGetCount(CFSELF);
}

- (id)objectForKey:(id)key {
    if (key == nil)
        return nil;
    return (id)CFDictionaryGetValue(CFSELF, key);
}

- (void)getObjects:(id *)objects andKeys:(id *)keys {
    CFDictionaryGetKeysAndValues(CFSELF, (const void **)keys, (const void **)objects);
}

- (NSArray *)allKeys {
    CFIndex count = CFDictionaryGetCount(CFSELF);
    if (count == 0)
        return [NSArray array];
    const void **keys = malloc(sizeof(void *) * (size_t)count);
    CFDictionaryGetKeysAndValues(CFSELF, keys, NULL);
    NSArray *result = [NSArray arrayWithObjects:(id *)keys count:(NSUInteger)count];
    free(keys);
    return result;
}

- (NSArray *)allValues {
    CFIndex count = CFDictionaryGetCount(CFSELF);
    if (count == 0)
        return [NSArray array];
    const void **values = malloc(sizeof(void *) * (size_t)count);
    CFDictionaryGetKeysAndValues(CFSELF, NULL, values);
    NSArray *result = [NSArray arrayWithObjects:(id *)values count:(NSUInteger)count];
    free(values);
    return result;
}

// A snapshot of the keys, as mutating the dictionary while enumerating is
// not allowed anyway.
- (NSEnumerator *)keyEnumerator {
    return [[self allKeys] objectEnumerator];
}

- (NSEnumerator *)objectEnumerator {
    return [[self allValues] objectEnumerator];
}

- (void)setObject:(id)object forKey:(id)key {
    checkMutableDictionary(self, _cmd);
    if (object == nil)
        raiseNil(self, _cmd, @"object");
    if (key == nil)
        raiseNil(self, _cmd, @"key");
    // As with every NSMutableDictionary, the key is copied.
    id copy = [key copyWithZone:NULL];
    CFDictionarySetValue(CFSELF, copy, object);
    [copy release];
}

- (void)removeObjectForKey:(id)key {
    checkMutableDictionary(self, _cmd);
    if (key == nil)
        raiseNil(self, _cmd, @"key");
    CFDictionaryRemoveValue(CFSELF, key);
}

- (void)removeAllObjects {
    checkMutableDictionary(self, _cmd);
    CFDictionaryRemoveAllValues(CFSELF);
}

- (id)copyWithZone:(NSZone *)zone {
    return (id)CFDictionaryCreateCopy(kCFAllocatorDefault, CFSELF);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, CFSELF);
}

- (Class)classForCoder {
    return _CFDictionaryIsMutable(CFSELF) ? [NSMutableDictionary class] : [NSDictionary class];
}

#undef CFSELF
@end

/* The messages CF's functions send to Foundation's own collections. */

@implementation NSMutableArray (CFBridging)

// CFArraySetValueAtIndex: an index equal to the count appends.
- (void)setObject:(id)object atIndex:(NSUInteger)index {
    if (index == [self count])
        [self addObject:object];
    else
        [self replaceObjectAtIndex:index withObject:object];
}

- (void)replaceObjectsInRange:(NSRange)range withObjects:(id *)objects count:(NSUInteger)count {
    [self removeObjectsInRange:range];
    for (NSUInteger i = 0; i < count; i++)
        [self insertObject:objects[i] atIndex:range.location + i];
}

@end

@implementation NSDictionary (CFBridging)

- (NSUInteger)countForKey:(id)key {
    return [self objectForKey:key] != nil ? 1 : 0;
}

- (BOOL)containsKey:(id)key {
    return [self objectForKey:key] != nil;
}

- (BOOL)__getValue:(id *)value forKey:(id)key {
    id object = [self objectForKey:key];
    if (object != nil && value != NULL)
        *value = object;
    return object != nil;
}

- (NSUInteger)countForObject:(id)object {
    NSUInteger result = 0;
    for (id key in self) {
        id value = [self objectForKey:key];
        if (value == object || [value isEqual:object])
            result++;
    }
    return result;
}

- (BOOL)containsObject:(id)object {
    return [self countForObject:object] != 0;
}

- (void)__apply:(void (*)(const void *, const void *, void *))applier context:(void *)context {
    for (id key in [self allKeys])
        applier(key, [self objectForKey:key], context);
}

@end

@implementation NSMutableDictionary (CFBridging)

// CFDictionaryAddValue: only when the key is not there.
- (void)__addObject:(id)object forKey:(id)key {
    if ([self objectForKey:key] == nil)
        [self setObject:object forKey:key];
}

// CFDictionaryReplaceValue: only when the key is there.
- (void)replaceObject:(id)object forKey:(id)key {
    if ([self objectForKey:key] != nil)
        [self setObject:object forKey:key];
}

- (void)__setObject:(id)object forKey:(id)key {
    [self setObject:object forKey:key];
}

@end
