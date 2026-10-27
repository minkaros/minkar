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

// Blocks as Objective-C objects. A block's isa is one of the blocks
// runtime's _NSConcrete*Block symbols (libsystem_blocks): storage the size
// of a class (32 pointers), left zeroed. Here each one becomes a real
// class, built in place as a subclass of NSBlock, so blocks answer
// messages: retain, release and copy (which ARC sends all the time),
// description, isKindOfClass:. Without this a message to a block jumped
// through a zeroed class.
//
// Not ARC: it implements retain and release.

#import <Foundation/NSObject.h>
#import <Foundation/NSZone.h>
#include <Block.h>
#include <objc/runtime.h>
#include <stdint.h>

extern void *_NSConcreteStackBlock[32];
extern void *_NSConcreteMallocBlock[32];
extern void *_NSConcreteAutoBlock[32];
extern void *_NSConcreteFinalizingBlock[32];
extern void *_NSConcreteGlobalBlock[32];
extern void *_NSConcreteWeakBlockVariable[32];

// objc4's in-place class construction (objc-internal.h).
OBJC_EXPORT Class objc_initializeClassPair(Class superclass, const char *name,
                                          Class cls, Class metacls);

@interface NSBlock : NSObject
- (void)invoke;
@end

#define BLOCK_CLASSES 6
static uintptr_t blockMetaclasses[BLOCK_CLASSES][32];  // OBJC_MAX_CLASS_SIZE each

static void buildBlockClass(void *storage, const char *name, int index)
{
    if (objc_lookUpClass(name) != Nil)
        return;
    Class cls = objc_initializeClassPair([NSBlock class], name, (Class)storage,
                                         (Class)(void *)blockMetaclasses[index]);
    if (cls != Nil)
        objc_registerClassPair(cls);
}

@implementation NSBlock

+ (void)load
{
    buildBlockClass(_NSConcreteStackBlock, "__NSStackBlock__", 0);
    buildBlockClass(_NSConcreteMallocBlock, "__NSMallocBlock__", 1);
    buildBlockClass(_NSConcreteAutoBlock, "__NSAutoBlock__", 2);
    buildBlockClass(_NSConcreteFinalizingBlock, "__NSFinalizingBlock__", 3);
    buildBlockClass(_NSConcreteGlobalBlock, "__NSGlobalBlock__", 4);
    buildBlockClass(_NSConcreteWeakBlockVariable, "__NSBlockVariable__", 5);
}

static BOOL isHeapBlock(id block)
{
    return object_getClass(block) == (Class)(void *)_NSConcreteMallocBlock;
}

// Heap blocks count references in the blocks runtime; stack and global
// blocks live as long as their frame or the program, and ignore both.
- (id)retain
{
    if (isHeapBlock(self))
        return (id)_Block_copy(self);
    return self;
}

- (oneway void)release
{
    if (isHeapBlock(self))
        _Block_release(self);
}

- (id)autorelease
{
    if (isHeapBlock(self))
        return [super autorelease];
    return self;
}

- (NSUInteger)retainCount
{
    return NSUIntegerMax;   // not meaningful for blocks
}

// A stack block is copied to the heap; a heap block gains a reference; a
// global block is itself.
- (id)copy
{
    return (id)_Block_copy(self);
}

- (id)copyWithZone:(NSZone *)zone
{
    (void)zone;
    return (id)_Block_copy(self);
}

- (void)invoke
{
    void (^block)(void) = (void (^)(void))self;
    block();
}

@end
