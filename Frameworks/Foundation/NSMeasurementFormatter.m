/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Measurements as text (API 19): the value with the locale's number format
 * and the unit's symbol. Without the provided-unit option the value is
 * shown in the locale's system (metric or not) for length, mass, volume,
 * speed and temperature. ARC.
 */

#import <Foundation/NSMeasurementFormatter.h>
#import <Foundation/NSNumberFormatter.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSString.h>
#import <Foundation/NSCoder.h>

@implementation NSMeasurementFormatter

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)init {
    if ((self = [super init]) == nil)
        return nil;
    _unitStyle = NSFormattingUnitStyleMedium;
    return self;
}

- (NSMeasurementFormatterUnitOptions)unitOptions { return _unitOptions; }
- (void)setUnitOptions:(NSMeasurementFormatterUnitOptions)options { _unitOptions = options; }
- (NSFormattingUnitStyle)unitStyle { return _unitStyle; }
- (void)setUnitStyle:(NSFormattingUnitStyle)style { _unitStyle = style; }

- (NSLocale *)locale {
    return _locale ?: [NSLocale currentLocale];
}

- (void)setLocale:(NSLocale *)locale {
    _locale = [locale copy];
}

- (NSNumberFormatter *)numberFormatter {
    if (_numberFormatter == nil) {
        _numberFormatter = [[NSNumberFormatter alloc] init];
        [_numberFormatter setNumberStyle:NSNumberFormatterDecimalStyle];
        [_numberFormatter setMaximumFractionDigits:3];
        [_numberFormatter setLocale:[self locale]];
    }
    return _numberFormatter;
}

- (void)setNumberFormatter:(NSNumberFormatter *)numberFormatter {
    _numberFormatter = [numberFormatter copy];
}

/* The unit the locale shows for this one's dimension, unless asked to
 * keep it. */
- (NSUnit *)_unitFor:(NSUnit *)unit {
    if (_unitOptions & NSMeasurementFormatterUnitOptionsProvidedUnit)
        return unit;
    BOOL metric = [[self locale] usesMetricSystem];
    if ([unit isKindOfClass:[NSUnitTemperature class]])
        return metric ? [NSUnitTemperature celsius] : [NSUnitTemperature fahrenheit];
    if ([unit isKindOfClass:[NSUnitLength class]])
        return metric ? [NSUnitLength meters] : [NSUnitLength feet];
    if ([unit isKindOfClass:[NSUnitMass class]])
        return metric ? [NSUnitMass kilograms] : [NSUnitMass poundsMass];
    if ([unit isKindOfClass:[NSUnitVolume class]])
        return metric ? [NSUnitVolume liters] : [NSUnitVolume gallons];
    if ([unit isKindOfClass:[NSUnitSpeed class]])
        return metric ? [NSUnitSpeed kilometersPerHour] : [NSUnitSpeed milesPerHour];
    return unit;
}

- (NSString *)stringFromUnit:(NSUnit *)unit {
    return [unit symbol];
}

- (NSString *)stringFromMeasurement:(NSMeasurement *)measurement {
    NSUnit *unit = [self _unitFor:[measurement unit]];
    NSMeasurement *shown = [measurement canBeConvertedToUnit:unit] ? [measurement measurementByConvertingToUnit:unit] : measurement;
    NSString *value = [[self numberFormatter] stringFromNumber:@([shown doubleValue])];
    if ((_unitOptions & NSMeasurementFormatterUnitOptionsTemperatureWithoutUnit) && [unit isKindOfClass:[NSUnitTemperature class]])
        return [value stringByAppendingString:@"°"];
    NSString *symbol = [[shown unit] symbol];
    BOOL tight = [symbol hasPrefix:@"°"] || [symbol isEqualToString:@"%"];
    return tight ? [value stringByAppendingString:symbol] : [NSString stringWithFormat:@"%@ %@", value, symbol];
}

- (NSString *)stringForObjectValue:(id)obj {
    if ([obj isKindOfClass:[NSMeasurement class]])
        return [self stringFromMeasurement:obj];
    if ([obj isKindOfClass:[NSUnit class]])
        return [self stringFromUnit:obj];
    return nil;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeInteger:(NSInteger)_unitOptions forKey:@"NS.unitOptions"];
    [coder encodeInteger:_unitStyle forKey:@"NS.unitStyle"];
    [coder encodeObject:_locale forKey:@"NS.locale"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [self init]) == nil)
        return nil;
    _unitOptions = (NSMeasurementFormatterUnitOptions)[coder decodeIntegerForKey:@"NS.unitOptions"];
    _unitStyle = [coder decodeIntegerForKey:@"NS.unitStyle"] ?: NSFormattingUnitStyleMedium;
    _locale = [coder decodeObjectOfClass:[NSLocale class] forKey:@"NS.locale"];
    return self;
}

@end
