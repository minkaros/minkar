/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What OpenDirectory's Objective-C classes take from Foundation's private
 * declarations: the exception of an abstract method a subclass must
 * implement. OpenDirectory's own copy, found before the SDK's headers.
 */

#import <Foundation/Foundation.h>

static inline void NSRequestConcreteImplementation(id self, SEL cmd, Class cls)
{
	[NSException raise:NSInvalidArgumentException
	    format:@"*** -%@ only defined for abstract class. Define -[%@ %@]!",
	    NSStringFromSelector(cmd), NSStringFromClass([self class]), NSStringFromSelector(cmd)];
	(void)cls;
}

/* The methods that make a CF type's instances Objective-C objects of its
   bridged class: memory, equality and description go to CF. */
#import <CoreFoundation/CFPriv.h>

#define CF_CLASSIMPLEMENTATION(name) \
- (id)retain { return (id)CFRetain((CFTypeRef)self); } \
- (oneway void)release { CFRelease((CFTypeRef)self); } \
- (NSUInteger)retainCount { return (NSUInteger)CFGetRetainCount((CFTypeRef)self); } \
- (NSUInteger)hash { return (NSUInteger)CFHash((CFTypeRef)self); } \
- (BOOL)isEqual:(id)other { return other != nil && CFEqual((CFTypeRef)self, (CFTypeRef)other); }

#ifndef NSMakeCollectable
#define NSMakeCollectable(cf) ((id)(cf))
#endif
