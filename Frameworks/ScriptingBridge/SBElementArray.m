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

#import <ScriptingBridge/SBElementArray.h>
#import <Foundation/NSRaise.h>

/* Minkar: an element array of another application, always empty until
 * there is an Open Scripting Architecture to ask. */
@implementation SBElementArray {
    NSMutableArray *_objects;
}

- (instancetype)init {
    return [self initWithCapacity:0];
}

- (instancetype)initWithCapacity:(NSUInteger)capacity {
    if ((self = [super init]) != nil)
        _objects = [[NSMutableArray alloc] initWithCapacity:capacity];
    return self;
}

- (NSUInteger)count {
    return [_objects count];
}

- (id)objectAtIndex:(NSUInteger)index {
    return [_objects objectAtIndex:index];
}

- (void)insertObject:(id)object atIndex:(NSUInteger)index {
    [_objects insertObject:object atIndex:index];
}

- (void)removeObjectAtIndex:(NSUInteger)index {
    [_objects removeObjectAtIndex:index];
}

- (void)addObject:(id)object {
    [_objects addObject:object];
}

- (void)removeLastObject {
    [_objects removeLastObject];
}

- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(id)object {
    [_objects replaceObjectAtIndex:index withObject:object];
}

- (id)objectWithName:(NSString *)name {
    NSUnimplementedMethod();
    return nil;
}

- (id)objectWithID:(id)identifier {
    NSUnimplementedMethod();
    return nil;
}

- (id)objectAtLocation:(id)location {
    NSUnimplementedMethod();
    return nil;
}

- (NSArray *)arrayByApplyingSelector:(SEL)selector {
    return [_objects valueForKey:NSStringFromSelector(selector)];
}

- (NSArray *)arrayByApplyingSelector:(SEL)aSelector withObject:(id)argument {
    NSUnimplementedMethod();
    return @[];
}

- (NSArray *)get {
    return [_objects copy];
}

@end
