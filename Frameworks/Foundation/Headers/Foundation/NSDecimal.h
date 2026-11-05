/* Copyright (c) 2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#include <limits.h>

@class NSDictionary, NSString;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, NSRoundingMode) {
    NSRoundPlain,
    NSRoundDown,
    NSRoundUp,
    NSRoundBankers
};

typedef NS_ENUM(NSUInteger, NSCalculationError) {
    NSCalculationNoError = 0,
    NSCalculationLossOfPrecision,
    NSCalculationUnderflow,
    NSCalculationOverflow,
    NSCalculationDivideByZero
};

#define NSDecimalMaxSize (8)
#define NSDecimalNoScale SHRT_MAX

/* A decimal number: mantissa x 10^exponent, the mantissa in up to eight
 * 16-bit words, least significant first. Not a number: length 0 and
 * negative. */
typedef struct {
    signed int _exponent:8;
    unsigned int _length:4;
    unsigned int _isNegative:1;
    unsigned int _isCompact:1;
    unsigned int _reserved:18;
    unsigned short _mantissa[NSDecimalMaxSize];
} NSDecimal;

NS_INLINE BOOL NSDecimalIsNotANumber(const NSDecimal *dcm) {
    return ((dcm->_length == 0) && dcm->_isNegative);
}

FOUNDATION_EXPORT void NSDecimalCopy(NSDecimal *destination, const NSDecimal *source);
FOUNDATION_EXPORT void NSDecimalCompact(NSDecimal *number);
FOUNDATION_EXPORT NSComparisonResult NSDecimalCompare(const NSDecimal *leftOperand, const NSDecimal *rightOperand);
FOUNDATION_EXPORT void NSDecimalRound(NSDecimal *result, const NSDecimal *number, NSInteger scale, NSRoundingMode roundingMode);
FOUNDATION_EXPORT NSCalculationError NSDecimalNormalize(NSDecimal *number1, NSDecimal *number2, NSRoundingMode roundingMode);
FOUNDATION_EXPORT NSCalculationError NSDecimalAdd(NSDecimal *result, const NSDecimal *leftOperand, const NSDecimal *rightOperand, NSRoundingMode roundingMode);
FOUNDATION_EXPORT NSCalculationError NSDecimalSubtract(NSDecimal *result, const NSDecimal *leftOperand, const NSDecimal *rightOperand, NSRoundingMode roundingMode);
FOUNDATION_EXPORT NSCalculationError NSDecimalMultiply(NSDecimal *result, const NSDecimal *leftOperand, const NSDecimal *rightOperand, NSRoundingMode roundingMode);
FOUNDATION_EXPORT NSCalculationError NSDecimalDivide(NSDecimal *result, const NSDecimal *leftOperand, const NSDecimal *rightOperand, NSRoundingMode roundingMode);
FOUNDATION_EXPORT NSCalculationError NSDecimalPower(NSDecimal *result, const NSDecimal *number, NSUInteger power, NSRoundingMode roundingMode);
FOUNDATION_EXPORT NSCalculationError NSDecimalMultiplyByPowerOf10(NSDecimal *result, const NSDecimal *number, short power, NSRoundingMode roundingMode);
FOUNDATION_EXPORT NSString *NSDecimalString(const NSDecimal *dcm, id _Nullable locale);

NS_ASSUME_NONNULL_END
