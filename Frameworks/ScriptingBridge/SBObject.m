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

#import <ScriptingBridge/SBObject.h>
#import <Foundation/NSRaise.h>

/* Minkar: with no Open Scripting Architecture, an object of another
 * application holds only the properties it was made with. */
@implementation SBObject {
    NSDictionary *_properties;
}

- (instancetype)init {
    return [self initWithProperties:@{}];
}

- (instancetype)initWithProperties:(NSDictionary *)properties {
    if ((self = [super init]) != nil)
        _properties = [properties copy];
    return self;
}

- (instancetype)initWithData:(id)data {
    NSUnimplementedMethod();
    return [self init];
}

- (id)get {
    NSUnimplementedMethod();
    return nil;
}

- (NSError *)lastError {
    return nil;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self init];
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

@end
