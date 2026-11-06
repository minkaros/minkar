/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/*
 * Toll-free bridging of URLs. __NSCFURL is the class of CF's URL objects
 * once Foundation is loaded (CFSetupFoundationBridging): the object is
 * CF's, with CF's layout and memory, so it takes no NSURL method that
 * reads NSURL's instance variables. It says it is an NSURL, and each NSURL
 * message goes to an NSURL made from its string and base. In the other
 * direction NSURL answers -_cfurl with a CF URL of its own string and
 * base, which CF's URL functions use (CFURL.c).
 */

#import <Foundation/NSURL.h>
#import <Foundation/NSString.h>
#import <CoreFoundation/CFURL.h>
#import <objc/runtime.h>

@interface __NSCFType : NSObject
@end

@interface __NSCFURL : __NSCFType
@end

@implementation __NSCFURL

/* The same URL as Foundation's NSURL (autoreleased). */
- (NSURL *)_nsurl {
    CFURLRef base = CFURLGetBaseURL((CFURLRef)self);
    NSURL *nsBase = base ? [(id)base _nsurl] : nil;

    return [NSURL URLWithString:(NSString *)CFURLGetString((CFURLRef)self) relativeToURL:nsBase];
}

- (CFURLRef)_cfurl {
    return (CFURLRef)self;
}

- (Class)class {
    return [NSURL class];
}

- (BOOL)isKindOfClass:(Class)cls {
    return [NSURL isSubclassOfClass:cls] || [super isKindOfClass:cls];
}

- (BOOL)isMemberOfClass:(Class)cls {
    return cls == [NSURL class];
}

- (BOOL)respondsToSelector:(SEL)sel {
    return [super respondsToSelector:sel] || [NSURL instancesRespondToSelector:sel];
}

- (BOOL)conformsToProtocol:(Protocol *)protocol {
    return [NSURL conformsToProtocol:protocol] || [super conformsToProtocol:protocol];
}

- (id)forwardingTargetForSelector:(SEL)sel {
    return [self _nsurl];
}

- (NSString *)description {
    return [[self _nsurl] description];
}

- (NSString *)absoluteString {
    return [[self _nsurl] absoluteString];
}

- (NSString *)relativeString {
    return (NSString *)CFURLGetString((CFURLRef)self);
}

- (NSURL *)baseURL {
    return (NSURL *)CFURLGetBaseURL((CFURLRef)self);
}

- (BOOL)isFileURL {
    return [[self _nsurl] isFileURL];
}

- (NSString *)path {
    return [[self _nsurl] path];
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSURL class]])
        return NO;
    return [[self _nsurl] isEqual:other];
}

- (NSUInteger)hash {
    return [[self _nsurl] hash];
}

@end
