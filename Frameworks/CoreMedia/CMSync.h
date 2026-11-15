/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Clocks and timebases. A clock's time always moves on (the host's, an
 * audio device's); a timebase's moves at a rate, set, against its master
 * (a clock or another timebase), and fires its timers when it reaches
 * their times. Times convert between any two of them.
 */

#ifndef __CMSYNC__
#define __CMSYNC__

#include <CoreMedia/CMBase.h>
#include <CoreMedia/CMTime.h>
#include <dispatch/dispatch.h>

typedef struct OpaqueCMClock *CMClockRef;
typedef struct OpaqueCMTimebase *CMTimebaseRef;
typedef CFTypeRef CMClockOrTimebaseRef;

enum {
    kCMClockError_MissingRequiredParameter = -12745,
    kCMClockError_InvalidParameter = -12746,
    kCMClockError_AllocationFailed = -12747,
    kCMClockError_UnsupportedOperation = -12756,
};

enum {
    kCMTimebaseError_MissingRequiredParameter = -12748,
    kCMTimebaseError_InvalidParameter = -12749,
    kCMTimebaseError_AllocationFailed = -12750,
    kCMTimebaseError_TimerIntervalTooShort = -12751,
    kCMTimebaseError_ReadOnly = -12757,
};

enum {
    kCMSyncError_MissingRequiredParameter = -12752,
    kCMSyncError_InvalidParameter = -12753,
    kCMSyncError_AllocationFailed = -12754,
    kCMSyncError_RateMustBeNonZero = -12755,
};

/* ---- Clocks ---- */

CM_EXPORT CFTypeID CMClockGetTypeID(void);
CM_EXPORT CMClockRef CMClockGetHostTimeClock(void);
CM_EXPORT uint64_t CMClockConvertHostTimeToSystemUnits(CMTime hostTime);
CM_EXPORT CMTime CMClockMakeHostTimeFromSystemUnits(uint64_t hostTime);
CM_EXPORT CMTime CMClockGetTime(CMClockRef clock);
CM_EXPORT OSStatus CMClockGetAnchorTime(CMClockRef clock, CMTime *clockTimeOut, CMTime *referenceClockTimeOut);
CM_EXPORT Boolean CMClockMightDrift(CMClockRef clock, CMClockRef otherClock);
CM_EXPORT void CMClockInvalidate(CMClockRef clock);

/* ---- Timebases ---- */

CM_EXPORT CFTypeID CMTimebaseGetTypeID(void);
CM_EXPORT OSStatus CMTimebaseCreateWithMasterClock(CFAllocatorRef allocator, CMClockRef masterClock,
                                                   CMTimebaseRef *timebaseOut);
CM_EXPORT OSStatus CMTimebaseCreateWithMasterTimebase(CFAllocatorRef allocator, CMTimebaseRef masterTimebase,
                                                      CMTimebaseRef *timebaseOut);
CM_EXPORT OSStatus CMTimebaseCreateWithSourceClock(CFAllocatorRef allocator, CMClockRef sourceClock,
                                                   CMTimebaseRef *timebaseOut);
CM_EXPORT OSStatus CMTimebaseCreateWithSourceTimebase(CFAllocatorRef allocator, CMTimebaseRef sourceTimebase,
                                                      CMTimebaseRef *timebaseOut);
CM_EXPORT CMClockOrTimebaseRef CMTimebaseCopyMaster(CMTimebaseRef timebase);
CM_EXPORT CMTimebaseRef CMTimebaseCopyMasterTimebase(CMTimebaseRef timebase);
CM_EXPORT CMClockRef CMTimebaseCopyMasterClock(CMTimebaseRef timebase);
CM_EXPORT CMClockRef CMTimebaseCopyUltimateMasterClock(CMTimebaseRef timebase);
CM_EXPORT CMClockOrTimebaseRef CMTimebaseCopySource(CMTimebaseRef timebase);
CM_EXPORT CMClockRef CMTimebaseCopySourceClock(CMTimebaseRef timebase);
CM_EXPORT CMTimebaseRef CMTimebaseCopySourceTimebase(CMTimebaseRef timebase);
CM_EXPORT CMClockRef CMTimebaseCopyUltimateSourceClock(CMTimebaseRef timebase);
CM_EXPORT CMClockOrTimebaseRef CMTimebaseGetMaster(CMTimebaseRef timebase);
CM_EXPORT CMTimebaseRef CMTimebaseGetMasterTimebase(CMTimebaseRef timebase);
CM_EXPORT CMClockRef CMTimebaseGetMasterClock(CMTimebaseRef timebase);
CM_EXPORT CMClockRef CMTimebaseGetUltimateMasterClock(CMTimebaseRef timebase);
CM_EXPORT OSStatus CMTimebaseSetMasterClock(CMTimebaseRef timebase, CMClockRef newMasterClock);
CM_EXPORT OSStatus CMTimebaseSetMasterTimebase(CMTimebaseRef timebase, CMTimebaseRef newMasterTimebase);
CM_EXPORT OSStatus CMTimebaseSetSourceClock(CMTimebaseRef timebase, CMClockRef newSourceClock);
CM_EXPORT OSStatus CMTimebaseSetSourceTimebase(CMTimebaseRef timebase, CMTimebaseRef newSourceTimebase);
CM_EXPORT CMTime CMTimebaseGetTime(CMTimebaseRef timebase);
CM_EXPORT CMTime CMTimebaseGetTimeWithTimeScale(CMTimebaseRef timebase, CMTimeScale timescale,
                                                CMTimeRoundingMethod method);
CM_EXPORT OSStatus CMTimebaseSetTime(CMTimebaseRef timebase, CMTime time);
CM_EXPORT OSStatus CMTimebaseSetAnchorTime(CMTimebaseRef timebase, CMTime timebaseTime, CMTime immediateMasterTime);
CM_EXPORT Float64 CMTimebaseGetRate(CMTimebaseRef timebase);
CM_EXPORT OSStatus CMTimebaseGetTimeAndRate(CMTimebaseRef timebase, CMTime *timeOut, Float64 *rateOut);
CM_EXPORT OSStatus CMTimebaseSetRate(CMTimebaseRef timebase, Float64 rate);
CM_EXPORT OSStatus CMTimebaseSetRateAndAnchorTime(CMTimebaseRef timebase, Float64 rate, CMTime timebaseTime,
                                                  CMTime immediateMasterTime);
CM_EXPORT Float64 CMTimebaseGetEffectiveRate(CMTimebaseRef timebase);

CM_EXPORT OSStatus CMTimebaseAddTimer(CMTimebaseRef timebase, CFRunLoopTimerRef timer, CFRunLoopRef runloop);
#define kCMTimebaseVeryLongCFTimeInterval (CFTimeInterval)(256.0 * 365.0 * 24.0 * 60.0 * 60.0)
#define kCMTimebaseFarFutureCFAbsoluteTime ((CFAbsoluteTime)kCMTimebaseVeryLongCFTimeInterval)
CM_EXPORT OSStatus CMTimebaseRemoveTimer(CMTimebaseRef timebase, CFRunLoopTimerRef timer);
CM_EXPORT OSStatus CMTimebaseSetTimerNextFireTime(CMTimebaseRef timebase, CFRunLoopTimerRef timer, CMTime fireTime,
                                                  uint32_t flags);
CM_EXPORT OSStatus CMTimebaseSetTimerToFireImmediately(CMTimebaseRef timebase, CFRunLoopTimerRef timer);
CM_EXPORT OSStatus CMTimebaseAddTimerDispatchSource(CMTimebaseRef timebase, dispatch_source_t timerSource);
CM_EXPORT OSStatus CMTimebaseRemoveTimerDispatchSource(CMTimebaseRef timebase, dispatch_source_t timerSource);
CM_EXPORT OSStatus CMTimebaseSetTimerDispatchSourceNextFireTime(CMTimebaseRef timebase, dispatch_source_t timerSource,
                                                                CMTime fireTime, uint32_t flags);
CM_EXPORT OSStatus CMTimebaseSetTimerDispatchSourceToFireImmediately(CMTimebaseRef timebase,
                                                                     dispatch_source_t timerSource);
CM_EXPORT OSStatus CMTimebaseNotificationBarrier(CMTimebaseRef timebase);

CM_EXPORT const CFStringRef kCMTimebaseNotification_EffectiveRateChanged;
CM_EXPORT const CFStringRef kCMTimebaseNotification_TimeJumped;
CM_EXPORT const CFStringRef kCMTimebaseNotificationKey_EventTime;

/* ---- Between them ---- */

CM_EXPORT Float64 CMSyncGetRelativeRate(CMClockOrTimebaseRef ofClockOrTimebase,
                                        CMClockOrTimebaseRef relativeToClockOrTimebase);
CM_EXPORT OSStatus CMSyncGetRelativeRateAndAnchorTime(CMClockOrTimebaseRef ofClockOrTimebase,
                                                      CMClockOrTimebaseRef relativeToClockOrTimebase,
                                                      Float64 *outRelativeRate, CMTime *outOfClockOrTimebaseAnchorTime,
                                                      CMTime *outRelativeToClockOrTimebaseAnchorTime);
CM_EXPORT CMTime CMSyncConvertTime(CMTime time, CMClockOrTimebaseRef fromClockOrTimebase,
                                   CMClockOrTimebaseRef toClockOrTimebase);
CM_EXPORT Boolean CMSyncMightDrift(CMClockOrTimebaseRef clockOrTimebase1, CMClockOrTimebaseRef clockOrTimebase2);
CM_EXPORT CMTime CMSyncGetTime(CMClockOrTimebaseRef clockOrTimebase);

#endif
