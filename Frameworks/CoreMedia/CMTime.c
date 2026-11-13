/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Exact arithmetic on rational times: sums and differences in the least
 * common timescale that fits, rounded when it does not.
 */

#include "CMTime.h"
#include "CMSampleBuffer.h"
#include <math.h>

const CMTime kCMTimeInvalid = { 0, 0, 0, 0 };
const CMTime kCMTimeIndefinite = { 0, 0, kCMTimeFlags_Valid | kCMTimeFlags_Indefinite, 0 };
const CMTime kCMTimePositiveInfinity = { 0, 0, kCMTimeFlags_Valid | kCMTimeFlags_PositiveInfinity, 0 };
const CMTime kCMTimeNegativeInfinity = { 0, 0, kCMTimeFlags_Valid | kCMTimeFlags_NegativeInfinity, 0 };
const CMTime kCMTimeZero = { 0, 1, kCMTimeFlags_Valid, 0 };

CMTime CMTimeMake(int64_t value, int32_t timescale) {
    return CMTimeMakeWithEpoch(value, timescale, 0);
}

CMTime CMTimeMakeWithEpoch(int64_t value, int32_t timescale, int64_t epoch) {
    if (timescale <= 0)
        return kCMTimeInvalid;
    return (CMTime){ value, timescale, kCMTimeFlags_Valid, epoch };
}

CMTime CMTimeMakeWithSeconds(Float64 seconds, int32_t preferredTimescale) {
    if (isnan(seconds) || preferredTimescale <= 0)
        return kCMTimeInvalid;
    if (isinf(seconds))
        return seconds > 0 ? kCMTimePositiveInfinity : kCMTimeNegativeInfinity;
    CMTime t = CMTimeMake((int64_t)llround(seconds * preferredTimescale), preferredTimescale);
    if ((Float64)t.value / preferredTimescale != seconds)
        t.flags |= kCMTimeFlags_HasBeenRounded;
    return t;
}

Float64 CMTimeGetSeconds(CMTime t) {
    if (!CMTIME_IS_VALID(t) || CMTIME_IS_INDEFINITE(t))
        return NAN;
    if (CMTIME_IS_POSITIVE_INFINITY(t))
        return INFINITY;
    if (CMTIME_IS_NEGATIVE_INFINITY(t))
        return -INFINITY;
    return (Float64)t.value / t.timescale;
}

static int64_t divideRounding(__int128 n, __int128 d, CMTimeRoundingMethod method, Boolean *rounded) {
    __int128 q = n / d, r = n % d;
    if (r == 0)
        return (int64_t)q;
    *rounded = true;
    Boolean negative = (n < 0) != (d < 0);
    switch (method) {
    case kCMTimeRoundingMethod_RoundTowardZero:
        return (int64_t)q;
    case kCMTimeRoundingMethod_RoundAwayFromZero:
        return (int64_t)(negative ? q - 1 : q + 1);
    case kCMTimeRoundingMethod_RoundTowardPositiveInfinity:
        return (int64_t)(negative ? q : q + 1);
    case kCMTimeRoundingMethod_RoundTowardNegativeInfinity:
        return (int64_t)(negative ? q - 1 : q);
    default: {
        __int128 twice = (r < 0 ? -r : r) * 2, dd = d < 0 ? -d : d;
        if (twice >= dd)
            return (int64_t)(negative ? q - 1 : q + 1);
        return (int64_t)q;
    }
    }
}

CMTime CMTimeConvertScale(CMTime t, int32_t newTimescale, CMTimeRoundingMethod method) {
    if (!CMTIME_IS_NUMERIC(t) || newTimescale <= 0)
        return CMTIME_IS_NUMERIC(t) ? kCMTimeInvalid : t;
    if (t.timescale == newTimescale)
        return t;
    Boolean rounded = false;
    int64_t value = divideRounding((__int128)t.value * newTimescale, t.timescale, method, &rounded);
    CMTime result = { value, newTimescale, t.flags | (rounded ? kCMTimeFlags_HasBeenRounded : 0), t.epoch };
    return result;
}

static int64_t gcd(int64_t a, int64_t b) {
    while (b != 0) {
        int64_t t = a % b;
        a = b;
        b = t;
    }
    return a < 0 ? -a : a;
}

/* The common timescale of two times, at most kCMTimeMaxTimescale. */
static int32_t commonScale(int32_t a, int32_t b) {
    int64_t lcm = (int64_t)a / gcd(a, b) * b;
    if (lcm <= kCMTimeMaxTimescale)
        return (int32_t)lcm;
    return a > b ? a : b;
}

static CMTime nonNumericSum(CMTime a, CMTime b, int sign) {
    if (!CMTIME_IS_VALID(a) || !CMTIME_IS_VALID(b))
        return kCMTimeInvalid;
    if (CMTIME_IS_INDEFINITE(a) || CMTIME_IS_INDEFINITE(b))
        return kCMTimeIndefinite;
    Boolean aPos = CMTIME_IS_POSITIVE_INFINITY(a), aNeg = CMTIME_IS_NEGATIVE_INFINITY(a);
    Boolean bPos = CMTIME_IS_POSITIVE_INFINITY(b), bNeg = CMTIME_IS_NEGATIVE_INFINITY(b);
    if (sign < 0) {
        Boolean t = bPos;
        bPos = bNeg;
        bNeg = t;
    }
    if ((aPos && bNeg) || (aNeg && bPos))
        return kCMTimeInvalid;
    if (aPos || bPos)
        return kCMTimePositiveInfinity;
    return kCMTimeNegativeInfinity;
}

static CMTime sum(CMTime a, CMTime b, int sign) {
    if (!CMTIME_IS_NUMERIC(a) || !CMTIME_IS_NUMERIC(b))
        return nonNumericSum(a, b, sign);
    int32_t scale = commonScale(a.timescale, b.timescale);
    CMTime x = CMTimeConvertScale(a, scale, kCMTimeRoundingMethod_Default);
    CMTime y = CMTimeConvertScale(b, scale, kCMTimeRoundingMethod_Default);
    __int128 v = (__int128)x.value + sign * (__int128)y.value;
    if (v > INT64_MAX)
        return kCMTimePositiveInfinity;
    if (v < INT64_MIN)
        return kCMTimeNegativeInfinity;
    CMTime r = { (int64_t)v, scale, kCMTimeFlags_Valid | ((x.flags | y.flags) & kCMTimeFlags_HasBeenRounded),
                 a.epoch == b.epoch ? a.epoch : 0 };
    return r;
}

CMTime CMTimeAdd(CMTime lhs, CMTime rhs) {
    return sum(lhs, rhs, 1);
}

CMTime CMTimeSubtract(CMTime lhs, CMTime rhs) {
    return sum(lhs, rhs, -1);
}

CMTime CMTimeMultiply(CMTime t, int32_t multiplier) {
    if (!CMTIME_IS_NUMERIC(t))
        return t;
    __int128 v = (__int128)t.value * multiplier;
    if (v > INT64_MAX)
        return kCMTimePositiveInfinity;
    if (v < INT64_MIN)
        return kCMTimeNegativeInfinity;
    t.value = (int64_t)v;
    return t;
}

CMTime CMTimeMultiplyByFloat64(CMTime t, Float64 multiplier) {
    if (!CMTIME_IS_NUMERIC(t))
        return t;
    return CMTimeMakeWithSeconds(CMTimeGetSeconds(t) * multiplier, t.timescale);
}

/* Invalid < negative infinity < numbers < positive infinity < indefinite. */
static int rank(CMTime t) {
    if (!CMTIME_IS_VALID(t))
        return 0;
    if (CMTIME_IS_NEGATIVE_INFINITY(t))
        return 1;
    if (CMTIME_IS_POSITIVE_INFINITY(t))
        return 3;
    if (CMTIME_IS_INDEFINITE(t))
        return 4;
    return 2;
}

int32_t CMTimeCompare(CMTime a, CMTime b) {
    int ra = rank(a), rb = rank(b);
    if (ra != rb)
        return ra < rb ? -1 : 1;
    if (ra != 2)
        return 0;
    __int128 x = (__int128)a.value * b.timescale, y = (__int128)b.value * a.timescale;
    return x < y ? -1 : (x > y ? 1 : 0);
}

CMTime CMTimeMinimum(CMTime a, CMTime b) {
    return CMTimeCompare(a, b) <= 0 ? a : b;
}

CMTime CMTimeMaximum(CMTime a, CMTime b) {
    return CMTimeCompare(a, b) >= 0 ? a : b;
}

CMTime CMTimeAbsoluteValue(CMTime t) {
    if (CMTIME_IS_NEGATIVE_INFINITY(t))
        return kCMTimePositiveInfinity;
    if (CMTIME_IS_NUMERIC(t) && t.value < 0)
        t.value = -t.value;
    return t;
}

const CMSampleTimingInfo kCMTimingInfoInvalid = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };

const CMTimeRange kCMTimeRangeZero = { { 0, 1, kCMTimeFlags_Valid, 0 }, { 0, 1, kCMTimeFlags_Valid, 0 } };
const CMTimeRange kCMTimeRangeInvalid = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };

CMTimeRange CMTimeRangeMake(CMTime start, CMTime duration) {
    if (!CMTIME_IS_VALID(start) || !CMTIME_IS_VALID(duration) || CMTimeCompare(duration, kCMTimeZero) < 0)
        return kCMTimeRangeInvalid;
    return (CMTimeRange){ start, duration };
}

CMTime CMTimeRangeGetEnd(CMTimeRange range) {
    return CMTimeAdd(range.start, range.duration);
}

/* The start is in the range, the end is not. */
Boolean CMTimeRangeContainsTime(CMTimeRange range, CMTime time) {
    return CMTimeCompare(time, range.start) >= 0 && CMTimeCompare(time, CMTimeRangeGetEnd(range)) < 0;
}
