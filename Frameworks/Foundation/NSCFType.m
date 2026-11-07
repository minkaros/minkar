/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * __NSCFType: the class of CoreFoundation's objects of the types with no
 * Foundation class of their own (a CGDisplayModeRef, a CFRunLoopSourceRef,
 * a CFUUIDRef...), as on the reference platform. Their isa was CF's own
 * class record, not an Objective-C class, and putting one in an NSArray
 * (which retains it) crashed. Everything goes back to CF; the memory is
 * CF's.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSException.h>
#import <CoreFoundation/CoreFoundation.h>

@interface __NSCFType : NSObject
@end

@implementation __NSCFType

+ (id)allocWithZone:(NSZone *)zone {
    [NSException raise:NSInternalInconsistencyException
                format:@"Private class __NSCFType is not intended to be alloced."];
    return nil;
}

- (id)retain {
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release {
    CFRelease((CFTypeRef)self);
}

- (id)autorelease {
    return [super autorelease];
}

- (NSUInteger)retainCount {
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}

- (void)dealloc {
    /* CF frees it when its count reaches zero. */
    return;
    [super dealloc];
}

- (NSUInteger)hash {
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (other == nil)
        return NO;
    return CFEqual((CFTypeRef)self, (CFTypeRef)other);
}

- (NSString *)description {
    return [(NSString *)CFCopyDescription((CFTypeRef)self) autorelease];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

@end
