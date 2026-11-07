/* Copyright (c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSDecimal.h>
#import <Foundation/NSString.h>
#import <Foundation/NSLocale.h>
#import <Foundation/NSDictionary.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/*
 * Decimal arithmetic: a value is mantissa x 10^exponent, the mantissa an
 * unsigned 128-bit integer (the eight 16-bit words), the exponent -128 to
 * 127. Results that do not fit are rounded, dropping low digits; wider
 * intermediates (products, scaled dividends) are 256-bit.
 */

/* Conversions for the rest of Foundation (NSDecimalNumber, NSNumber,
 * NSScanner). */
FOUNDATION_EXPORT void _NSDecimalFromUnsignedLongLong(NSDecimal *d, unsigned long long value, BOOL negative);
FOUNDATION_EXPORT void _NSDecimalFromLongLong(NSDecimal *d, long long value);
FOUNDATION_EXPORT BOOL _NSDecimalFromString(NSDecimal *d, NSString *string, NSString *separator, NSUInteger *consumed);
FOUNDATION_EXPORT void _NSDecimalFromDouble(NSDecimal *d, double value);
FOUNDATION_EXPORT double _NSDecimalToDouble(const NSDecimal *d);

typedef unsigned __int128 u128;
typedef struct { u128 hi, lo; } u256;

#define EXP_MIN (-128)
#define EXP_MAX 127

static u128 mantissaOf(const NSDecimal *d) {
    u128 m = 0;
    for (int i = (int)d->_length - 1; i >= 0; i--)
        m = (m << 16) | d->_mantissa[i];
    return m;
}

/* Sets the value; the length is the words in use, zero has length 0 and
 * is not negative. */
static void setDecimal(NSDecimal *d, u128 m, int exponent, BOOL negative) {
    unsigned length = 0;
    for (int i = 0; i < NSDecimalMaxSize; i++) {
        d->_mantissa[i] = (unsigned short)(m & 0xffff);
        m >>= 16;
        if (d->_mantissa[i] != 0)
            length = (unsigned)i + 1;
    }
    d->_length = length;
    d->_exponent = length == 0 ? 0 : exponent;
    d->_isNegative = length == 0 ? 0 : (negative ? 1 : 0);
    d->_isCompact = 0;
    d->_reserved = 0;
}

static void setNaN(NSDecimal *d) {
    memset(d, 0, sizeof(*d));
    d->_isNegative = 1;
}

/* -- 256-bit helpers -- */

static u256 u256From(u128 v) {
    u256 r = { 0, v };
    return r;
}

static BOOL u256IsZero(u256 a) {
    return a.hi == 0 && a.lo == 0;
}

static int u256Compare(u256 a, u256 b) {
    if (a.hi != b.hi) return a.hi < b.hi ? -1 : 1;
    if (a.lo != b.lo) return a.lo < b.lo ? -1 : 1;
    return 0;
}

static u256 u256Add(u256 a, u256 b) {
    u256 r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo ? 1 : 0);
    return r;
}

static u256 u256Sub(u256 a, u256 b) {
    u256 r;
    r.lo = a.lo - b.lo;
    r.hi = a.hi - b.hi - (a.lo < b.lo ? 1 : 0);
    return r;
}

static u256 u256Shl1(u256 a) {
    u256 r = { (a.hi << 1) | (a.lo >> 127), a.lo << 1 };
    return r;
}

/* a x small, with the overflow out of 256 bits reported. */
static u256 u256MulSmall(u256 a, unsigned small, BOOL *overflow) {
    const u128 mask64 = (((u128)1) << 64) - 1;
    u128 limbs[4] = { a.lo & mask64, a.lo >> 64, a.hi & mask64, a.hi >> 64 };
    u128 carry = 0;
    for (int i = 0; i < 4; i++) {
        u128 v = limbs[i] * small + carry;
        limbs[i] = v & mask64;
        carry = v >> 64;
    }
    if (overflow != NULL)
        *overflow = carry != 0;
    u256 r = { limbs[2] | (limbs[3] << 64), limbs[0] | (limbs[1] << 64) };
    return r;
}

/* a / small, the remainder returned. */
static u256 u256DivSmall(u256 a, unsigned small, unsigned *remainder) {
    const u128 mask64 = (((u128)1) << 64) - 1;
    u128 limbs[4] = { a.hi >> 64, a.hi & mask64, a.lo >> 64, a.lo & mask64 };
    u128 rem = 0;
    for (int i = 0; i < 4; i++) {
        u128 v = (rem << 64) | limbs[i];
        limbs[i] = v / small;
        rem = v % small;
    }
    if (remainder != NULL)
        *remainder = (unsigned)rem;
    u256 r = { (limbs[0] << 64) | limbs[1], (limbs[2] << 64) | limbs[3] };
    return r;
}

static u256 u256Mul128(u128 a, u128 b) {
    const u128 mask64 = (((u128)1) << 64) - 1;
    u128 a0 = a & mask64, a1 = a >> 64, b0 = b & mask64, b1 = b >> 64;
    u128 p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    u128 middle = (p00 >> 64) + (p01 & mask64) + (p10 & mask64);
    u256 r;
    r.lo = (p00 & mask64) | (middle << 64);
    r.hi = p11 + (p01 >> 64) + (p10 >> 64) + (middle >> 64);
    return r;
}

/* a / b (b fits 128 bits), bit by bit; the remainder returned. */
static u256 u256Div(u256 a, u128 b, u128 *remainder) {
    u256 q = { 0, 0 }, r = { 0, 0 }, divisor = u256From(b);
    for (int i = 255; i >= 0; i--) {
        r = u256Shl1(r);
        u128 bit = i >= 128 ? (a.hi >> (i - 128)) & 1 : (a.lo >> i) & 1;
        r.lo |= bit;
        q = u256Shl1(q);
        if (u256Compare(r, divisor) >= 0) {
            r = u256Sub(r, divisor);
            q.lo |= 1;
        }
    }
    if (remainder != NULL)
        *remainder = r.lo;
    return q;
}

/* -- Rounding -- */

/* Whether dropping digits rounds the magnitude up: the first digit dropped,
 * whether any digit after it is not zero, the last digit kept. */
static BOOL roundsUp(NSRoundingMode mode, BOOL negative, unsigned firstDropped, BOOL restNonZero, unsigned lastKept) {
    BOOL anything = firstDropped != 0 || restNonZero;
    switch (mode) {
    case NSRoundDown: return negative && anything;
    case NSRoundUp: return !negative && anything;
    case NSRoundBankers:
        if (firstDropped > 5 || (firstDropped == 5 && restNonZero)) return YES;
        if (firstDropped == 5) return (lastKept & 1) != 0;
        return NO;
    case NSRoundPlain:
    default:
        return firstDropped >= 5;
    }
}

/* A wide value made to fit 128 bits (and the exponent's range), dropping
 * low digits with rounding. */
static NSCalculationError fit(NSDecimal *result, u256 m, int exponent, BOOL negative, NSRoundingMode mode, BOOL inexact) {
    unsigned first = 0;
    BOOL rest = NO, dropped = NO;
    NSCalculationError error = NSCalculationNoError;

    /* At most 38 digits, as on the reference platform (10^38 fits). */
    const u128 limit = (u128)10000000000000000000ULL * (u128)10000000000000000000ULL;
    while (m.hi != 0 || m.lo >= limit || exponent < EXP_MIN) {
        unsigned digit;
        if (dropped)
            rest = rest || first != 0;
        m = u256DivSmall(m, 10, &digit);
        first = digit;
        dropped = YES;
        exponent++;
    }
    if (dropped) {
        rest = rest || inexact;
        if (roundsUp(mode, negative, first, rest, (unsigned)(m.lo % 10))) {
            m = u256Add(m, u256From(1));
            if (m.hi != 0 || m.lo >= limit) {
                unsigned digit;
                m = u256DivSmall(m, 10, &digit);
                exponent++;
            }
        }
        if (first != 0 || rest)
            error = NSCalculationLossOfPrecision;
    } else if (inexact)
        error = NSCalculationLossOfPrecision;
    /* Too large an exponent: the mantissa takes what it can. */
    while (exponent > EXP_MAX && m.lo != 0) {
        BOOL overflow;
        u256 bigger = u256MulSmall(m, 10, &overflow);
        if (overflow || bigger.hi != 0) {
            setNaN(result);
            return NSCalculationOverflow;
        }
        m = bigger;
        exponent--;
    }
    if (m.lo == 0 && dropped && error == NSCalculationLossOfPrecision && exponent >= EXP_MIN) {
        setDecimal(result, 0, 0, NO);
        return NSCalculationUnderflow;
    }
    setDecimal(result, m.lo, exponent > EXP_MAX ? EXP_MAX : exponent, negative);
    return error;
}

/* -- The API -- */

void NSDecimalCopy(NSDecimal *destination, const NSDecimal *source) {
    *destination = *source;
}

/* The least words and the largest exponent: trailing zeros out. */
void NSDecimalCompact(NSDecimal *number) {
    if (NSDecimalIsNotANumber(number) || number->_isCompact)
        return;
    u128 m = mantissaOf(number);
    int exponent = number->_exponent;
    BOOL negative = number->_isNegative;
    if (m == 0) {
        setDecimal(number, 0, 0, NO);
        number->_isCompact = 1;
        return;
    }
    while (m % 10 == 0 && exponent < EXP_MAX) {
        m /= 10;
        exponent++;
    }
    setDecimal(number, m, exponent, negative);
    number->_isCompact = 1;
}

/* Both at the same exponent: the larger one's mantissa scaled up as far as
 * it fits, then the other's scaled down (losing digits). */
NSCalculationError NSDecimalNormalize(NSDecimal *number1, NSDecimal *number2, NSRoundingMode roundingMode) {
    if (NSDecimalIsNotANumber(number1) || NSDecimalIsNotANumber(number2))
        return NSCalculationNoError;
    NSDecimal *high = number1->_exponent >= number2->_exponent ? number1 : number2;
    NSDecimal *low = high == number1 ? number2 : number1;
    u128 hm = mantissaOf(high), lm = mantissaOf(low);
    int he = high->_exponent, le = low->_exponent;
    if (he == le)
        return NSCalculationNoError;
    if (hm == 0) {
        setDecimal(high, 0, le, NO);
        high->_exponent = le;
        return NSCalculationNoError;
    }
    while (he > le) {
        u128 bigger = hm * 10;
        if (bigger / 10 != hm)
            break;
        hm = bigger;
        he--;
    }
    setDecimal(high, hm, he, high->_isNegative);
    if (he == le)
        return NSCalculationNoError;
    /* The low one loses digits. */
    u256 m = u256From(lm);
    unsigned first = 0, digit;
    BOOL rest = NO;
    while (le < he) {
        rest = rest || first != 0;
        m = u256DivSmall(m, 10, &digit);
        first = digit;
        le++;
    }
    if (roundsUp(roundingMode, low->_isNegative, first, rest, (unsigned)(m.lo % 10)))
        m = u256Add(m, u256From(1));
    setDecimal(low, m.lo, le, low->_isNegative);
    if (m.lo == 0)
        low->_exponent = le;
    return NSCalculationLossOfPrecision;
}

NSComparisonResult NSDecimalCompare(const NSDecimal *leftOperand, const NSDecimal *rightOperand) {
    BOOL leftNaN = NSDecimalIsNotANumber(leftOperand), rightNaN = NSDecimalIsNotANumber(rightOperand);
    if (leftNaN || rightNaN)
        return leftNaN == rightNaN ? NSOrderedSame : (leftNaN ? NSOrderedAscending : NSOrderedDescending);
    NSDecimal a = *leftOperand, b = *rightOperand;
    u128 am = mantissaOf(&a), bm = mantissaOf(&b);
    BOOL an = am != 0 && a._isNegative, bn = bm != 0 && b._isNegative;
    if (an != bn)
        return an ? NSOrderedAscending : NSOrderedDescending;
    if (am == 0 && bm == 0)
        return NSOrderedSame;
    NSDecimalNormalize(&a, &b, NSRoundPlain);
    am = mantissaOf(&a);
    bm = mantissaOf(&b);
    NSComparisonResult magnitude = am == bm ? NSOrderedSame : (am < bm ? NSOrderedAscending : NSOrderedDescending);
    if (an && magnitude != NSOrderedSame)
        magnitude = magnitude == NSOrderedAscending ? NSOrderedDescending : NSOrderedAscending;
    return magnitude;
}

/* To scale digits after the point (NSDecimalNoScale: as it is). */
void NSDecimalRound(NSDecimal *result, const NSDecimal *number, NSInteger scale, NSRoundingMode roundingMode) {
    if (NSDecimalIsNotANumber(number) || scale == NSDecimalNoScale || -(NSInteger)number->_exponent <= scale) {
        *result = *number;
        return;
    }
    u256 m = u256From(mantissaOf(number));
    int exponent = number->_exponent;
    unsigned first = 0, digit;
    BOOL rest = NO;
    while (-(NSInteger)exponent > scale) {
        rest = rest || first != 0;
        m = u256DivSmall(m, 10, &digit);
        first = digit;
        exponent++;
    }
    if (roundsUp(roundingMode, number->_isNegative, first, rest, (unsigned)(m.lo % 10)))
        m = u256Add(m, u256From(1));
    fit(result, m, exponent, number->_isNegative, roundingMode, NO);
}

NSCalculationError NSDecimalAdd(NSDecimal *result, const NSDecimal *leftOperand, const NSDecimal *rightOperand, NSRoundingMode roundingMode) {
    if (NSDecimalIsNotANumber(leftOperand) || NSDecimalIsNotANumber(rightOperand)) {
        setNaN(result);
        return NSCalculationNoError;
    }
    NSDecimal a = *leftOperand, b = *rightOperand;
    NSCalculationError error = NSDecimalNormalize(&a, &b, roundingMode);
    u128 am = mantissaOf(&a), bm = mantissaOf(&b);
    int exponent = am != 0 ? a._exponent : b._exponent;
    if (am == 0) { *result = b; return error; }
    if (bm == 0) { *result = a; return error; }
    BOOL an = a._isNegative, bn = b._isNegative;
    if (an == bn) {
        NSCalculationError e = fit(result, u256Add(u256From(am), u256From(bm)), exponent, an, roundingMode, NO);
        return e != NSCalculationNoError ? e : error;
    }
    if (am >= bm)
        setDecimal(result, am - bm, exponent, an);
    else
        setDecimal(result, bm - am, exponent, bn);
    return error;
}

NSCalculationError NSDecimalSubtract(NSDecimal *result, const NSDecimal *leftOperand, const NSDecimal *rightOperand, NSRoundingMode roundingMode) {
    NSDecimal negated = *rightOperand;
    if (!NSDecimalIsNotANumber(&negated) && negated._length != 0)
        negated._isNegative = !negated._isNegative;
    return NSDecimalAdd(result, leftOperand, &negated, roundingMode);
}

NSCalculationError NSDecimalMultiply(NSDecimal *result, const NSDecimal *leftOperand, const NSDecimal *rightOperand, NSRoundingMode roundingMode) {
    if (NSDecimalIsNotANumber(leftOperand) || NSDecimalIsNotANumber(rightOperand)) {
        setNaN(result);
        return NSCalculationNoError;
    }
    u128 am = mantissaOf(leftOperand), bm = mantissaOf(rightOperand);
    if (am == 0 || bm == 0) {
        setDecimal(result, 0, 0, NO);
        return NSCalculationNoError;
    }
    BOOL negative = leftOperand->_isNegative != rightOperand->_isNegative;
    int exponent = leftOperand->_exponent + rightOperand->_exponent;
    return fit(result, u256Mul128(am, bm), exponent, negative, roundingMode, NO);
}

NSCalculationError NSDecimalDivide(NSDecimal *result, const NSDecimal *leftOperand, const NSDecimal *rightOperand, NSRoundingMode roundingMode) {
    if (NSDecimalIsNotANumber(leftOperand) || NSDecimalIsNotANumber(rightOperand)) {
        setNaN(result);
        return NSCalculationNoError;
    }
    u128 am = mantissaOf(leftOperand), bm = mantissaOf(rightOperand);
    if (bm == 0) {
        setNaN(result);
        return NSCalculationDivideByZero;
    }
    if (am == 0) {
        setDecimal(result, 0, 0, NO);
        return NSCalculationNoError;
    }
    BOOL negative = leftOperand->_isNegative != rightOperand->_isNegative;
    int exponent = leftOperand->_exponent - rightOperand->_exponent;
    /* The dividend scaled up as far as 256 bits take it, for the quotient's
     * digits; the remainder decides the rounding. */
    u256 n = u256From(am);
    for (;;) {
        BOOL overflow;
        u256 bigger = u256MulSmall(n, 10, &overflow);
        if (overflow || (bigger.hi >> 124) != 0)
            break;
        n = bigger;
        exponent--;
    }
    u128 remainder;
    u256 q = u256Div(n, bm, &remainder);
    BOOL inexact = remainder != 0;
    /* Half of the divisor or more left over rounds up, as the next digit. */
    if (inexact && q.hi == 0) {
        u256 twice = u256Add(u256From(remainder), u256From(remainder));
        int half = u256Compare(twice, u256From(bm));
        unsigned next = half > 0 ? 6 : half == 0 ? 5 : 4;
        if (roundsUp(roundingMode, negative, next, half == 0 ? NO : YES, (unsigned)(q.lo % 10)))
            q = u256Add(q, u256From(1));
        NSCalculationError e = fit(result, q, exponent, negative, roundingMode, NO);
        return e == NSCalculationNoError ? NSCalculationLossOfPrecision : e;
    }
    /* An exact quotient carries the scaling's zeros: 1/4 is 0.25. */
    NSCalculationError e = fit(result, q, exponent, negative, roundingMode, inexact);
    if (e == NSCalculationNoError)
        NSDecimalCompact(result);
    return e;
}

NSCalculationError NSDecimalPower(NSDecimal *result, const NSDecimal *number, NSUInteger power, NSRoundingMode roundingMode) {
    NSDecimal base = *number, acc;
    NSCalculationError error = NSCalculationNoError;

    if (NSDecimalIsNotANumber(number)) {
        setNaN(result);
        return NSCalculationNoError;
    }
    setDecimal(&acc, 1, 0, NO);
    while (power > 0) {
        NSCalculationError e;
        if (power & 1) {
            e = NSDecimalMultiply(&acc, &acc, &base, roundingMode);
            if (e > error) error = e;
            if (e == NSCalculationOverflow) break;
        }
        power >>= 1;
        if (power > 0) {
            e = NSDecimalMultiply(&base, &base, &base, roundingMode);
            if (e == NSCalculationOverflow) { error = e; setNaN(&acc); break; }
            if (e > error) error = e;
        }
    }
    *result = acc;
    return error;
}

NSCalculationError NSDecimalMultiplyByPowerOf10(NSDecimal *result, const NSDecimal *number, short power, NSRoundingMode roundingMode) {
    if (NSDecimalIsNotANumber(number)) {
        setNaN(result);
        return NSCalculationNoError;
    }
    u128 m = mantissaOf(number);
    if (m == 0) {
        setDecimal(result, 0, 0, NO);
        return NSCalculationNoError;
    }
    return fit(result, u256From(m), number->_exponent + power, number->_isNegative, roundingMode, NO);
}

/* The digits, with the locale's decimal separator (an NSLocale or a
 * dictionary with NSLocaleDecimalSeparator). */
NSString *NSDecimalString(const NSDecimal *dcm, id locale) {
    if (NSDecimalIsNotANumber(dcm))
        return @"NaN";
    NSString *separator = @".";
    if ([locale isKindOfClass:[NSLocale class]])
        separator = [locale decimalSeparator] ?: @".";
    else if ([locale isKindOfClass:[NSDictionary class]])
        separator = [locale objectForKey:NSLocaleDecimalSeparator] ?: @".";
    u128 m = mantissaOf(dcm);
    if (m == 0)
        return @"0";
    char digits[64];
    int n = 0;
    while (m != 0) {
        digits[n++] = (char)('0' + (int)(m % 10));
        m /= 10;
    }
    NSMutableString *s = [NSMutableString string];
    if (dcm->_isNegative)
        [s appendString:@"-"];
    int exponent = dcm->_exponent;
    if (exponent >= 0) {
        for (int i = n - 1; i >= 0; i--)
            [s appendFormat:@"%c", digits[i]];
        for (int i = 0; i < exponent; i++)
            [s appendString:@"0"];
        return s;
    }
    int point = -exponent;
    if (point >= n) {
        [s appendString:@"0"];
        [s appendString:separator];
        for (int i = 0; i < point - n; i++)
            [s appendString:@"0"];
        for (int i = n - 1; i >= 0; i--)
            [s appendFormat:@"%c", digits[i]];
        return s;
    }
    for (int i = n - 1; i >= 0; i--) {
        [s appendFormat:@"%c", digits[i]];
        if (i == point && i != 0)
            [s appendString:separator];
    }
    return s;
}

/* -- Conversions, for NSDecimalNumber, NSNumber and NSScanner -- */

void _NSDecimalFromUnsignedLongLong(NSDecimal *d, unsigned long long value, BOOL negative) {
    setDecimal(d, value, 0, negative);
}

void _NSDecimalFromLongLong(NSDecimal *d, long long value) {
    unsigned long long magnitude = value < 0 ? (unsigned long long)(-(value + 1)) + 1 : (unsigned long long)value;
    setDecimal(d, magnitude, 0, value < 0);
}

/* A decimal from its text: sign, digits, the separator, digits, an
 * exponent (e or E). Digits past the mantissa's 38 are dropped (rounded),
 * the exponent made up. The characters taken are counted. */
BOOL _NSDecimalFromString(NSDecimal *d, NSString *string, NSString *separator, NSUInteger *consumed) {
    NSUInteger length = [string length], i = 0;
    u256 m = { 0, 0 };
    int exponent = 0, digits = 0;
    BOOL negative = NO, any = NO, droppedNonZero = NO;
    unsigned firstDropped = 0;
    BOOL firstDroppedSet = NO;
    unichar sep = [separator length] > 0 ? [separator characterAtIndex:0] : '.';

    if (i < length && ([string characterAtIndex:i] == '-' || [string characterAtIndex:i] == '+')) {
        negative = [string characterAtIndex:i] == '-';
        i++;
    }
    BOOL point = NO;
    for (; i < length; i++) {
        unichar c = [string characterAtIndex:i];
        if (c == sep && !point) {
            point = YES;
            continue;
        }
        if (c < '0' || c > '9')
            break;
        any = YES;
        if (digits < 38) {
            BOOL overflow;
            u256 bigger = u256Add(u256MulSmall(m, 10, &overflow), u256From((u128)(c - '0')));
            if (bigger.hi == 0 && !overflow) {
                m = bigger;
                if (m.lo != 0)
                    digits++;
                if (point)
                    exponent--;
                continue;
            }
        }
        /* No room: the digit is dropped, rounding later. */
        if (!firstDroppedSet) {
            firstDropped = (unsigned)(c - '0');
            firstDroppedSet = YES;
        } else if (c != '0')
            droppedNonZero = YES;
        if (!point)
            exponent++;
    }
    if (!any) {
        if (consumed != NULL)
            *consumed = 0;
        return NO;
    }
    if (i < length && ([string characterAtIndex:i] == 'e' || [string characterAtIndex:i] == 'E')) {
        NSUInteger j = i + 1;
        BOOL expNegative = NO;
        int value = 0;
        if (j < length && ([string characterAtIndex:j] == '-' || [string characterAtIndex:j] == '+')) {
            expNegative = [string characterAtIndex:j] == '-';
            j++;
        }
        if (j < length && [string characterAtIndex:j] >= '0' && [string characterAtIndex:j] <= '9') {
            for (; j < length; j++) {
                unichar c = [string characterAtIndex:j];
                if (c < '0' || c > '9')
                    break;
                if (value < 10000)
                    value = value * 10 + (c - '0');
            }
            exponent += expNegative ? -value : value;
            i = j;
        }
    }
    if (firstDroppedSet && roundsUp(NSRoundPlain, negative, firstDropped, droppedNonZero, (unsigned)(m.lo % 10)))
        m = u256Add(m, u256From(1));
    if (consumed != NULL)
        *consumed = i;
    if (exponent > EXP_MAX || exponent < EXP_MIN) {
        if (fit(d, m, exponent, negative, NSRoundPlain, NO) == NSCalculationOverflow)
            return NO;
        return YES;
    }
    setDecimal(d, m.lo, exponent, negative);
    return YES;
}

/* A double's 17 significant digits, as text then as a decimal. */
void _NSDecimalFromDouble(NSDecimal *d, double value) {
    if (value != value) {
        setNaN(d);
        return;
    }
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.17g", value);
    if (!_NSDecimalFromString(d, [NSString stringWithUTF8String:buffer], @".", NULL))
        setNaN(d);
}

double _NSDecimalToDouble(const NSDecimal *d) {
    if (NSDecimalIsNotANumber(d))
        return NAN;
    double result = (double)mantissaOf(d);
    int exponent = d->_exponent;
    result *= pow(10.0, exponent);
    return d->_isNegative ? -result : result;
}
