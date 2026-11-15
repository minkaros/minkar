/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Sample buffers: one or more media samples with their format, timing,
 * sizes and attachments, their data in a block buffer (compressed
 * samples, audio) or an image buffer (a decoded frame). The data may be
 * made ready later; a buffer may be invalidated, telling its owner.
 */

#ifndef __CMSAMPLEBUFFER__
#define __CMSAMPLEBUFFER__

#include <CoreMedia/CMBase.h>
#include <CoreMedia/CMTime.h>
#include <CoreMedia/CMBlockBuffer.h>
#include <CoreMedia/CMFormatDescription.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreVideo/CVPixelBuffer.h>

enum {
    kCMSampleBufferError_AllocationFailed = -12730,
    kCMSampleBufferError_RequiredParameterMissing = -12731,
    kCMSampleBufferError_AlreadyHasDataBuffer = -12732,
    kCMSampleBufferError_BufferNotReady = -12733,
    kCMSampleBufferError_SampleIndexOutOfRange = -12734,
    kCMSampleBufferError_BufferHasNoSampleSizes = -12735,
    kCMSampleBufferError_BufferHasNoSampleTimingInfo = -12736,
    kCMSampleBufferError_ArrayTooSmall = -12737,
    kCMSampleBufferError_InvalidEntryCount = -12738,
    kCMSampleBufferError_CannotSubdivide = -12739,
    kCMSampleBufferError_SampleTimingInfoInvalid = -12740,
    kCMSampleBufferError_InvalidMediaTypeForOperation = -12741,
    kCMSampleBufferError_InvalidSampleData = -12742,
    kCMSampleBufferError_InvalidMediaFormat = -12743,
    kCMSampleBufferError_Invalidated = -12744,
    kCMSampleBufferError_DataFailed = -16750,
    kCMSampleBufferError_DataCanceled = -16751,
};

enum {
    kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment = (1L << 0),
};

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

typedef struct {
    CMTime duration;
    CMTime presentationTimeStamp;
    CMTime decodeTimeStamp;
} CMSampleTimingInfo;

CM_EXPORT const CMSampleTimingInfo kCMTimingInfoInvalid;

typedef OSStatus (*CMSampleBufferMakeDataReadyCallback)(CMSampleBufferRef sbuf, void *makeDataReadyRefcon);
typedef OSStatus (^CMSampleBufferMakeDataReadyHandler)(CMSampleBufferRef sbuf);
typedef void (*CMSampleBufferInvalidateCallback)(CMSampleBufferRef sbuf, uint64_t invalidateRefCon);
typedef void (^CMSampleBufferInvalidateHandler)(CMSampleBufferRef sbuf);

CM_EXPORT CFTypeID CMSampleBufferGetTypeID(void);

CM_EXPORT OSStatus CMSampleBufferCreate(CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady,
                                        CMSampleBufferMakeDataReadyCallback makeDataReadyCallback,
                                        void *makeDataReadyRefcon, CMFormatDescriptionRef formatDescription,
                                        CMItemCount numSamples, CMItemCount numSampleTimingEntries,
                                        const CMSampleTimingInfo *sampleTimingArray, CMItemCount numSampleSizeEntries,
                                        const size_t *sampleSizeArray, CMSampleBufferRef *sampleBufferOut);
CM_EXPORT OSStatus CMSampleBufferCreateWithMakeDataReadyHandler(
    CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady, CMFormatDescriptionRef formatDescription,
    CMItemCount numSamples, CMItemCount numSampleTimingEntries, const CMSampleTimingInfo *sampleTimingArray,
    CMItemCount numSampleSizeEntries, const size_t *sampleSizeArray, CMSampleBufferRef *sampleBufferOut,
    CMSampleBufferMakeDataReadyHandler makeDataReadyHandler);
CM_EXPORT OSStatus CMSampleBufferCreateReady(CFAllocatorRef allocator, CMBlockBufferRef dataBuffer,
                                             CMFormatDescriptionRef formatDescription, CMItemCount numSamples,
                                             CMItemCount numSampleTimingEntries,
                                             const CMSampleTimingInfo *sampleTimingArray,
                                             CMItemCount numSampleSizeEntries, const size_t *sampleSizeArray,
                                             CMSampleBufferRef *sampleBufferOut);
CM_EXPORT OSStatus CMSampleBufferCreateForImageBuffer(CFAllocatorRef allocator, CVImageBufferRef imageBuffer,
                                                      Boolean dataReady,
                                                      CMSampleBufferMakeDataReadyCallback makeDataReadyCallback,
                                                      void *makeDataReadyRefcon,
                                                      CMVideoFormatDescriptionRef formatDescription,
                                                      const CMSampleTimingInfo *sampleTiming,
                                                      CMSampleBufferRef *sampleBufferOut);
CM_EXPORT OSStatus CMSampleBufferCreateForImageBufferWithMakeDataReadyHandler(
    CFAllocatorRef allocator, CVImageBufferRef imageBuffer, Boolean dataReady,
    CMVideoFormatDescriptionRef formatDescription, const CMSampleTimingInfo *sampleTiming,
    CMSampleBufferRef *sampleBufferOut, CMSampleBufferMakeDataReadyHandler makeDataReadyHandler);
CM_EXPORT OSStatus CMSampleBufferCreateReadyWithImageBuffer(CFAllocatorRef allocator, CVImageBufferRef imageBuffer,
                                                            CMVideoFormatDescriptionRef formatDescription,
                                                            const CMSampleTimingInfo *sampleTiming,
                                                            CMSampleBufferRef *sampleBufferOut);
CM_EXPORT OSStatus CMAudioSampleBufferCreateWithPacketDescriptions(
    CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady,
    CMSampleBufferMakeDataReadyCallback makeDataReadyCallback, void *makeDataReadyRefcon,
    CMFormatDescriptionRef formatDescription, CMItemCount numSamples, CMTime presentationTimeStamp,
    const AudioStreamPacketDescription *packetDescriptions, CMSampleBufferRef *sampleBufferOut);
CM_EXPORT OSStatus CMAudioSampleBufferCreateWithPacketDescriptionsAndMakeDataReadyHandler(
    CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, Boolean dataReady, CMFormatDescriptionRef formatDescription,
    CMItemCount numSamples, CMTime presentationTimeStamp, const AudioStreamPacketDescription *packetDescriptions,
    CMSampleBufferRef *sampleBufferOut, CMSampleBufferMakeDataReadyHandler makeDataReadyHandler);
CM_EXPORT OSStatus CMAudioSampleBufferCreateReadyWithPacketDescriptions(
    CFAllocatorRef allocator, CMBlockBufferRef dataBuffer, CMFormatDescriptionRef formatDescription,
    CMItemCount numSamples, CMTime presentationTimeStamp, const AudioStreamPacketDescription *packetDescriptions,
    CMSampleBufferRef *sampleBufferOut);
CM_EXPORT OSStatus CMSampleBufferCreateCopy(CFAllocatorRef allocator, CMSampleBufferRef sbuf,
                                            CMSampleBufferRef *sampleBufferOut);
CM_EXPORT OSStatus CMSampleBufferCreateCopyWithNewTiming(CFAllocatorRef allocator, CMSampleBufferRef originalSBuf,
                                                         CMItemCount numSampleTimingEntries,
                                                         const CMSampleTimingInfo *sampleTimingArray,
                                                         CMSampleBufferRef *sampleBufferOut);
CM_EXPORT OSStatus CMSampleBufferCopySampleBufferForRange(CFAllocatorRef allocator, CMSampleBufferRef sbuf,
                                                          CFRange sampleRange, CMSampleBufferRef *sampleBufferOut);

CM_EXPORT OSStatus CMSampleBufferSetDataBuffer(CMSampleBufferRef sbuf, CMBlockBufferRef dataBuffer);
CM_EXPORT CMBlockBufferRef CMSampleBufferGetDataBuffer(CMSampleBufferRef sbuf);
CM_EXPORT CVImageBufferRef CMSampleBufferGetImageBuffer(CMSampleBufferRef sbuf);
CM_EXPORT OSStatus CMSampleBufferSetDataBufferFromAudioBufferList(CMSampleBufferRef sbuf, CFAllocatorRef blockBufferStructureAllocator,
                                                                  CFAllocatorRef blockBufferBlockAllocator, uint32_t flags,
                                                                  const AudioBufferList *bufferList);
CM_EXPORT OSStatus CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
    CMSampleBufferRef sbuf, size_t *bufferListSizeNeededOut, AudioBufferList *bufferListOut, size_t bufferListSize,
    CFAllocatorRef blockBufferStructureAllocator, CFAllocatorRef blockBufferBlockAllocator, uint32_t flags,
    CMBlockBufferRef *blockBufferOut);
CM_EXPORT OSStatus CMSampleBufferGetAudioStreamPacketDescriptions(CMSampleBufferRef sbuf, size_t packetDescriptionsSize,
                                                                  AudioStreamPacketDescription *packetDescriptionsOut,
                                                                  size_t *packetDescriptionsSizeNeededOut);
CM_EXPORT OSStatus CMSampleBufferGetAudioStreamPacketDescriptionsPtr(
    CMSampleBufferRef sbuf, const AudioStreamPacketDescription **packetDescriptionsPointerOut,
    size_t *packetDescriptionsSizeOut);
CM_EXPORT OSStatus CMSampleBufferCopyPCMDataIntoAudioBufferList(CMSampleBufferRef sbuf, int32_t frameOffset,
                                                                int32_t numFrames, AudioBufferList *bufferList);

CM_EXPORT OSStatus CMSampleBufferSetDataReady(CMSampleBufferRef sbuf);
CM_EXPORT Boolean CMSampleBufferDataIsReady(CMSampleBufferRef sbuf);
CM_EXPORT OSStatus CMSampleBufferSetDataFailed(CMSampleBufferRef sbuf, OSStatus status);
CM_EXPORT Boolean CMSampleBufferHasDataFailed(CMSampleBufferRef sbuf, OSStatus *statusOut);
CM_EXPORT OSStatus CMSampleBufferMakeDataReady(CMSampleBufferRef sbuf);
CM_EXPORT OSStatus CMSampleBufferTrackDataReadiness(CMSampleBufferRef sbuf, CMSampleBufferRef sampleBufferToTrack);
CM_EXPORT OSStatus CMSampleBufferInvalidate(CMSampleBufferRef sbuf);
CM_EXPORT OSStatus CMSampleBufferSetInvalidateCallback(CMSampleBufferRef sbuf,
                                                       CMSampleBufferInvalidateCallback invalidateCallback,
                                                       uint64_t invalidateRefCon);
CM_EXPORT OSStatus CMSampleBufferSetInvalidateHandler(CMSampleBufferRef sbuf,
                                                      CMSampleBufferInvalidateHandler invalidateHandler);
CM_EXPORT Boolean CMSampleBufferIsValid(CMSampleBufferRef sbuf);

CM_EXPORT CMItemCount CMSampleBufferGetNumSamples(CMSampleBufferRef sbuf);
CM_EXPORT CMTime CMSampleBufferGetDuration(CMSampleBufferRef sbuf);
CM_EXPORT CMTime CMSampleBufferGetPresentationTimeStamp(CMSampleBufferRef sbuf);
CM_EXPORT CMTime CMSampleBufferGetDecodeTimeStamp(CMSampleBufferRef sbuf);
CM_EXPORT CMTime CMSampleBufferGetOutputDuration(CMSampleBufferRef sbuf);
CM_EXPORT CMTime CMSampleBufferGetOutputPresentationTimeStamp(CMSampleBufferRef sbuf);
CM_EXPORT OSStatus CMSampleBufferSetOutputPresentationTimeStamp(CMSampleBufferRef sbuf,
                                                                CMTime outputPresentationTimeStamp);
CM_EXPORT CMTime CMSampleBufferGetOutputDecodeTimeStamp(CMSampleBufferRef sbuf);
CM_EXPORT OSStatus CMSampleBufferGetSampleTimingInfoArray(CMSampleBufferRef sbuf, CMItemCount numSampleTimingEntries,
                                                          CMSampleTimingInfo *timingArrayOut,
                                                          CMItemCount *timingArrayEntriesNeededOut);
CM_EXPORT OSStatus CMSampleBufferGetOutputSampleTimingInfoArray(CMSampleBufferRef sbuf,
                                                                CMItemCount timingArrayEntries,
                                                                CMSampleTimingInfo *timingArrayOut,
                                                                CMItemCount *timingArrayEntriesNeededOut);
CM_EXPORT OSStatus CMSampleBufferGetSampleTimingInfo(CMSampleBufferRef sbuf, CMItemIndex sampleIndex,
                                                     CMSampleTimingInfo *timingInfoOut);
CM_EXPORT OSStatus CMSampleBufferGetSampleSizeArray(CMSampleBufferRef sbuf, CMItemCount sizeArrayEntries,
                                                    size_t *sizeArrayOut, CMItemCount *sizeArrayEntriesNeededOut);
CM_EXPORT size_t CMSampleBufferGetSampleSize(CMSampleBufferRef sbuf, CMItemIndex sampleIndex);
CM_EXPORT size_t CMSampleBufferGetTotalSampleSize(CMSampleBufferRef sbuf);
CM_EXPORT CMFormatDescriptionRef CMSampleBufferGetFormatDescription(CMSampleBufferRef sbuf);
CM_EXPORT CFArrayRef CMSampleBufferGetSampleAttachmentsArray(CMSampleBufferRef sbuf, Boolean createIfNecessary);

CM_EXPORT OSStatus CMSampleBufferCallForEachSample(CMSampleBufferRef sbuf,
                                                   OSStatus (*callback)(CMSampleBufferRef sampleBuffer,
                                                                        CMItemCount index, void *refcon),
                                                   void *refcon);
CM_EXPORT OSStatus CMSampleBufferCallBlockForEachSample(CMSampleBufferRef sbuf,
                                                        OSStatus (^handler)(CMSampleBufferRef, CMItemCount));

/* A sample's attachments (CMSampleBufferGetSampleAttachmentsArray). */
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_NotSync;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_PartialSync;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_HasRedundantCoding;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_IsDependedOnByOthers;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_DependsOnOthers;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_EarlierDisplayTimesAllowed;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_DisplayImmediately;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_DoNotDisplay;

/* The buffer's own attachments (CMSetAttachment). */
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_DrainAfterDecoding;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_ResumeOutput;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_TransitionID;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_TrimDurationAtStart;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_TrimDurationAtEnd;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_SpeedMultiplier;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_Reverse;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_FillDiscontinuitiesWithSilence;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_EmptyMedia;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_PermanentEmptyMedia;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_EndsPreviousSampleDuration;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_SampleReferenceURL;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_SampleReferenceByteOffset;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_GradualDecoderRefresh;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_DroppedFrameReason;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_DroppedFrameReasonInfo;
CM_EXPORT const CFStringRef kCMSampleBufferAttachmentKey_ForceKeyFrame;
CM_EXPORT const CFStringRef kCMSampleBufferDroppedFrameReason_FrameWasLate;
CM_EXPORT const CFStringRef kCMSampleBufferDroppedFrameReason_OutOfBuffers;
CM_EXPORT const CFStringRef kCMSampleBufferDroppedFrameReason_Discontinuity;

CM_EXPORT const CFStringRef kCMSampleBufferNotification_DataBecameReady;
CM_EXPORT const CFStringRef kCMSampleBufferNotification_DataFailed;
CM_EXPORT const CFStringRef kCMSampleBufferNotificationParameter_OSStatus;

#endif
