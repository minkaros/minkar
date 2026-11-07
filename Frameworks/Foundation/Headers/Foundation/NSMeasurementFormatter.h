/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Measurements as text (API 19).
 */

#import <Foundation/NSFormatter.h>
#import <Foundation/NSMeasurement.h>

@class NSLocale, NSNumberFormatter, NSString;

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, NSMeasurementFormatterUnitOptions) {
    NSMeasurementFormatterUnitOptionsProvidedUnit = (1UL << 0),
    NSMeasurementFormatterUnitOptionsNaturalScale = (1UL << 1),
    NSMeasurementFormatterUnitOptionsTemperatureWithoutUnit = (1UL << 2),
};

@interface NSMeasurementFormatter : NSFormatter <NSSecureCoding> {
    NSMeasurementFormatterUnitOptions _unitOptions;
    NSFormattingUnitStyle _unitStyle;
    NSLocale *_locale;
    NSNumberFormatter *_numberFormatter;
}

@property NSMeasurementFormatterUnitOptions unitOptions;
@property NSFormattingUnitStyle unitStyle;
@property (null_resettable, copy) NSLocale *locale;
@property (null_resettable, copy) NSNumberFormatter *numberFormatter;

- (NSString *)stringFromMeasurement:(NSMeasurement *)measurement;
- (NSString *)stringFromUnit:(NSUnit *)unit;

@end

NS_ASSUME_NONNULL_END
