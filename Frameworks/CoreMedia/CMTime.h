/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Media time: a rational number of seconds (value / timescale), with flags
 * for the times that are not numbers (invalid, infinite, indefinite).
 */

#ifndef __CMTIME__
#define __CMTIME__

#include <CoreMedia/CMBase.h>

typedef int64_t CMTimeValue;
typedef int32_t CMTimeScale;
typedef int64_t CMTimeEpoch;

typedef CF_OPTIONS(uint32_t, CMTimeFlags) {
    kCMTimeFlags_Valid = 1UL << 0,
    kCMTimeFlags_HasBeenRounded = 1UL << 1,
    kCMTimeFlags_PositiveInfinity = 1UL << 2,
    kCMTimeFlags_NegativeInfinity = 1UL << 3,
    kCMTimeFlags_Indefinite = 1UL << 4,
    kCMTimeFlags_ImpliedValueFlagsMask = kCMTimeFlags_PositiveInfinity | kCMTimeFlags_NegativeInfinity | kCMTimeFlags_Indefinite,
};

#pragma pack(push, 4)
typedef struct {
    CMTimeValue value;
    CMTimeScale timescale;
    CMTimeFlags flags;
    CMTimeEpoch epoch;
} CMTime;
#pragma pack(pop)

typedef CF_ENUM(uint32_t, CMTimeRoundingMethod) {
    kCMTimeRoundingMethod_RoundHalfAwayFromZero = 1,
    kCMTimeRoundingMethod_RoundTowardZero = 2,
    kCMTimeRoundingMethod_RoundAwayFromZero = 3,
    kCMTimeRoundingMethod_QuickTime = 4,
    kCMTimeRoundingMethod_RoundTowardPositiveInfinity = 5,
    kCMTimeRoundingMethod_RoundTowardNegativeInfinity = 6,
    kCMTimeRoundingMethod_Default = kCMTimeRoundingMethod_RoundHalfAwayFromZero,
};

#define kCMTimeMaxTimescale 0x7fffffffL

CM_EXPORT const CMTime kCMTimeInvalid;
CM_EXPORT const CMTime kCMTimeIndefinite;
CM_EXPORT const CMTime kCMTimePositiveInfinity;
CM_EXPORT const CMTime kCMTimeNegativeInfinity;
CM_EXPORT const CMTime kCMTimeZero;

#define CMTIME_IS_VALID(time) ((Boolean)(((time).flags & kCMTimeFlags_Valid) != 0))
#define CMTIME_IS_INVALID(time) (!CMTIME_IS_VALID(time))
#define CMTIME_IS_POSITIVE_INFINITY(time) ((Boolean)(CMTIME_IS_VALID(time) && (((time).flags & kCMTimeFlags_PositiveInfinity) != 0)))
#define CMTIME_IS_NEGATIVE_INFINITY(time) ((Boolean)(CMTIME_IS_VALID(time) && (((time).flags & kCMTimeFlags_NegativeInfinity) != 0)))
#define CMTIME_IS_INDEFINITE(time) ((Boolean)(CMTIME_IS_VALID(time) && (((time).flags & kCMTimeFlags_Indefinite) != 0)))
#define CMTIME_IS_NUMERIC(time) ((Boolean)(((time).flags & (kCMTimeFlags_Valid | kCMTimeFlags_ImpliedValueFlagsMask)) == kCMTimeFlags_Valid))

CM_EXPORT CMTime CMTimeMake(int64_t value, int32_t timescale);
CM_EXPORT CMTime CMTimeMakeWithEpoch(int64_t value, int32_t timescale, int64_t epoch);
CM_EXPORT CMTime CMTimeMakeWithSeconds(Float64 seconds, int32_t preferredTimescale);
CM_EXPORT Float64 CMTimeGetSeconds(CMTime time);
CM_EXPORT CMTime CMTimeConvertScale(CMTime time, int32_t newTimescale, CMTimeRoundingMethod method);
CM_EXPORT CMTime CMTimeAdd(CMTime lhs, CMTime rhs);
CM_EXPORT CMTime CMTimeSubtract(CMTime lhs, CMTime rhs);
CM_EXPORT CMTime CMTimeMultiply(CMTime time, int32_t multiplier);
CM_EXPORT CMTime CMTimeMultiplyByFloat64(CMTime time, Float64 multiplier);
CM_EXPORT int32_t CMTimeCompare(CMTime time1, CMTime time2);
CM_EXPORT CMTime CMTimeMinimum(CMTime time1, CMTime time2);
CM_EXPORT CMTime CMTimeMaximum(CMTime time1, CMTime time2);
CM_EXPORT CMTime CMTimeAbsoluteValue(CMTime time);
CM_EXPORT CMTime CMTimeMultiplyByRatio(CMTime time, int32_t multiplier, int32_t divisor);

CM_EXPORT const CFStringRef kCMTimeValueKey;
CM_EXPORT const CFStringRef kCMTimeScaleKey;
CM_EXPORT const CFStringRef kCMTimeEpochKey;
CM_EXPORT const CFStringRef kCMTimeFlagsKey;

CM_EXPORT CFDictionaryRef CMTimeCopyAsDictionary(CMTime time, CFAllocatorRef allocator);
CM_EXPORT CMTime CMTimeMakeFromDictionary(CFDictionaryRef dictionaryRepresentation);
CM_EXPORT CFStringRef CMTimeCopyDescription(CFAllocatorRef allocator, CMTime time);
CM_EXPORT void CMTimeShow(CMTime time);

#define CMTIME_COMPARE_INLINE(time1, comparator, time2) ((Boolean)(CMTimeCompare(time1, time2) comparator 0))

/* A range of time: a start and a duration. */
typedef struct {
    CMTime start;
    CMTime duration;
} CMTimeRange;

CM_EXPORT const CMTimeRange kCMTimeRangeZero;
CM_EXPORT const CMTimeRange kCMTimeRangeInvalid;

#define CMTIMERANGE_IS_VALID(range) ((Boolean)(CMTIME_IS_VALID((range).start) && CMTIME_IS_VALID((range).duration) && \
    (range).duration.epoch == 0 && (range).duration.value >= 0))
#define CMTIMERANGE_IS_INVALID(range) (!CMTIMERANGE_IS_VALID(range))
#define CMTIMERANGE_IS_INDEFINITE(range) ((Boolean)(CMTIMERANGE_IS_VALID(range) && \
    (CMTIME_IS_INDEFINITE((range).start) || CMTIME_IS_INDEFINITE((range).duration))))
#define CMTIMERANGE_IS_EMPTY(range) ((Boolean)(CMTIMERANGE_IS_VALID(range) && \
    CMTIME_COMPARE_INLINE((range).duration, ==, kCMTimeZero)))

CM_EXPORT CMTimeRange CMTimeRangeMake(CMTime start, CMTime duration);
CM_EXPORT CMTimeRange CMTimeRangeFromTimeToTime(CMTime start, CMTime end);
CM_EXPORT CMTime CMTimeRangeGetEnd(CMTimeRange range);
CM_EXPORT Boolean CMTimeRangeContainsTime(CMTimeRange range, CMTime time);
CM_EXPORT Boolean CMTimeRangeContainsTimeRange(CMTimeRange range, CMTimeRange otherRange);
CM_EXPORT Boolean CMTimeRangeEqual(CMTimeRange range1, CMTimeRange range2);
CM_EXPORT CMTimeRange CMTimeRangeGetUnion(CMTimeRange range, CMTimeRange otherRange);
CM_EXPORT CMTimeRange CMTimeRangeGetIntersection(CMTimeRange range, CMTimeRange otherRange);
CM_EXPORT CMTime CMTimeClampToRange(CMTime time, CMTimeRange range);
CM_EXPORT CMTime CMTimeFoldIntoRange(CMTime time, CMTimeRange foldRange);
CM_EXPORT CMTime CMTimeMapTimeFromRangeToRange(CMTime t, CMTimeRange fromRange, CMTimeRange toRange);
CM_EXPORT CMTime CMTimeMapDurationFromRangeToRange(CMTime dur, CMTimeRange fromRange, CMTimeRange toRange);

CM_EXPORT const CFStringRef kCMTimeRangeStartKey;
CM_EXPORT const CFStringRef kCMTimeRangeDurationKey;

CM_EXPORT CFDictionaryRef CMTimeRangeCopyAsDictionary(CMTimeRange range, CFAllocatorRef allocator);
CM_EXPORT CMTimeRange CMTimeRangeMakeFromDictionary(CFDictionaryRef dictionaryRepresentation);
CM_EXPORT CFStringRef CMTimeRangeCopyDescription(CFAllocatorRef allocator, CMTimeRange range);
CM_EXPORT void CMTimeRangeShow(CMTimeRange range);

/* A segment of a track's media (source) put at a place of its timeline
   (target); an empty edit has an invalid source start. */
typedef struct {
    CMTimeRange source;
    CMTimeRange target;
} CMTimeMapping;

CM_EXPORT const CMTimeMapping kCMTimeMappingInvalid;

#define CMTIMEMAPPING_IS_VALID(mapping) (CMTIMERANGE_IS_VALID((mapping).target))
#define CMTIMEMAPPING_IS_INVALID(mapping) (!CMTIMEMAPPING_IS_VALID(mapping))
#define CMTIMEMAPPING_IS_EMPTY(mapping) ((Boolean)(!CMTIME_IS_NUMERIC((mapping).source.start) && CMTIMERANGE_IS_VALID((mapping).target)))

CM_EXPORT CMTimeMapping CMTimeMappingMake(CMTimeRange source, CMTimeRange target);
CM_EXPORT CMTimeMapping CMTimeMappingMakeEmpty(CMTimeRange target);

CM_EXPORT const CFStringRef kCMTimeMappingSourceKey;
CM_EXPORT const CFStringRef kCMTimeMappingTargetKey;

CM_EXPORT CFDictionaryRef CMTimeMappingCopyAsDictionary(CMTimeMapping mapping, CFAllocatorRef allocator);
CM_EXPORT CMTimeMapping CMTimeMappingMakeFromDictionary(CFDictionaryRef dictionaryRepresentation);
CM_EXPORT CFStringRef CMTimeMappingCopyDescription(CFAllocatorRef allocator, CMTimeMapping mapping);
CM_EXPORT void CMTimeMappingShow(CMTimeMapping mapping);

#endif
