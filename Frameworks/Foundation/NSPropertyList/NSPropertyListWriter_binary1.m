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

// Binary property lists, version 00. Minkar: Foundation could read them
// but not write them (dataFromPropertyList: returned nil).
//
// Layout: "bplist00", the objects, an offset table (each object's offset,
// big-endian, offsetSize bytes each), and a 32-byte trailer: 6 unused bytes,
// the offset size, the object reference size, then as 8-byte big-endian
// numbers the object count, the top object's index and the offset table's
// offset. An object starts with a marker byte: high nibble the type, low
// nibble a size or count (0xF: a count follows as an integer object).
//
//   0x08/0x09 false/true   0x1n integer, 2^n bytes   0x23 double
//   0x33 date (double, seconds since 2001)           0x4n data
//   0x5n ASCII string      0x6n UTF-16BE string      0x8n UID, n+1 bytes
//   0xAn array of refs     0xDn dictionary: key refs, then value refs
//
// Strings are written once however often they occur (dictionary keys).
// A dictionary {CF$UID = n}, which NSKeyedArchiver makes, and a CFUID are
// written as UIDs, as on macOS.

#import "NSPropertyListWriter_binary1.h"
#import <Foundation/NSArray.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSString.h>
#import <Foundation/NSValue.h>
#import <Foundation/CFUID.h>
#import <CoreFoundation/CFNumber.h>

#include <string.h>

@implementation NSPropertyListWriter_binary1 {
    NSMutableArray *_objects;           // by index
    NSMutableArray *_children;          // by index: child indices (or NSNull)
    NSMutableDictionary *_strings;      // string -> index
    NSMutableData *_out;
    NSUInteger _refSize;
    NSString *_why;
}

static BOOL isUID(id obj, unsigned long long *value) {
    if ([obj isKindOfClass:[CFUID class]]) {
        *value = [obj unsignedLongLongValue];
        return YES;
    }
    if ([obj isKindOfClass:[NSDictionary class]] && [obj count] == 1) {
        id n = [obj objectForKey:@"CF$UID"];
        if ([n isKindOfClass:[NSNumber class]]) {
            *value = [n unsignedLongLongValue];
            return YES;
        }
    }
    return NO;
}

- (NSUInteger)add:(id)obj children:(id)children {
    NSUInteger index = [_objects count];

    [_objects addObject:obj];
    [_children addObject:children];
    return index;
}

// Gives obj and everything in it an index, depth first; containers record
// their children's indices. NSNotFound on a type a plist cannot hold.
- (NSUInteger)collect:(id)obj {
    unsigned long long uid;

    if ([obj isKindOfClass:[NSString class]]) {
        NSNumber *have = [_strings objectForKey:obj];
        if (have != nil)
            return [have unsignedIntegerValue];
        NSUInteger index = [self add:obj children:[NSNull null]];
        [_strings setObject:[NSNumber numberWithUnsignedInteger:index] forKey:obj];
        return index;
    }
    if (isUID(obj, &uid) || [obj isKindOfClass:[NSNumber class]] ||
        [obj isKindOfClass:[NSData class]] || [obj isKindOfClass:[NSDate class]])
        return [self add:obj children:[NSNull null]];
    if ([obj isKindOfClass:[NSArray class]]) {
        NSMutableArray *children = [NSMutableArray arrayWithCapacity:[obj count]];
        NSUInteger index = [self add:obj children:children];
        for (id item in obj) {
            NSUInteger child = [self collect:item];
            if (child == NSNotFound)
                return NSNotFound;
            [children addObject:[NSNumber numberWithUnsignedInteger:child]];
        }
        return index;
    }
    if ([obj isKindOfClass:[NSDictionary class]]) {
        NSArray *keys = [obj allKeys];
        NSMutableArray *children = [NSMutableArray arrayWithCapacity:2 * [keys count]];
        NSMutableArray *values = [NSMutableArray arrayWithCapacity:[keys count]];
        NSUInteger index = [self add:obj children:children];
        for (id key in keys) {
            if (![key isKindOfClass:[NSString class]]) {
                _why = @"dictionary keys must be strings";
                return NSNotFound;
            }
            NSUInteger k = [self collect:key];
            NSUInteger v = [self collect:[obj objectForKey:key]];
            if (k == NSNotFound || v == NSNotFound)
                return NSNotFound;
            [children addObject:[NSNumber numberWithUnsignedInteger:k]];
            [values addObject:[NSNumber numberWithUnsignedInteger:v]];
        }
        [children addObjectsFromArray:values];
        return index;
    }
    _why = [NSString stringWithFormat:@"%@ cannot be in a property list", [obj class]];
    return NSNotFound;
}

static void appendBigEndian(NSMutableData *out, uint64_t value, NSUInteger size) {
    uint8_t bytes[8];

    for (NSUInteger i = 0; i < size; i++)
        bytes[i] = (uint8_t)(value >> (8 * (size - 1 - i)));
    [out appendBytes:bytes length:size];
}

static NSUInteger sizeFor(uint64_t value) {
    if (value <= 0xff)
        return 1;
    if (value <= 0xffff)
        return 2;
    if (value <= 0xffffffffULL)
        return 4;
    return 8;
}

static uint8_t log2Size(NSUInteger size) {
    return size == 1 ? 0 : size == 2 ? 1 : size == 4 ? 2 : 3;
}

static void appendMarker(NSMutableData *out, uint8_t type, uint64_t count) {
    uint8_t marker;

    if (count < 15) {
        marker = (uint8_t)(type | count);
        [out appendBytes:&marker length:1];
        return;
    }
    marker = (uint8_t)(type | 0x0f);
    [out appendBytes:&marker length:1];
    NSUInteger size = sizeFor(count);
    marker = (uint8_t)(0x10 | log2Size(size));
    [out appendBytes:&marker length:1];
    appendBigEndian(out, count, size);
}

- (void)writeNumber:(NSNumber *)n {
    const char *type = [n objCType];
    uint8_t marker;

    if ((CFBooleanRef)n == kCFBooleanTrue || (CFBooleanRef)n == kCFBooleanFalse) {
        marker = ((CFBooleanRef)n == kCFBooleanTrue) ? 0x09 : 0x08;
        [_out appendBytes:&marker length:1];
        return;
    }
    if (type[0] == 'f' || type[0] == 'd') {
        double d = [n doubleValue];
        uint64_t bits;
        memcpy(&bits, &d, 8);
        marker = 0x23;
        [_out appendBytes:&marker length:1];
        appendBigEndian(_out, bits, 8);
        return;
    }
    if (type[0] == 'Q' || type[0] == 'L') {
        unsigned long long u = [n unsignedLongLongValue];
        if (u > (unsigned long long)INT64_MAX) {
            marker = 0x14;                      // 16 bytes; the high half is zero
            [_out appendBytes:&marker length:1];
            appendBigEndian(_out, 0, 8);
            appendBigEndian(_out, u, 8);
            return;
        }
    }
    long long v = [n longLongValue];
    if (v < 0) {
        marker = 0x13;                          // negative: always 8 bytes
        [_out appendBytes:&marker length:1];
        appendBigEndian(_out, (uint64_t)v, 8);
        return;
    }
    NSUInteger size = sizeFor((uint64_t)v);
    marker = (uint8_t)(0x10 | log2Size(size));
    [_out appendBytes:&marker length:1];
    appendBigEndian(_out, (uint64_t)v, size);
}

- (void)writeString:(NSString *)s {
    NSUInteger length = [s length];
    unichar   *chars = malloc((length + 1) * sizeof(unichar));
    BOOL       ascii = YES;

    [s getCharacters:chars range:NSMakeRange(0, length)];
    for (NSUInteger i = 0; i < length; i++)
        if (chars[i] >= 0x80) {
            ascii = NO;
            break;
        }
    if (ascii) {
        appendMarker(_out, 0x50, length);
        for (NSUInteger i = 0; i < length; i++) {
            uint8_t c = (uint8_t)chars[i];
            [_out appendBytes:&c length:1];
        }
    } else {
        appendMarker(_out, 0x60, length);
        for (NSUInteger i = 0; i < length; i++)
            appendBigEndian(_out, chars[i], 2);
    }
    free(chars);
}

- (void)writeObjectAtIndex:(NSUInteger)index {
    id obj = [_objects objectAtIndex:index];
    unsigned long long uid;

    if ([obj isKindOfClass:[NSString class]]) {
        [self writeString:obj];
    } else if (isUID(obj, &uid)) {
        NSUInteger size = sizeFor(uid);
        uint8_t marker = (uint8_t)(0x80 | (size - 1));
        [_out appendBytes:&marker length:1];
        appendBigEndian(_out, uid, size);
    } else if ([obj isKindOfClass:[NSNumber class]]) {
        [self writeNumber:obj];
    } else if ([obj isKindOfClass:[NSData class]]) {
        appendMarker(_out, 0x40, [obj length]);
        [_out appendData:obj];
    } else if ([obj isKindOfClass:[NSDate class]]) {
        double   t = [obj timeIntervalSinceReferenceDate];
        uint64_t bits;
        uint8_t  marker = 0x33;
        memcpy(&bits, &t, 8);
        [_out appendBytes:&marker length:1];
        appendBigEndian(_out, bits, 8);
    } else {
        NSArray *children = [_children objectAtIndex:index];
        BOOL     isArray = [obj isKindOfClass:[NSArray class]];

        appendMarker(_out, isArray ? 0xA0 : 0xD0, isArray ? [children count] : [children count] / 2);
        for (NSNumber *child in children)
            appendBigEndian(_out, [child unsignedIntegerValue], _refSize);
    }
}

- (NSData *)write:(id)plist {
    NSUInteger count, offsetSize, tableOffset;
    uint64_t  *offsets;

    _objects = [NSMutableArray array];
    _children = [NSMutableArray array];
    _strings = [NSMutableDictionary dictionary];
    if ([self collect:plist] == NSNotFound)
        return nil;
    count = [_objects count];
    _refSize = sizeFor(count);
    _out = [NSMutableData dataWithBytes:"bplist00" length:8];
    offsets = malloc(count * sizeof(uint64_t));
    for (NSUInteger i = 0; i < count; i++) {
        offsets[i] = [_out length];
        [self writeObjectAtIndex:i];
    }
    tableOffset = [_out length];
    offsetSize = sizeFor(tableOffset);
    for (NSUInteger i = 0; i < count; i++)
        appendBigEndian(_out, offsets[i], offsetSize);
    free(offsets);

    uint8_t trailer[8] = { 0, 0, 0, 0, 0, 0, (uint8_t)offsetSize, (uint8_t)_refSize };
    [_out appendBytes:trailer length:8];
    appendBigEndian(_out, count, 8);
    appendBigEndian(_out, 0, 8);                // the top object is the first
    appendBigEndian(_out, tableOffset, 8);
    return _out;
}

+ (NSData *)dataWithPropertyList:(id)plist error:(NSString **)why {
    NSPropertyListWriter_binary1 *writer = [[[self alloc] init] autorelease];
    NSData *data = [writer write:plist];

    if (data == nil && why != NULL)
        *why = writer->_why;
    return data;
}

@end
