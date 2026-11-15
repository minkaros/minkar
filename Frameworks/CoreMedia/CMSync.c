/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Clocks and timebases. Every one maps to the host's time by an anchor
 * pair and a rate: the host clock is the host's time; an audio device's
 * clock is the device's sample time over its rate, anchored at its last
 * time stamp (it may drift from the host); a timebase is its anchor time
 * plus its master's time since the master's anchor, times its rate.
 * Converting a time goes through the host's time; a timebase's timers
 * are rescheduled whenever its time or rate changes.
 */

#include "CMSync.h"
#include "CMAudioDeviceClock.h"
#include "CMNotificationCenter.h"
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CFRuntime.h>
#include <math.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

const CFStringRef kCMTimebaseNotification_EffectiveRateChanged = CFSTR("CMTimebaseEffectiveRateChanged");
const CFStringRef kCMTimebaseNotification_TimeJumped = CFSTR("CMTimebaseTimeJumped");
const CFStringRef kCMTimebaseNotificationKey_EventTime = CFSTR("EventTime");

#define NANOS 1000000000

/* ---- Host time ---- */

static mach_timebase_info_data_t timebaseInfo(void) {
    static mach_timebase_info_data_t tb;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ mach_timebase_info(&tb); });
    return tb;
}

CMTime CMClockMakeHostTimeFromSystemUnits(uint64_t host) {
    mach_timebase_info_data_t tb = timebaseInfo();
    return CMTimeMake((int64_t)((__int128)host * tb.numer / tb.denom), NANOS);
}

uint64_t CMClockConvertHostTimeToSystemUnits(CMTime t) {
    if (!CMTIME_IS_NUMERIC(t) || t.value < 0)
        return 0;
    mach_timebase_info_data_t tb = timebaseInfo();
    CMTime ns = CMTimeConvertScale(t, NANOS, kCMTimeRoundingMethod_Default);
    return (uint64_t)((__int128)ns.value * tb.denom / tb.numer);
}

static CMTime hostNow(void) {
    return CMClockMakeHostTimeFromSystemUnits(mach_absolute_time());
}

/* ---- Clocks ---- */

typedef enum { kHostClock, kAudioDeviceClock } ClockKind;

struct OpaqueCMClock {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    ClockKind kind;
    Boolean invalid;
    /* an audio device's */
    AudioObjectID device;
    CFStringRef deviceUID;
    Boolean tracksDefault;
};

static CFTypeID clockTypeID, timebaseTypeID;

static void clockFinalize(CFTypeRef cf) {
    CMClockRef c = (CMClockRef)cf;
    if (c->deviceUID != NULL)
        CFRelease(c->deviceUID);
    pthread_mutex_destroy(&c->lock);
}

static const CFRuntimeClass clockClass = { 0, "CMClock", NULL, NULL, clockFinalize, NULL, NULL, NULL, NULL };

CFTypeID CMClockGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ clockTypeID = _CFRuntimeRegisterClass(&clockClass); });
    return clockTypeID;
}

static CMClockRef newClock(CFAllocatorRef allocator, ClockKind kind) {
    size_t extra = sizeof(struct OpaqueCMClock) - sizeof(CFRuntimeBase);
    CMClockRef c = (CMClockRef)_CFRuntimeCreateInstance(allocator, CMClockGetTypeID(), extra, NULL);
    if (c != NULL) {
        pthread_mutex_init(&c->lock, NULL);
        c->kind = kind;
    }
    return c;
}

CMClockRef CMClockGetHostTimeClock(void) {
    static CMClockRef host;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ host = newClock(NULL, kHostClock); });
    return host;
}

/* The device's clock: its time at its last time stamp, and the host's then. */
static Boolean deviceAnchor(CMClockRef c, CMTime *clockTime, CMTime *hostTime) {
    AudioObjectID device = c->device;
    if (c->tracksDefault) {
        AudioObjectPropertyAddress a = { kAudioHardwarePropertyDefaultOutputDevice, kAudioObjectPropertyScopeGlobal,
                                         kAudioObjectPropertyElementMain };
        UInt32 size = sizeof(device);
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &device);
    }
    AudioTimeStamp ts;
    memset(&ts, 0, sizeof(ts));
    Float64 rate = 0;
    AudioObjectPropertyAddress r = { kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal,
                                     kAudioObjectPropertyElementMain };
    UInt32 size = sizeof(rate);
    if (device == kAudioObjectUnknown || AudioDeviceGetCurrentTime(device, &ts) != noErr ||
        AudioObjectGetPropertyData(device, &r, 0, NULL, &size, &rate) != noErr || rate <= 0 ||
        !(ts.mFlags & kAudioTimeStampHostTimeValid))
        return false;
    *clockTime = CMTimeMake((int64_t)llround(ts.mSampleTime), (int32_t)rate);
    *hostTime = CMClockMakeHostTimeFromSystemUnits(ts.mHostTime);
    return true;
}

/* A clock's time against the host's: (its time, the host's) at an anchor;
   rate 1. A device that does not run follows the host's time. */
static void clockAnchor(CMClockRef c, CMTime *clockTime, CMTime *hostTime) {
    if (c->kind == kAudioDeviceClock && deviceAnchor(c, clockTime, hostTime))
        return;
    *hostTime = *clockTime = hostNow();
}

CMTime CMClockGetTime(CMClockRef c) {
    if (c == NULL || c->invalid)
        return kCMTimeInvalid;
    if (c->kind == kHostClock)
        return hostNow();
    CMTime clockTime, hostTime;
    clockAnchor(c, &clockTime, &hostTime);
    return CMTimeAdd(clockTime, CMTimeSubtract(hostNow(), hostTime));
}

OSStatus CMClockGetAnchorTime(CMClockRef c, CMTime *clockTimeOut, CMTime *referenceTimeOut) {
    if (c == NULL || clockTimeOut == NULL || referenceTimeOut == NULL)
        return kCMClockError_MissingRequiredParameter;
    clockAnchor(c, clockTimeOut, referenceTimeOut);
    return noErr;
}

/* An audio device's clock may drift from every other clock. */
Boolean CMClockMightDrift(CMClockRef a, CMClockRef b) {
    if (a == NULL || b == NULL || a == b)
        return false;
    if (a->kind == kHostClock && b->kind == kHostClock)
        return false;
    if (a->kind == kAudioDeviceClock && b->kind == kAudioDeviceClock && !a->tracksDefault && !b->tracksDefault)
        return a->device != b->device;
    return true;
}

void CMClockInvalidate(CMClockRef c) {
    if (c != NULL && c->kind != kHostClock)
        c->invalid = true;
}

/* ---- Audio device clocks ---- */

OSStatus CMAudioDeviceClockCreateFromAudioDeviceID(CFAllocatorRef allocator, AudioDeviceID device, CMClockRef *out) {
    if (out == NULL)
        return kCMClockError_MissingRequiredParameter;
    *out = newClock(allocator, kAudioDeviceClock);
    if (*out == NULL)
        return kCMClockError_AllocationFailed;
    (*out)->device = device;
    return noErr;
}

OSStatus CMAudioDeviceClockCreate(CFAllocatorRef allocator, CFStringRef deviceUID, CMClockRef *out) {
    if (out == NULL)
        return kCMClockError_MissingRequiredParameter;
    *out = newClock(allocator, kAudioDeviceClock);
    if (*out == NULL)
        return kCMClockError_AllocationFailed;
    OSStatus err = CMAudioDeviceClockSetAudioDeviceUID(*out, deviceUID);
    if (err != noErr) {
        CFRelease(*out);
        *out = NULL;
    }
    return err;
}

OSStatus CMAudioDeviceClockSetAudioDeviceUID(CMClockRef c, CFStringRef uid) {
    if (c == NULL || c->kind != kAudioDeviceClock)
        return kCMClockError_InvalidParameter;
    AudioObjectID device = kAudioObjectUnknown;
    if (uid != NULL) {
        AudioObjectPropertyAddress a = { kAudioHardwarePropertyTranslateUIDToDevice, kAudioObjectPropertyScopeGlobal,
                                         kAudioObjectPropertyElementMain };
        UInt32 size = sizeof(device);
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, sizeof(uid), &uid, &size, &device) != noErr ||
            device == kAudioObjectUnknown)
            return kCMClockError_InvalidParameter;
    }
    pthread_mutex_lock(&c->lock);
    if (c->deviceUID != NULL)
        CFRelease(c->deviceUID);
    c->deviceUID = uid != NULL ? CFStringCreateCopy(NULL, uid) : NULL;
    c->device = device;
    c->tracksDefault = uid == NULL;
    pthread_mutex_unlock(&c->lock);
    return noErr;
}

OSStatus CMAudioDeviceClockSetAudioDeviceID(CMClockRef c, AudioDeviceID device) {
    if (c == NULL || c->kind != kAudioDeviceClock)
        return kCMClockError_InvalidParameter;
    pthread_mutex_lock(&c->lock);
    if (c->deviceUID != NULL)
        CFRelease(c->deviceUID);
    c->deviceUID = NULL;
    c->device = device;
    c->tracksDefault = false;
    pthread_mutex_unlock(&c->lock);
    return noErr;
}

OSStatus CMAudioDeviceClockGetAudioDevice(CMClockRef c, CFStringRef *uidOut, AudioDeviceID *deviceOut,
                                          Boolean *tracksDefaultOut) {
    if (c == NULL || c->kind != kAudioDeviceClock)
        return kCMClockError_InvalidParameter;
    pthread_mutex_lock(&c->lock);
    if (uidOut != NULL)
        *uidOut = c->deviceUID;
    if (deviceOut != NULL)
        *deviceOut = c->device;
    if (tracksDefaultOut != NULL)
        *tracksDefaultOut = c->tracksDefault;
    pthread_mutex_unlock(&c->lock);
    return noErr;
}

/* ---- Timebases ---- */

typedef struct {
    CFRunLoopTimerRef timer;
    dispatch_source_t source;
    CMTime fireTime;            /* invalid: not set */
} Timer;

struct OpaqueCMTimebase {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    CFTypeRef master;           /* a CMClock or a CMTimebase */
    Float64 rate;
    CMTime anchorTime, anchorMasterTime;
    Timer *timers;
    size_t timerCount, timerCapacity;
};

static void timebaseFinalize(CFTypeRef cf) {
    CMTimebaseRef t = (CMTimebaseRef)cf;
    for (size_t i = 0; i < t->timerCount; i++) {
        if (t->timers[i].timer != NULL)
            CFRelease(t->timers[i].timer);
        if (t->timers[i].source != NULL)
            dispatch_release(t->timers[i].source);
    }
    free(t->timers);
    if (t->master != NULL)
        CFRelease(t->master);
    pthread_mutex_destroy(&t->lock);
}

static const CFRuntimeClass timebaseClass = { 0, "CMTimebase", NULL, NULL, timebaseFinalize, NULL, NULL, NULL, NULL };

CFTypeID CMTimebaseGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ timebaseTypeID = _CFRuntimeRegisterClass(&timebaseClass); });
    return timebaseTypeID;
}

static Boolean isClock(CFTypeRef x) {
    return x != NULL && CFGetTypeID(x) == CMClockGetTypeID();
}

static Boolean isTimebase(CFTypeRef x) {
    return x != NULL && CFGetTypeID(x) == CMTimebaseGetTypeID();
}

CMTime CMSyncGetTime(CMClockOrTimebaseRef x) {
    if (isClock(x))
        return CMClockGetTime((CMClockRef)x);
    if (isTimebase(x))
        return CMTimebaseGetTime((CMTimebaseRef)x);
    return kCMTimeInvalid;
}

/* t + d * rate, exactly when the rate is 1. */
static CMTime advance(CMTime t, CMTime d, Float64 rate) {
    if (!CMTIME_IS_NUMERIC(t) || !CMTIME_IS_NUMERIC(d))
        return kCMTimeInvalid;
    if (rate == 1.0)
        return CMTimeAdd(t, d);
    if (rate == 0.0)
        return t;
    return CMTimeAdd(t, CMTimeMakeWithSeconds(CMTimeGetSeconds(d) * rate, t.timescale > NANOS / 1000 ? t.timescale : NANOS));
}

static CMTime timebaseTimeLocked(CMTimebaseRef t, CMTime masterNow) {
    return advance(t->anchorTime, CMTimeSubtract(masterNow, t->anchorMasterTime), t->rate);
}

CMTime CMTimebaseGetTime(CMTimebaseRef t) {
    if (t == NULL)
        return kCMTimeInvalid;
    CMTime masterNow = CMSyncGetTime(t->master);
    pthread_mutex_lock(&t->lock);
    CMTime now = timebaseTimeLocked(t, masterNow);
    pthread_mutex_unlock(&t->lock);
    return now;
}

CMTime CMTimebaseGetTimeWithTimeScale(CMTimebaseRef t, CMTimeScale scale, CMTimeRoundingMethod method) {
    return CMTimeConvertScale(CMTimebaseGetTime(t), scale, method);
}

static OSStatus createTimebase(CFAllocatorRef allocator, CFTypeRef master, CMTimebaseRef *out) {
    if (out == NULL || master == NULL)
        return kCMTimebaseError_MissingRequiredParameter;
    size_t extra = sizeof(struct OpaqueCMTimebase) - sizeof(CFRuntimeBase);
    CMTimebaseRef t = (CMTimebaseRef)_CFRuntimeCreateInstance(allocator, CMTimebaseGetTypeID(), extra, NULL);
    if (t == NULL)
        return kCMTimebaseError_AllocationFailed;
    pthread_mutex_init(&t->lock, NULL);
    t->master = CFRetain(master);
    t->rate = 0;
    t->anchorTime = kCMTimeZero;
    t->anchorMasterTime = CMSyncGetTime(master);
    *out = t;
    return noErr;
}

OSStatus CMTimebaseCreateWithMasterClock(CFAllocatorRef allocator, CMClockRef master, CMTimebaseRef *out) {
    return isClock(master) ? createTimebase(allocator, master, out) : kCMTimebaseError_MissingRequiredParameter;
}

OSStatus CMTimebaseCreateWithMasterTimebase(CFAllocatorRef allocator, CMTimebaseRef master, CMTimebaseRef *out) {
    return isTimebase(master) ? createTimebase(allocator, master, out) : kCMTimebaseError_MissingRequiredParameter;
}

OSStatus CMTimebaseCreateWithSourceClock(CFAllocatorRef allocator, CMClockRef source, CMTimebaseRef *out) {
    return CMTimebaseCreateWithMasterClock(allocator, source, out);
}

OSStatus CMTimebaseCreateWithSourceTimebase(CFAllocatorRef allocator, CMTimebaseRef source, CMTimebaseRef *out) {
    return CMTimebaseCreateWithMasterTimebase(allocator, source, out);
}

CMClockOrTimebaseRef CMTimebaseGetMaster(CMTimebaseRef t) {
    return t != NULL ? t->master : NULL;
}

CMTimebaseRef CMTimebaseGetMasterTimebase(CMTimebaseRef t) {
    return t != NULL && isTimebase(t->master) ? (CMTimebaseRef)t->master : NULL;
}

CMClockRef CMTimebaseGetMasterClock(CMTimebaseRef t) {
    return t != NULL && isClock(t->master) ? (CMClockRef)t->master : NULL;
}

CMClockRef CMTimebaseGetUltimateMasterClock(CMTimebaseRef t) {
    CFTypeRef m = t != NULL ? t->master : NULL;
    while (isTimebase(m))
        m = ((CMTimebaseRef)m)->master;
    return isClock(m) ? (CMClockRef)m : NULL;
}

static CFTypeRef retained(CFTypeRef x) {
    return x != NULL ? CFRetain(x) : NULL;
}

CMClockOrTimebaseRef CMTimebaseCopyMaster(CMTimebaseRef t) { return retained(CMTimebaseGetMaster(t)); }
CMTimebaseRef CMTimebaseCopyMasterTimebase(CMTimebaseRef t) { return (CMTimebaseRef)retained(CMTimebaseGetMasterTimebase(t)); }
CMClockRef CMTimebaseCopyMasterClock(CMTimebaseRef t) { return (CMClockRef)retained(CMTimebaseGetMasterClock(t)); }
CMClockRef CMTimebaseCopyUltimateMasterClock(CMTimebaseRef t) { return (CMClockRef)retained(CMTimebaseGetUltimateMasterClock(t)); }
CMClockOrTimebaseRef CMTimebaseCopySource(CMTimebaseRef t) { return CMTimebaseCopyMaster(t); }
CMClockRef CMTimebaseCopySourceClock(CMTimebaseRef t) { return CMTimebaseCopyMasterClock(t); }
CMTimebaseRef CMTimebaseCopySourceTimebase(CMTimebaseRef t) { return CMTimebaseCopyMasterTimebase(t); }
CMClockRef CMTimebaseCopyUltimateSourceClock(CMTimebaseRef t) { return CMTimebaseCopyUltimateMasterClock(t); }

Float64 CMTimebaseGetRate(CMTimebaseRef t) {
    if (t == NULL)
        return 0;
    pthread_mutex_lock(&t->lock);
    Float64 r = t->rate;
    pthread_mutex_unlock(&t->lock);
    return r;
}

OSStatus CMTimebaseGetTimeAndRate(CMTimebaseRef t, CMTime *timeOut, Float64 *rateOut) {
    if (t == NULL)
        return kCMTimebaseError_MissingRequiredParameter;
    CMTime masterNow = CMSyncGetTime(t->master);
    pthread_mutex_lock(&t->lock);
    if (timeOut != NULL)
        *timeOut = timebaseTimeLocked(t, masterNow);
    if (rateOut != NULL)
        *rateOut = t->rate;
    pthread_mutex_unlock(&t->lock);
    return noErr;
}

/* The rate against the host's time: its own times its master's. */
Float64 CMTimebaseGetEffectiveRate(CMTimebaseRef t) {
    if (t == NULL)
        return 0;
    Float64 r = CMTimebaseGetRate(t);
    if (isTimebase(t->master))
        r *= CMTimebaseGetEffectiveRate((CMTimebaseRef)t->master);
    return r;
}

/* When a timer's time comes, at the effective rate from now; far in the
   future if never. */
static Boolean secondsUntil(CMTimebaseRef t, CMTime fire, double *seconds) {
    if (!CMTIME_IS_NUMERIC(fire))
        return false;
    Float64 rate = CMTimebaseGetEffectiveRate(t);
    if (rate == 0)
        return false;
    double d = CMTimeGetSeconds(CMTimeSubtract(fire, CMTimebaseGetTime(t))) / rate;
    *seconds = d > 0 ? d : 0;
    return true;
}

static void schedule(CMTimebaseRef t, Timer *timer) {
    double s = 0;
    Boolean due = secondsUntil(t, timer->fireTime, &s);
    if (timer->timer != NULL)
        CFRunLoopTimerSetNextFireDate(timer->timer, due ? CFAbsoluteTimeGetCurrent() + s : kCMTimebaseFarFutureCFAbsoluteTime);
    if (timer->source != NULL)
        dispatch_source_set_timer(timer->source, due ? dispatch_time(DISPATCH_TIME_NOW, (int64_t)(s * NANOS)) : DISPATCH_TIME_FOREVER,
                                  DISPATCH_TIME_FOREVER, 0);
}

static void rescheduleAll(CMTimebaseRef t) {
    pthread_mutex_lock(&t->lock);
    size_t n = t->timerCount;
    Timer *copy = malloc((n > 0 ? n : 1) * sizeof(Timer));
    memcpy(copy, t->timers, n * sizeof(Timer));
    pthread_mutex_unlock(&t->lock);
    for (size_t i = 0; i < n; i++)
        schedule(t, &copy[i]);
    free(copy);
}

static void post(CMTimebaseRef t, CFStringRef name, CMTime eventTime) {
    CFDictionaryRef time = CMTimeCopyAsDictionary(eventTime, NULL);
    CFDictionaryRef payload = CFDictionaryCreate(NULL, (const void **)&kCMTimebaseNotificationKey_EventTime,
                                                 (const void **)&time, 1, &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    CMNotificationCenterPostNotification(CMNotificationCenterGetDefaultLocalCenter(), name, t, payload);
    CFRelease(payload);
    CFRelease(time);
}

OSStatus CMTimebaseSetRateAndAnchorTime(CMTimebaseRef t, Float64 rate, CMTime time, CMTime masterTime) {
    if (t == NULL)
        return kCMTimebaseError_MissingRequiredParameter;
    if (!CMTIME_IS_NUMERIC(time) || !CMTIME_IS_NUMERIC(masterTime) || isnan(rate))
        return kCMTimebaseError_InvalidParameter;
    pthread_mutex_lock(&t->lock);
    Float64 was = t->rate;
    t->rate = rate;
    t->anchorTime = time;
    t->anchorMasterTime = masterTime;
    pthread_mutex_unlock(&t->lock);
    rescheduleAll(t);
    post(t, kCMTimebaseNotification_TimeJumped, time);
    if (was != rate)
        post(t, kCMTimebaseNotification_EffectiveRateChanged, CMTimebaseGetTime(t));
    return noErr;
}

OSStatus CMTimebaseSetAnchorTime(CMTimebaseRef t, CMTime time, CMTime masterTime) {
    return CMTimebaseSetRateAndAnchorTime(t, CMTimebaseGetRate(t), time, masterTime);
}

OSStatus CMTimebaseSetTime(CMTimebaseRef t, CMTime time) {
    if (t == NULL)
        return kCMTimebaseError_MissingRequiredParameter;
    return CMTimebaseSetAnchorTime(t, time, CMSyncGetTime(t->master));
}

/* A new rate from now on: the time goes on from where it is. */
OSStatus CMTimebaseSetRate(CMTimebaseRef t, Float64 rate) {
    if (t == NULL)
        return kCMTimebaseError_MissingRequiredParameter;
    if (isnan(rate))
        return kCMTimebaseError_InvalidParameter;
    CMTime masterNow = CMSyncGetTime(t->master);
    pthread_mutex_lock(&t->lock);
    Float64 was = t->rate;
    t->anchorTime = timebaseTimeLocked(t, masterNow);
    t->anchorMasterTime = masterNow;
    t->rate = rate;
    CMTime now = t->anchorTime;
    pthread_mutex_unlock(&t->lock);
    rescheduleAll(t);
    if (was != rate)
        post(t, kCMTimebaseNotification_EffectiveRateChanged, now);
    return noErr;
}

static OSStatus setMaster(CMTimebaseRef t, CFTypeRef master) {
    if (t == NULL || master == NULL)
        return kCMTimebaseError_MissingRequiredParameter;
    for (CFTypeRef m = master; isTimebase(m); m = ((CMTimebaseRef)m)->master)
        if (m == t)
            return kCMTimebaseError_InvalidParameter;      /* a loop */
    CMTime now = CMTimebaseGetTime(t);
    CMTime masterNow = CMSyncGetTime(master);
    pthread_mutex_lock(&t->lock);
    CFTypeRef old = t->master;
    t->master = CFRetain(master);
    t->anchorTime = now;
    t->anchorMasterTime = masterNow;
    pthread_mutex_unlock(&t->lock);
    CFRelease(old);
    rescheduleAll(t);
    return noErr;
}

OSStatus CMTimebaseSetMasterClock(CMTimebaseRef t, CMClockRef c) {
    return isClock(c) ? setMaster(t, c) : kCMTimebaseError_MissingRequiredParameter;
}

OSStatus CMTimebaseSetMasterTimebase(CMTimebaseRef t, CMTimebaseRef m) {
    return isTimebase(m) ? setMaster(t, m) : kCMTimebaseError_MissingRequiredParameter;
}

OSStatus CMTimebaseSetSourceClock(CMTimebaseRef t, CMClockRef c) { return CMTimebaseSetMasterClock(t, c); }
OSStatus CMTimebaseSetSourceTimebase(CMTimebaseRef t, CMTimebaseRef m) { return CMTimebaseSetMasterTimebase(t, m); }

/* ---- Timers ---- */

static Timer *findTimer(CMTimebaseRef t, CFRunLoopTimerRef timer, dispatch_source_t source) {
    for (size_t i = 0; i < t->timerCount; i++)
        if ((timer != NULL && t->timers[i].timer == timer) || (source != NULL && t->timers[i].source == source))
            return &t->timers[i];
    return NULL;
}

static OSStatus addTimer(CMTimebaseRef t, CFRunLoopTimerRef timer, dispatch_source_t source) {
    pthread_mutex_lock(&t->lock);
    if (findTimer(t, timer, source) != NULL) {
        pthread_mutex_unlock(&t->lock);
        return noErr;
    }
    if (t->timerCount == t->timerCapacity) {
        size_t capacity = t->timerCapacity > 0 ? t->timerCapacity * 2 : 4;
        Timer *grown = realloc(t->timers, capacity * sizeof(Timer));
        if (grown == NULL) {
            pthread_mutex_unlock(&t->lock);
            return kCMTimebaseError_AllocationFailed;
        }
        t->timers = grown;
        t->timerCapacity = capacity;
    }
    if (timer != NULL)
        CFRetain(timer);
    if (source != NULL)
        dispatch_retain(source);
    t->timers[t->timerCount++] = (Timer){ timer, source, kCMTimeInvalid };
    pthread_mutex_unlock(&t->lock);
    return noErr;
}

OSStatus CMTimebaseAddTimer(CMTimebaseRef t, CFRunLoopTimerRef timer, CFRunLoopRef runloop) {
    if (t == NULL || timer == NULL || runloop == NULL)
        return kCMTimebaseError_MissingRequiredParameter;
    /* A timebase's timer repeats only when its timebase says so. */
    if (CFRunLoopTimerGetInterval(timer) < kCMTimebaseVeryLongCFTimeInterval)
        return kCMTimebaseError_TimerIntervalTooShort;
    OSStatus err = addTimer(t, timer, NULL);
    if (err == noErr) {
        CFRunLoopTimerSetNextFireDate(timer, kCMTimebaseFarFutureCFAbsoluteTime);
        CFRunLoopAddTimer(runloop, timer, kCFRunLoopCommonModes);
    }
    return err;
}

OSStatus CMTimebaseAddTimerDispatchSource(CMTimebaseRef t, dispatch_source_t source) {
    if (t == NULL || source == NULL)
        return kCMTimebaseError_MissingRequiredParameter;
    OSStatus err = addTimer(t, NULL, source);
    if (err == noErr)
        dispatch_source_set_timer(source, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 0);
    return err;
}

static OSStatus removeTimer(CMTimebaseRef t, CFRunLoopTimerRef timer, dispatch_source_t source) {
    if (t == NULL || (timer == NULL && source == NULL))
        return kCMTimebaseError_MissingRequiredParameter;
    pthread_mutex_lock(&t->lock);
    Timer *found = findTimer(t, timer, source);
    Timer removed = found != NULL ? *found : (Timer){ NULL, NULL, kCMTimeInvalid };
    if (found != NULL)
        *found = t->timers[--t->timerCount];
    pthread_mutex_unlock(&t->lock);
    if (removed.timer != NULL) {
        CFRunLoopTimerSetNextFireDate(removed.timer, kCMTimebaseFarFutureCFAbsoluteTime);
        CFRelease(removed.timer);
    }
    if (removed.source != NULL) {
        dispatch_source_set_timer(removed.source, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 0);
        dispatch_release(removed.source);
    }
    return found != NULL ? noErr : kCMTimebaseError_InvalidParameter;
}

OSStatus CMTimebaseRemoveTimer(CMTimebaseRef t, CFRunLoopTimerRef timer) {
    return removeTimer(t, timer, NULL);
}

OSStatus CMTimebaseRemoveTimerDispatchSource(CMTimebaseRef t, dispatch_source_t source) {
    return removeTimer(t, NULL, source);
}

static OSStatus setFireTime(CMTimebaseRef t, CFRunLoopTimerRef timer, dispatch_source_t source, CMTime fire,
                            Boolean immediately) {
    if (t == NULL || (timer == NULL && source == NULL))
        return kCMTimebaseError_MissingRequiredParameter;
    pthread_mutex_lock(&t->lock);
    Timer *found = findTimer(t, timer, source);
    if (found != NULL)
        found->fireTime = immediately ? kCMTimeInvalid : fire;
    Timer copy = found != NULL ? *found : (Timer){ NULL, NULL, kCMTimeInvalid };
    pthread_mutex_unlock(&t->lock);
    if (found == NULL)
        return kCMTimebaseError_InvalidParameter;
    if (immediately) {
        if (copy.timer != NULL)
            CFRunLoopTimerSetNextFireDate(copy.timer, CFAbsoluteTimeGetCurrent());
        if (copy.source != NULL)
            dispatch_source_set_timer(copy.source, DISPATCH_TIME_NOW, DISPATCH_TIME_FOREVER, 0);
    } else
        schedule(t, &copy);
    return noErr;
}

OSStatus CMTimebaseSetTimerNextFireTime(CMTimebaseRef t, CFRunLoopTimerRef timer, CMTime fire, uint32_t flags) {
    return setFireTime(t, timer, NULL, fire, false);
}

OSStatus CMTimebaseSetTimerToFireImmediately(CMTimebaseRef t, CFRunLoopTimerRef timer) {
    return setFireTime(t, timer, NULL, kCMTimeInvalid, true);
}

OSStatus CMTimebaseSetTimerDispatchSourceNextFireTime(CMTimebaseRef t, dispatch_source_t source, CMTime fire,
                                                      uint32_t flags) {
    return setFireTime(t, NULL, source, fire, false);
}

OSStatus CMTimebaseSetTimerDispatchSourceToFireImmediately(CMTimebaseRef t, dispatch_source_t source) {
    return setFireTime(t, NULL, source, kCMTimeInvalid, true);
}

/* Notifications are posted as they happen: nothing is pending. */
OSStatus CMTimebaseNotificationBarrier(CMTimebaseRef t) {
    return t != NULL ? noErr : kCMTimebaseError_MissingRequiredParameter;
}

/* ---- Between them ---- */

/* x's time against the host's: (its time, the host's) at an anchor, and its
   rate against the host's. */
static Boolean toHost(CMClockOrTimebaseRef x, CMTime *time, CMTime *host, Float64 *rate) {
    if (isClock(x)) {
        clockAnchor((CMClockRef)x, time, host);
        *rate = 1.0;
        return true;
    }
    if (!isTimebase(x))
        return false;
    CMTimebaseRef t = (CMTimebaseRef)x;
    CMTime masterTime, masterHost;
    Float64 masterRate;
    if (!toHost(t->master, &masterTime, &masterHost, &masterRate))
        return false;
    pthread_mutex_lock(&t->lock);
    CMTime anchor = t->anchorTime, anchorMaster = t->anchorMasterTime;
    Float64 r = t->rate;
    pthread_mutex_unlock(&t->lock);
    /* The host's time at the master's anchor time. */
    *time = anchor;
    *host = advance(masterHost, CMTimeSubtract(anchorMaster, masterTime), masterRate != 0 ? 1.0 / masterRate : 0);
    *rate = r * masterRate;
    return true;
}

CMTime CMSyncConvertTime(CMTime t, CMClockOrTimebaseRef from, CMClockOrTimebaseRef to) {
    if (!CMTIME_IS_NUMERIC(t))
        return t;
    if (from == to)
        return t;
    CMTime fromTime, fromHost, toTime, toHost_;
    Float64 fromRate, toRate;
    if (!toHost(from, &fromTime, &fromHost, &fromRate) || !toHost(to, &toTime, &toHost_, &toRate) || fromRate == 0)
        return kCMTimeInvalid;
    /* from's time to the host's, then the host's to to's. */
    CMTime host = advance(fromHost, CMTimeSubtract(t, fromTime), 1.0 / fromRate);
    return advance(toTime, CMTimeSubtract(host, toHost_), toRate);
}

Float64 CMSyncGetRelativeRate(CMClockOrTimebaseRef of, CMClockOrTimebaseRef relativeTo) {
    Float64 a = 0, b = 0;
    CMTime t1, h1;
    if (!toHost(of, &t1, &h1, &a) || !toHost(relativeTo, &t1, &h1, &b) || b == 0)
        return 0;
    return a / b;
}

OSStatus CMSyncGetRelativeRateAndAnchorTime(CMClockOrTimebaseRef of, CMClockOrTimebaseRef relativeTo, Float64 *rate,
                                            CMTime *ofAnchor, CMTime *relativeToAnchor) {
    if (of == NULL || relativeTo == NULL)
        return kCMSyncError_MissingRequiredParameter;
    CMTime now = CMSyncGetTime(of);
    if (rate != NULL)
        *rate = CMSyncGetRelativeRate(of, relativeTo);
    if (ofAnchor != NULL)
        *ofAnchor = now;
    if (relativeToAnchor != NULL)
        *relativeToAnchor = CMSyncConvertTime(now, of, relativeTo);
    return noErr;
}

static CMClockRef ultimateClock(CMClockOrTimebaseRef x) {
    if (isClock(x))
        return (CMClockRef)x;
    return isTimebase(x) ? CMTimebaseGetUltimateMasterClock((CMTimebaseRef)x) : NULL;
}

Boolean CMSyncMightDrift(CMClockOrTimebaseRef a, CMClockOrTimebaseRef b) {
    return CMClockMightDrift(ultimateClock(a), ultimateClock(b));
}
