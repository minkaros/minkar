/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Measurements (API 19): values in units of one dimension convert through
 * the dimension's base unit. ARC.
 */

#import <Foundation/NSMeasurement.h>
#import <Foundation/NSUnit.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>

@implementation NSMeasurement

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithDoubleValue:(double)doubleValue unit:(NSUnit *)unit {
    if ((self = [super init]) == nil)
        return nil;
    _doubleValue = doubleValue;
    _unit = [unit copy];
    return self;
}

- (NSUnit *)unit {
    return _unit;
}

- (double)doubleValue {
    return _doubleValue;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

/* Units of one dimension class (their converters lead to its base unit),
 * or the same unit. */
- (BOOL)canBeConvertedToUnit:(NSUnit *)unit {
    if ([unit isEqual:_unit])
        return YES;
    if (![unit isKindOfClass:[NSDimension class]] || ![_unit isKindOfClass:[NSDimension class]])
        return NO;
    return [unit isKindOfClass:[_unit class]] || [_unit isKindOfClass:[unit class]];
}

- (NSMeasurement *)measurementByConvertingToUnit:(NSUnit *)unit {
    if ([unit isEqual:_unit])
        return [[NSMeasurement alloc] initWithDoubleValue:_doubleValue unit:unit];
    if (![self canBeConvertedToUnit:unit])
        [NSException raise:NSInvalidArgumentException format:@"cannot convert %@ to %@", _unit.symbol, unit.symbol];
    double base = [[(NSDimension *)_unit converter] baseUnitValueFromValue:_doubleValue];
    double value = [[(NSDimension *)unit converter] valueFromBaseUnitValue:base];
    return [[NSMeasurement alloc] initWithDoubleValue:value unit:unit];
}

/* In this unit when both are; else in the base unit. */
- (NSMeasurement *)_combine:(NSMeasurement *)other sign:(double)sign {
    if ([other.unit isEqual:_unit])
        return [[NSMeasurement alloc] initWithDoubleValue:_doubleValue + sign * other.doubleValue unit:_unit];
    if (![self canBeConvertedToUnit:other.unit])
        [NSException raise:NSInvalidArgumentException format:@"cannot combine %@ and %@", _unit.symbol, other.unit.symbol];
    NSDimension *base = [[_unit class] baseUnit];
    double mine = [[(NSDimension *)_unit converter] baseUnitValueFromValue:_doubleValue];
    double theirs = [[(NSDimension *)other.unit converter] baseUnitValueFromValue:other.doubleValue];
    return [[NSMeasurement alloc] initWithDoubleValue:mine + sign * theirs unit:base];
}

- (NSMeasurement *)measurementByAddingMeasurement:(NSMeasurement *)measurement {
    return [self _combine:measurement sign:1];
}

- (NSMeasurement *)measurementBySubtractingMeasurement:(NSMeasurement *)measurement {
    return [self _combine:measurement sign:-1];
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSMeasurement class]])
        return NO;
    NSMeasurement *m = other;
    if ([m.unit isEqual:_unit])
        return m.doubleValue == _doubleValue;
    if (![self canBeConvertedToUnit:m.unit])
        return NO;
    return [[self measurementByConvertingToUnit:m.unit] doubleValue] == m.doubleValue;
}

- (NSUInteger)hash {
    return (NSUInteger)_doubleValue;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%g %@", _doubleValue, _unit.symbol];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeDouble:_doubleValue forKey:@"NS.value"];
    [coder encodeObject:_unit forKey:@"NS.unit"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithDoubleValue:[coder decodeDoubleForKey:@"NS.value"]
        unit:[coder decodeObjectOfClass:[NSUnit class] forKey:@"NS.unit"]];
}

@end
