#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 The Minkar contributors
# Part of Minkar.
#
"""Generates Foundation's units (NSUnit.h, NSUnit.m) from table.py.

Usage: python3 tools/units/gen.py Frameworks/Foundation
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from table import TABLE

out = sys.argv[1]
LIC = """/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * %s
 * Generated from a table of the units and their factors to the base unit
 * of their dimension (tools/units/gen.py); edit the table, not this.
 */
"""

def c(v):
    r = repr(float(v))
    return r

h = [LIC % "Units of measure (API 19): units, dimensions and their converters."]
h.append("""#import <Foundation/NSObject.h>

@class NSString;

NS_ASSUME_NONNULL_BEGIN

/* Converts a unit's values to and from its dimension's base unit. */
@interface NSUnitConverter : NSObject
- (double)baseUnitValueFromValue:(double)value;
- (double)valueFromBaseUnitValue:(double)baseUnitValue;
@end

/* value x coefficient + constant, in the base unit. */
@interface NSUnitConverterLinear : NSUnitConverter <NSSecureCoding> {
    double _coefficient;
    double _constant;
}
@property (readonly) double coefficient;
@property (readonly) double constant;
- (instancetype)initWithCoefficient:(double)coefficient;
- (instancetype)initWithCoefficient:(double)coefficient constant:(double)constant NS_DESIGNATED_INITIALIZER;
@end

@interface NSUnit : NSObject <NSCopying, NSSecureCoding> {
    NSString *_symbol;
}
@property (readonly, copy) NSString *symbol;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)initWithSymbol:(NSString *)symbol NS_DESIGNATED_INITIALIZER;
@end

@interface NSDimension : NSUnit <NSSecureCoding> {
    NSUnitConverter *_converter;
}
@property (readonly, copy) NSUnitConverter *converter;
- (instancetype)initWithSymbol:(NSString *)symbol converter:(NSUnitConverter *)converter NS_DESIGNATED_INITIALIZER;
+ (instancetype)baseUnit;
@end
""")
for cls, base, units in TABLE:
    h.append("\n@interface %s : NSDimension <NSSecureCoding>" % cls)
    if cls == "NSUnitConcentrationMass":
        h.append("+ (NSUnitConcentrationMass *)millimolesPerLiterWithGramsPerMole:(double)gramsPerMole;")
    for u in units:
        h.append("@property (class, readonly, copy) %s *%s;" % (cls, u[0]))
    h.append("@end")
h.append("\nNS_ASSUME_NONNULL_END\n")
open(os.path.join(out, "Headers/Foundation/NSUnit.h"), "w").write("\n".join(h))

m = [LIC % "Units of measure (API 19). ARC."]
m.append("""#import <Foundation/NSUnit.h>
#import <Foundation/NSString.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSMutableDictionary.h>

@implementation NSUnitConverter

- (double)baseUnitValueFromValue:(double)value {
    return value;
}

- (double)valueFromBaseUnitValue:(double)baseUnitValue {
    return baseUnitValue;
}

@end

@implementation NSUnitConverterLinear

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoefficient:(double)coefficient {
    return [self initWithCoefficient:coefficient constant:0];
}

- (instancetype)initWithCoefficient:(double)coefficient constant:(double)constant {
    if ((self = [super init]) == nil)
        return nil;
    _coefficient = coefficient;
    _constant = constant;
    return self;
}

- (double)coefficient {
    return _coefficient;
}

- (double)constant {
    return _constant;
}

- (double)baseUnitValueFromValue:(double)value {
    return value * _coefficient + _constant;
}

- (double)valueFromBaseUnitValue:(double)baseUnitValue {
    return (baseUnitValue - _constant) / _coefficient;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeDouble:_coefficient forKey:@"NS.coefficient"];
    [coder encodeDouble:_constant forKey:@"NS.constant"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithCoefficient:[coder decodeDoubleForKey:@"NS.coefficient"]
        constant:[coder decodeDoubleForKey:@"NS.constant"]];
}

- (BOOL)isEqual:(id)other {
    return [other isKindOfClass:[NSUnitConverterLinear class]] &&
        [other coefficient] == _coefficient && [other constant] == _constant;
}

- (NSUInteger)hash {
    return (NSUInteger)(_coefficient * 1000);
}

@end

/* coefficient / value, in the base unit (fuel efficiency's miles per
 * gallon against liters per 100 km). */
@interface _NSUnitConverterReciprocal : NSUnitConverter <NSSecureCoding>
- (instancetype)initWithReciprocal:(double)reciprocal;
@end

@implementation _NSUnitConverterReciprocal {
    double _reciprocal;
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithReciprocal:(double)reciprocal {
    if ((self = [super init]) == nil)
        return nil;
    _reciprocal = reciprocal;
    return self;
}

- (double)baseUnitValueFromValue:(double)value {
    return _reciprocal / value;
}

- (double)valueFromBaseUnitValue:(double)baseUnitValue {
    return _reciprocal / baseUnitValue;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeDouble:_reciprocal forKey:@"NS.reciprocal"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithReciprocal:[coder decodeDoubleForKey:@"NS.reciprocal"]];
}

@end

@implementation NSUnit

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithSymbol:(NSString *)symbol {
    if ((self = [super init]) == nil)
        return nil;
    _symbol = [symbol copy];
    return self;
}

- (NSString *)symbol {
    return _symbol;
}

- (id)copyWithZone:(NSZone *)zone {
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_symbol forKey:@"NS.symbol"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithSymbol:[coder decodeObjectOfClass:[NSString class] forKey:@"NS.symbol"] ?: @""];
}

- (BOOL)isEqual:(id)other {
    return other == self || ([other isMemberOfClass:[self class]] && [[other symbol] isEqualToString:_symbol]);
}

- (NSUInteger)hash {
    return [_symbol hash];
}

- (NSString *)description {
    return _symbol;
}

@end

@implementation NSDimension

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithSymbol:(NSString *)symbol {
    return [self initWithSymbol:symbol converter:[[NSUnitConverterLinear alloc] initWithCoefficient:1]];
}

- (instancetype)initWithSymbol:(NSString *)symbol converter:(NSUnitConverter *)converter {
    if ((self = [super initWithSymbol:symbol]) == nil)
        return nil;
    _converter = converter;
    return self;
}

- (NSUnitConverter *)converter {
    return _converter;
}

/* Each dimension says its own. */
+ (instancetype)baseUnit {
    return nil;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [super encodeWithCoder:coder];
    [coder encodeObject:_converter forKey:@"NS.converter"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSString *symbol = [coder decodeObjectOfClass:[NSString class] forKey:@"NS.symbol"] ?: @"";
    NSUnitConverter *converter = [coder decodeObjectOfClass:[NSUnitConverter class] forKey:@"NS.converter"];
    return [self initWithSymbol:symbol converter:converter ?: [[NSUnitConverterLinear alloc] initWithCoefficient:1]];
}

- (BOOL)isEqual:(id)other {
    return other == self || ([super isEqual:other] && [[other converter] isEqual:_converter]);
}

@end

/* A unit of a class, made once: the class and its name are the key. */
static id unitOf(Class cls, NSString *name, NSString *symbol, NSUnitConverter *(^converter)(void)) {
    static NSMutableDictionary *units;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ units = [NSMutableDictionary dictionary]; });
    NSString *key = [NSString stringWithFormat:@"%s.%@", class_getName(cls), name];
    @synchronized(units) {
        id unit = units[key];
        if (unit == nil) {
            unit = [[cls alloc] initWithSymbol:symbol converter:converter()];
            units[key] = unit;
        }
        return unit;
    }
}
""")
for cls, base, units in TABLE:
    m.append("\n@implementation %s\n" % cls)
    m.append("+ (BOOL)supportsSecureCoding {\n    return YES;\n}\n")
    m.append("+ (instancetype)baseUnit {\n    return (id)[%s %s];\n}\n" % (cls, base))
    if cls == "NSUnitConcentrationMass":
        m.append("""/* Millimoles per liter of a substance of this molar mass: 1 mmol/L is
 * gramsPerMole / 1000 g/L. */
+ (NSUnitConcentrationMass *)millimolesPerLiterWithGramsPerMole:(double)gramsPerMole {
    return [[NSUnitConcentrationMass alloc] initWithSymbol:@"mmol/L"
        converter:[[NSUnitConverterLinear alloc] initWithCoefficient:gramsPerMole / 1000.0]];
}
""")
    for u in units:
        name, sym, coef = u[0], u[1], u[2]
        const = u[3] if len(u) > 3 and u[3] is not None else 0
        recip = len(u) > 4 and u[4] == "reciprocal"
        if recip:
            conv = "[[_NSUnitConverterReciprocal alloc] initWithReciprocal:%s]" % c(coef)
        elif const:
            conv = "[[NSUnitConverterLinear alloc] initWithCoefficient:%s constant:%s]" % (c(coef), c(const))
        else:
            conv = "[[NSUnitConverterLinear alloc] initWithCoefficient:%s]" % c(coef)
        m.append("+ (%s *)%s {\n    return unitOf(self == [%s class] ? self : [%s class], @\"%s\", @\"%s\", ^NSUnitConverter *{ return %s; });\n}\n"
                 % (cls, name, cls, cls, name, sym, conv))
    m.append("@end")
m.insert(1, "#include <objc/runtime.h>\n#include <dispatch/dispatch.h>\n")
open(os.path.join(out, "NSUnit.m"), "w").write("\n".join(m) + "\n")
print("units:", sum(len(u) for _, _, u in TABLE))
