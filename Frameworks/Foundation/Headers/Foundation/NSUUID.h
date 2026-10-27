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

#import <Foundation/NSObject.h>
#include <uuid/uuid.h>

@class NSString;

NS_ASSUME_NONNULL_BEGIN

// A universally unique identifier (RFC 4122): 16 bytes, written as
// "E621E1F8-C36C-495A-93FC-0C247A3E6E5F". Immutable.
@interface NSUUID : NSObject <NSCopying, NSSecureCoding>

// A new random (version 4) UUID.
+ (instancetype)UUID;

// A new random (version 4) UUID.
- (instancetype)init;

// The UUID a string names, in either case; nil if the string is not a UUID.
- (nullable instancetype)initWithUUIDString:(NSString *)string;

// The UUID with these 16 bytes.
- (instancetype)initWithUUIDBytes:(const uuid_t _Nullable)bytes NS_DESIGNATED_INITIALIZER;

- (nullable instancetype)initWithCoder:(NSCoder *)coder;

- (void)getUUIDBytes:(uuid_t _Nonnull)uuid;

// Orders UUIDs by their bytes.
- (NSComparisonResult)compare:(NSUUID *)otherUUID;

// Upper-case hexadecimal, with hyphens.
@property (readonly, copy) NSString *UUIDString;

@end

NS_ASSUME_NONNULL_END
