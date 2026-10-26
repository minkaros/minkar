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

// NSUUID on uuid(3) from libSystem. Built with ARC.

#import <Foundation/NSUUID.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSString.h>
#include <string.h>

// The key the 16 bytes are archived under, as macOS archives them, so that
// keyed archives move between the systems.
static NSString *const NSUUIDBytesKey = @"NS.uuidbytes";

@implementation NSUUID {
    uuid_t _bytes;
}

+ (instancetype)UUID
{
    return [[self alloc] init];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)init
{
    uuid_t bytes;
    uuid_generate_random(bytes);
    return [self initWithUUIDBytes:bytes];
}

- (instancetype)initWithUUIDBytes:(const uuid_t)bytes
{
    self = [super init];
    if (self == nil)
        return nil;
    if (bytes != NULL)
        memcpy(_bytes, bytes, sizeof(uuid_t));
    else
        uuid_clear(_bytes);
    return self;
}

- (nullable instancetype)initWithUUIDString:(NSString *)string
{
    // uuid_parse takes 36 characters: 8-4-4-4-12 hexadecimal digits.
    const char *text = [string UTF8String];
    uuid_t bytes;
    if (text == NULL || strlen(text) != 36 || uuid_parse(text, bytes) != 0)
        return nil;
    return [self initWithUUIDBytes:bytes];
}

- (nullable instancetype)initWithCoder:(NSCoder *)coder
{
    const uint8_t *bytes = NULL;
    NSUInteger length = 0;
    if ([coder allowsKeyedCoding]) {
        bytes = [coder decodeBytesForKey:NSUUIDBytesKey returnedLength:&length];
    } else {
        bytes = [coder decodeBytesWithReturnedLength:&length];
    }
    if (bytes == NULL || length != sizeof(uuid_t))
        return nil;
    return [self initWithUUIDBytes:bytes];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    if ([coder allowsKeyedCoding])
        [coder encodeBytes:_bytes length:sizeof(uuid_t) forKey:NSUUIDBytesKey];
    else
        [coder encodeBytes:_bytes length:sizeof(uuid_t)];
}

- (id)copyWithZone:(NSZone *)zone
{
    (void)zone;
    return self;    // immutable
}

- (void)getUUIDBytes:(uuid_t)uuid
{
    memcpy(uuid, _bytes, sizeof(uuid_t));
}

- (NSString *)UUIDString
{
    uuid_string_t text;
    uuid_unparse_upper(_bytes, text);
    return [NSString stringWithUTF8String:text];
}

- (NSComparisonResult)compare:(NSUUID *)otherUUID
{
    uuid_t other;
    [otherUUID getUUIDBytes:other];
    int order = memcmp(_bytes, other, sizeof(uuid_t));
    if (order < 0)
        return NSOrderedAscending;
    if (order > 0)
        return NSOrderedDescending;
    return NSOrderedSame;
}

- (BOOL)isEqual:(id)object
{
    if (object == self)
        return YES;
    if (![object isKindOfClass:[NSUUID class]])
        return NO;
    return [self compare:object] == NSOrderedSame;
}

- (NSUInteger)hash
{
    // The bytes are random already: fold them.
    uint64_t halves[2];
    memcpy(halves, _bytes, sizeof(halves));
    return (NSUInteger)(halves[0] ^ halves[1]);
}

// As on macOS: the class and address, then the UUID.
- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %p> %@", [self class], self, [self UUIDString]];
}

@end
