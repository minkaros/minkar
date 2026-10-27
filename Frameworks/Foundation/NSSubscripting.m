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

// Object subscripting (macOS 10.8): the compiler turns dictionary[key] and
// array[index], read or assigned, into these messages. They are defined on
// the abstract classes, on top of each class's primitive methods, so every
// concrete subclass has them. Built with ARC.

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSException.h>
#import <Foundation/NSMutableArray.h>
#import <Foundation/NSMutableDictionary.h>

@implementation NSDictionary (NSSubscripting)

- (id)objectForKeyedSubscript:(id)key
{
    return [self objectForKey:key];
}

@end

@implementation NSMutableDictionary (NSSubscripting)

- (void)setObject:(id)object forKeyedSubscript:(id <NSCopying>)key
{
    if (object == nil)
        [self removeObjectForKey:key];
    else
        [self setObject:object forKey:key];
}

@end

@implementation NSArray (NSSubscripting)

- (id)objectAtIndexedSubscript:(NSUInteger)index
{
    return [self objectAtIndex:index];
}

@end

@implementation NSMutableArray (NSSubscripting)

- (void)setObject:(id)object atIndexedSubscript:(NSUInteger)index
{
    if (object == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"-[%@ %@]: object cannot be nil", [self class],
                           NSStringFromSelector(_cmd)];
    }
    NSUInteger count = [self count];
    if (index < count) {
        [self replaceObjectAtIndex:index withObject:object];
    } else if (index == count) {
        [self addObject:object];
    } else {
        [NSException raise:NSRangeException
                    format:@"-[%@ %@]: index %lu beyond bounds [0 .. %lu]", [self class],
                           NSStringFromSelector(_cmd), (unsigned long)index,
                           (unsigned long)count];
    }
}

@end
