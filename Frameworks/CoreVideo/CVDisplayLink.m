/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The display link: a thread of its own that wakes at each refresh of
 * the main display (60 Hz) and calls the output callback with the time
 * now and the time the next frame shows, as host time (mach absolute
 * time) and as video time.
 */

#import <CoreVideo/CVDisplayLink.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#define REFRESH_HZ 60
#define VIDEO_TIMESCALE (REFRESH_HZ * 1000)
#define VIDEO_PERIOD 1000

static mach_timebase_info_data_t timebase(void) {
    static mach_timebase_info_data_t info;
    if (info.denom == 0)
        mach_timebase_info(&info);
    return info;
}

uint64_t CVGetCurrentHostTime(void) {
    return mach_absolute_time();
}

double CVGetHostClockFrequency(void) {
    mach_timebase_info_data_t info = timebase();
    return 1e9 * info.denom / info.numer;
}

uint32_t CVGetHostClockMinimumTimeDelta(void) {
    return 1;
}

static uint64_t nanosToHost(uint64_t ns) {
    mach_timebase_info_data_t info = timebase();
    return ns * info.denom / info.numer;
}

static uint64_t hostToNanos(uint64_t host) {
    mach_timebase_info_data_t info = timebase();
    return host * info.numer / info.denom;
}

@interface CVDisplayLink : NSObject {
@public
    pthread_mutex_t _lock;
    CVDisplayLinkOutputCallback _callback;
    void *_userInfo;
    _Atomic(int) _running;
    pthread_t _thread;
    uint64_t _epoch;		/* host time of video time 0 */
    CGDirectDisplayID _display;
}
@end

@implementation CVDisplayLink

- init {
    pthread_mutex_init(&_lock, NULL);
    _display = CGMainDisplayID();
    return self;
}

- (void)dealloc {
    atomic_store(&_running, 0);
    pthread_mutex_destroy(&_lock);
    [super dealloc];
}

static void fillTimeStamp(CVTimeStamp *ts, CVDisplayLink *link, uint64_t frame) {
    memset(ts, 0, sizeof(*ts));
    ts->version = 0;
    ts->videoTimeScale = VIDEO_TIMESCALE;
    ts->videoTime = (int64_t)frame * VIDEO_PERIOD;
    ts->hostTime = link->_epoch + nanosToHost(frame * 1000000000ull / REFRESH_HZ);
    ts->rateScalar = 1.0;
    ts->videoRefreshPeriod = VIDEO_PERIOD;
    ts->flags = kCVTimeStampVideoTimeValid | kCVTimeStampHostTimeValid | kCVTimeStampVideoRefreshPeriodValid |
        kCVTimeStampRateScalarValid;
}

/* Sleeps to each refresh; the callback gets now (the refresh just past)
   and the next one, when what it draws will show. */
static void *linkThread(void *arg) {
    CVDisplayLink *link = arg;
    pthread_setname_np("CVDisplayLink");
    uint64_t period = 1000000000ull / REFRESH_HZ;
    while (atomic_load(&link->_running)) {
        uint64_t now = hostToNanos(mach_absolute_time() - link->_epoch);
        uint64_t frame = now / period + 1;
        uint64_t wait = frame * period - now;
        struct timespec ts = { (time_t)(wait / 1000000000ull), (long)(wait % 1000000000ull) };
        nanosleep(&ts, NULL);
        if (!atomic_load(&link->_running))
            break;
        CVTimeStamp inNow, inOutput;
        fillTimeStamp(&inNow, link, frame);
        fillTimeStamp(&inOutput, link, frame + 1);
        pthread_mutex_lock(&link->_lock);
        CVDisplayLinkOutputCallback callback = link->_callback;
        void *userInfo = link->_userInfo;
        pthread_mutex_unlock(&link->_lock);
        if (callback != NULL) {
            CVOptionFlags flagsOut = 0;
            @autoreleasepool {
                callback(link, &inNow, &inOutput, 0, &flagsOut, userInfo);
            }
        }
    }
    [link release];
    return NULL;
}

@end

CVReturn CVDisplayLinkCreateWithActiveCGDisplays(CVDisplayLinkRef *result) {
    if (result == NULL)
        return kCVReturnInvalidArgument;
    *result = [[CVDisplayLink alloc] init];
    return *result != nil ? kCVReturnSuccess : kCVReturnAllocationFailed;
}

CVReturn CVDisplayLinkCreateWithCGDisplay(CGDirectDisplayID displayID, CVDisplayLinkRef *result) {
    if (displayID != CGMainDisplayID())
        return kCVReturnInvalidDisplay;
    return CVDisplayLinkCreateWithActiveCGDisplays(result);
}

CVReturn CVDisplayLinkCreateWithCGDisplays(CGDirectDisplayID *displays, CFIndex count, CVDisplayLinkRef *result) {
    for (CFIndex i = 0; i < count; i++)
        if (displays[i] == CGMainDisplayID())
            return CVDisplayLinkCreateWithActiveCGDisplays(result);
    return kCVReturnInvalidDisplay;
}

CVReturn CVDisplayLinkSetOutputCallback(CVDisplayLinkRef self, CVDisplayLinkOutputCallback callback, void *userInfo) {
    if (self == nil)
        return kCVReturnInvalidArgument;
    pthread_mutex_lock(&self->_lock);
    self->_callback = callback;
    self->_userInfo = userInfo;
    pthread_mutex_unlock(&self->_lock);
    return kCVReturnSuccess;
}

CVReturn CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(CVDisplayLinkRef self, CGLContextObj cglContext,
    CGLPixelFormatObj cglPixelFormat) {
    return self != nil ? kCVReturnSuccess : kCVReturnInvalidArgument;
}

CVReturn CVDisplayLinkStart(CVDisplayLinkRef self) {
    if (self == nil)
        return kCVReturnInvalidArgument;
    pthread_mutex_lock(&self->_lock);
    if (atomic_load(&self->_running)) {
        pthread_mutex_unlock(&self->_lock);
        return kCVReturnDisplayLinkAlreadyRunning;
    }
    if (self->_callback == NULL) {
        pthread_mutex_unlock(&self->_lock);
        return kCVReturnDisplayLinkCallbacksNotSet;
    }
    if (self->_epoch == 0)
        self->_epoch = mach_absolute_time();
    atomic_store(&self->_running, 1);
    [self retain];		/* the thread's, released when it ends */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    struct sched_param param = { .sched_priority = 47 };
    pthread_attr_setschedparam(&attr, &param);
    int error = pthread_create(&self->_thread, &attr, linkThread, self);
    pthread_attr_destroy(&attr);
    if (error != 0) {
        atomic_store(&self->_running, 0);
        [self release];
    }
    pthread_mutex_unlock(&self->_lock);
    return error == 0 ? kCVReturnSuccess : kCVReturnError;
}

/* The thread ends at its next wake; a callback running finishes. */
CVReturn CVDisplayLinkStop(CVDisplayLinkRef self) {
    if (self == nil)
        return kCVReturnInvalidArgument;
    int expected = 1;
    if (!atomic_compare_exchange_strong(&self->_running, &expected, 0))
        return kCVReturnDisplayLinkNotRunning;
    return kCVReturnSuccess;
}

Boolean CVDisplayLinkIsRunning(CVDisplayLinkRef self) {
    return self != nil && atomic_load(&self->_running);
}

CVDisplayLinkRef CVDisplayLinkRetain(CVDisplayLinkRef self) {
    return [self retain];
}

void CVDisplayLinkRelease(CVDisplayLinkRef self) {
    [self release];
}

CGDirectDisplayID CVDisplayLinkGetCurrentCGDisplay(CVDisplayLinkRef self) {
    return self != nil ? self->_display : kCGNullDirectDisplay;
}

CVReturn CVDisplayLinkSetCurrentCGDisplay(CVDisplayLinkRef self, CGDirectDisplayID displayID) {
    if (self == nil)
        return kCVReturnInvalidArgument;
    return displayID == CGMainDisplayID() ? kCVReturnSuccess : kCVReturnInvalidDisplay;
}

CVTime CVDisplayLinkGetNominalOutputVideoRefreshPeriod(CVDisplayLinkRef self) {
    return (CVTime){ VIDEO_PERIOD, VIDEO_TIMESCALE, 0 };
}

double CVDisplayLinkGetActualOutputVideoRefreshPeriod(CVDisplayLinkRef self) {
    return 1.0 / REFRESH_HZ;
}

/* A frame is shown at the refresh after it is drawn: no added latency. */
CVTime CVDisplayLinkGetOutputVideoLatency(CVDisplayLinkRef self) {
    return (CVTime){ 0, VIDEO_TIMESCALE, 0 };
}

CVReturn CVDisplayLinkGetCurrentTime(CVDisplayLinkRef self, CVTimeStamp *outTime) {
    if (self == nil || outTime == NULL)
        return kCVReturnInvalidArgument;
    if (!atomic_load(&self->_running))
        return kCVReturnDisplayLinkNotRunning;
    uint64_t frame = hostToNanos(mach_absolute_time() - self->_epoch) / (1000000000ull / REFRESH_HZ);
    fillTimeStamp(outTime, self, frame);
    return kCVReturnSuccess;
}
