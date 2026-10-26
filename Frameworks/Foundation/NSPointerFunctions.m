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

// NSPointerFunctions: the options a collection was made with. Minkar: the
// class did not exist (its header was empty). The function properties are
// kept for callers that read or set them; NSHashTable works from the
// options.

#import <Foundation/NSPointerFunctions.h>
#import <Foundation/NSZone.h>

@implementation NSPointerFunctions {
    NSPointerFunctionsOptions _options;
}

@synthesize hashFunction = _hashFunction;
@synthesize isEqualFunction = _isEqualFunction;
@synthesize sizeFunction = _sizeFunction;
@synthesize descriptionFunction = _descriptionFunction;
@synthesize relinquishFunction = _relinquishFunction;
@synthesize acquireFunction = _acquireFunction;
@synthesize usesStrongWriteBarrier = _usesStrongWriteBarrier;
@synthesize usesWeakReadAndWriteBarriers = _usesWeakReadAndWriteBarriers;

- (instancetype)initWithOptions:(NSPointerFunctionsOptions)options {
    if ((self = [super init]) != nil) {
        NSUInteger memory = options & 0xff;

        _options = options;
        _usesWeakReadAndWriteBarriers = (memory == NSPointerFunctionsWeakMemory ||
                                         memory == NSPointerFunctionsZeroingWeakMemory);
    }
    return self;
}

- (instancetype)init {
    return [self initWithOptions:NSPointerFunctionsStrongMemory];
}

+ (NSPointerFunctions *)pointerFunctionsWithOptions:(NSPointerFunctionsOptions)options {
    return [[[self alloc] initWithOptions:options] autorelease];
}

- (id)copyWithZone:(NSZone *)zone {
    NSPointerFunctions *copy = [[NSPointerFunctions allocWithZone:zone] initWithOptions:_options];

    copy->_hashFunction = _hashFunction;
    copy->_isEqualFunction = _isEqualFunction;
    copy->_sizeFunction = _sizeFunction;
    copy->_descriptionFunction = _descriptionFunction;
    copy->_relinquishFunction = _relinquishFunction;
    copy->_acquireFunction = _acquireFunction;
    copy->_usesStrongWriteBarrier = _usesStrongWriteBarrier;
    copy->_usesWeakReadAndWriteBarriers = _usesWeakReadAndWriteBarriers;
    return copy;
}

@end
