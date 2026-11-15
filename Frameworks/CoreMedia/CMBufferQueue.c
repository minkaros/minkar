/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Simple queues (a ring of pointers), buffer queues and memory pools.
 * A buffer queue keeps its buffers in arrival order, or sorted by its
 * compare callback; after every change its triggers are tested and the
 * ones whose condition became true (or whose value changed) are called,
 * outside the queue's lock, on the thread that changed it. A trigger
 * callback may not change the queue.
 */

#include "CMBufferQueue.h"
#include "CMSampleBuffer.h"
#include <CoreFoundation/CFRuntime.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* ---- Simple queues ---- */

struct opaqueCMSimpleQueue {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    const void **elements;
    int32_t capacity, head, count;
};

static CFTypeID simpleTypeID, bufferTypeID, poolTypeID;

static void simpleFinalize(CFTypeRef cf) {
    CMSimpleQueueRef q = (CMSimpleQueueRef)cf;
    free(q->elements);
    pthread_mutex_destroy(&q->lock);
}

static const CFRuntimeClass simpleClass = { 0, "CMSimpleQueue", NULL, NULL, simpleFinalize, NULL, NULL, NULL, NULL };

CFTypeID CMSimpleQueueGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ simpleTypeID = _CFRuntimeRegisterClass(&simpleClass); });
    return simpleTypeID;
}

OSStatus CMSimpleQueueCreate(CFAllocatorRef allocator, int32_t capacity, CMSimpleQueueRef *out) {
    if (out == NULL)
        return kCMSimpleQueueError_RequiredParameterMissing;
    if (capacity <= 0)
        return kCMSimpleQueueError_ParameterOutOfRange;
    size_t extra = sizeof(struct opaqueCMSimpleQueue) - sizeof(CFRuntimeBase);
    CMSimpleQueueRef q = (CMSimpleQueueRef)_CFRuntimeCreateInstance(allocator, CMSimpleQueueGetTypeID(), extra, NULL);
    if (q == NULL || (q->elements = calloc((size_t)capacity, sizeof(void *))) == NULL) {
        if (q != NULL)
            CFRelease(q);
        return kCMSimpleQueueError_AllocationFailed;
    }
    pthread_mutex_init(&q->lock, NULL);
    q->capacity = capacity;
    *out = q;
    return noErr;
}

OSStatus CMSimpleQueueEnqueue(CMSimpleQueueRef q, const void *element) {
    if (q == NULL || element == NULL)
        return kCMSimpleQueueError_RequiredParameterMissing;
    pthread_mutex_lock(&q->lock);
    OSStatus err = noErr;
    if (q->count == q->capacity)
        err = kCMSimpleQueueError_QueueIsFull;
    else
        q->elements[(q->head + q->count++) % q->capacity] = element;
    pthread_mutex_unlock(&q->lock);
    return err;
}

const void *CMSimpleQueueDequeue(CMSimpleQueueRef q) {
    if (q == NULL)
        return NULL;
    pthread_mutex_lock(&q->lock);
    const void *e = NULL;
    if (q->count > 0) {
        e = q->elements[q->head];
        q->head = (q->head + 1) % q->capacity;
        q->count--;
    }
    pthread_mutex_unlock(&q->lock);
    return e;
}

const void *CMSimpleQueueGetHead(CMSimpleQueueRef q) {
    if (q == NULL)
        return NULL;
    pthread_mutex_lock(&q->lock);
    const void *e = q->count > 0 ? q->elements[q->head] : NULL;
    pthread_mutex_unlock(&q->lock);
    return e;
}

OSStatus CMSimpleQueueReset(CMSimpleQueueRef q) {
    if (q == NULL)
        return kCMSimpleQueueError_RequiredParameterMissing;
    pthread_mutex_lock(&q->lock);
    q->head = q->count = 0;
    pthread_mutex_unlock(&q->lock);
    return noErr;
}

int32_t CMSimpleQueueGetCapacity(CMSimpleQueueRef q) {
    return q != NULL ? q->capacity : 0;
}

int32_t CMSimpleQueueGetCount(CMSimpleQueueRef q) {
    if (q == NULL)
        return 0;
    pthread_mutex_lock(&q->lock);
    int32_t n = q->count;
    pthread_mutex_unlock(&q->lock);
    return n;
}

/* ---- Sample buffer callbacks ---- */

static CMTime sbDTS(CMBufferRef b, void *r) { return CMSampleBufferGetDecodeTimeStamp((CMSampleBufferRef)b); }
static CMTime sbPTS(CMBufferRef b, void *r) { return CMSampleBufferGetPresentationTimeStamp((CMSampleBufferRef)b); }
static CMTime sbDuration(CMBufferRef b, void *r) { return CMSampleBufferGetDuration((CMSampleBufferRef)b); }
static Boolean sbReady(CMBufferRef b, void *r) { return CMSampleBufferDataIsReady((CMSampleBufferRef)b); }
static size_t sbSize(CMBufferRef b, void *r) { return CMSampleBufferGetTotalSampleSize((CMSampleBufferRef)b); }
static CMTime sbOutputDTS(CMBufferRef b, void *r) { return CMSampleBufferGetOutputDecodeTimeStamp((CMSampleBufferRef)b); }
static CMTime sbOutputPTS(CMBufferRef b, void *r) { return CMSampleBufferGetOutputPresentationTimeStamp((CMSampleBufferRef)b); }
static CMTime sbOutputDuration(CMBufferRef b, void *r) { return CMSampleBufferGetOutputDuration((CMSampleBufferRef)b); }

static CFComparisonResult byOutputPTS(CMBufferRef a, CMBufferRef b, void *r) {
    int32_t c = CMTimeCompare(sbOutputPTS(a, r), sbOutputPTS(b, r));
    return c < 0 ? kCFCompareLessThan : (c > 0 ? kCFCompareGreaterThan : kCFCompareEqualTo);
}

const CMBufferCallbacks *CMBufferQueueGetCallbacksForUnsortedSampleBuffers(void) {
    static const CMBufferCallbacks callbacks = { 0, NULL, sbDTS, sbPTS, sbDuration, sbReady, NULL, NULL, sbSize };
    return &callbacks;
}

const CMBufferCallbacks *CMBufferQueueGetCallbacksForSampleBuffersSortedByOutputPTS(void) {
    static const CMBufferCallbacks callbacks = { 0, NULL, sbOutputDTS, sbOutputPTS, sbOutputDuration, sbReady,
                                                 byOutputPTS, NULL, sbSize };
    return &callbacks;
}

/* ---- Buffer queues ---- */

struct opaqueCMBufferQueueTriggerToken {
    CMBufferQueueTriggerCallback callback;
    void *refcon;
    CMBufferQueueTriggerCondition condition;
    CMTime time;
    CMItemCount threshold;
    Boolean was;                /* the condition, last tested */
    CMTime lastValue;           /* the PTS the "changes" conditions watch */
    struct opaqueCMBufferQueueTriggerToken *next;
};

struct opaqueCMBufferQueue {
    CFRuntimeBase base;
    pthread_mutex_t lock;
    CMBufferCallbacks callbacks;
    CMItemCount capacity;
    CMBufferRef *buffers;
    CMItemCount count, allocated;
    Boolean endOfData, wasAtEnd, headWasReady;
    CMBufferQueueTriggerToken triggers;
    OSStatus (*validate)(CMBufferQueueRef, CMBufferRef, void *);
    void *validateRefcon;
    pthread_t triggerThread;    /* a trigger is being called on it */
    Boolean inTrigger;
};

static void queueFinalize(CFTypeRef cf) {
    CMBufferQueueRef q = (CMBufferQueueRef)cf;
    for (CMItemCount i = 0; i < q->count; i++)
        CFRelease(q->buffers[i]);
    free(q->buffers);
    for (CMBufferQueueTriggerToken t = q->triggers, next; t != NULL; t = next) {
        next = t->next;
        free(t);
    }
    pthread_mutex_destroy(&q->lock);
}

static const CFRuntimeClass queueClass = { 0, "CMBufferQueue", NULL, NULL, queueFinalize, NULL, NULL, NULL, NULL };

CFTypeID CMBufferQueueGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ bufferTypeID = _CFRuntimeRegisterClass(&queueClass); });
    return bufferTypeID;
}

OSStatus CMBufferQueueCreate(CFAllocatorRef allocator, CMItemCount capacity, const CMBufferCallbacks *callbacks,
                             CMBufferQueueRef *out) {
    if (out == NULL || callbacks == NULL)
        return kCMBufferQueueError_RequiredParameterMissing;
    if (callbacks->version != 0 || capacity < 0)
        return kCMBufferQueueError_InvalidCMBufferCallbacksStruct;
    size_t extra = sizeof(struct opaqueCMBufferQueue) - sizeof(CFRuntimeBase);
    CMBufferQueueRef q = (CMBufferQueueRef)_CFRuntimeCreateInstance(allocator, CMBufferQueueGetTypeID(), extra, NULL);
    if (q == NULL)
        return kCMBufferQueueError_AllocationFailed;
    pthread_mutex_init(&q->lock, NULL);
    q->callbacks = *callbacks;
    q->capacity = capacity;
    *out = q;
    return noErr;
}

static Boolean modifyingFromTrigger(CMBufferQueueRef q) {
    return q->inTrigger && pthread_equal(q->triggerThread, pthread_self());
}

/* Everything below with the lock held. */

static CMTime sumDurations(CMBufferQueueRef q) {
    if (q->callbacks.getDuration == NULL || q->count == 0)
        return q->count == 0 ? kCMTimeZero : kCMTimeInvalid;
    CMTime d = kCMTimeZero;
    for (CMItemCount i = 0; i < q->count; i++)
        d = CMTimeAdd(d, q->callbacks.getDuration(q->buffers[i], q->callbacks.refcon));
    return d;
}

static CMTime extremePTS(CMBufferQueueRef q, int sign) {
    if (q->callbacks.getPresentationTimeStamp == NULL || q->count == 0)
        return kCMTimeInvalid;
    CMTime best = kCMTimeInvalid;
    for (CMItemCount i = 0; i < q->count; i++) {
        CMTime t = q->callbacks.getPresentationTimeStamp(q->buffers[i], q->callbacks.refcon);
        if (CMTIME_IS_NUMERIC(t) && (!CMTIME_IS_NUMERIC(best) || CMTimeCompare(t, best) * sign > 0))
            best = t;
    }
    return best;
}

static Boolean headReady(CMBufferQueueRef q) {
    return q->count > 0 && (q->callbacks.isDataReady == NULL || q->callbacks.isDataReady(q->buffers[0], q->callbacks.refcon));
}

static Boolean condition(CMBufferQueueRef q, CMBufferQueueTriggerToken t) {
    CMTime d;
    switch (t->condition) {
    case kCMBufferQueueTrigger_WhenDurationBecomesLessThan:
        d = sumDurations(q);
        return CMTimeCompare(d, t->time) < 0;
    case kCMBufferQueueTrigger_WhenDurationBecomesLessThanOrEqualTo:
        d = sumDurations(q);
        return CMTimeCompare(d, t->time) <= 0;
    case kCMBufferQueueTrigger_WhenDurationBecomesGreaterThan:
        d = sumDurations(q);
        return CMTimeCompare(d, t->time) > 0;
    case kCMBufferQueueTrigger_WhenDurationBecomesGreaterThanOrEqualTo:
        d = sumDurations(q);
        return CMTimeCompare(d, t->time) >= 0;
    case kCMBufferQueueTrigger_WhenDataBecomesReady:
        return headReady(q);
    case kCMBufferQueueTrigger_WhenEndOfDataReached:
        return q->endOfData && q->count == 0;
    case kCMBufferQueueTrigger_WhenBufferCountBecomesLessThan:
        return q->count < t->threshold;
    case kCMBufferQueueTrigger_WhenBufferCountBecomesGreaterThan:
        return q->count > t->threshold;
    case kCMBufferQueueTrigger_WhenDurationBecomesGreaterThanOrEqualToAndBufferCountBecomesGreaterThan:
        d = sumDurations(q);
        return CMTimeCompare(d, t->time) >= 0 && q->count > t->threshold;
    default:
        return false;
    }
}

static Boolean watchesPTS(CMBufferQueueTriggerCondition c) {
    return c == kCMBufferQueueTrigger_WhenMinPresentationTimeStampChanges ||
           c == kCMBufferQueueTrigger_WhenMaxPresentationTimeStampChanges;
}

static CMTime watchedPTS(CMBufferQueueRef q, CMBufferQueueTriggerToken t) {
    return extremePTS(q, t->condition == kCMBufferQueueTrigger_WhenMinPresentationTimeStampChanges ? -1 : 1);
}

/* The triggers to call after a change (reset: the queue was reset);
   unlocks. */
static void changed(CMBufferQueueRef q, Boolean reset) {
    CMBufferQueueTriggerToken fire[32];
    int n = 0;
    for (CMBufferQueueTriggerToken t = q->triggers; t != NULL && n < 32; t = t->next) {
        if (t->condition == kCMBufferQueueTrigger_WhenReset) {
            if (reset)
                fire[n++] = t;
            continue;
        }
        if (watchesPTS(t->condition)) {
            CMTime now = watchedPTS(q, t);
            if (CMTimeCompare(now, t->lastValue) != 0)
                fire[n++] = t;
            t->lastValue = now;
            continue;
        }
        Boolean is = condition(q, t);
        if (is && !t->was)
            fire[n++] = t;
        t->was = is;
    }
    q->inTrigger = n > 0;
    q->triggerThread = pthread_self();
    pthread_mutex_unlock(&q->lock);
    for (int i = 0; i < n; i++)
        fire[i]->callback(fire[i]->refcon, fire[i]);
    if (n > 0) {
        pthread_mutex_lock(&q->lock);
        q->inTrigger = false;
        pthread_mutex_unlock(&q->lock);
    }
}

OSStatus CMBufferQueueEnqueue(CMBufferQueueRef q, CMBufferRef buf) {
    if (q == NULL || buf == NULL)
        return kCMBufferQueueError_RequiredParameterMissing;
    if (q->validate != NULL) {
        OSStatus err = q->validate(q, buf, q->validateRefcon);
        if (err != noErr)
            return err;
    }
    pthread_mutex_lock(&q->lock);
    if (modifyingFromTrigger(q)) {
        pthread_mutex_unlock(&q->lock);
        return kCMBufferQueueError_CannotModifyQueueFromTriggerCallback;
    }
    if (q->endOfData) {
        pthread_mutex_unlock(&q->lock);
        return kCMBufferQueueError_EnqueueAfterEndOfData;
    }
    if (q->capacity > 0 && q->count >= q->capacity) {
        pthread_mutex_unlock(&q->lock);
        return kCMBufferQueueError_QueueIsFull;
    }
    if (q->count == q->allocated) {
        CMItemCount n = q->allocated > 0 ? q->allocated * 2 : 16;
        CMBufferRef *grown = realloc(q->buffers, (size_t)n * sizeof(CMBufferRef));
        if (grown == NULL) {
            pthread_mutex_unlock(&q->lock);
            return kCMBufferQueueError_AllocationFailed;
        }
        q->buffers = grown;
        q->allocated = n;
    }
    /* After every buffer that is not later than it. */
    CMItemCount at = q->count;
    if (q->callbacks.compare != NULL)
        while (at > 0 && q->callbacks.compare(q->buffers[at - 1], buf, q->callbacks.refcon) == kCFCompareGreaterThan)
            at--;
    memmove(q->buffers + at + 1, q->buffers + at, (size_t)(q->count - at) * sizeof(CMBufferRef));
    q->buffers[at] = CFRetain(buf);
    q->count++;
    changed(q, false);
    return noErr;
}

static CMBufferRef dequeue(CMBufferQueueRef q, Boolean onlyIfReady) {
    if (q == NULL)
        return NULL;
    pthread_mutex_lock(&q->lock);
    if (q->count == 0 || modifyingFromTrigger(q) || (onlyIfReady && !headReady(q))) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }
    CMBufferRef b = q->buffers[0];
    memmove(q->buffers, q->buffers + 1, (size_t)(q->count - 1) * sizeof(CMBufferRef));
    q->count--;
    changed(q, false);
    return b;
}

CMBufferRef CMBufferQueueDequeueAndRetain(CMBufferQueueRef q) {
    return dequeue(q, false);
}

CMBufferRef CMBufferQueueDequeueIfDataReadyAndRetain(CMBufferQueueRef q) {
    return dequeue(q, true);
}

CMBufferRef CMBufferQueueGetHead(CMBufferQueueRef q) {
    if (q == NULL)
        return NULL;
    pthread_mutex_lock(&q->lock);
    CMBufferRef b = q->count > 0 ? q->buffers[0] : NULL;
    pthread_mutex_unlock(&q->lock);
    return b;
}

CMBufferRef CMBufferQueueCopyHead(CMBufferQueueRef q) {
    if (q == NULL)
        return NULL;
    pthread_mutex_lock(&q->lock);
    CMBufferRef b = q->count > 0 ? CFRetain(q->buffers[0]) : NULL;
    pthread_mutex_unlock(&q->lock);
    return b;
}

Boolean CMBufferQueueIsEmpty(CMBufferQueueRef q) {
    return CMBufferQueueGetBufferCount(q) == 0;
}

OSStatus CMBufferQueueMarkEndOfData(CMBufferQueueRef q) {
    if (q == NULL)
        return kCMBufferQueueError_RequiredParameterMissing;
    pthread_mutex_lock(&q->lock);
    if (modifyingFromTrigger(q)) {
        pthread_mutex_unlock(&q->lock);
        return kCMBufferQueueError_CannotModifyQueueFromTriggerCallback;
    }
    q->endOfData = true;
    changed(q, false);
    return noErr;
}

Boolean CMBufferQueueContainsEndOfData(CMBufferQueueRef q) {
    if (q == NULL)
        return false;
    pthread_mutex_lock(&q->lock);
    Boolean e = q->endOfData;
    pthread_mutex_unlock(&q->lock);
    return e;
}

Boolean CMBufferQueueIsAtEndOfData(CMBufferQueueRef q) {
    if (q == NULL)
        return false;
    pthread_mutex_lock(&q->lock);
    Boolean e = q->endOfData && q->count == 0;
    pthread_mutex_unlock(&q->lock);
    return e;
}

OSStatus CMBufferQueueResetWithCallback(CMBufferQueueRef q, void (*callback)(CMBufferRef, void *), void *refcon) {
    if (q == NULL)
        return kCMBufferQueueError_RequiredParameterMissing;
    pthread_mutex_lock(&q->lock);
    if (modifyingFromTrigger(q)) {
        pthread_mutex_unlock(&q->lock);
        return kCMBufferQueueError_CannotModifyQueueFromTriggerCallback;
    }
    CMItemCount n = q->count;
    CMBufferRef *gone = malloc((size_t)(n > 0 ? n : 1) * sizeof(CMBufferRef));
    memcpy(gone, q->buffers, (size_t)n * sizeof(CMBufferRef));
    q->count = 0;
    q->endOfData = false;
    changed(q, true);
    for (CMItemCount i = 0; i < n; i++) {
        if (callback != NULL)
            callback(gone[i], refcon);
        CFRelease(gone[i]);
    }
    free(gone);
    return noErr;
}

OSStatus CMBufferQueueReset(CMBufferQueueRef q) {
    return CMBufferQueueResetWithCallback(q, NULL, NULL);
}

CMItemCount CMBufferQueueGetBufferCount(CMBufferQueueRef q) {
    if (q == NULL)
        return 0;
    pthread_mutex_lock(&q->lock);
    CMItemCount n = q->count;
    pthread_mutex_unlock(&q->lock);
    return n;
}

CMTime CMBufferQueueGetDuration(CMBufferQueueRef q) {
    if (q == NULL)
        return kCMTimeInvalid;
    pthread_mutex_lock(&q->lock);
    CMTime d = sumDurations(q);
    pthread_mutex_unlock(&q->lock);
    return d;
}

static CMTime headTime(CMBufferQueueRef q, CMBufferGetTimeCallback get) {
    if (q == NULL || get == NULL)
        return kCMTimeInvalid;
    pthread_mutex_lock(&q->lock);
    CMTime t = q->count > 0 ? get(q->buffers[0], q->callbacks.refcon) : kCMTimeInvalid;
    pthread_mutex_unlock(&q->lock);
    return t;
}

CMTime CMBufferQueueGetFirstDecodeTimeStamp(CMBufferQueueRef q) {
    return q != NULL ? headTime(q, q->callbacks.getDecodeTimeStamp) : kCMTimeInvalid;
}

CMTime CMBufferQueueGetFirstPresentationTimeStamp(CMBufferQueueRef q) {
    return q != NULL ? headTime(q, q->callbacks.getPresentationTimeStamp) : kCMTimeInvalid;
}

CMTime CMBufferQueueGetMinDecodeTimeStamp(CMBufferQueueRef q) {
    if (q == NULL || q->callbacks.getDecodeTimeStamp == NULL)
        return kCMTimeInvalid;
    pthread_mutex_lock(&q->lock);
    CMTime best = kCMTimeInvalid;
    for (CMItemCount i = 0; i < q->count; i++) {
        CMTime t = q->callbacks.getDecodeTimeStamp(q->buffers[i], q->callbacks.refcon);
        if (CMTIME_IS_NUMERIC(t) && (!CMTIME_IS_NUMERIC(best) || CMTimeCompare(t, best) < 0))
            best = t;
    }
    pthread_mutex_unlock(&q->lock);
    return best;
}

CMTime CMBufferQueueGetMinPresentationTimeStamp(CMBufferQueueRef q) {
    if (q == NULL)
        return kCMTimeInvalid;
    pthread_mutex_lock(&q->lock);
    CMTime t = extremePTS(q, -1);
    pthread_mutex_unlock(&q->lock);
    return t;
}

CMTime CMBufferQueueGetMaxPresentationTimeStamp(CMBufferQueueRef q) {
    if (q == NULL)
        return kCMTimeInvalid;
    pthread_mutex_lock(&q->lock);
    CMTime t = extremePTS(q, 1);
    pthread_mutex_unlock(&q->lock);
    return t;
}

/* The latest end: a buffer's presentation time plus its duration. */
CMTime CMBufferQueueGetEndPresentationTimeStamp(CMBufferQueueRef q) {
    if (q == NULL || q->callbacks.getPresentationTimeStamp == NULL || q->callbacks.getDuration == NULL)
        return kCMTimeInvalid;
    pthread_mutex_lock(&q->lock);
    CMTime best = kCMTimeInvalid;
    for (CMItemCount i = 0; i < q->count; i++) {
        CMTime end = CMTimeAdd(q->callbacks.getPresentationTimeStamp(q->buffers[i], q->callbacks.refcon),
                               q->callbacks.getDuration(q->buffers[i], q->callbacks.refcon));
        if (CMTIME_IS_NUMERIC(end) && (!CMTIME_IS_NUMERIC(best) || CMTimeCompare(end, best) > 0))
            best = end;
    }
    pthread_mutex_unlock(&q->lock);
    return best;
}

size_t CMBufferQueueGetTotalSize(CMBufferQueueRef q) {
    if (q == NULL || q->callbacks.getSize == NULL)
        return 0;
    pthread_mutex_lock(&q->lock);
    size_t total = 0;
    for (CMItemCount i = 0; i < q->count; i++)
        total += q->callbacks.getSize(q->buffers[i], q->callbacks.refcon);
    pthread_mutex_unlock(&q->lock);
    return total;
}

OSStatus CMBufferQueueCallForEachBuffer(CMBufferQueueRef q, OSStatus (*callback)(CMBufferRef, void *), void *refcon) {
    if (q == NULL || callback == NULL)
        return kCMBufferQueueError_RequiredParameterMissing;
    pthread_mutex_lock(&q->lock);
    CMItemCount n = q->count;
    CMBufferRef *all = malloc((size_t)(n > 0 ? n : 1) * sizeof(CMBufferRef));
    for (CMItemCount i = 0; i < n; i++)
        all[i] = CFRetain(q->buffers[i]);
    pthread_mutex_unlock(&q->lock);
    OSStatus err = noErr;
    for (CMItemCount i = 0; i < n; i++) {
        if (err == noErr)
            err = callback(all[i], refcon);
        CFRelease(all[i]);
    }
    free(all);
    return err;
}

OSStatus CMBufferQueueSetValidationCallback(CMBufferQueueRef q, OSStatus (*callback)(CMBufferQueueRef, CMBufferRef, void *),
                                            void *refcon) {
    if (q == NULL || callback == NULL)
        return kCMBufferQueueError_RequiredParameterMissing;
    pthread_mutex_lock(&q->lock);
    q->validate = callback;
    q->validateRefcon = refcon;
    pthread_mutex_unlock(&q->lock);
    return noErr;
}

static OSStatus install(CMBufferQueueRef q, CMBufferQueueTriggerCallback callback, void *refcon,
                        CMBufferQueueTriggerCondition cond, CMTime time, CMItemCount threshold,
                        CMBufferQueueTriggerToken *out) {
    if (q == NULL || callback == NULL)
        return kCMBufferQueueError_RequiredParameterMissing;
    if (cond < kCMBufferQueueTrigger_WhenDurationBecomesLessThan ||
        cond > kCMBufferQueueTrigger_WhenDurationBecomesGreaterThanOrEqualToAndBufferCountBecomesGreaterThan)
        return kCMBufferQueueError_InvalidTriggerCondition;
    if (cond <= kCMBufferQueueTrigger_WhenDurationBecomesGreaterThanOrEqualTo && !CMTIME_IS_NUMERIC(time))
        return kCMBufferQueueError_BadTriggerDuration;
    CMBufferQueueTriggerToken t = calloc(1, sizeof(*t));
    if (t == NULL)
        return kCMBufferQueueError_AllocationFailed;
    t->callback = callback;
    t->refcon = refcon;
    t->condition = cond;
    t->time = time;
    t->threshold = threshold;
    pthread_mutex_lock(&q->lock);
    t->was = watchesPTS(cond) || cond == kCMBufferQueueTrigger_WhenReset ? false : condition(q, t);
    t->lastValue = watchesPTS(cond) ? watchedPTS(q, t) : kCMTimeInvalid;
    t->next = q->triggers;
    q->triggers = t;
    pthread_mutex_unlock(&q->lock);
    if (out != NULL)
        *out = t;
    return noErr;
}

OSStatus CMBufferQueueInstallTrigger(CMBufferQueueRef q, CMBufferQueueTriggerCallback callback, void *refcon,
                                     CMBufferQueueTriggerCondition cond, CMTime time, CMBufferQueueTriggerToken *out) {
    if (cond == kCMBufferQueueTrigger_WhenBufferCountBecomesLessThan ||
        cond == kCMBufferQueueTrigger_WhenBufferCountBecomesGreaterThan ||
        cond == kCMBufferQueueTrigger_WhenDurationBecomesGreaterThanOrEqualToAndBufferCountBecomesGreaterThan)
        return kCMBufferQueueError_InvalidTriggerCondition;
    return install(q, callback, refcon, cond, time, 0, out);
}

OSStatus CMBufferQueueInstallTriggerWithIntegerThreshold(CMBufferQueueRef q, CMBufferQueueTriggerCallback callback,
                                                         void *refcon, CMBufferQueueTriggerCondition cond,
                                                         CMItemCount threshold, CMBufferQueueTriggerToken *out) {
    if (cond != kCMBufferQueueTrigger_WhenBufferCountBecomesLessThan &&
        cond != kCMBufferQueueTrigger_WhenBufferCountBecomesGreaterThan)
        return kCMBufferQueueError_InvalidTriggerCondition;
    return install(q, callback, refcon, cond, kCMTimeInvalid, threshold, out);
}

OSStatus CMBufferQueueRemoveTrigger(CMBufferQueueRef q, CMBufferQueueTriggerToken token) {
    if (q == NULL || token == NULL)
        return kCMBufferQueueError_RequiredParameterMissing;
    pthread_mutex_lock(&q->lock);
    for (CMBufferQueueTriggerToken *p = &q->triggers; *p != NULL; p = &(*p)->next)
        if (*p == token) {
            *p = token->next;
            pthread_mutex_unlock(&q->lock);
            free(token);
            return noErr;
        }
    pthread_mutex_unlock(&q->lock);
    return kCMBufferQueueError_InvalidTriggerToken;
}

Boolean CMBufferQueueTestTrigger(CMBufferQueueRef q, CMBufferQueueTriggerToken token) {
    if (q == NULL || token == NULL)
        return false;
    pthread_mutex_lock(&q->lock);
    Boolean is = watchesPTS(token->condition) || token->condition == kCMBufferQueueTrigger_WhenReset
                     ? false
                     : condition(q, token);
    pthread_mutex_unlock(&q->lock);
    return is;
}

/* ---- Memory pools ---- */

const CFStringRef kCMMemoryPoolOption_AgeOutPeriod = CFSTR("AgeOutPeriod");

/* A block keeps its size in front of it; freed blocks wait in their
   size's list for the next allocation of that size. The lists are shared
   by the pool and its allocator, which may outlive it: once the pool is
   invalidated or gone, freed blocks go back to the system. */
typedef struct FreeBlock {
    struct FreeBlock *next;
} FreeBlock;

typedef struct {
    size_t size;
    FreeBlock *free;
} Bucket;

typedef struct {
    int refs;
    pthread_mutex_t lock;
    Bucket buckets[32];
    int bucketCount;
    Boolean invalid;
} PoolState;

struct OpaqueCMMemoryPool {
    CFRuntimeBase base;
    PoolState *state;
    CFAllocatorRef allocator;
};

#define HEADER 16

static void flushLocked(PoolState *p) {
    for (int i = 0; i < p->bucketCount; i++)
        for (FreeBlock *b = p->buckets[i].free, *next; b != NULL; b = next) {
            next = b->next;
            free((char *)b - HEADER);
        }
    p->bucketCount = 0;
}

static const void *retainState(const void *info) {
    __atomic_add_fetch(&((PoolState *)info)->refs, 1, __ATOMIC_ACQ_REL);
    return info;
}

static void releaseState(const void *info) {
    PoolState *p = (PoolState *)info;
    if (__atomic_sub_fetch(&p->refs, 1, __ATOMIC_ACQ_REL) != 0)
        return;
    flushLocked(p);
    pthread_mutex_destroy(&p->lock);
    free(p);
}

static void *poolAllocate(CFIndex size, CFOptionFlags hint, void *info) {
    PoolState *p = info;
    pthread_mutex_lock(&p->lock);
    for (int i = 0; i < p->bucketCount && !p->invalid; i++)
        if (p->buckets[i].size == (size_t)size && p->buckets[i].free != NULL) {
            FreeBlock *b = p->buckets[i].free;
            p->buckets[i].free = b->next;
            pthread_mutex_unlock(&p->lock);
            return b;
        }
    pthread_mutex_unlock(&p->lock);
    char *raw = malloc(HEADER + (size_t)(size > (CFIndex)sizeof(FreeBlock) ? size : (CFIndex)sizeof(FreeBlock)));
    if (raw == NULL)
        return NULL;
    *(size_t *)raw = (size_t)size;
    return raw + HEADER;
}

static void poolDeallocate(void *ptr, void *info) {
    PoolState *p = info;
    size_t size = *(size_t *)((char *)ptr - HEADER);
    pthread_mutex_lock(&p->lock);
    if (!p->invalid) {
        int i = 0;
        while (i < p->bucketCount && p->buckets[i].size != size)
            i++;
        if (i == p->bucketCount && i < 32)
            p->buckets[p->bucketCount++] = (Bucket){ size, NULL };
        if (i < p->bucketCount) {
            FreeBlock *b = ptr;
            b->next = p->buckets[i].free;
            p->buckets[i].free = b;
            pthread_mutex_unlock(&p->lock);
            return;
        }
    }
    pthread_mutex_unlock(&p->lock);
    free((char *)ptr - HEADER);
}

static void *poolReallocate(void *ptr, CFIndex newSize, CFOptionFlags hint, void *info) {
    void *grown = poolAllocate(newSize, hint, info);
    if (grown == NULL)
        return NULL;
    size_t old = *(size_t *)((char *)ptr - HEADER);
    memcpy(grown, ptr, old < (size_t)newSize ? old : (size_t)newSize);
    poolDeallocate(ptr, info);
    return grown;
}

static void invalidateState(PoolState *p) {
    pthread_mutex_lock(&p->lock);
    flushLocked(p);
    p->invalid = true;
    pthread_mutex_unlock(&p->lock);
}

static void poolFinalize(CFTypeRef cf) {
    CMMemoryPoolRef pool = (CMMemoryPoolRef)cf;
    invalidateState(pool->state);
    CFRelease(pool->allocator);
    releaseState(pool->state);
}

static const CFRuntimeClass poolClass = { 0, "CMMemoryPool", NULL, NULL, poolFinalize, NULL, NULL, NULL, NULL };

CFTypeID CMMemoryPoolGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{ poolTypeID = _CFRuntimeRegisterClass(&poolClass); });
    return poolTypeID;
}

CMMemoryPoolRef CMMemoryPoolCreate(CFDictionaryRef options) {
    size_t extra = sizeof(struct OpaqueCMMemoryPool) - sizeof(CFRuntimeBase);
    PoolState *state = calloc(1, sizeof(PoolState));
    CMMemoryPoolRef pool = state != NULL ? (CMMemoryPoolRef)_CFRuntimeCreateInstance(NULL, CMMemoryPoolGetTypeID(), extra, NULL)
                                         : NULL;
    if (pool == NULL) {
        free(state);
        return NULL;
    }
    state->refs = 1;
    pthread_mutex_init(&state->lock, NULL);
    pool->state = state;
    CFAllocatorContext context = { 0, state, retainState, releaseState, NULL, poolAllocate, poolReallocate,
                                   poolDeallocate, NULL };
    pool->allocator = CFAllocatorCreate(NULL, &context);
    return pool;
}

CFAllocatorRef CMMemoryPoolGetAllocator(CMMemoryPoolRef pool) {
    return pool != NULL ? pool->allocator : NULL;
}

void CMMemoryPoolFlush(CMMemoryPoolRef pool) {
    if (pool == NULL)
        return;
    pthread_mutex_lock(&pool->state->lock);
    flushLocked(pool->state);
    pthread_mutex_unlock(&pool->state->lock);
}

/* Blocks freed from now on go back to the system. */
void CMMemoryPoolInvalidate(CMMemoryPoolRef pool) {
    if (pool != NULL)
        invalidateState(pool->state);
}
