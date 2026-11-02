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
 * Toll-free bridging of data (as NSCFCollections.m does for arrays and
 * dictionaries). __NSCFData is the class of CF's data objects once
 * Foundation is loaded (CFSetupFoundationBridging): the object is CF's,
 * with CF's layout and memory, and each Objective-C method calls the CF
 * function. In the other direction CFData.c sends Foundation's own NSData
 * the messages.
 */

#import <Foundation/NSData.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <CoreFoundation/CFData.h>
#include <string.h>

CF_EXPORT Boolean _CFDataIsMutable(CFDataRef data);

@interface __NSCFData : NSMutableData
@end

@implementation __NSCFData

#define CFSELF ((CFMutableDataRef)self)

static void checkMutable(__NSCFData *self, SEL cmd) {
    if (!_CFDataIsMutable((CFDataRef)self))
        [NSException raise:NSInternalInconsistencyException
                    format:@"-[%@ %@]: mutating method sent to immutable object",
                           NSStringFromClass([self class]), NSStringFromSelector(cmd)];
}

static void checkRange(__NSCFData *self, SEL cmd, NSRange range) {
    NSUInteger length = (NSUInteger)CFDataGetLength((CFDataRef)self);

    if (range.location > length || range.length > length - range.location)
        [NSException raise:NSRangeException
                    format:@"-[%@ %@]: range {%lu, %lu} beyond length %lu",
                           NSStringFromClass([self class]), NSStringFromSelector(cmd),
                           (unsigned long)range.location, (unsigned long)range.length,
                           (unsigned long)length];
}

// The memory is CF's: CF frees the data when its count reaches zero.
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
    return CFDataGetTypeID();
}

- (NSUInteger)length {
    return (NSUInteger)CFDataGetLength((CFDataRef)self);
}

- (const void *)bytes {
    return CFDataGetBytePtr((CFDataRef)self);
}

- (void *)mutableBytes {
    checkMutable(self, _cmd);
    return CFDataGetMutableBytePtr(CFSELF);
}

- (void)getBytes:(void *)buffer range:(NSRange)range {
    checkRange(self, _cmd, range);
    CFDataGetBytes((CFDataRef)self, CFRangeMake((CFIndex)range.location, (CFIndex)range.length),
                   buffer);
}

- (void)getBytes:(void *)buffer length:(NSUInteger)length {
    NSUInteger have = [self length];
    [self getBytes:buffer range:NSMakeRange(0, length < have ? length : have)];
}

- (void)setLength:(NSUInteger)length {
    checkMutable(self, _cmd);
    CFDataSetLength(CFSELF, (CFIndex)length);
}

- (void)increaseLengthBy:(NSUInteger)extra {
    checkMutable(self, _cmd);
    CFDataIncreaseLength(CFSELF, (CFIndex)extra);
}

- (void)appendBytes:(const void *)bytes length:(NSUInteger)length {
    checkMutable(self, _cmd);
    CFDataAppendBytes(CFSELF, bytes, (CFIndex)length);
}

- (void)appendData:(NSData *)data {
    [self appendBytes:[data bytes] length:[data length]];
}

- (void)replaceBytesInRange:(NSRange)range withBytes:(const void *)bytes {
    [self replaceBytesInRange:range withBytes:bytes length:range.length];
}

- (void)replaceBytesInRange:(NSRange)range withBytes:(const void *)bytes length:(NSUInteger)length {
    checkMutable(self, _cmd);
    checkRange(self, _cmd, range);
    CFDataReplaceBytes(CFSELF, CFRangeMake((CFIndex)range.location, (CFIndex)range.length),
                       bytes, (CFIndex)length);
}

- (void)resetBytesInRange:(NSRange)range {
    checkMutable(self, _cmd);
    checkRange(self, _cmd, range);
    memset(CFDataGetMutableBytePtr(CFSELF) + range.location, 0, range.length);
}

- (void)setData:(NSData *)data {
    checkMutable(self, _cmd);
    CFDataReplaceBytes(CFSELF, CFRangeMake(0, CFDataGetLength((CFDataRef)self)), [data bytes],
                       (CFIndex)[data length]);
}

- (id)copyWithZone:(NSZone *)zone {
    if (!_CFDataIsMutable((CFDataRef)self))
        return [self retain];
    return (id)CFDataCreateCopy(kCFAllocatorDefault, (CFDataRef)self);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFDataCreateMutableCopy(kCFAllocatorDefault, 0, (CFDataRef)self);
}

- (Class)classForCoder {
    return _CFDataIsMutable((CFDataRef)self) ? [NSMutableData class] : [NSData class];
}

@end
