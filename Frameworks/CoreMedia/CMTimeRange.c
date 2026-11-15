/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Time ranges and time mappings, and times and both as dictionaries and
 * descriptions. A range holds its start and not its end; mapping a time
 * from one range to another scales its offset by the ranges' durations,
 * exactly, in the target start's timescale.
 */

#include "CMTime.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>

const CFStringRef kCMTimeValueKey = CFSTR("value");
const CFStringRef kCMTimeScaleKey = CFSTR("timescale");
const CFStringRef kCMTimeEpochKey = CFSTR("epoch");
const CFStringRef kCMTimeFlagsKey = CFSTR("flags");
const CFStringRef kCMTimeRangeStartKey = CFSTR("start");
const CFStringRef kCMTimeRangeDurationKey = CFSTR("duration");
const CFStringRef kCMTimeMappingSourceKey = CFSTR("source");
const CFStringRef kCMTimeMappingTargetKey = CFSTR("target");

const CMTimeMapping kCMTimeMappingInvalid = { { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }, { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } } };

/* ---- Times ---- */

static int64_t roundedQuotient(__int128 n, __int128 d, Boolean *rounded) {
    if (d < 0) {
        n = -n;
        d = -d;
    }
    __int128 q = n / d, r = n % d;
    if (r == 0)
        return (int64_t)q;
    *rounded = true;
    /* Half away from zero, the default rounding. */
    if ((r < 0 ? -r : r) * 2 >= d)
        q += n < 0 ? -1 : 1;
    if (q > INT64_MAX)
        return INT64_MAX;
    if (q < INT64_MIN)
        return INT64_MIN;
    return (int64_t)q;
}

CMTime CMTimeMultiplyByRatio(CMTime t, int32_t multiplier, int32_t divisor) {
    if (!CMTIME_IS_NUMERIC(t))
        return t;
    if (divisor == 0)
        return kCMTimeInvalid;
    Boolean rounded = false;
    __int128 n = (__int128)t.value * multiplier;
    __int128 q = n / divisor;
    if (q > INT64_MAX || q < INT64_MIN)
        return (q > 0) == (divisor > 0) ? kCMTimePositiveInfinity : kCMTimeNegativeInfinity;
    t.value = roundedQuotient(n, divisor, &rounded);
    if (rounded)
        t.flags |= kCMTimeFlags_HasBeenRounded;
    return t;
}

static void setNumber(CFMutableDictionaryRef d, CFStringRef key, CFNumberType type, const void *value) {
    CFNumberRef n = CFNumberCreate(NULL, type, value);
    CFDictionarySetValue(d, key, n);
    CFRelease(n);
}

CFDictionaryRef CMTimeCopyAsDictionary(CMTime t, CFAllocatorRef allocator) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(allocator, 4, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    setNumber(d, kCMTimeValueKey, kCFNumberSInt64Type, &t.value);
    setNumber(d, kCMTimeScaleKey, kCFNumberSInt32Type, &t.timescale);
    setNumber(d, kCMTimeEpochKey, kCFNumberSInt64Type, &t.epoch);
    setNumber(d, kCMTimeFlagsKey, kCFNumberSInt32Type, &t.flags);
    return d;
}

static Boolean getNumber(CFDictionaryRef d, CFStringRef key, CFNumberType type, void *value) {
    CFTypeRef n = d != NULL ? CFDictionaryGetValue(d, key) : NULL;
    return n != NULL && CFGetTypeID(n) == CFNumberGetTypeID() && CFNumberGetValue(n, type, value);
}

CMTime CMTimeMakeFromDictionary(CFDictionaryRef d) {
    CMTime t = kCMTimeInvalid;
    if (d == NULL || CFGetTypeID(d) != CFDictionaryGetTypeID() ||
        !getNumber(d, kCMTimeValueKey, kCFNumberSInt64Type, &t.value) ||
        !getNumber(d, kCMTimeScaleKey, kCFNumberSInt32Type, &t.timescale) ||
        !getNumber(d, kCMTimeFlagsKey, kCFNumberSInt32Type, &t.flags))
        return kCMTimeInvalid;
    getNumber(d, kCMTimeEpochKey, kCFNumberSInt64Type, &t.epoch);
    return t;
}

CFStringRef CMTimeCopyDescription(CFAllocatorRef allocator, CMTime t) {
    if (!CMTIME_IS_VALID(t))
        return CFStringCreateWithCString(allocator, "{INVALID}", kCFStringEncodingUTF8);
    if (CMTIME_IS_POSITIVE_INFINITY(t))
        return CFStringCreateWithCString(allocator, "{+INFINITY}", kCFStringEncodingUTF8);
    if (CMTIME_IS_NEGATIVE_INFINITY(t))
        return CFStringCreateWithCString(allocator, "{-INFINITY}", kCFStringEncodingUTF8);
    if (CMTIME_IS_INDEFINITE(t))
        return CFStringCreateWithCString(allocator, "{INDEFINITE}", kCFStringEncodingUTF8);
    const char *rounded = (t.flags & kCMTimeFlags_HasBeenRounded) ? ", rounded" : "";
    if (t.epoch != 0)
        return CFStringCreateWithFormat(allocator, NULL, CFSTR("{%" PRId64 "/%d = %.3f%s, epoch %" PRId64 "}"), t.value,
                                        t.timescale, CMTimeGetSeconds(t), rounded, t.epoch);
    return CFStringCreateWithFormat(allocator, NULL, CFSTR("{%" PRId64 "/%d = %.3f%s}"), t.value, t.timescale,
                                    CMTimeGetSeconds(t), rounded);
}

static void show(CFStringRef s) {
    char buf[512];
    if (s != NULL && CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8))
        printf("%s\n", buf);
    if (s != NULL)
        CFRelease(s);
}

void CMTimeShow(CMTime t) {
    show(CMTimeCopyDescription(NULL, t));
}

/* ---- Ranges ---- */

CMTimeRange CMTimeRangeFromTimeToTime(CMTime start, CMTime end) {
    return CMTimeRangeMake(start, CMTimeSubtract(end, start));
}

Boolean CMTimeRangeContainsTimeRange(CMTimeRange range, CMTimeRange other) {
    if (!CMTIMERANGE_IS_VALID(range) || !CMTIMERANGE_IS_VALID(other))
        return false;
    return CMTimeCompare(other.start, range.start) >= 0 &&
           CMTimeCompare(CMTimeRangeGetEnd(other), CMTimeRangeGetEnd(range)) <= 0;
}

Boolean CMTimeRangeEqual(CMTimeRange a, CMTimeRange b) {
    return CMTimeCompare(a.start, b.start) == 0 && CMTimeCompare(a.duration, b.duration) == 0;
}

CMTimeRange CMTimeRangeGetUnion(CMTimeRange a, CMTimeRange b) {
    if (!CMTIMERANGE_IS_VALID(a) || !CMTIMERANGE_IS_VALID(b))
        return kCMTimeRangeInvalid;
    return CMTimeRangeFromTimeToTime(CMTimeMinimum(a.start, b.start),
                                     CMTimeMaximum(CMTimeRangeGetEnd(a), CMTimeRangeGetEnd(b)));
}

/* Ranges that do not meet: an empty range at the later start. */
CMTimeRange CMTimeRangeGetIntersection(CMTimeRange a, CMTimeRange b) {
    if (!CMTIMERANGE_IS_VALID(a) || !CMTIMERANGE_IS_VALID(b))
        return kCMTimeRangeInvalid;
    CMTime start = CMTimeMaximum(a.start, b.start);
    CMTime end = CMTimeMinimum(CMTimeRangeGetEnd(a), CMTimeRangeGetEnd(b));
    if (CMTimeCompare(end, start) <= 0)
        return CMTimeRangeMake(start, CMTimeConvertScale(kCMTimeZero, CMTIME_IS_NUMERIC(start) ? start.timescale : 1,
                                                         kCMTimeRoundingMethod_Default));
    return CMTimeRangeFromTimeToTime(start, end);
}

CMTime CMTimeClampToRange(CMTime t, CMTimeRange range) {
    if (!CMTIME_IS_VALID(t) || !CMTIMERANGE_IS_VALID(range))
        return kCMTimeInvalid;
    if (CMTimeCompare(t, range.start) < 0)
        return range.start;
    CMTime end = CMTimeRangeGetEnd(range);
    if (CMTimeCompare(t, end) > 0)
        return end;
    return t;
}

/* Into the range as if it repeated: its start plus the offset modulo its
   duration. */
CMTime CMTimeFoldIntoRange(CMTime t, CMTimeRange range) {
    if (!CMTIME_IS_NUMERIC(t) || !CMTIMERANGE_IS_VALID(range) || !CMTIME_IS_NUMERIC(range.start) ||
        !CMTIME_IS_NUMERIC(range.duration) || range.duration.value == 0)
        return CMTIME_IS_VALID(t) && CMTIMERANGE_IS_VALID(range) ? range.start : kCMTimeInvalid;
    CMTime offset = CMTimeSubtract(t, range.start);
    /* offset mod duration, exactly, in the offset's common scale. */
    __int128 n = (__int128)offset.value * range.duration.timescale;
    __int128 d = (__int128)range.duration.value * offset.timescale;
    __int128 m = n % d;
    if (m < 0)
        m += d;
    Boolean rounded = false;
    int64_t value = roundedQuotient(m, range.duration.timescale, &rounded);
    CMTime folded = CMTimeMake(value, offset.timescale);
    if (rounded)
        folded.flags |= kCMTimeFlags_HasBeenRounded;
    return CMTimeAdd(range.start, folded);
}

/* d * to.duration / from.duration, in scale. */
static CMTime scaleDuration(CMTime d, CMTimeRange from, CMTimeRange to, int32_t scale) {
    if (!CMTIME_IS_NUMERIC(from.duration) || !CMTIME_IS_NUMERIC(to.duration) || from.duration.value == 0)
        return kCMTimeInvalid;
    __int128 n = (__int128)d.value * to.duration.value * from.duration.timescale * scale;
    __int128 den = (__int128)d.timescale * to.duration.timescale * from.duration.value;
    Boolean rounded = false;
    CMTime r = CMTimeMake(roundedQuotient(n, den, &rounded), scale);
    if (rounded)
        r.flags |= kCMTimeFlags_HasBeenRounded;
    return r;
}

CMTime CMTimeMapTimeFromRangeToRange(CMTime t, CMTimeRange from, CMTimeRange to) {
    if (!CMTIME_IS_NUMERIC(t) || !CMTIMERANGE_IS_VALID(from) || !CMTIMERANGE_IS_VALID(to) ||
        !CMTIME_IS_NUMERIC(to.start))
        return kCMTimeInvalid;
    CMTime offset = CMTimeSubtract(t, from.start);
    if (!CMTIME_IS_NUMERIC(offset))
        return kCMTimeInvalid;
    return CMTimeAdd(to.start, scaleDuration(offset, from, to, to.start.timescale));
}

CMTime CMTimeMapDurationFromRangeToRange(CMTime d, CMTimeRange from, CMTimeRange to) {
    if (!CMTIME_IS_NUMERIC(d) || !CMTIMERANGE_IS_VALID(from) || !CMTIMERANGE_IS_VALID(to))
        return kCMTimeInvalid;
    return scaleDuration(d, from, to, CMTIME_IS_NUMERIC(to.duration) ? to.duration.timescale : d.timescale);
}

static void setTime(CFMutableDictionaryRef d, CFStringRef key, CMTime t, CFAllocatorRef allocator) {
    CFDictionaryRef v = CMTimeCopyAsDictionary(t, allocator);
    CFDictionarySetValue(d, key, v);
    CFRelease(v);
}

CFDictionaryRef CMTimeRangeCopyAsDictionary(CMTimeRange range, CFAllocatorRef allocator) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(allocator, 2, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    setTime(d, kCMTimeRangeStartKey, range.start, allocator);
    setTime(d, kCMTimeRangeDurationKey, range.duration, allocator);
    return d;
}

CMTimeRange CMTimeRangeMakeFromDictionary(CFDictionaryRef d) {
    if (d == NULL || CFGetTypeID(d) != CFDictionaryGetTypeID())
        return kCMTimeRangeInvalid;
    CMTimeRange r = { CMTimeMakeFromDictionary(CFDictionaryGetValue(d, kCMTimeRangeStartKey)),
                      CMTimeMakeFromDictionary(CFDictionaryGetValue(d, kCMTimeRangeDurationKey)) };
    return r;
}

CFStringRef CMTimeRangeCopyDescription(CFAllocatorRef allocator, CMTimeRange range) {
    CFStringRef start = CMTimeCopyDescription(allocator, range.start);
    CFStringRef duration = CMTimeCopyDescription(allocator, range.duration);
    CFStringRef s = CFStringCreateWithFormat(allocator, NULL, CFSTR("{%@, %@}"), start, duration);
    CFRelease(start);
    CFRelease(duration);
    return s;
}

void CMTimeRangeShow(CMTimeRange range) {
    show(CMTimeRangeCopyDescription(NULL, range));
}

/* ---- Mappings ---- */

CMTimeMapping CMTimeMappingMake(CMTimeRange source, CMTimeRange target) {
    if (!CMTIMERANGE_IS_VALID(source) || !CMTIMERANGE_IS_VALID(target))
        return kCMTimeMappingInvalid;
    return (CMTimeMapping){ source, target };
}

CMTimeMapping CMTimeMappingMakeEmpty(CMTimeRange target) {
    if (!CMTIMERANGE_IS_VALID(target))
        return kCMTimeMappingInvalid;
    return (CMTimeMapping){ { kCMTimeInvalid, target.duration }, target };
}

CFDictionaryRef CMTimeMappingCopyAsDictionary(CMTimeMapping mapping, CFAllocatorRef allocator) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(allocator, 2, &kCFTypeDictionaryKeyCallBacks,
                                                         &kCFTypeDictionaryValueCallBacks);
    CFDictionaryRef source = CMTimeRangeCopyAsDictionary(mapping.source, allocator);
    CFDictionaryRef target = CMTimeRangeCopyAsDictionary(mapping.target, allocator);
    CFDictionarySetValue(d, kCMTimeMappingSourceKey, source);
    CFDictionarySetValue(d, kCMTimeMappingTargetKey, target);
    CFRelease(source);
    CFRelease(target);
    return d;
}

CMTimeMapping CMTimeMappingMakeFromDictionary(CFDictionaryRef d) {
    if (d == NULL || CFGetTypeID(d) != CFDictionaryGetTypeID())
        return kCMTimeMappingInvalid;
    CMTimeMapping m = { CMTimeRangeMakeFromDictionary(CFDictionaryGetValue(d, kCMTimeMappingSourceKey)),
                        CMTimeRangeMakeFromDictionary(CFDictionaryGetValue(d, kCMTimeMappingTargetKey)) };
    return m;
}

CFStringRef CMTimeMappingCopyDescription(CFAllocatorRef allocator, CMTimeMapping mapping) {
    CFStringRef source = CMTimeRangeCopyDescription(allocator, mapping.source);
    CFStringRef target = CMTimeRangeCopyDescription(allocator, mapping.target);
    CFStringRef s = CFStringCreateWithFormat(allocator, NULL, CFSTR("{source = %@, target = %@}"), source, target);
    CFRelease(source);
    CFRelease(target);
    return s;
}

void CMTimeMappingShow(CMTimeMapping mapping) {
    show(CMTimeMappingCopyDescription(NULL, mapping));
}
