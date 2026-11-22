/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * __NSCFError: the class of CoreFoundation's CFErrors, an NSError, as on
 * the reference platform, so a CFError a framework makes (OpenDirectory's,
 * CFURL's...) is an NSError to Objective-C: its domain, code and user
 * info are CF's, and NSError's methods, which go through them, follow.
 * The other way, CF's CFError functions send an NSError the messages
 * (CF_OBJC_FUNCDISPATCHV in CFError.c). CF keeps the memory.
 */

#import <Foundation/NSError.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSException.h>
#import <Foundation/NSCoder.h>
#import <CoreFoundation/CoreFoundation.h>

@interface __NSCFError : NSError
@end

@implementation __NSCFError

+ (id)allocWithZone:(NSZone *)zone {
    [NSException raise:NSInternalInconsistencyException
                format:@"Private class __NSCFError is not intended to be alloced."];
    return nil;
}

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

- (void)dealloc {
    /* CF frees it when its count reaches zero. */
    return;
    [super dealloc];
}

- (NSString *)domain {
    return (NSString *)CFErrorGetDomain((CFErrorRef)self);
}

- (NSInteger)code {
    return (NSInteger)CFErrorGetCode((CFErrorRef)self);
}

- (NSDictionary *)userInfo {
    CFDictionaryRef info = CFErrorCopyUserInfo((CFErrorRef)self);

    return info != NULL ? [(NSDictionary *)info autorelease] : [NSDictionary dictionary];
}

- (NSUInteger)hash {
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSError class]])
        return NO;
    return [[self domain] isEqual:[other domain]] && [self code] == [other code] &&
        [[self userInfo] isEqual:[other userInfo]];
}

/* Archived as a plain NSError. */
- (Class)classForCoder {
    return [NSError class];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    NSError *plain = [[NSError alloc] initWithDomain:[self domain] code:[self code]
        userInfo:[self userInfo]];

    [plain encodeWithCoder:coder];
    [plain release];
}

@end
