/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A measurement: a value in a unit (API 19).
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSUnit.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSMeasurement<UnitType : NSUnit *> : NSObject <NSCopying, NSSecureCoding> {
    UnitType _unit;
    double _doubleValue;
}

@property (readonly, copy) UnitType unit;
@property (readonly) double doubleValue;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDoubleValue:(double)doubleValue unit:(UnitType)unit NS_DESIGNATED_INITIALIZER;

- (BOOL)canBeConvertedToUnit:(NSUnit *)unit;
- (NSMeasurement *)measurementByConvertingToUnit:(NSUnit *)unit;
- (NSMeasurement<UnitType> *)measurementByAddingMeasurement:(NSMeasurement<UnitType> *)measurement;
- (NSMeasurement<UnitType> *)measurementBySubtractingMeasurement:(NSMeasurement<UnitType> *)measurement;

@end

NS_ASSUME_NONNULL_END
