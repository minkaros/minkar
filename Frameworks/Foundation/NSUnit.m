/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Units of measure (API 19). ARC.
 * Generated from a table of the units and their factors to the base unit
 * of their dimension (tools/units/gen.py); edit the table, not this.
 */

#include <objc/runtime.h>
#include <dispatch/dispatch.h>

#import <Foundation/NSUnit.h>
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


@implementation NSUnitAcceleration

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitAcceleration metersPerSecondSquared];
}

+ (NSUnitAcceleration *)metersPerSecondSquared {
    return unitOf(self == [NSUnitAcceleration class] ? self : [NSUnitAcceleration class], @"metersPerSecondSquared", @"m/s²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitAcceleration *)gravity {
    return unitOf(self == [NSUnitAcceleration class] ? self : [NSUnitAcceleration class], @"gravity", @"g", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:9.81]; });
}

@end

@implementation NSUnitAngle

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitAngle degrees];
}

+ (NSUnitAngle *)degrees {
    return unitOf(self == [NSUnitAngle class] ? self : [NSUnitAngle class], @"degrees", @"°", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitAngle *)arcMinutes {
    return unitOf(self == [NSUnitAngle class] ? self : [NSUnitAngle class], @"arcMinutes", @"ʹ", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.016666666666666666]; });
}

+ (NSUnitAngle *)arcSeconds {
    return unitOf(self == [NSUnitAngle class] ? self : [NSUnitAngle class], @"arcSeconds", @"ʺ", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0002777777777777778]; });
}

+ (NSUnitAngle *)radians {
    return unitOf(self == [NSUnitAngle class] ? self : [NSUnitAngle class], @"radians", @"rad", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:57.2957795131]; });
}

+ (NSUnitAngle *)gradians {
    return unitOf(self == [NSUnitAngle class] ? self : [NSUnitAngle class], @"gradians", @"grad", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.9]; });
}

+ (NSUnitAngle *)revolutions {
    return unitOf(self == [NSUnitAngle class] ? self : [NSUnitAngle class], @"revolutions", @"rev", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:360.0]; });
}

@end

@implementation NSUnitArea

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitArea squareMeters];
}

+ (NSUnitArea *)squareMegameters {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareMegameters", @"Mm²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000000.0]; });
}

+ (NSUnitArea *)squareKilometers {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareKilometers", @"km²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitArea *)squareMeters {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareMeters", @"m²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitArea *)squareCentimeters {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareCentimeters", @"cm²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0001]; });
}

+ (NSUnitArea *)squareMillimeters {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareMillimeters", @"mm²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

+ (NSUnitArea *)squareMicrometers {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareMicrometers", @"µm²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-12]; });
}

+ (NSUnitArea *)squareNanometers {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareNanometers", @"nm²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-18]; });
}

+ (NSUnitArea *)squareInches {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareInches", @"in²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.00064516]; });
}

+ (NSUnitArea *)squareFeet {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareFeet", @"ft²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.09290304]; });
}

+ (NSUnitArea *)squareYards {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareYards", @"yd²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.83612736]; });
}

+ (NSUnitArea *)squareMiles {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"squareMiles", @"mi²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:2589988.110336]; });
}

+ (NSUnitArea *)acres {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"acres", @"ac", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:4046.8564224]; });
}

+ (NSUnitArea *)ares {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"ares", @"a", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:100.0]; });
}

+ (NSUnitArea *)hectares {
    return unitOf(self == [NSUnitArea class] ? self : [NSUnitArea class], @"hectares", @"ha", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:10000.0]; });
}

@end

@implementation NSUnitConcentrationMass

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitConcentrationMass gramsPerLiter];
}

/* Millimoles per liter of a substance of this molar mass: 1 mmol/L is
 * gramsPerMole / 1000 g/L. */
+ (NSUnitConcentrationMass *)millimolesPerLiterWithGramsPerMole:(double)gramsPerMole {
    return [[NSUnitConcentrationMass alloc] initWithSymbol:@"mmol/L"
        converter:[[NSUnitConverterLinear alloc] initWithCoefficient:gramsPerMole / 1000.0]];
}

+ (NSUnitConcentrationMass *)gramsPerLiter {
    return unitOf(self == [NSUnitConcentrationMass class] ? self : [NSUnitConcentrationMass class], @"gramsPerLiter", @"g/L", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitConcentrationMass *)milligramsPerDeciliter {
    return unitOf(self == [NSUnitConcentrationMass class] ? self : [NSUnitConcentrationMass class], @"milligramsPerDeciliter", @"mg/dL", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.01]; });
}

@end

@implementation NSUnitDispersion

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitDispersion partsPerMillion];
}

+ (NSUnitDispersion *)partsPerMillion {
    return unitOf(self == [NSUnitDispersion class] ? self : [NSUnitDispersion class], @"partsPerMillion", @"ppm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

@end

@implementation NSUnitDuration

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitDuration seconds];
}

+ (NSUnitDuration *)hours {
    return unitOf(self == [NSUnitDuration class] ? self : [NSUnitDuration class], @"hours", @"hr", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:3600.0]; });
}

+ (NSUnitDuration *)minutes {
    return unitOf(self == [NSUnitDuration class] ? self : [NSUnitDuration class], @"minutes", @"min", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:60.0]; });
}

+ (NSUnitDuration *)seconds {
    return unitOf(self == [NSUnitDuration class] ? self : [NSUnitDuration class], @"seconds", @"s", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitDuration *)milliseconds {
    return unitOf(self == [NSUnitDuration class] ? self : [NSUnitDuration class], @"milliseconds", @"ms", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitDuration *)microseconds {
    return unitOf(self == [NSUnitDuration class] ? self : [NSUnitDuration class], @"microseconds", @"µs", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

+ (NSUnitDuration *)nanoseconds {
    return unitOf(self == [NSUnitDuration class] ? self : [NSUnitDuration class], @"nanoseconds", @"ns", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-09]; });
}

+ (NSUnitDuration *)picoseconds {
    return unitOf(self == [NSUnitDuration class] ? self : [NSUnitDuration class], @"picoseconds", @"ps", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-12]; });
}

@end

@implementation NSUnitElectricCharge

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitElectricCharge coulombs];
}

+ (NSUnitElectricCharge *)coulombs {
    return unitOf(self == [NSUnitElectricCharge class] ? self : [NSUnitElectricCharge class], @"coulombs", @"C", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitElectricCharge *)megaampereHours {
    return unitOf(self == [NSUnitElectricCharge class] ? self : [NSUnitElectricCharge class], @"megaampereHours", @"MAh", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:3600000000.0]; });
}

+ (NSUnitElectricCharge *)kiloampereHours {
    return unitOf(self == [NSUnitElectricCharge class] ? self : [NSUnitElectricCharge class], @"kiloampereHours", @"kAh", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:3600000.0]; });
}

+ (NSUnitElectricCharge *)ampereHours {
    return unitOf(self == [NSUnitElectricCharge class] ? self : [NSUnitElectricCharge class], @"ampereHours", @"Ah", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:3600.0]; });
}

+ (NSUnitElectricCharge *)milliampereHours {
    return unitOf(self == [NSUnitElectricCharge class] ? self : [NSUnitElectricCharge class], @"milliampereHours", @"mAh", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:3.6]; });
}

+ (NSUnitElectricCharge *)microampereHours {
    return unitOf(self == [NSUnitElectricCharge class] ? self : [NSUnitElectricCharge class], @"microampereHours", @"µAh", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0036]; });
}

@end

@implementation NSUnitElectricCurrent

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitElectricCurrent amperes];
}

+ (NSUnitElectricCurrent *)megaamperes {
    return unitOf(self == [NSUnitElectricCurrent class] ? self : [NSUnitElectricCurrent class], @"megaamperes", @"MA", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitElectricCurrent *)kiloamperes {
    return unitOf(self == [NSUnitElectricCurrent class] ? self : [NSUnitElectricCurrent class], @"kiloamperes", @"kA", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitElectricCurrent *)amperes {
    return unitOf(self == [NSUnitElectricCurrent class] ? self : [NSUnitElectricCurrent class], @"amperes", @"A", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitElectricCurrent *)milliamperes {
    return unitOf(self == [NSUnitElectricCurrent class] ? self : [NSUnitElectricCurrent class], @"milliamperes", @"mA", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitElectricCurrent *)microamperes {
    return unitOf(self == [NSUnitElectricCurrent class] ? self : [NSUnitElectricCurrent class], @"microamperes", @"µA", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

@end

@implementation NSUnitElectricPotentialDifference

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitElectricPotentialDifference volts];
}

+ (NSUnitElectricPotentialDifference *)megavolts {
    return unitOf(self == [NSUnitElectricPotentialDifference class] ? self : [NSUnitElectricPotentialDifference class], @"megavolts", @"MV", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitElectricPotentialDifference *)kilovolts {
    return unitOf(self == [NSUnitElectricPotentialDifference class] ? self : [NSUnitElectricPotentialDifference class], @"kilovolts", @"kV", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitElectricPotentialDifference *)volts {
    return unitOf(self == [NSUnitElectricPotentialDifference class] ? self : [NSUnitElectricPotentialDifference class], @"volts", @"V", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitElectricPotentialDifference *)millivolts {
    return unitOf(self == [NSUnitElectricPotentialDifference class] ? self : [NSUnitElectricPotentialDifference class], @"millivolts", @"mV", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitElectricPotentialDifference *)microvolts {
    return unitOf(self == [NSUnitElectricPotentialDifference class] ? self : [NSUnitElectricPotentialDifference class], @"microvolts", @"µV", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

@end

@implementation NSUnitElectricResistance

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitElectricResistance ohms];
}

+ (NSUnitElectricResistance *)megaohms {
    return unitOf(self == [NSUnitElectricResistance class] ? self : [NSUnitElectricResistance class], @"megaohms", @"MΩ", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitElectricResistance *)kiloohms {
    return unitOf(self == [NSUnitElectricResistance class] ? self : [NSUnitElectricResistance class], @"kiloohms", @"kΩ", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitElectricResistance *)ohms {
    return unitOf(self == [NSUnitElectricResistance class] ? self : [NSUnitElectricResistance class], @"ohms", @"Ω", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitElectricResistance *)milliohms {
    return unitOf(self == [NSUnitElectricResistance class] ? self : [NSUnitElectricResistance class], @"milliohms", @"mΩ", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitElectricResistance *)microohms {
    return unitOf(self == [NSUnitElectricResistance class] ? self : [NSUnitElectricResistance class], @"microohms", @"µΩ", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

@end

@implementation NSUnitEnergy

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitEnergy joules];
}

+ (NSUnitEnergy *)kilojoules {
    return unitOf(self == [NSUnitEnergy class] ? self : [NSUnitEnergy class], @"kilojoules", @"kJ", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitEnergy *)joules {
    return unitOf(self == [NSUnitEnergy class] ? self : [NSUnitEnergy class], @"joules", @"J", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitEnergy *)kilocalories {
    return unitOf(self == [NSUnitEnergy class] ? self : [NSUnitEnergy class], @"kilocalories", @"kCal", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:4184.0]; });
}

+ (NSUnitEnergy *)calories {
    return unitOf(self == [NSUnitEnergy class] ? self : [NSUnitEnergy class], @"calories", @"cal", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:4.184]; });
}

+ (NSUnitEnergy *)kilowattHours {
    return unitOf(self == [NSUnitEnergy class] ? self : [NSUnitEnergy class], @"kilowattHours", @"kWh", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:3600000.0]; });
}

@end

@implementation NSUnitFrequency

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitFrequency hertz];
}

+ (NSUnitFrequency *)terahertz {
    return unitOf(self == [NSUnitFrequency class] ? self : [NSUnitFrequency class], @"terahertz", @"THz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000000.0]; });
}

+ (NSUnitFrequency *)gigahertz {
    return unitOf(self == [NSUnitFrequency class] ? self : [NSUnitFrequency class], @"gigahertz", @"GHz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000.0]; });
}

+ (NSUnitFrequency *)megahertz {
    return unitOf(self == [NSUnitFrequency class] ? self : [NSUnitFrequency class], @"megahertz", @"MHz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitFrequency *)kilohertz {
    return unitOf(self == [NSUnitFrequency class] ? self : [NSUnitFrequency class], @"kilohertz", @"kHz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitFrequency *)hertz {
    return unitOf(self == [NSUnitFrequency class] ? self : [NSUnitFrequency class], @"hertz", @"Hz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitFrequency *)millihertz {
    return unitOf(self == [NSUnitFrequency class] ? self : [NSUnitFrequency class], @"millihertz", @"mHz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitFrequency *)microhertz {
    return unitOf(self == [NSUnitFrequency class] ? self : [NSUnitFrequency class], @"microhertz", @"µHz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

+ (NSUnitFrequency *)nanohertz {
    return unitOf(self == [NSUnitFrequency class] ? self : [NSUnitFrequency class], @"nanohertz", @"nHz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-09]; });
}

+ (NSUnitFrequency *)framesPerSecond {
    return unitOf(self == [NSUnitFrequency class] ? self : [NSUnitFrequency class], @"framesPerSecond", @"FPS", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

@end

@implementation NSUnitFuelEfficiency

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitFuelEfficiency litersPer100Kilometers];
}

+ (NSUnitFuelEfficiency *)litersPer100Kilometers {
    return unitOf(self == [NSUnitFuelEfficiency class] ? self : [NSUnitFuelEfficiency class], @"litersPer100Kilometers", @"L/100km", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitFuelEfficiency *)milesPerImperialGallon {
    return unitOf(self == [NSUnitFuelEfficiency class] ? self : [NSUnitFuelEfficiency class], @"milesPerImperialGallon", @"mpg", ^NSUnitConverter *{ return [[_NSUnitConverterReciprocal alloc] initWithReciprocal:282.480936]; });
}

+ (NSUnitFuelEfficiency *)milesPerGallon {
    return unitOf(self == [NSUnitFuelEfficiency class] ? self : [NSUnitFuelEfficiency class], @"milesPerGallon", @"mpg", ^NSUnitConverter *{ return [[_NSUnitConverterReciprocal alloc] initWithReciprocal:235.214583]; });
}

@end

@implementation NSUnitIlluminance

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitIlluminance lux];
}

+ (NSUnitIlluminance *)lux {
    return unitOf(self == [NSUnitIlluminance class] ? self : [NSUnitIlluminance class], @"lux", @"lx", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

@end

@implementation NSUnitInformationStorage

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitInformationStorage bits];
}

+ (NSUnitInformationStorage *)bytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"bytes", @"B", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8.0]; });
}

+ (NSUnitInformationStorage *)bits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"bits", @"bit", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitInformationStorage *)nibbles {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"nibbles", @"nibble", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:4.0]; });
}

+ (NSUnitInformationStorage *)yottabytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"yottabytes", @"YB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8e+24]; });
}

+ (NSUnitInformationStorage *)zettabytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"zettabytes", @"ZB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8e+21]; });
}

+ (NSUnitInformationStorage *)exabytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"exabytes", @"EB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8e+18]; });
}

+ (NSUnitInformationStorage *)petabytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"petabytes", @"PB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8000000000000000.0]; });
}

+ (NSUnitInformationStorage *)terabytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"terabytes", @"TB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8000000000000.0]; });
}

+ (NSUnitInformationStorage *)gigabytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"gigabytes", @"GB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8000000000.0]; });
}

+ (NSUnitInformationStorage *)megabytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"megabytes", @"MB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8000000.0]; });
}

+ (NSUnitInformationStorage *)kilobytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"kilobytes", @"kB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8000.0]; });
}

+ (NSUnitInformationStorage *)yottabits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"yottabits", @"Yb", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e+24]; });
}

+ (NSUnitInformationStorage *)zettabits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"zettabits", @"Zb", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e+21]; });
}

+ (NSUnitInformationStorage *)exabits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"exabits", @"Eb", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e+18]; });
}

+ (NSUnitInformationStorage *)petabits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"petabits", @"Pb", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000000000.0]; });
}

+ (NSUnitInformationStorage *)terabits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"terabits", @"Tb", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000000.0]; });
}

+ (NSUnitInformationStorage *)gigabits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"gigabits", @"Gb", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000.0]; });
}

+ (NSUnitInformationStorage *)megabits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"megabits", @"Mb", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitInformationStorage *)kilobits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"kilobits", @"kb", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitInformationStorage *)yobibytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"yobibytes", @"YiB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:9.671406556917033e+24]; });
}

+ (NSUnitInformationStorage *)zebibytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"zebibytes", @"ZiB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:9.44473296573929e+21]; });
}

+ (NSUnitInformationStorage *)exbibytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"exbibytes", @"EiB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:9.223372036854776e+18]; });
}

+ (NSUnitInformationStorage *)pebibytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"pebibytes", @"PiB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:9007199254740992.0]; });
}

+ (NSUnitInformationStorage *)tebibytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"tebibytes", @"TiB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8796093022208.0]; });
}

+ (NSUnitInformationStorage *)gibibytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"gibibytes", @"GiB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8589934592.0]; });
}

+ (NSUnitInformationStorage *)mebibytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"mebibytes", @"MiB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8388608.0]; });
}

+ (NSUnitInformationStorage *)kibibytes {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"kibibytes", @"KiB", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:8192.0]; });
}

+ (NSUnitInformationStorage *)yobibits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"yobibits", @"Yib", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.2089258196146292e+24]; });
}

+ (NSUnitInformationStorage *)zebibits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"zebibits", @"Zib", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.1805916207174113e+21]; });
}

+ (NSUnitInformationStorage *)exbibits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"exbibits", @"Eib", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.152921504606847e+18]; });
}

+ (NSUnitInformationStorage *)pebibits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"pebibits", @"Pib", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1125899906842624.0]; });
}

+ (NSUnitInformationStorage *)tebibits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"tebibits", @"Tib", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1099511627776.0]; });
}

+ (NSUnitInformationStorage *)gibibits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"gibibits", @"Gib", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1073741824.0]; });
}

+ (NSUnitInformationStorage *)mebibits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"mebibits", @"Mib", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1048576.0]; });
}

+ (NSUnitInformationStorage *)kibibits {
    return unitOf(self == [NSUnitInformationStorage class] ? self : [NSUnitInformationStorage class], @"kibibits", @"Kib", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1024.0]; });
}

@end

@implementation NSUnitLength

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitLength meters];
}

+ (NSUnitLength *)megameters {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"megameters", @"Mm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitLength *)kilometers {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"kilometers", @"km", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitLength *)hectometers {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"hectometers", @"hm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:100.0]; });
}

+ (NSUnitLength *)decameters {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"decameters", @"dam", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:10.0]; });
}

+ (NSUnitLength *)meters {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"meters", @"m", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitLength *)decimeters {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"decimeters", @"dm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.1]; });
}

+ (NSUnitLength *)centimeters {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"centimeters", @"cm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.01]; });
}

+ (NSUnitLength *)millimeters {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"millimeters", @"mm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitLength *)micrometers {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"micrometers", @"µm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

+ (NSUnitLength *)nanometers {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"nanometers", @"nm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-09]; });
}

+ (NSUnitLength *)picometers {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"picometers", @"pm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-12]; });
}

+ (NSUnitLength *)inches {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"inches", @"in", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0254]; });
}

+ (NSUnitLength *)feet {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"feet", @"ft", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.3048]; });
}

+ (NSUnitLength *)yards {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"yards", @"yd", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.9144]; });
}

+ (NSUnitLength *)miles {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"miles", @"mi", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1609.344]; });
}

+ (NSUnitLength *)scandinavianMiles {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"scandinavianMiles", @"smi", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:10000.0]; });
}

+ (NSUnitLength *)lightyears {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"lightyears", @"ly", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:9460730472580800.0]; });
}

+ (NSUnitLength *)nauticalMiles {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"nauticalMiles", @"NM", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1852.0]; });
}

+ (NSUnitLength *)fathoms {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"fathoms", @"ftm", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.8288]; });
}

+ (NSUnitLength *)furlongs {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"furlongs", @"fur", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:201.168]; });
}

+ (NSUnitLength *)astronomicalUnits {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"astronomicalUnits", @"ua", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:149597870700.0]; });
}

+ (NSUnitLength *)parsecs {
    return unitOf(self == [NSUnitLength class] ? self : [NSUnitLength class], @"parsecs", @"pc", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:3.085677581491367e+16]; });
}

@end

@implementation NSUnitMass

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitMass kilograms];
}

+ (NSUnitMass *)kilograms {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"kilograms", @"kg", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitMass *)grams {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"grams", @"g", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitMass *)decigrams {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"decigrams", @"dg", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0001]; });
}

+ (NSUnitMass *)centigrams {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"centigrams", @"cg", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-05]; });
}

+ (NSUnitMass *)milligrams {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"milligrams", @"mg", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

+ (NSUnitMass *)micrograms {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"micrograms", @"µg", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-09]; });
}

+ (NSUnitMass *)nanograms {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"nanograms", @"ng", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-12]; });
}

+ (NSUnitMass *)picograms {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"picograms", @"pg", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-15]; });
}

+ (NSUnitMass *)ounces {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"ounces", @"oz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.028349523125]; });
}

+ (NSUnitMass *)poundsMass {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"poundsMass", @"lb", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.45359237]; });
}

+ (NSUnitMass *)stones {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"stones", @"st", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:6.35029318]; });
}

+ (NSUnitMass *)metricTons {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"metricTons", @"t", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitMass *)shortTons {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"shortTons", @"ton", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:907.18474]; });
}

+ (NSUnitMass *)carats {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"carats", @"ct", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0002]; });
}

+ (NSUnitMass *)ouncesTroy {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"ouncesTroy", @"oz t", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0311034768]; });
}

+ (NSUnitMass *)slugs {
    return unitOf(self == [NSUnitMass class] ? self : [NSUnitMass class], @"slugs", @"slug", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:14.593903]; });
}

@end

@implementation NSUnitPower

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitPower watts];
}

+ (NSUnitPower *)terawatts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"terawatts", @"TW", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000000.0]; });
}

+ (NSUnitPower *)gigawatts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"gigawatts", @"GW", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000.0]; });
}

+ (NSUnitPower *)megawatts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"megawatts", @"MW", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitPower *)kilowatts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"kilowatts", @"kW", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitPower *)watts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"watts", @"W", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitPower *)milliwatts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"milliwatts", @"mW", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitPower *)microwatts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"microwatts", @"µW", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

+ (NSUnitPower *)nanowatts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"nanowatts", @"nW", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-09]; });
}

+ (NSUnitPower *)picowatts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"picowatts", @"pW", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-12]; });
}

+ (NSUnitPower *)femtowatts {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"femtowatts", @"fW", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-15]; });
}

+ (NSUnitPower *)horsepower {
    return unitOf(self == [NSUnitPower class] ? self : [NSUnitPower class], @"horsepower", @"hp", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:745.6998715822702]; });
}

@end

@implementation NSUnitPressure

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitPressure newtonsPerMetersSquared];
}

+ (NSUnitPressure *)newtonsPerMetersSquared {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"newtonsPerMetersSquared", @"N/m²", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitPressure *)gigapascals {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"gigapascals", @"GPa", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000.0]; });
}

+ (NSUnitPressure *)megapascals {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"megapascals", @"MPa", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitPressure *)kilopascals {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"kilopascals", @"kPa", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitPressure *)hectopascals {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"hectopascals", @"hPa", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:100.0]; });
}

+ (NSUnitPressure *)inchesOfMercury {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"inchesOfMercury", @"inHg", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:3386.389]; });
}

+ (NSUnitPressure *)bars {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"bars", @"bar", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:100000.0]; });
}

+ (NSUnitPressure *)millibars {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"millibars", @"mbar", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:100.0]; });
}

+ (NSUnitPressure *)millimetersOfMercury {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"millimetersOfMercury", @"mmHg", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:133.322387415]; });
}

+ (NSUnitPressure *)poundsForcePerSquareInch {
    return unitOf(self == [NSUnitPressure class] ? self : [NSUnitPressure class], @"poundsForcePerSquareInch", @"psi", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:6894.757293168]; });
}

@end

@implementation NSUnitSpeed

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitSpeed metersPerSecond];
}

+ (NSUnitSpeed *)metersPerSecond {
    return unitOf(self == [NSUnitSpeed class] ? self : [NSUnitSpeed class], @"metersPerSecond", @"m/s", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitSpeed *)kilometersPerHour {
    return unitOf(self == [NSUnitSpeed class] ? self : [NSUnitSpeed class], @"kilometersPerHour", @"km/h", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.2777777777777778]; });
}

+ (NSUnitSpeed *)milesPerHour {
    return unitOf(self == [NSUnitSpeed class] ? self : [NSUnitSpeed class], @"milesPerHour", @"mph", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.44704]; });
}

+ (NSUnitSpeed *)knots {
    return unitOf(self == [NSUnitSpeed class] ? self : [NSUnitSpeed class], @"knots", @"kn", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.5144444444444445]; });
}

@end

@implementation NSUnitTemperature

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitTemperature kelvin];
}

+ (NSUnitTemperature *)kelvin {
    return unitOf(self == [NSUnitTemperature class] ? self : [NSUnitTemperature class], @"kelvin", @"K", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitTemperature *)celsius {
    return unitOf(self == [NSUnitTemperature class] ? self : [NSUnitTemperature class], @"celsius", @"°C", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0 constant:273.15]; });
}

+ (NSUnitTemperature *)fahrenheit {
    return unitOf(self == [NSUnitTemperature class] ? self : [NSUnitTemperature class], @"fahrenheit", @"°F", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.5555555555555556 constant:255.3722222222222]; });
}

@end

@implementation NSUnitVolume

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)baseUnit {
    return (id)[NSUnitVolume liters];
}

+ (NSUnitVolume *)megaliters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"megaliters", @"ML", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000.0]; });
}

+ (NSUnitVolume *)kiloliters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"kiloliters", @"kL", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitVolume *)liters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"liters", @"L", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitVolume *)deciliters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"deciliters", @"dL", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.1]; });
}

+ (NSUnitVolume *)centiliters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"centiliters", @"cL", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.01]; });
}

+ (NSUnitVolume *)milliliters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"milliliters", @"mL", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitVolume *)cubicKilometers {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cubicKilometers", @"km³", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000000000000.0]; });
}

+ (NSUnitVolume *)cubicMeters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cubicMeters", @"m³", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1000.0]; });
}

+ (NSUnitVolume *)cubicDecimeters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cubicDecimeters", @"dm³", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.0]; });
}

+ (NSUnitVolume *)cubicCentimeters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cubicCentimeters", @"cm³", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.001]; });
}

+ (NSUnitVolume *)cubicMillimeters {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cubicMillimeters", @"mm³", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1e-06]; });
}

+ (NSUnitVolume *)cubicInches {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cubicInches", @"in³", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.016387064]; });
}

+ (NSUnitVolume *)cubicFeet {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cubicFeet", @"ft³", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:28.316846592]; });
}

+ (NSUnitVolume *)cubicYards {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cubicYards", @"yd³", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:764.554857984]; });
}

+ (NSUnitVolume *)cubicMiles {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cubicMiles", @"mi³", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:4168181825440.58]; });
}

+ (NSUnitVolume *)acreFeet {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"acreFeet", @"af", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1233481.83754752]; });
}

+ (NSUnitVolume *)bushels {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"bushels", @"bsh", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:35.23907016688]; });
}

+ (NSUnitVolume *)teaspoons {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"teaspoons", @"tsp", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.00492892159375]; });
}

+ (NSUnitVolume *)tablespoons {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"tablespoons", @"tbsp", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.01478676478125]; });
}

+ (NSUnitVolume *)fluidOunces {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"fluidOunces", @"fl oz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0295735295625]; });
}

+ (NSUnitVolume *)cups {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"cups", @"cup", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.24]; });
}

+ (NSUnitVolume *)pints {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"pints", @"pt", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.473176473]; });
}

+ (NSUnitVolume *)quarts {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"quarts", @"qt", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.946352946]; });
}

+ (NSUnitVolume *)gallons {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"gallons", @"gal", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:3.785411784]; });
}

+ (NSUnitVolume *)imperialTeaspoons {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"imperialTeaspoons", @"tsp", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.00591938802083]; });
}

+ (NSUnitVolume *)imperialTablespoons {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"imperialTablespoons", @"tbsp", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0177581640625]; });
}

+ (NSUnitVolume *)imperialFluidOunces {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"imperialFluidOunces", @"fl oz", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.0284130625]; });
}

+ (NSUnitVolume *)imperialPints {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"imperialPints", @"pt", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.56826125]; });
}

+ (NSUnitVolume *)imperialQuarts {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"imperialQuarts", @"qt", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:1.1365225]; });
}

+ (NSUnitVolume *)imperialGallons {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"imperialGallons", @"gal", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:4.54609]; });
}

+ (NSUnitVolume *)metricCups {
    return unitOf(self == [NSUnitVolume class] ? self : [NSUnitVolume class], @"metricCups", @"metric cup", ^NSUnitConverter *{ return [[NSUnitConverterLinear alloc] initWithCoefficient:0.25]; });
}

@end
