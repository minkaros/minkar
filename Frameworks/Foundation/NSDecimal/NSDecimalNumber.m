/* Copyright (c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSDecimalNumber.h>
#import <Foundation/NSString.h>
#import <Foundation/NSRaise.h>
#import <Foundation/NSException.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSData.h>
#import <Foundation/NSCharacterSet.h>

FOUNDATION_EXPORT BOOL _NSDecimalFromString(NSDecimal *d, NSString *string, NSString *separator, NSUInteger *consumed);
FOUNDATION_EXPORT void _NSDecimalFromUnsignedLongLong(NSDecimal *d, unsigned long long value, BOOL negative);
FOUNDATION_EXPORT void _NSDecimalFromLongLong(NSDecimal *d, long long value);
FOUNDATION_EXPORT void _NSDecimalFromDouble(NSDecimal *d, double value);
FOUNDATION_EXPORT double _NSDecimalToDouble(const NSDecimal *d);

NSString * const NSDecimalNumberExactnessException = @"NSDecimalNumberExactnessException";
NSString * const NSDecimalNumberOverflowException = @"NSDecimalNumberOverflowException";
NSString * const NSDecimalNumberUnderflowException = @"NSDecimalNumberUnderflowException";
NSString * const NSDecimalNumberDivideByZeroException = @"NSDecimalNumberDivideByZeroException";

static NSString *separatorOf(id locale) {
    if ([locale isKindOfClass:[NSLocale class]])
        return [locale decimalSeparator] ?: @".";
    if ([locale isKindOfClass:[NSDictionary class]])
        return [locale objectForKey:NSLocaleDecimalSeparator] ?: @".";
    return @".";
}

@implementation NSDecimalNumber

static id<NSDecimalNumberBehaviors> defaultBehavior = nil;

+ allocWithZone:(NSZone *)zone {
    return NSAllocateObject(self, 0, zone);
}

- (instancetype)initWithDecimal:(NSDecimal)dcm {
    _decimal = dcm;
    return self;
}

- (instancetype)initWithMantissa:(unsigned long long)mantissa exponent:(short)exponent isNegative:(BOOL)flag {
    NSDecimal d;
    _NSDecimalFromUnsignedLongLong(&d, mantissa, flag);
    NSDecimalMultiplyByPowerOf10(&d, &d, exponent, NSRoundPlain);
    return [self initWithDecimal:d];
}

- (instancetype)initWithString:(NSString *)numberValue {
    return [self initWithString:numberValue locale:nil];
}

/* Not a number when the text is not one, as on the reference platform. */
- (instancetype)initWithString:(NSString *)numberValue locale:(id)locale {
    NSDecimal d;
    NSString *trimmed = [numberValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
    if (trimmed == nil || !_NSDecimalFromString(&d, trimmed, separatorOf(locale), NULL)) {
        memset(&d, 0, sizeof(d));
        d._isNegative = 1;
    }
    return [self initWithDecimal:d];
}

- (instancetype)initWithBool:(BOOL)value { NSDecimal d; _NSDecimalFromLongLong(&d, value ? 1 : 0); return [self initWithDecimal:d]; }
- (instancetype)initWithChar:(char)value { NSDecimal d; _NSDecimalFromLongLong(&d, value); return [self initWithDecimal:d]; }
- (instancetype)initWithShort:(short)value { NSDecimal d; _NSDecimalFromLongLong(&d, value); return [self initWithDecimal:d]; }
- (instancetype)initWithInt:(int)value { NSDecimal d; _NSDecimalFromLongLong(&d, value); return [self initWithDecimal:d]; }
- (instancetype)initWithLong:(long)value { NSDecimal d; _NSDecimalFromLongLong(&d, value); return [self initWithDecimal:d]; }
- (instancetype)initWithLongLong:(long long)value { NSDecimal d; _NSDecimalFromLongLong(&d, value); return [self initWithDecimal:d]; }
- (instancetype)initWithInteger:(NSInteger)value { NSDecimal d; _NSDecimalFromLongLong(&d, value); return [self initWithDecimal:d]; }
- (instancetype)initWithUnsignedChar:(unsigned char)value { NSDecimal d; _NSDecimalFromUnsignedLongLong(&d, value, NO); return [self initWithDecimal:d]; }
- (instancetype)initWithUnsignedShort:(unsigned short)value { NSDecimal d; _NSDecimalFromUnsignedLongLong(&d, value, NO); return [self initWithDecimal:d]; }
- (instancetype)initWithUnsignedInt:(unsigned int)value { NSDecimal d; _NSDecimalFromUnsignedLongLong(&d, value, NO); return [self initWithDecimal:d]; }
- (instancetype)initWithUnsignedLong:(unsigned long)value { NSDecimal d; _NSDecimalFromUnsignedLongLong(&d, value, NO); return [self initWithDecimal:d]; }
- (instancetype)initWithUnsignedLongLong:(unsigned long long)value { NSDecimal d; _NSDecimalFromUnsignedLongLong(&d, value, NO); return [self initWithDecimal:d]; }
- (instancetype)initWithUnsignedInteger:(NSUInteger)value { NSDecimal d; _NSDecimalFromUnsignedLongLong(&d, value, NO); return [self initWithDecimal:d]; }
- (instancetype)initWithFloat:(float)value { NSDecimal d; _NSDecimalFromDouble(&d, value); return [self initWithDecimal:d]; }
- (instancetype)initWithDouble:(double)value { NSDecimal d; _NSDecimalFromDouble(&d, value); return [self initWithDecimal:d]; }

+ (NSDecimalNumber *)decimalNumberWithDecimal:(NSDecimal)dcm {
    return [[[self alloc] initWithDecimal:dcm] autorelease];
}

+ (NSDecimalNumber *)decimalNumberWithMantissa:(unsigned long long)mantissa exponent:(short)exponent isNegative:(BOOL)flag {
    return [[[self alloc] initWithMantissa:mantissa exponent:exponent isNegative:flag] autorelease];
}

+ (NSDecimalNumber *)decimalNumberWithString:(NSString *)numberValue {
    return [[[self alloc] initWithString:numberValue] autorelease];
}

+ (NSDecimalNumber *)decimalNumberWithString:(NSString *)numberValue locale:(id)locale {
    return [[[self alloc] initWithString:numberValue locale:locale] autorelease];
}

+ (NSDecimalNumber *)zero {
    return [self decimalNumberWithMantissa:0 exponent:0 isNegative:NO];
}

+ (NSDecimalNumber *)one {
    return [self decimalNumberWithMantissa:1 exponent:0 isNegative:NO];
}

/* The mantissa's largest value (2^128 - 1) at the largest exponent. */
+ (NSDecimalNumber *)maximumDecimalNumber {
    NSDecimal d;
    memset(&d, 0, sizeof(d));
    for (int i = 0; i < NSDecimalMaxSize; i++)
        d._mantissa[i] = 0xffff;
    d._length = NSDecimalMaxSize;
    d._exponent = 127;
    return [self decimalNumberWithDecimal:d];
}

+ (NSDecimalNumber *)minimumDecimalNumber {
    NSDecimal d = [[self maximumDecimalNumber] decimalValue];
    d._isNegative = 1;
    return [self decimalNumberWithDecimal:d];
}

+ (NSDecimalNumber *)notANumber {
    NSDecimal d;
    memset(&d, 0, sizeof(d));
    d._isNegative = 1;
    return [self decimalNumberWithDecimal:d];
}

+ (id<NSDecimalNumberBehaviors>)defaultBehavior {
    return defaultBehavior ?: [NSDecimalNumberHandler defaultDecimalNumberHandler];
}

+ (void)setDefaultBehavior:(id<NSDecimalNumberBehaviors>)behavior {
    [(id)behavior retain];
    [(id)defaultBehavior release];
    defaultBehavior = behavior;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

/* -- NSCoding: the reference platform's keys -- */

- (void)encodeWithCoder:(NSCoder *)coder {
    if ([coder allowsKeyedCoding]) {
        [coder encodeInt:_decimal._exponent forKey:@"NS.exponent"];
        [coder encodeInt:_decimal._length forKey:@"NS.length"];
        [coder encodeBool:_decimal._isNegative forKey:@"NS.negative"];
        [coder encodeBool:_decimal._isCompact forKey:@"NS.compact"];
        [coder encodeInt:1 forKey:@"NS.mantissa.bo"];
        [coder encodeBytes:(const uint8_t *)_decimal._mantissa length:sizeof(_decimal._mantissa) forKey:@"NS.mantissa"];
    } else
        [coder encodeValueOfObjCType:@encode(NSDecimal) at:&_decimal];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSDecimal d;
    memset(&d, 0, sizeof(d));
    if ([coder allowsKeyedCoding]) {
        NSUInteger length = 0;
        d._exponent = [coder decodeIntForKey:@"NS.exponent"];
        d._length = [coder decodeIntForKey:@"NS.length"];
        d._isNegative = [coder decodeBoolForKey:@"NS.negative"];
        d._isCompact = [coder decodeBoolForKey:@"NS.compact"];
        const uint8_t *bytes = [coder decodeBytesForKey:@"NS.mantissa" returnedLength:&length];
        if (bytes != NULL)
            memcpy(d._mantissa, bytes, MIN(length, sizeof(d._mantissa)));
    } else
        [coder decodeValueOfObjCType:@encode(NSDecimal) at:&d];
    return [self initWithDecimal:d];
}

/* -- Values -- */

- (NSDecimal)decimalValue {
    return _decimal;
}

- (const char *)objCType {
    return "d";
}

- (double)doubleValue {
    return _NSDecimalToDouble(&_decimal);
}

- (float)floatValue {
    return (float)[self doubleValue];
}

/* Integers: the integral part, from the digits (doubles lose them). */
- (long long)longLongValue {
    NSDecimal integral;
    NSDecimalRound(&integral, &_decimal, 0, _decimal._isNegative ? NSRoundUp : NSRoundDown);
    unsigned long long m = 0;
    for (int i = (int)integral._length - 1; i >= 0; i--)
        m = (m << 16) | integral._mantissa[i];
    for (int e = integral._exponent; e > 0; e--)
        m *= 10;
    return integral._isNegative ? -(long long)m : (long long)m;
}

- (unsigned long long)unsignedLongLongValue {
    return (unsigned long long)[self longLongValue];
}

- (BOOL)boolValue { return [self longLongValue] != 0 || [self doubleValue] != 0; }
- (char)charValue { return (char)[self longLongValue]; }
- (short)shortValue { return (short)[self longLongValue]; }
- (int)intValue { return (int)[self longLongValue]; }
- (long)longValue { return (long)[self longLongValue]; }
- (NSInteger)integerValue { return (NSInteger)[self longLongValue]; }
- (unsigned char)unsignedCharValue { return (unsigned char)[self longLongValue]; }
- (unsigned short)unsignedShortValue { return (unsigned short)[self longLongValue]; }
- (unsigned int)unsignedIntValue { return (unsigned int)[self longLongValue]; }
- (unsigned long)unsignedLongValue { return (unsigned long)[self unsignedLongLongValue]; }
- (NSUInteger)unsignedIntegerValue { return (NSUInteger)[self unsignedLongLongValue]; }

- (NSString *)descriptionWithLocale:(id)locale {
    return NSDecimalString(&_decimal, locale);
}

- (NSString *)stringValue {
    return [self descriptionWithLocale:nil];
}

- (NSString *)description {
    return [self descriptionWithLocale:nil];
}

- (NSUInteger)hash {
    return (NSUInteger)[self doubleValue];
}

- (BOOL)isEqual:(id)other {
    if (![other isKindOfClass:[NSNumber class]])
        return NO;
    return [self compare:other] == NSOrderedSame;
}

- (NSComparisonResult)compare:(NSNumber *)decimalNumber {
    NSDecimal other = [decimalNumber decimalValue];
    return NSDecimalCompare(&_decimal, &other);
}

/* -- Arithmetic, with a behavior's rounding, scale and errors -- */

- (NSDecimalNumber *)_result:(NSDecimal)result error:(NSCalculationError)error operation:(SEL)operation
    other:(NSDecimalNumber *)other behavior:(id<NSDecimalNumberBehaviors>)behavior {
    if (behavior == nil)
        behavior = [NSDecimalNumber defaultBehavior];
    if (error != NSCalculationNoError) {
        NSDecimalNumber *replacement = [behavior exceptionDuringOperation:operation error:error leftOperand:self rightOperand:other];
        if (replacement != nil)
            return replacement;
    }
    short scale = [behavior scale];
    if (scale != NSDecimalNoScale && !NSDecimalIsNotANumber(&result))
        NSDecimalRound(&result, &result, scale, [behavior roundingMode]);
    return [NSDecimalNumber decimalNumberWithDecimal:result];
}

#define BINARY(name, function) \
- (NSDecimalNumber *)name:(NSDecimalNumber *)decimalNumber { \
    return [self name:decimalNumber withBehavior:nil]; \
} \
- (NSDecimalNumber *)name:(NSDecimalNumber *)decimalNumber withBehavior:(id<NSDecimalNumberBehaviors>)behavior { \
    NSDecimal result, other = [decimalNumber decimalValue]; \
    NSRoundingMode mode = [(behavior ?: [NSDecimalNumber defaultBehavior]) roundingMode]; \
    NSCalculationError error = function(&result, &_decimal, &other, mode); \
    return [self _result:result error:error operation:_cmd other:decimalNumber behavior:behavior]; \
}
BINARY(decimalNumberByAdding, NSDecimalAdd)
BINARY(decimalNumberBySubtracting, NSDecimalSubtract)
BINARY(decimalNumberByMultiplyingBy, NSDecimalMultiply)
BINARY(decimalNumberByDividingBy, NSDecimalDivide)
#undef BINARY

- (NSDecimalNumber *)decimalNumberByRaisingToPower:(NSUInteger)power {
    return [self decimalNumberByRaisingToPower:power withBehavior:nil];
}

- (NSDecimalNumber *)decimalNumberByRaisingToPower:(NSUInteger)power withBehavior:(id<NSDecimalNumberBehaviors>)behavior {
    NSDecimal result;
    NSCalculationError error = NSDecimalPower(&result, &_decimal, power, [(behavior ?: [NSDecimalNumber defaultBehavior]) roundingMode]);
    return [self _result:result error:error operation:_cmd other:nil behavior:behavior];
}

- (NSDecimalNumber *)decimalNumberByMultiplyingByPowerOf10:(short)power {
    return [self decimalNumberByMultiplyingByPowerOf10:power withBehavior:nil];
}

- (NSDecimalNumber *)decimalNumberByMultiplyingByPowerOf10:(short)power withBehavior:(id<NSDecimalNumberBehaviors>)behavior {
    NSDecimal result;
    NSCalculationError error = NSDecimalMultiplyByPowerOf10(&result, &_decimal, power, [(behavior ?: [NSDecimalNumber defaultBehavior]) roundingMode]);
    return [self _result:result error:error operation:_cmd other:nil behavior:behavior];
}

- (NSDecimalNumber *)decimalNumberByRoundingAccordingToBehavior:(id<NSDecimalNumberBehaviors>)behavior {
    NSDecimal result;
    if (behavior == nil)
        behavior = [NSDecimalNumber defaultBehavior];
    NSDecimalRound(&result, &_decimal, [behavior scale], [behavior roundingMode]);
    return [NSDecimalNumber decimalNumberWithDecimal:result];
}

@end

@implementation NSDecimalNumberHandler

+ (NSDecimalNumberHandler *)defaultDecimalNumberHandler {
    static NSDecimalNumberHandler *handler = nil;
    @synchronized(self) {
        if (handler == nil)
            handler = [[self alloc] initWithRoundingMode:NSRoundPlain scale:NSDecimalNoScale raiseOnExactness:NO
                raiseOnOverflow:YES raiseOnUnderflow:YES raiseOnDivideByZero:YES];
    }
    return handler;
}

+ (instancetype)decimalNumberHandlerWithRoundingMode:(NSRoundingMode)roundingMode scale:(short)scale raiseOnExactness:(BOOL)exact raiseOnOverflow:(BOOL)overflow raiseOnUnderflow:(BOOL)underflow raiseOnDivideByZero:(BOOL)divideByZero {
    return [[[self alloc] initWithRoundingMode:roundingMode scale:scale raiseOnExactness:exact raiseOnOverflow:overflow
        raiseOnUnderflow:underflow raiseOnDivideByZero:divideByZero] autorelease];
}

- (instancetype)init {
    return [self initWithRoundingMode:NSRoundPlain scale:NSDecimalNoScale raiseOnExactness:NO raiseOnOverflow:YES
        raiseOnUnderflow:YES raiseOnDivideByZero:YES];
}

- (instancetype)initWithRoundingMode:(NSRoundingMode)roundingMode scale:(short)scale raiseOnExactness:(BOOL)exact raiseOnOverflow:(BOOL)overflow raiseOnUnderflow:(BOOL)underflow raiseOnDivideByZero:(BOOL)divideByZero {
    if ((self = [super init]) == nil)
        return nil;
    _roundingMode = roundingMode;
    _scale = scale;
    _raiseOnExactness = exact;
    _raiseOnOverflow = overflow;
    _raiseOnUnderflow = underflow;
    _raiseOnDivideByZero = divideByZero;
    return self;
}

- (NSRoundingMode)roundingMode {
    return _roundingMode;
}

- (short)scale {
    return _scale;
}

/* Raise, or give not a number (or nil, for a loss of precision: the
 * rounded result stands). */
- (NSDecimalNumber *)exceptionDuringOperation:(SEL)operation error:(NSCalculationError)error leftOperand:(NSDecimalNumber *)leftOperand rightOperand:(NSDecimalNumber *)rightOperand {
    NSString *name = nil;
    BOOL raise = NO;
    switch (error) {
    case NSCalculationLossOfPrecision: name = NSDecimalNumberExactnessException; raise = _raiseOnExactness; break;
    case NSCalculationOverflow: name = NSDecimalNumberOverflowException; raise = _raiseOnOverflow; break;
    case NSCalculationUnderflow: name = NSDecimalNumberUnderflowException; raise = _raiseOnUnderflow; break;
    case NSCalculationDivideByZero: name = NSDecimalNumberDivideByZeroException; raise = _raiseOnDivideByZero; break;
    default: return nil;
    }
    if (raise)
        [NSException raise:name format:@"NSDecimalNumber %@: %@", NSStringFromSelector(operation), name];
    if (error == NSCalculationLossOfPrecision)
        return nil;
    if (error == NSCalculationUnderflow)
        return [NSDecimalNumber zero];
    return [NSDecimalNumber notANumber];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeInt:_roundingMode forKey:@"NS.roundingMode"];
    [coder encodeInt:_scale forKey:@"NS.scale"];
    [coder encodeBool:_raiseOnExactness forKey:@"NS.raise.exactness"];
    [coder encodeBool:_raiseOnOverflow forKey:@"NS.raise.overflow"];
    [coder encodeBool:_raiseOnUnderflow forKey:@"NS.raise.underflow"];
    [coder encodeBool:_raiseOnDivideByZero forKey:@"NS.raise.dividebyzero"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [self initWithRoundingMode:[coder decodeIntForKey:@"NS.roundingMode"]
        scale:(short)[coder decodeIntForKey:@"NS.scale"]
        raiseOnExactness:[coder decodeBoolForKey:@"NS.raise.exactness"]
        raiseOnOverflow:[coder decodeBoolForKey:@"NS.raise.overflow"]
        raiseOnUnderflow:[coder decodeBoolForKey:@"NS.raise.underflow"]
        raiseOnDivideByZero:[coder decodeBoolForKey:@"NS.raise.dividebyzero"]];
}

@end
