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
 * Toll-free bridging of character sets, as of arrays (NSCFCollections.m).
 *
 * __NSCFCharacterSet is the class of CF's character sets once Foundation
 * is loaded (CFSetupFoundationBridging): Swift's CharacterSet is a
 * CFCharacterSet, and Foundation sent it NSCharacterSet's messages. Each
 * method calls the CF function. In the other direction CF's functions
 * send Foundation's own character sets the messages in the category
 * below.
 */

#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSData.h>
#import <Foundation/NSString.h>
#import <CoreFoundation/CFCharacterSet.h>

@interface __NSCFCharacterSet : NSMutableCharacterSet
@end

#define CFSELF ((CFMutableCharacterSetRef)self)

@implementation __NSCFCharacterSet

// The memory is CF's: CF frees the set when its count reaches zero.
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
    return CFCharacterSetGetTypeID();
}

- (NSUInteger)hash {
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSCharacterSet class]])
        return NO;
    return [[self bitmapRepresentation] isEqual:[other bitmapRepresentation]];
}

- (BOOL)characterIsMember:(unichar)character {
    return CFCharacterSetIsCharacterMember(CFSELF, character);
}

- (BOOL)longCharacterIsMember:(UTF32Char)character {
    return CFCharacterSetIsLongCharacterMember(CFSELF, character);
}

- (BOOL)hasMemberInPlane:(uint8_t)plane {
    return CFCharacterSetHasMemberInPlane(CFSELF, plane);
}

- (BOOL)isSupersetOfSet:(NSCharacterSet *)other {
    return CFCharacterSetIsSupersetOfSet(CFSELF, (CFCharacterSetRef)other);
}

- (NSData *)bitmapRepresentation {
    return [(NSData *)CFCharacterSetCreateBitmapRepresentation(NULL, CFSELF) autorelease];
}

- (NSCharacterSet *)invertedSet {
    return [(NSCharacterSet *)CFCharacterSetCreateInvertedSet(NULL, CFSELF) autorelease];
}

- (id)copyWithZone:(NSZone *)zone {
    return (id)CFCharacterSetCreateCopy(NULL, CFSELF);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFCharacterSetCreateMutableCopy(NULL, CFSELF);
}

- (void)addCharactersInRange:(NSRange)range {
    CFCharacterSetAddCharactersInRange(CFSELF, CFRangeMake(range.location, range.length));
}

- (void)removeCharactersInRange:(NSRange)range {
    CFCharacterSetRemoveCharactersInRange(CFSELF, CFRangeMake(range.location, range.length));
}

- (void)addCharactersInString:(NSString *)string {
    CFCharacterSetAddCharactersInString(CFSELF, (CFStringRef)string);
}

- (void)removeCharactersInString:(NSString *)string {
    CFCharacterSetRemoveCharactersInString(CFSELF, (CFStringRef)string);
}

- (void)formUnionWithCharacterSet:(NSCharacterSet *)other {
    CFCharacterSetUnion(CFSELF, (CFCharacterSetRef)other);
}

- (void)formIntersectionWithCharacterSet:(NSCharacterSet *)other {
    CFCharacterSetIntersect(CFSELF, (CFCharacterSetRef)other);
}

- (void)invert {
    CFCharacterSetInvert(CFSELF);
}

@end

// What CF's functions send Foundation's own character sets.
@implementation NSCharacterSet (CFBridging)

- (BOOL)hasMemberInPlane:(uint8_t)plane {
    if (plane != 0)
        return NO;
    NSData *bitmap = [self bitmapRepresentation];
    const uint8_t *bytes = [bitmap bytes];
    NSUInteger i, length = [bitmap length];

    for (i = 0; i < length; i++)
        if (bytes[i] != 0)
            return YES;
    return NO;
}

- (CFDataRef)_retainedBitmapRepresentation {
    return (CFDataRef)[[self bitmapRepresentation] retain];
}

// No CF set stands for it: CF takes the general path.
- (CFCharacterSetRef)_expandedCFCharacterSet {
    return NULL;
}

@end
