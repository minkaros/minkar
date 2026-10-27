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

// NSCache. Built with ARC.
//
// Entries live in a CFDictionary keyed by the caller's key (retained, not
// copied; equality and hash from the key's isEqual: and hash) and in a
// list from least to most recently used. Going over countLimit or
// totalCostLimit evicts from the least recently used end.
//
// The delegate is told after the lock is released, so it may use the
// cache; the evicted objects stay alive until it has been told.

#import <Foundation/NSCache.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#import <CoreFoundation/CFDictionary.h>

#include <pthread.h>

@interface _NSCacheEntry : NSObject {
@public
    id _key;
    id _object;
    NSUInteger _cost;
    __unsafe_unretained _NSCacheEntry *_prev;     // the list owns nothing:
    __unsafe_unretained _NSCacheEntry *_next;     // the dictionary does
}
@end

@implementation _NSCacheEntry
@end

@implementation NSCache {
    pthread_mutex_t _lock;
    CFMutableDictionaryRef _entries;    // key -> _NSCacheEntry
    __unsafe_unretained _NSCacheEntry *_oldest;
    __unsafe_unretained _NSCacheEntry *_newest;
    NSUInteger _totalCost;
    NSString *_name;
    NSUInteger _countLimit;
    NSUInteger _totalCostLimit;
    BOOL _evictsObjectsWithDiscardedContent;
}

@synthesize delegate = _delegate;

- (instancetype)init
{
    if ((self = [super init]) != nil) {
        pthread_mutex_init(&_lock, NULL);
        _entries = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        _name = @"";
        _evictsObjectsWithDiscardedContent = YES;
    }
    return self;
}

- (void)dealloc
{
    if (_entries != NULL)
        CFRelease(_entries);
    pthread_mutex_destroy(&_lock);
}

/* ---- The usage list (the lock is held) --------------------------------- */

static void
unlinkEntry(NSCache *self, _NSCacheEntry *e)
{
    if (e->_prev != nil)
        e->_prev->_next = e->_next;
    else
        self->_oldest = e->_next;
    if (e->_next != nil)
        e->_next->_prev = e->_prev;
    else
        self->_newest = e->_prev;
    e->_prev = e->_next = nil;
}

static void
appendEntry(NSCache *self, _NSCacheEntry *e)
{
    e->_prev = self->_newest;
    e->_next = nil;
    if (self->_newest != nil)
        self->_newest->_next = e;
    else
        self->_oldest = e;
    self->_newest = e;
}

// Takes e out of the cache; its object goes to `removed` if given.
static void
removeEntry(NSCache *self, _NSCacheEntry *e, NSMutableArray *removed)
{
    if (removed != nil)
        [removed addObject:e->_object];
    self->_totalCost -= e->_cost;
    unlinkEntry(self, e);
    CFDictionaryRemoveValue(self->_entries, (__bridge const void *)e->_key);
}

static BOOL
overLimits(NSCache *self)
{
    NSUInteger count = (NSUInteger)CFDictionaryGetCount(self->_entries);

    return (self->_countLimit > 0 && count > self->_countLimit) ||
           (self->_totalCostLimit > 0 && self->_totalCost > self->_totalCostLimit);
}

static void
evictToLimits(NSCache *self, NSMutableArray *evicted)
{
    while (self->_oldest != nil && overLimits(self))
        removeEntry(self, self->_oldest, evicted);
}

- (void)tellDelegate:(NSArray *)objects
{
    id<NSCacheDelegate> delegate = _delegate;

    if ([objects count] == 0 || ![delegate respondsToSelector:@selector(cache:willEvictObject:)])
        return;
    for (id obj in objects)
        [delegate cache:self willEvictObject:obj];
}

/* ---- Public API -------------------------------------------------------- */

- (id)objectForKey:(id)key
{
    id obj = nil;
    NSMutableArray *discarded = nil;

    if (key == nil)
        return nil;
    pthread_mutex_lock(&_lock);
    _NSCacheEntry *e = (__bridge _NSCacheEntry *)CFDictionaryGetValue(_entries, (__bridge const void *)key);
    if (e != nil) {
        obj = e->_object;
        if (_evictsObjectsWithDiscardedContent &&
            [obj conformsToProtocol:@protocol(NSDiscardableContent)] &&
            [(id<NSDiscardableContent>)obj isContentDiscarded]) {
            discarded = [NSMutableArray array];
            removeEntry(self, e, discarded);
            obj = nil;
        } else {
            unlinkEntry(self, e);
            appendEntry(self, e);
        }
    }
    pthread_mutex_unlock(&_lock);
    [self tellDelegate:discarded];
    return obj;
}

- (void)setObject:(id)obj forKey:(id)key
{
    [self setObject:obj forKey:key cost:0];
}

- (void)setObject:(id)obj forKey:(id)key cost:(NSUInteger)cost
{
    NSMutableArray *evicted = [NSMutableArray array];

    if (key == nil)
        return;
    if (obj == nil) {
        [self removeObjectForKey:key];
        return;
    }
    pthread_mutex_lock(&_lock);
    _NSCacheEntry *e = (__bridge _NSCacheEntry *)CFDictionaryGetValue(_entries, (__bridge const void *)key);
    if (e != nil) {
        _totalCost -= e->_cost;
        unlinkEntry(self, e);
    } else {
        e = [_NSCacheEntry new];
        e->_key = key;
        CFDictionarySetValue(_entries, (__bridge const void *)key, (__bridge const void *)e);
    }
    e->_object = obj;
    e->_cost = cost;
    _totalCost += cost;
    appendEntry(self, e);
    evictToLimits(self, evicted);
    pthread_mutex_unlock(&_lock);
    [self tellDelegate:evicted];
}

- (void)removeObjectForKey:(id)key
{
    NSMutableArray *removed = [NSMutableArray array];

    if (key == nil)
        return;
    pthread_mutex_lock(&_lock);
    _NSCacheEntry *e = (__bridge _NSCacheEntry *)CFDictionaryGetValue(_entries, (__bridge const void *)key);
    if (e != nil)
        removeEntry(self, e, removed);
    pthread_mutex_unlock(&_lock);
    [self tellDelegate:removed];
}

- (void)removeAllObjects
{
    NSMutableArray *removed = [NSMutableArray array];

    pthread_mutex_lock(&_lock);
    while (_oldest != nil)
        removeEntry(self, _oldest, removed);
    pthread_mutex_unlock(&_lock);
    [self tellDelegate:removed];
}

- (NSString *)name
{
    pthread_mutex_lock(&_lock);
    NSString *name = _name;
    pthread_mutex_unlock(&_lock);
    return name;
}

- (void)setName:(NSString *)name
{
    name = [name copy];
    pthread_mutex_lock(&_lock);
    _name = name != nil ? name : @"";
    pthread_mutex_unlock(&_lock);
}

- (NSUInteger)countLimit
{
    return _countLimit;
}

- (void)setCountLimit:(NSUInteger)limit
{
    NSMutableArray *evicted = [NSMutableArray array];

    pthread_mutex_lock(&_lock);
    _countLimit = limit;
    evictToLimits(self, evicted);
    pthread_mutex_unlock(&_lock);
    [self tellDelegate:evicted];
}

- (NSUInteger)totalCostLimit
{
    return _totalCostLimit;
}

- (void)setTotalCostLimit:(NSUInteger)limit
{
    NSMutableArray *evicted = [NSMutableArray array];

    pthread_mutex_lock(&_lock);
    _totalCostLimit = limit;
    evictToLimits(self, evicted);
    pthread_mutex_unlock(&_lock);
    [self tellDelegate:evicted];
}

- (BOOL)evictsObjectsWithDiscardedContent
{
    return _evictsObjectsWithDiscardedContent;
}

- (void)setEvictsObjectsWithDiscardedContent:(BOOL)evicts
{
    _evictsObjectsWithDiscardedContent = evicts;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p> name=\"%@\" count=%ld", [self class],
        (void *)self, [self name], (long)CFDictionaryGetCount(_entries)];
}

@end
