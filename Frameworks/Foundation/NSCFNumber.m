/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * __NSCFNumber: the class of CoreFoundation's numbers, so a CFNumber is an
 * NSNumber (toll-free bridged, as on the reference platform). CF made its
 * numbers with no Objective-C class: one read from a property list or the
 * preferences crashed at its first message. The value is CF's, read with
 * CFNumberGetValue; the memory is CF's too.
 */

#import <Foundation/NSNumber.h>
#import <Foundation/NSString.h>
#import <Foundation/NSException.h>
#import <CoreFoundation/CFNumber.h>
#include <math.h>

@interface __NSCFNumber : NSNumber
@end

@implementation __NSCFNumber

#define CFSELF ((CFNumberRef)self)

+ (id)allocWithZone:(NSZone *)zone {
    [NSException raise:NSInternalInconsistencyException
                format:@"Private class __NSCFNumber is not intended to be alloced."];
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

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

static long long llValue(__NSCFNumber *self) {
    long long v = 0;

    if (CFNumberIsFloatType(CFSELF)) {
        double d = 0;
        CFNumberGetValue(CFSELF, kCFNumberDoubleType, &d);
        return (long long)d;
    }
    CFNumberGetValue(CFSELF, kCFNumberLongLongType, &v);
    return v;
}

static double dValue(__NSCFNumber *self) {
    double d = 0;

    CFNumberGetValue(CFSELF, kCFNumberDoubleType, &d);
    return d;
}

- (const char *)objCType {
    switch (CFNumberGetType(CFSELF)) {
    case kCFNumberSInt8Type: case kCFNumberCharType: return @encode(char);
    case kCFNumberSInt16Type: case kCFNumberShortType: return @encode(short);
    case kCFNumberSInt32Type: case kCFNumberIntType: return @encode(int);
    case kCFNumberLongType: case kCFNumberCFIndexType: case kCFNumberNSIntegerType: return @encode(long);
    case kCFNumberFloat32Type: case kCFNumberFloatType: return @encode(float);
    case kCFNumberFloat64Type: case kCFNumberDoubleType: case kCFNumberCGFloatType: return @encode(double);
    default: return @encode(long long);
    }
}

- (void)getValue:(void *)value {
    switch (CFNumberGetType(CFSELF)) {
    case kCFNumberSInt8Type: case kCFNumberCharType: *(char *)value = (char)llValue(self); break;
    case kCFNumberSInt16Type: case kCFNumberShortType: *(short *)value = (short)llValue(self); break;
    case kCFNumberSInt32Type: case kCFNumberIntType: *(int *)value = (int)llValue(self); break;
    case kCFNumberLongType: case kCFNumberCFIndexType: case kCFNumberNSIntegerType:
        *(long *)value = (long)llValue(self); break;
    case kCFNumberFloat32Type: case kCFNumberFloatType: *(float *)value = (float)dValue(self); break;
    case kCFNumberFloat64Type: case kCFNumberDoubleType: case kCFNumberCGFloatType:
        *(double *)value = dValue(self); break;
    default: *(long long *)value = llValue(self); break;
    }
}

- (BOOL)boolValue { return CFNumberIsFloatType(CFSELF) ? dValue(self) != 0 : llValue(self) != 0; }
- (char)charValue { return (char)llValue(self); }
- (short)shortValue { return (short)llValue(self); }
- (int)intValue { return (int)llValue(self); }
- (long)longValue { return (long)llValue(self); }
- (long long)longLongValue { return llValue(self); }
- (float)floatValue { return (float)dValue(self); }
- (double)doubleValue { return dValue(self); }
- (NSInteger)integerValue { return (NSInteger)llValue(self); }
- (unsigned char)unsignedCharValue { return (unsigned char)llValue(self); }
- (unsigned short)unsignedShortValue { return (unsigned short)llValue(self); }
- (unsigned int)unsignedIntValue { return (unsigned int)llValue(self); }
- (unsigned long)unsignedLongValue { return (unsigned long)llValue(self); }
- (unsigned long long)unsignedLongLongValue { return (unsigned long long)llValue(self); }
- (NSUInteger)unsignedIntegerValue { return (NSUInteger)llValue(self); }

- (NSComparisonResult)compare:(NSNumber *)other {
    if (!CFNumberIsFloatType(CFSELF) && [other isKindOfClass:[__NSCFNumber class]] &&
        !CFNumberIsFloatType((CFNumberRef)other)) {
        long long a = llValue(self), b = [other longLongValue];
        return a == b ? NSOrderedSame : a < b ? NSOrderedAscending : NSOrderedDescending;
    }
    return [super compare:other];
}

- (NSString *)descriptionWithLocale:(NSDictionary *)locale {
    if (CFNumberIsFloatType(CFSELF)) {
        double d = dValue(self);
        if (isnan(d))
            return @"nan";
        if (isinf(d))
            return d > 0 ? @"inf" : @"-inf";
        return [NSString stringWithFormat:@"%.17g", d];
    }
    return [NSString stringWithFormat:@"%lld", llValue(self)];
}

@end
