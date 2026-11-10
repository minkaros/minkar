/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The class tests Swift's runtime asks of Objective-C objects (since Swift
 * 6.4, comparing a Swift string with an NSString sends it isNSString__):
 * NO from NSObject, YES from the class each one names.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSOrderedSet.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSTimeZone.h>

@implementation NSObject (SwiftBridging)
- (BOOL)isNSObject__ { return YES; }
- (BOOL)isNSString__ { return NO; }
- (BOOL)isNSCFConstantString__ { return NO; }
- (BOOL)isNSArray__ { return NO; }
- (BOOL)isNSDictionary__ { return NO; }
- (BOOL)isNSSet__ { return NO; }
- (BOOL)isNSOrderedSet__ { return NO; }
- (BOOL)isNSNumber__ { return NO; }
- (BOOL)isNSValue__ { return NO; }
- (BOOL)isNSData__ { return NO; }
- (BOOL)isNSDate__ { return NO; }
- (BOOL)isNSTimeZone__ { return NO; }
@end

@implementation NSString (SwiftBridging)
- (BOOL)isNSString__ { return YES; }
@end

@implementation NSConstantString (SwiftBridging)
- (BOOL)isNSCFConstantString__ { return YES; }
@end

@implementation NSArray (SwiftBridging)
- (BOOL)isNSArray__ { return YES; }
@end

@implementation NSDictionary (SwiftBridging)
- (BOOL)isNSDictionary__ { return YES; }
@end

@implementation NSSet (SwiftBridging)
- (BOOL)isNSSet__ { return YES; }
@end

@implementation NSOrderedSet (SwiftBridging)
- (BOOL)isNSOrderedSet__ { return YES; }
@end

@implementation NSValue (SwiftBridging)
- (BOOL)isNSValue__ { return YES; }
@end

@implementation NSNumber (SwiftBridging)
- (BOOL)isNSNumber__ { return YES; }
@end

@implementation NSData (SwiftBridging)
- (BOOL)isNSData__ { return YES; }
@end

@implementation NSDate (SwiftBridging)
- (BOOL)isNSDate__ { return YES; }
@end

@implementation NSTimeZone (SwiftBridging)
- (BOOL)isNSTimeZone__ { return YES; }
@end
