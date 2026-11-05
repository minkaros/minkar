/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSNumber.h>
#import <Foundation/NSDecimal.h>
#import <Foundation/NSException.h>

@class NSDictionary;

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSExceptionName const NSDecimalNumberExactnessException;
FOUNDATION_EXPORT NSExceptionName const NSDecimalNumberOverflowException;
FOUNDATION_EXPORT NSExceptionName const NSDecimalNumberUnderflowException;
FOUNDATION_EXPORT NSExceptionName const NSDecimalNumberDivideByZeroException;

@class NSDecimalNumber;

@protocol NSDecimalNumberBehaviors
- (NSRoundingMode)roundingMode;
- (short)scale;
- (nullable NSDecimalNumber *)exceptionDuringOperation:(SEL)operation error:(NSCalculationError)error leftOperand:(NSDecimalNumber *)leftOperand rightOperand:(nullable NSDecimalNumber *)rightOperand;
@end

@interface NSDecimalNumber : NSNumber {
    NSDecimal _decimal;
}

- (instancetype)initWithMantissa:(unsigned long long)mantissa exponent:(short)exponent isNegative:(BOOL)flag;
- (instancetype)initWithDecimal:(NSDecimal)dcm NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithString:(nullable NSString *)numberValue;
- (instancetype)initWithString:(nullable NSString *)numberValue locale:(nullable id)locale;

- (NSString *)descriptionWithLocale:(nullable id)locale;

@property (readonly) NSDecimal decimalValue;

+ (NSDecimalNumber *)decimalNumberWithMantissa:(unsigned long long)mantissa exponent:(short)exponent isNegative:(BOOL)flag;
+ (NSDecimalNumber *)decimalNumberWithDecimal:(NSDecimal)dcm;
+ (NSDecimalNumber *)decimalNumberWithString:(nullable NSString *)numberValue;
+ (NSDecimalNumber *)decimalNumberWithString:(nullable NSString *)numberValue locale:(nullable id)locale;

@property (class, readonly, copy) NSDecimalNumber *zero;
@property (class, readonly, copy) NSDecimalNumber *one;
@property (class, readonly, copy) NSDecimalNumber *minimumDecimalNumber;
@property (class, readonly, copy) NSDecimalNumber *maximumDecimalNumber;
@property (class, readonly, copy) NSDecimalNumber *notANumber;

- (NSDecimalNumber *)decimalNumberByAdding:(NSDecimalNumber *)decimalNumber;
- (NSDecimalNumber *)decimalNumberByAdding:(NSDecimalNumber *)decimalNumber withBehavior:(nullable id <NSDecimalNumberBehaviors>)behavior;
- (NSDecimalNumber *)decimalNumberBySubtracting:(NSDecimalNumber *)decimalNumber;
- (NSDecimalNumber *)decimalNumberBySubtracting:(NSDecimalNumber *)decimalNumber withBehavior:(nullable id <NSDecimalNumberBehaviors>)behavior;
- (NSDecimalNumber *)decimalNumberByMultiplyingBy:(NSDecimalNumber *)decimalNumber;
- (NSDecimalNumber *)decimalNumberByMultiplyingBy:(NSDecimalNumber *)decimalNumber withBehavior:(nullable id <NSDecimalNumberBehaviors>)behavior;
- (NSDecimalNumber *)decimalNumberByDividingBy:(NSDecimalNumber *)decimalNumber;
- (NSDecimalNumber *)decimalNumberByDividingBy:(NSDecimalNumber *)decimalNumber withBehavior:(nullable id <NSDecimalNumberBehaviors>)behavior;
- (NSDecimalNumber *)decimalNumberByRaisingToPower:(NSUInteger)power;
- (NSDecimalNumber *)decimalNumberByRaisingToPower:(NSUInteger)power withBehavior:(nullable id <NSDecimalNumberBehaviors>)behavior;
- (NSDecimalNumber *)decimalNumberByMultiplyingByPowerOf10:(short)power;
- (NSDecimalNumber *)decimalNumberByMultiplyingByPowerOf10:(short)power withBehavior:(nullable id <NSDecimalNumberBehaviors>)behavior;
- (NSDecimalNumber *)decimalNumberByRoundingAccordingToBehavior:(nullable id <NSDecimalNumberBehaviors>)behavior;

- (NSComparisonResult)compare:(NSNumber *)decimalNumber;

@property (class, strong) id <NSDecimalNumberBehaviors> defaultBehavior;

@property (readonly) const char *objCType NS_RETURNS_INNER_POINTER;
@property (readonly) double doubleValue;

@end

@interface NSDecimalNumberHandler : NSObject <NSDecimalNumberBehaviors, NSCoding> {
    NSRoundingMode _roundingMode;
    short _scale;
    BOOL _raiseOnExactness, _raiseOnOverflow, _raiseOnUnderflow, _raiseOnDivideByZero;
}

@property (class, readonly, strong) NSDecimalNumberHandler *defaultDecimalNumberHandler;

- (instancetype)initWithRoundingMode:(NSRoundingMode)roundingMode scale:(short)scale raiseOnExactness:(BOOL)exact raiseOnOverflow:(BOOL)overflow raiseOnUnderflow:(BOOL)underflow raiseOnDivideByZero:(BOOL)divideByZero NS_DESIGNATED_INITIALIZER;
+ (instancetype)decimalNumberHandlerWithRoundingMode:(NSRoundingMode)roundingMode scale:(short)scale raiseOnExactness:(BOOL)exact raiseOnOverflow:(BOOL)overflow raiseOnUnderflow:(BOOL)underflow raiseOnDivideByZero:(BOOL)divideByZero;

@end

NS_ASSUME_NONNULL_END
