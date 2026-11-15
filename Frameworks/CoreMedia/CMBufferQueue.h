/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Queues: a simple queue of pointers, of fixed capacity, and a buffer
 * queue of CF objects (sample buffers, usually) in arrival or
 * presentation order, which knows its buffers' times, duration and size,
 * marks the end of data, and calls triggers when they pass a threshold.
 * A memory pool keeps freed blocks for the next allocations of their
 * size.
 */

#ifndef __CMBUFFERQUEUE__
#define __CMBUFFERQUEUE__

#include <CoreMedia/CMBase.h>
#include <CoreMedia/CMTime.h>

/* ---- Simple queues ---- */

enum {
    kCMSimpleQueueError_AllocationFailed = -12770,
    kCMSimpleQueueError_RequiredParameterMissing = -12771,
    kCMSimpleQueueError_ParameterOutOfRange = -12772,
    kCMSimpleQueueError_QueueIsFull = -12773,
};

typedef struct opaqueCMSimpleQueue *CMSimpleQueueRef;

CM_EXPORT CFTypeID CMSimpleQueueGetTypeID(void);
CM_EXPORT OSStatus CMSimpleQueueCreate(CFAllocatorRef allocator, int32_t capacity, CMSimpleQueueRef *queueOut);
CM_EXPORT OSStatus CMSimpleQueueEnqueue(CMSimpleQueueRef queue, const void *element);
CM_EXPORT const void *CMSimpleQueueDequeue(CMSimpleQueueRef queue);
CM_EXPORT const void *CMSimpleQueueGetHead(CMSimpleQueueRef queue);
CM_EXPORT OSStatus CMSimpleQueueReset(CMSimpleQueueRef queue);
CM_EXPORT int32_t CMSimpleQueueGetCapacity(CMSimpleQueueRef queue);
CM_EXPORT int32_t CMSimpleQueueGetCount(CMSimpleQueueRef queue);
#define CMSimpleQueueGetFullness(queue) \
    (CMSimpleQueueGetCapacity(queue) ? (CMSimpleQueueGetCount(queue) / (Float32)CMSimpleQueueGetCapacity(queue)) : 0.0f)

/* ---- Buffer queues ---- */

enum {
    kCMBufferQueueError_AllocationFailed = -12760,
    kCMBufferQueueError_RequiredParameterMissing = -12761,
    kCMBufferQueueError_InvalidCMBufferCallbacksStruct = -12762,
    kCMBufferQueueError_EnqueueAfterEndOfData = -12763,
    kCMBufferQueueError_QueueIsFull = -12764,
    kCMBufferQueueError_BadTriggerDuration = -12765,
    kCMBufferQueueError_CannotModifyQueueFromTriggerCallback = -12766,
    kCMBufferQueueError_InvalidTriggerCondition = -12767,
    kCMBufferQueueError_InvalidTriggerToken = -12768,
    kCMBufferQueueError_InvalidBuffer = -12769,
};

typedef struct opaqueCMBufferQueue *CMBufferQueueRef;
typedef CFTypeRef CMBufferRef;

typedef CMTime (*CMBufferGetTimeCallback)(CMBufferRef buf, void *refcon);
typedef Boolean (*CMBufferGetBooleanCallback)(CMBufferRef buf, void *refcon);
typedef CFComparisonResult (*CMBufferCompareCallback)(CMBufferRef buf1, CMBufferRef buf2, void *refcon);
typedef size_t (*CMBufferGetSizeCallback)(CMBufferRef buf, void *refcon);

#pragma pack(push, 4)
typedef struct {
    uint32_t version;
    void *refcon;
    CMBufferGetTimeCallback getDecodeTimeStamp;
    CMBufferGetTimeCallback getPresentationTimeStamp;
    CMBufferGetTimeCallback getDuration;
    CMBufferGetBooleanCallback isDataReady;
    CMBufferCompareCallback compare;
    CFStringRef dataBecameReadyNotification;
    CMBufferGetSizeCallback getSize;
} CMBufferCallbacks;
#pragma pack(pop)

CM_EXPORT const CMBufferCallbacks *CMBufferQueueGetCallbacksForUnsortedSampleBuffers(void);
CM_EXPORT const CMBufferCallbacks *CMBufferQueueGetCallbacksForSampleBuffersSortedByOutputPTS(void);

CM_EXPORT CFTypeID CMBufferQueueGetTypeID(void);
CM_EXPORT OSStatus CMBufferQueueCreate(CFAllocatorRef allocator, CMItemCount capacity, const CMBufferCallbacks *callbacks,
                                       CMBufferQueueRef *queueOut);
CM_EXPORT OSStatus CMBufferQueueEnqueue(CMBufferQueueRef queue, CMBufferRef buf);
CM_EXPORT CMBufferRef CMBufferQueueDequeueAndRetain(CMBufferQueueRef queue);
CM_EXPORT CMBufferRef CMBufferQueueDequeueIfDataReadyAndRetain(CMBufferQueueRef queue);
CM_EXPORT CMBufferRef CMBufferQueueGetHead(CMBufferQueueRef queue);
CM_EXPORT CMBufferRef CMBufferQueueCopyHead(CMBufferQueueRef queue);
CM_EXPORT Boolean CMBufferQueueIsEmpty(CMBufferQueueRef queue);
CM_EXPORT OSStatus CMBufferQueueMarkEndOfData(CMBufferQueueRef queue);
CM_EXPORT Boolean CMBufferQueueContainsEndOfData(CMBufferQueueRef queue);
CM_EXPORT Boolean CMBufferQueueIsAtEndOfData(CMBufferQueueRef queue);
CM_EXPORT OSStatus CMBufferQueueReset(CMBufferQueueRef queue);
CM_EXPORT OSStatus CMBufferQueueResetWithCallback(CMBufferQueueRef queue,
                                                  void (*callback)(CMBufferRef buffer, void *refcon), void *refcon);
CM_EXPORT CMItemCount CMBufferQueueGetBufferCount(CMBufferQueueRef queue);
CM_EXPORT CMTime CMBufferQueueGetDuration(CMBufferQueueRef queue);
CM_EXPORT CMTime CMBufferQueueGetMinDecodeTimeStamp(CMBufferQueueRef queue);
CM_EXPORT CMTime CMBufferQueueGetFirstDecodeTimeStamp(CMBufferQueueRef queue);
CM_EXPORT CMTime CMBufferQueueGetMinPresentationTimeStamp(CMBufferQueueRef queue);
CM_EXPORT CMTime CMBufferQueueGetFirstPresentationTimeStamp(CMBufferQueueRef queue);
CM_EXPORT CMTime CMBufferQueueGetMaxPresentationTimeStamp(CMBufferQueueRef queue);
CM_EXPORT CMTime CMBufferQueueGetEndPresentationTimeStamp(CMBufferQueueRef queue);
CM_EXPORT size_t CMBufferQueueGetTotalSize(CMBufferQueueRef queue);
CM_EXPORT OSStatus CMBufferQueueCallForEachBuffer(CMBufferQueueRef queue,
                                                  OSStatus (*callback)(CMBufferRef buffer, void *refcon), void *refcon);
CM_EXPORT OSStatus CMBufferQueueSetValidationCallback(CMBufferQueueRef queue,
                                                      OSStatus (*callback)(CMBufferQueueRef queue, CMBufferRef buf,
                                                                           void *refcon),
                                                      void *refcon);

typedef struct opaqueCMBufferQueueTriggerToken *CMBufferQueueTriggerToken;
typedef void (*CMBufferQueueTriggerCallback)(void *triggerRefcon, CMBufferQueueTriggerToken triggerToken);

typedef int32_t CMBufferQueueTriggerCondition;
enum {
    kCMBufferQueueTrigger_WhenDurationBecomesLessThan = 1,
    kCMBufferQueueTrigger_WhenDurationBecomesLessThanOrEqualTo = 2,
    kCMBufferQueueTrigger_WhenDurationBecomesGreaterThan = 3,
    kCMBufferQueueTrigger_WhenDurationBecomesGreaterThanOrEqualTo = 4,
    kCMBufferQueueTrigger_WhenMinPresentationTimeStampChanges = 5,
    kCMBufferQueueTrigger_WhenMaxPresentationTimeStampChanges = 6,
    kCMBufferQueueTrigger_WhenDataBecomesReady = 7,
    kCMBufferQueueTrigger_WhenEndOfDataReached = 8,
    kCMBufferQueueTrigger_WhenReset = 9,
    kCMBufferQueueTrigger_WhenBufferCountBecomesLessThan = 10,
    kCMBufferQueueTrigger_WhenBufferCountBecomesGreaterThan = 11,
    kCMBufferQueueTrigger_WhenDurationBecomesGreaterThanOrEqualToAndBufferCountBecomesGreaterThan = 12,
};

CM_EXPORT OSStatus CMBufferQueueInstallTrigger(CMBufferQueueRef queue, CMBufferQueueTriggerCallback callback,
                                               void *refcon, CMBufferQueueTriggerCondition condition, CMTime time,
                                               CMBufferQueueTriggerToken *triggerTokenOut);
CM_EXPORT OSStatus CMBufferQueueInstallTriggerWithIntegerThreshold(CMBufferQueueRef queue,
                                                                   CMBufferQueueTriggerCallback callback, void *refcon,
                                                                   CMBufferQueueTriggerCondition condition,
                                                                   CMItemCount threshold,
                                                                   CMBufferQueueTriggerToken *triggerTokenOut);
CM_EXPORT OSStatus CMBufferQueueRemoveTrigger(CMBufferQueueRef queue, CMBufferQueueTriggerToken triggerToken);
CM_EXPORT Boolean CMBufferQueueTestTrigger(CMBufferQueueRef queue, CMBufferQueueTriggerToken triggerToken);

/* ---- Memory pools ---- */

enum {
    kCMMemoryPoolError_AllocationFailed = -15490,
    kCMMemoryPoolError_InvalidParameter = -15491,
};

typedef struct OpaqueCMMemoryPool *CMMemoryPoolRef;

CM_EXPORT const CFStringRef kCMMemoryPoolOption_AgeOutPeriod;

CM_EXPORT CFTypeID CMMemoryPoolGetTypeID(void);
CM_EXPORT CMMemoryPoolRef CMMemoryPoolCreate(CFDictionaryRef options);
CM_EXPORT CFAllocatorRef CMMemoryPoolGetAllocator(CMMemoryPoolRef pool);
CM_EXPORT void CMMemoryPoolFlush(CMMemoryPoolRef pool);
CM_EXPORT void CMMemoryPoolInvalidate(CMMemoryPoolRef pool);

#endif
