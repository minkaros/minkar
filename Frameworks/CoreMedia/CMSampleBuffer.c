/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Sample buffers. Timing is given for no sample, for every sample alike
 * (one entry: each sample's times follow the previous one's by its
 * duration), or sample by sample; sizes the same way. A buffer's output
 * times are its presentation times less what its trim attachments cut
 * off at the start and the end, unless set. A copy of some of the
 * samples refers to their bytes in the original's data buffer.
 */

#include "CMSampleBuffer.h"
#include "CMInternal.h"
#include <Block.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <string.h>

#define KEY(name, value) const CFStringRef name = CFSTR(value)

KEY(kCMSampleBufferAttachmentKey_ResetDecoderBeforeDecoding, "ResetDecoderBeforeDecoding");
KEY(kCMSampleBufferAttachmentKey_DrainAfterDecoding, "DrainAfterDecoding");
KEY(kCMSampleBufferAttachmentKey_PostNotificationWhenConsumed, "PostNotificationWhenConsumed");
KEY(kCMSampleBufferAttachmentKey_ResumeOutput, "ResumeOutput");
KEY(kCMSampleBufferAttachmentKey_TransitionID, "TransitionID");
KEY(kCMSampleBufferAttachmentKey_TrimDurationAtStart, "TrimDurationAtStart");
KEY(kCMSampleBufferAttachmentKey_TrimDurationAtEnd, "TrimDurationAtEnd");
KEY(kCMSampleBufferAttachmentKey_SpeedMultiplier, "SpeedMultiplier");
KEY(kCMSampleBufferAttachmentKey_Reverse, "Reverse");
KEY(kCMSampleBufferAttachmentKey_FillDiscontinuitiesWithSilence, "FillDiscontinuitiesWithSilence");
KEY(kCMSampleBufferAttachmentKey_EmptyMedia, "EmptyMedia");
KEY(kCMSampleBufferAttachmentKey_PermanentEmptyMedia, "PermanentEmptyMedia");
KEY(kCMSampleBufferAttachmentKey_DisplayEmptyMediaImmediately, "DisplayEmptyMediaImmediately");
KEY(kCMSampleBufferAttachmentKey_EndsPreviousSampleDuration, "EndsPreviousSampleDuration");
KEY(kCMSampleBufferAttachmentKey_SampleReferenceURL, "SampleReferenceURL");
KEY(kCMSampleBufferAttachmentKey_SampleReferenceByteOffset, "SampleReferenceByteOffset");
KEY(kCMSampleBufferAttachmentKey_GradualDecoderRefresh, "GradualDecoderRefresh");
KEY(kCMSampleBufferAttachmentKey_DroppedFrameReason, "DroppedFrameReason");
KEY(kCMSampleBufferAttachmentKey_DroppedFrameReasonInfo, "DroppedFrameReasonInfo");
KEY(kCMSampleBufferAttachmentKey_ForceKeyFrame, "ForceKeyFrame");
KEY(kCMSampleBufferDroppedFrameReason_FrameWasLate, "FrameWasLate");
KEY(kCMSampleBufferDroppedFrameReason_OutOfBuffers, "OutOfBuffers");
KEY(kCMSampleBufferDroppedFrameReason_Discontinuity, "Discontinuity");
KEY(kCMSampleBufferNotification_DataBecameReady, "CMSampleBufferNotification_DataBecameReady");
KEY(kCMSampleBufferNotification_DataFailed, "CMSampleBufferNotification_DataFailed");
KEY(kCMSampleBufferNotificationParameter_OSStatus, "OSStatus");

struct opaqueCMSampleBuffer {
    CMBearerHeader header;              /* the attachments */
    pthread_mutex_t lock;
    CMBlockBufferRef dataBuffer;
    CVImageBufferRef imageBuffer;
    CMFormatDescriptionRef format;
    Boolean dataReady, invalid;
    OSStatus failure;
    CMSampleBufferMakeDataReadyCallback makeReady;
    void *makeReadyRefcon;
    CMSampleBufferMakeDataReadyHandler makeReadyHandler;
    CMSampleBufferRef tracked;          /* ready when it is */
    CMItemCount numSamples;
    CMItemCount timingCount, sizeCount;
    CMSampleTimingInfo *timing;
    size_t *sizes;
    AudioStreamPacketDescription *packets;
    CMItemCount packetCount;
    CFMutableArrayRef sampleAttachments;
    Boolean outputTimeSet;
    CMTime outputPTS;
    CMSampleBufferInvalidateCallback invalidateCallback;
    uint64_t invalidateRefcon;
    CMSampleBufferInvalidateHandler invalidateHandler;
};

static CFTypeID typeID;

static void finalize(CFTypeRef cf) {
    CMSampleBufferRef s = (CMSampleBufferRef)cf;
    if (s->dataBuffer != NULL)
        CFRelease(s->dataBuffer);
    if (s->imageBuffer != NULL)
        CFRelease(s->imageBuffer);
    if (s->format != NULL)
        CFRelease(s->format);
    if (s->tracked != NULL)
        CFRelease(s->tracked);
    if (s->sampleAttachments != NULL)
        CFRelease(s->sampleAttachments);
    if (s->makeReadyHandler != NULL)
        _Block_release(s->makeReadyHandler);
    if (s->invalidateHandler != NULL)
        _Block_release(s->invalidateHandler);
    free(s->timing);
    free(s->sizes);
    free(s->packets);
    _CMAttachmentStoreFinalize(&s->header.attachments);
    pthread_mutex_destroy(&s->lock);
}

static CFStringRef describe(CFTypeRef cf) {
    CMSampleBufferRef s = (CMSampleBufferRef)cf;
    CFStringRef pts = CMTimeCopyDescription(NULL, CMSampleBufferGetPresentationTimeStamp(s));
    CFStringRef d = CFStringCreateWithFormat(NULL, NULL, CFSTR("<CMSampleBuffer %p samples=%ld pts=%@ ready=%d%s%s>"),
                                             cf, (long)s->numSamples, pts, s->dataReady,
                                             s->imageBuffer != NULL ? " image" : "", s->invalid ? " invalid" : "");
    CFRelease(pts);
    return d;
}

static const CFRuntimeClass sampleBufferClass = { 0, "CMSampleBuffer", NULL, NULL, finalize, NULL, NULL, NULL, describe };

CFTypeID CMSampleBufferGetTypeID(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        typeID = _CFRuntimeRegisterClass(&sampleBufferClass);
        _CMRegisterBearerType(typeID);
    });
    return typeID;
}

static CMSampleBufferRef allocate(CFAllocatorRef allocator) {
    size_t extra = sizeof(struct opaqueCMSampleBuffer) - sizeof(CFRuntimeBase);
    CMSampleBufferRef s = (CMSampleBufferRef)_CFRuntimeCreateInstance(allocator, CMSampleBufferGetTypeID(), extra, NULL);
    if (s == NULL)
        return NULL;
    _CMAttachmentStoreInit(&s->header.attachments);
    pthread_mutex_init(&s->lock, NULL);
    s->outputPTS = kCMTimeInvalid;
    return s;
}

static Boolean isLinearPCM(CMFormatDescriptionRef f) {
    const AudioStreamBasicDescription *asbd = f != NULL ? CMAudioFormatDescriptionGetStreamBasicDescription(f) : NULL;
    return asbd != NULL && asbd->mFormatID == kAudioFormatLinearPCM && asbd->mBytesPerFrame > 0;
}

/* The common part of every creation. */
static OSStatus build(CFAllocatorRef allocator, CMBlockBufferRef data, CVImageBufferRef image, Boolean ready,
                      CMSampleBufferMakeDataReadyCallback makeReady, void *refcon,
                      CMSampleBufferMakeDataReadyHandler handler, CMFormatDescriptionRef format, CMItemCount numSamples,
                      CMItemCount timingCount, const CMSampleTimingInfo *timing, CMItemCount sizeCount,
                      const size_t *sizes, CMSampleBufferRef *out) {
    if (out == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    *out = NULL;
    if (numSamples < 0 || timingCount < 0 || sizeCount < 0 || (timingCount > 0 && timing == NULL) ||
        (sizeCount > 0 && sizes == NULL))
        return kCMSampleBufferError_RequiredParameterMissing;
    if (timingCount > 1 && timingCount != numSamples)
        return kCMSampleBufferError_InvalidEntryCount;
    if (sizeCount > 1 && sizeCount != numSamples)
        return kCMSampleBufferError_InvalidEntryCount;
    if (numSamples > 0 && data == NULL && image == NULL && ready)
        return kCMSampleBufferError_RequiredParameterMissing;
    CMSampleBufferRef s = allocate(allocator);
    if (s == NULL)
        return kCMSampleBufferError_AllocationFailed;
    s->dataBuffer = data != NULL ? (CMBlockBufferRef)CFRetain(data) : NULL;
    s->imageBuffer = image != NULL ? CFRetain(image) : NULL;
    s->format = format != NULL ? CFRetain(format) : NULL;
    s->dataReady = ready;
    s->makeReady = makeReady;
    s->makeReadyRefcon = refcon;
    s->makeReadyHandler = handler != NULL ? _Block_copy(handler) : NULL;
    s->numSamples = numSamples;
    if (timingCount > 0) {
        s->timing = malloc((size_t)timingCount * sizeof(CMSampleTimingInfo));
        memcpy(s->timing, timing, (size_t)timingCount * sizeof(CMSampleTimingInfo));
        s->timingCount = timingCount;
    }
    if (sizeCount > 0) {
        s->sizes = malloc((size_t)sizeCount * sizeof(size_t));
        memcpy(s->sizes, sizes, (size_t)sizeCount * sizeof(size_t));
        s->sizeCount = sizeCount;
    }
    *out = s;
    return noErr;
}

OSStatus CMSampleBufferCreate(CFAllocatorRef allocator, CMBlockBufferRef data, Boolean ready,
                              CMSampleBufferMakeDataReadyCallback makeReady, void *refcon, CMFormatDescriptionRef format,
                              CMItemCount numSamples, CMItemCount timingCount, const CMSampleTimingInfo *timing,
                              CMItemCount sizeCount, const size_t *sizes, CMSampleBufferRef *out) {
    return build(allocator, data, NULL, ready, makeReady, refcon, NULL, format, numSamples, timingCount, timing,
                 sizeCount, sizes, out);
}

OSStatus CMSampleBufferCreateWithMakeDataReadyHandler(CFAllocatorRef allocator, CMBlockBufferRef data, Boolean ready,
                                                      CMFormatDescriptionRef format, CMItemCount numSamples,
                                                      CMItemCount timingCount, const CMSampleTimingInfo *timing,
                                                      CMItemCount sizeCount, const size_t *sizes,
                                                      CMSampleBufferRef *out,
                                                      CMSampleBufferMakeDataReadyHandler handler) {
    return build(allocator, data, NULL, ready, NULL, NULL, handler, format, numSamples, timingCount, timing, sizeCount,
                 sizes, out);
}

OSStatus CMSampleBufferCreateReady(CFAllocatorRef allocator, CMBlockBufferRef data, CMFormatDescriptionRef format,
                                   CMItemCount numSamples, CMItemCount timingCount, const CMSampleTimingInfo *timing,
                                   CMItemCount sizeCount, const size_t *sizes, CMSampleBufferRef *out) {
    return build(allocator, data, NULL, true, NULL, NULL, NULL, format, numSamples, timingCount, timing, sizeCount,
                 sizes, out);
}

static OSStatus forImage(CFAllocatorRef allocator, CVImageBufferRef image, Boolean ready,
                         CMSampleBufferMakeDataReadyCallback makeReady, void *refcon,
                         CMSampleBufferMakeDataReadyHandler handler, CMVideoFormatDescriptionRef format,
                         const CMSampleTimingInfo *timing, CMSampleBufferRef *out) {
    if (out == NULL || image == NULL || format == NULL || timing == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (!CMVideoFormatDescriptionMatchesImageBuffer(format, image))
        return kCMSampleBufferError_InvalidMediaFormat;
    return build(allocator, NULL, image, ready, makeReady, refcon, handler, format, 1, 1, timing, 0, NULL, out);
}

OSStatus CMSampleBufferCreateForImageBuffer(CFAllocatorRef allocator, CVImageBufferRef image, Boolean ready,
                                            CMSampleBufferMakeDataReadyCallback makeReady, void *refcon,
                                            CMVideoFormatDescriptionRef format, const CMSampleTimingInfo *timing,
                                            CMSampleBufferRef *out) {
    return forImage(allocator, image, ready, makeReady, refcon, NULL, format, timing, out);
}

OSStatus CMSampleBufferCreateForImageBufferWithMakeDataReadyHandler(CFAllocatorRef allocator, CVImageBufferRef image,
                                                                    Boolean ready, CMVideoFormatDescriptionRef format,
                                                                    const CMSampleTimingInfo *timing,
                                                                    CMSampleBufferRef *out,
                                                                    CMSampleBufferMakeDataReadyHandler handler) {
    return forImage(allocator, image, ready, NULL, NULL, handler, format, timing, out);
}

OSStatus CMSampleBufferCreateReadyWithImageBuffer(CFAllocatorRef allocator, CVImageBufferRef image,
                                                  CMVideoFormatDescriptionRef format, const CMSampleTimingInfo *timing,
                                                  CMSampleBufferRef *out) {
    return forImage(allocator, image, true, NULL, NULL, NULL, format, timing, out);
}

/* Audio: linear PCM samples are frames, each 1/rate long, their size the
   frame's; other formats are packets, described by their descriptions,
   each as long as the format's frames per packet. */
static OSStatus audio(CFAllocatorRef allocator, CMBlockBufferRef data, Boolean ready,
                      CMSampleBufferMakeDataReadyCallback makeReady, void *refcon,
                      CMSampleBufferMakeDataReadyHandler handler, CMFormatDescriptionRef format, CMItemCount numSamples,
                      CMTime pts, const AudioStreamPacketDescription *packets, CMSampleBufferRef *out) {
    const AudioStreamBasicDescription *asbd = format != NULL ? CMAudioFormatDescriptionGetStreamBasicDescription(format)
                                                             : NULL;
    if (out == NULL || asbd == NULL || numSamples <= 0 || asbd->mSampleRate <= 0)
        return kCMSampleBufferError_RequiredParameterMissing;
    int32_t rate = (int32_t)asbd->mSampleRate;
    CMTime pts1 = CMTIME_IS_NUMERIC(pts) ? pts : CMTimeMake(0, rate);
    if (isLinearPCM(format)) {
        CMSampleTimingInfo t = { CMTimeMake(1, rate), pts1, kCMTimeInvalid };
        size_t size = asbd->mBytesPerFrame;
        return build(allocator, data, NULL, ready, makeReady, refcon, handler, format, numSamples, 1, &t, 1, &size, out);
    }
    if (packets == NULL && asbd->mBytesPerPacket == 0)
        return kCMSampleBufferError_RequiredParameterMissing;
    uint32_t framesPerPacket = asbd->mFramesPerPacket > 0 ? asbd->mFramesPerPacket : 1;
    CMSampleTimingInfo t = { CMTimeMake(framesPerPacket, rate), pts1, kCMTimeInvalid };
    size_t *sizes = malloc((size_t)numSamples * sizeof(size_t));
    for (CMItemCount i = 0; i < numSamples; i++)
        sizes[i] = packets != NULL ? packets[i].mDataByteSize : asbd->mBytesPerPacket;
    OSStatus err = build(allocator, data, NULL, ready, makeReady, refcon, handler, format, numSamples, 1, &t, numSamples,
                         sizes, out);
    free(sizes);
    if (err == noErr && packets != NULL) {
        (*out)->packets = malloc((size_t)numSamples * sizeof(AudioStreamPacketDescription));
        memcpy((*out)->packets, packets, (size_t)numSamples * sizeof(AudioStreamPacketDescription));
        (*out)->packetCount = numSamples;
    }
    return err;
}

OSStatus CMAudioSampleBufferCreateWithPacketDescriptions(CFAllocatorRef allocator, CMBlockBufferRef data, Boolean ready,
                                                         CMSampleBufferMakeDataReadyCallback makeReady, void *refcon,
                                                         CMFormatDescriptionRef format, CMItemCount numSamples,
                                                         CMTime pts, const AudioStreamPacketDescription *packets,
                                                         CMSampleBufferRef *out) {
    return audio(allocator, data, ready, makeReady, refcon, NULL, format, numSamples, pts, packets, out);
}

OSStatus CMAudioSampleBufferCreateWithPacketDescriptionsAndMakeDataReadyHandler(
    CFAllocatorRef allocator, CMBlockBufferRef data, Boolean ready, CMFormatDescriptionRef format,
    CMItemCount numSamples, CMTime pts, const AudioStreamPacketDescription *packets, CMSampleBufferRef *out,
    CMSampleBufferMakeDataReadyHandler handler) {
    return audio(allocator, data, ready, NULL, NULL, handler, format, numSamples, pts, packets, out);
}

OSStatus CMAudioSampleBufferCreateReadyWithPacketDescriptions(CFAllocatorRef allocator, CMBlockBufferRef data,
                                                              CMFormatDescriptionRef format, CMItemCount numSamples,
                                                              CMTime pts, const AudioStreamPacketDescription *packets,
                                                              CMSampleBufferRef *out) {
    return audio(allocator, data, true, NULL, NULL, NULL, format, numSamples, pts, packets, out);
}

/* ---- Copies ---- */

static void copyExtras(CMSampleBufferRef to, CMSampleBufferRef from) {
    CMPropagateAttachments(from, to);
    CFDictionaryRef notPropagated = CMCopyDictionaryOfAttachments(NULL, from, kCMAttachmentMode_ShouldNotPropagate);
    if (notPropagated != NULL) {
        CMSetAttachments(to, notPropagated, kCMAttachmentMode_ShouldNotPropagate);
        CFRelease(notPropagated);
    }
    to->outputTimeSet = from->outputTimeSet;
    to->outputPTS = from->outputPTS;
}

static CFMutableArrayRef copyAttachmentsArray(CFArrayRef from, CFRange range) {
    if (from == NULL)
        return NULL;
    CFMutableArrayRef to = CFArrayCreateMutable(NULL, range.length, &kCFTypeArrayCallBacks);
    for (CFIndex i = range.location; i < range.location + range.length; i++) {
        CFMutableDictionaryRef d = CFDictionaryCreateMutableCopy(NULL, 0, CFArrayGetValueAtIndex(from, i));
        CFArrayAppendValue(to, d);
        CFRelease(d);
    }
    return to;
}

OSStatus CMSampleBufferCreateCopy(CFAllocatorRef allocator, CMSampleBufferRef s, CMSampleBufferRef *out) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    return CMSampleBufferCreateCopyWithNewTiming(allocator, s, s->timingCount, s->timing, out);
}

OSStatus CMSampleBufferCreateCopyWithNewTiming(CFAllocatorRef allocator, CMSampleBufferRef s, CMItemCount timingCount,
                                               const CMSampleTimingInfo *timing, CMSampleBufferRef *out) {
    if (s == NULL || out == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    pthread_mutex_lock(&s->lock);
    OSStatus err = build(allocator, s->dataBuffer, s->imageBuffer, s->dataReady, s->makeReady, s->makeReadyRefcon,
                         s->makeReadyHandler, s->format, s->numSamples, timingCount, timing, s->sizeCount, s->sizes,
                         out);
    if (err == noErr) {
        if (s->packets != NULL) {
            (*out)->packets = malloc((size_t)s->packetCount * sizeof(AudioStreamPacketDescription));
            memcpy((*out)->packets, s->packets, (size_t)s->packetCount * sizeof(AudioStreamPacketDescription));
            (*out)->packetCount = s->packetCount;
        }
        (*out)->sampleAttachments = copyAttachmentsArray(s->sampleAttachments,
                                                         CFRangeMake(0, s->sampleAttachments != NULL
                                                                            ? CFArrayGetCount(s->sampleAttachments)
                                                                            : 0));
    }
    pthread_mutex_unlock(&s->lock);
    if (err == noErr)
        copyExtras(*out, s);
    return err;
}

static size_t sizeAt(CMSampleBufferRef s, CMItemIndex i) {
    return s->sizeCount == 0 ? 0 : (s->sizeCount == 1 ? s->sizes[0] : s->sizes[i]);
}

static CMSampleTimingInfo timingAt(CMSampleBufferRef s, CMItemIndex i) {
    if (s->timingCount == 0)
        return kCMTimingInfoInvalid;
    if (s->timingCount > 1)
        return s->timing[i];
    CMSampleTimingInfo t = s->timing[0];
    CMTime offset = CMTimeMultiply(t.duration, (int32_t)i);
    if (CMTIME_IS_NUMERIC(t.presentationTimeStamp))
        t.presentationTimeStamp = CMTimeAdd(t.presentationTimeStamp, offset);
    if (CMTIME_IS_NUMERIC(t.decodeTimeStamp))
        t.decodeTimeStamp = CMTimeAdd(t.decodeTimeStamp, offset);
    return t;
}

OSStatus CMSampleBufferCopySampleBufferForRange(CFAllocatorRef allocator, CMSampleBufferRef s, CFRange range,
                                                CMSampleBufferRef *out) {
    if (s == NULL || out == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    *out = NULL;
    if (range.location < 0 || range.length <= 0 || range.location + range.length > s->numSamples)
        return kCMSampleBufferError_SampleIndexOutOfRange;
    if (s->imageBuffer != NULL)
        return range.location == 0 && range.length == 1 ? CMSampleBufferCreateCopy(allocator, s, out)
                                                        : kCMSampleBufferError_CannotSubdivide;
    if (s->dataBuffer != NULL && s->sizeCount == 0)
        return kCMSampleBufferError_CannotSubdivide;
    pthread_mutex_lock(&s->lock);
    size_t offset = 0, length = 0;
    for (CMItemIndex i = 0; i < range.location + range.length; i++) {
        if (i < range.location)
            offset += sizeAt(s, i);
        else
            length += sizeAt(s, i);
    }
    CMBlockBufferRef data = NULL;
    OSStatus err = noErr;
    if (s->dataBuffer != NULL && length > 0)
        err = CMBlockBufferCreateWithBufferReference(allocator, s->dataBuffer, offset, length, 0, &data);
    CMSampleTimingInfo *timing = NULL;
    CMItemCount timingCount = 0;
    if (err == noErr && s->timingCount == 1) {
        timing = malloc(sizeof(CMSampleTimingInfo));
        timing[0] = timingAt(s, range.location);
        timingCount = 1;
    } else if (err == noErr && s->timingCount > 1) {
        timing = malloc((size_t)range.length * sizeof(CMSampleTimingInfo));
        memcpy(timing, s->timing + range.location, (size_t)range.length * sizeof(CMSampleTimingInfo));
        timingCount = range.length;
    }
    CMItemCount sizeCount = s->sizeCount > 1 ? range.length : s->sizeCount;
    const size_t *sizes = s->sizeCount > 1 ? s->sizes + range.location : s->sizes;
    if (err == noErr)
        err = build(allocator, data, NULL, s->dataReady, s->makeReady, s->makeReadyRefcon, s->makeReadyHandler,
                    s->format, range.length, timingCount, timing, sizeCount, sizes, out);
    if (err == noErr) {
        if (s->packets != NULL) {
            (*out)->packets = malloc((size_t)range.length * sizeof(AudioStreamPacketDescription));
            for (CFIndex i = 0; i < range.length; i++) {
                (*out)->packets[i] = s->packets[range.location + i];
                (*out)->packets[i].mStartOffset -= (SInt64)offset;
            }
            (*out)->packetCount = range.length;
        }
        (*out)->sampleAttachments = copyAttachmentsArray(s->sampleAttachments, range);
    }
    pthread_mutex_unlock(&s->lock);
    if (err == noErr) {
        CMPropagateAttachments(s, *out);
    }
    free(timing);
    if (data != NULL)
        CFRelease(data);
    return err;
}

/* ---- Data ---- */

OSStatus CMSampleBufferSetDataBuffer(CMSampleBufferRef s, CMBlockBufferRef data) {
    if (s == NULL || data == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    pthread_mutex_lock(&s->lock);
    OSStatus err = noErr;
    if (s->dataBuffer != NULL || s->imageBuffer != NULL)
        err = kCMSampleBufferError_AlreadyHasDataBuffer;
    else
        s->dataBuffer = (CMBlockBufferRef)CFRetain(data);
    pthread_mutex_unlock(&s->lock);
    return err;
}

CMBlockBufferRef CMSampleBufferGetDataBuffer(CMSampleBufferRef s) {
    return s != NULL ? s->dataBuffer : NULL;
}

CVImageBufferRef CMSampleBufferGetImageBuffer(CMSampleBufferRef s) {
    return s != NULL ? s->imageBuffer : NULL;
}

OSStatus CMSampleBufferSetDataReady(CMSampleBufferRef s) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    pthread_mutex_lock(&s->lock);
    s->dataReady = true;
    pthread_mutex_unlock(&s->lock);
    return noErr;
}

Boolean CMSampleBufferDataIsReady(CMSampleBufferRef s) {
    if (s == NULL)
        return false;
    pthread_mutex_lock(&s->lock);
    if (!s->dataReady && s->tracked != NULL && CMSampleBufferDataIsReady(s->tracked))
        s->dataReady = true;
    Boolean ready = s->dataReady;
    pthread_mutex_unlock(&s->lock);
    return ready;
}

OSStatus CMSampleBufferSetDataFailed(CMSampleBufferRef s, OSStatus status) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    pthread_mutex_lock(&s->lock);
    s->failure = status != noErr ? status : kCMSampleBufferError_DataFailed;
    pthread_mutex_unlock(&s->lock);
    return noErr;
}

Boolean CMSampleBufferHasDataFailed(CMSampleBufferRef s, OSStatus *statusOut) {
    if (s == NULL)
        return false;
    pthread_mutex_lock(&s->lock);
    OSStatus f = s->failure;
    pthread_mutex_unlock(&s->lock);
    if (statusOut != NULL)
        *statusOut = f;
    return f != noErr;
}

OSStatus CMSampleBufferMakeDataReady(CMSampleBufferRef s) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (CMSampleBufferDataIsReady(s))
        return noErr;
    OSStatus err = kCMSampleBufferError_BufferNotReady;
    if (s->makeReady != NULL)
        err = s->makeReady(s, s->makeReadyRefcon);
    else if (s->makeReadyHandler != NULL)
        err = s->makeReadyHandler(s);
    if (err == noErr)
        CMSampleBufferSetDataReady(s);
    return err;
}

OSStatus CMSampleBufferTrackDataReadiness(CMSampleBufferRef s, CMSampleBufferRef other) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    pthread_mutex_lock(&s->lock);
    if (s->tracked != NULL)
        CFRelease(s->tracked);
    s->tracked = other != NULL ? (CMSampleBufferRef)CFRetain(other) : NULL;
    pthread_mutex_unlock(&s->lock);
    return noErr;
}

OSStatus CMSampleBufferSetInvalidateCallback(CMSampleBufferRef s, CMSampleBufferInvalidateCallback callback,
                                             uint64_t refcon) {
    if (s == NULL || callback == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    pthread_mutex_lock(&s->lock);
    OSStatus err = noErr;
    if (s->invalid)
        err = kCMSampleBufferError_Invalidated;
    else if (s->invalidateCallback != NULL || s->invalidateHandler != NULL)
        err = kCMSampleBufferError_RequiredParameterMissing;
    else {
        s->invalidateCallback = callback;
        s->invalidateRefcon = refcon;
    }
    pthread_mutex_unlock(&s->lock);
    return err;
}

OSStatus CMSampleBufferSetInvalidateHandler(CMSampleBufferRef s, CMSampleBufferInvalidateHandler handler) {
    if (s == NULL || handler == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    pthread_mutex_lock(&s->lock);
    OSStatus err = noErr;
    if (s->invalid)
        err = kCMSampleBufferError_Invalidated;
    else if (s->invalidateCallback != NULL || s->invalidateHandler != NULL)
        err = kCMSampleBufferError_RequiredParameterMissing;
    else
        s->invalidateHandler = _Block_copy(handler);
    pthread_mutex_unlock(&s->lock);
    return err;
}

/* Once: the owner told through its callback or handler. */
OSStatus CMSampleBufferInvalidate(CMSampleBufferRef s) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    pthread_mutex_lock(&s->lock);
    if (s->invalid) {
        pthread_mutex_unlock(&s->lock);
        return kCMSampleBufferError_Invalidated;
    }
    s->invalid = true;
    CMSampleBufferInvalidateCallback callback = s->invalidateCallback;
    CMSampleBufferInvalidateHandler handler = s->invalidateHandler;
    s->invalidateHandler = NULL;
    pthread_mutex_unlock(&s->lock);
    if (callback != NULL)
        callback(s, s->invalidateRefcon);
    if (handler != NULL) {
        handler(s);
        _Block_release(handler);
    }
    return noErr;
}

Boolean CMSampleBufferIsValid(CMSampleBufferRef s) {
    if (s == NULL)
        return false;
    pthread_mutex_lock(&s->lock);
    Boolean valid = !s->invalid;
    pthread_mutex_unlock(&s->lock);
    return valid;
}

/* ---- Counts, times and sizes ---- */

CMItemCount CMSampleBufferGetNumSamples(CMSampleBufferRef s) {
    return s != NULL ? s->numSamples : 0;
}

CMTime CMSampleBufferGetDuration(CMSampleBufferRef s) {
    if (s == NULL || s->timingCount == 0 || s->numSamples == 0)
        return kCMTimeInvalid;
    if (s->timingCount == 1)
        return CMTimeMultiply(s->timing[0].duration, (int32_t)s->numSamples);
    CMTime d = kCMTimeZero;
    for (CMItemCount i = 0; i < s->timingCount; i++)
        d = CMTimeAdd(d, s->timing[i].duration);
    return d;
}

/* The earliest of the samples' presentation times. */
CMTime CMSampleBufferGetPresentationTimeStamp(CMSampleBufferRef s) {
    if (s == NULL || s->timingCount == 0)
        return kCMTimeInvalid;
    CMTime earliest = s->timing[0].presentationTimeStamp;
    for (CMItemCount i = 1; i < s->timingCount; i++)
        if (CMTimeCompare(s->timing[i].presentationTimeStamp, earliest) < 0)
            earliest = s->timing[i].presentationTimeStamp;
    return earliest;
}

CMTime CMSampleBufferGetDecodeTimeStamp(CMSampleBufferRef s) {
    return s != NULL && s->timingCount > 0 ? s->timing[0].decodeTimeStamp : kCMTimeInvalid;
}

static CMTime trim(CMSampleBufferRef s, CFStringRef key) {
    CFTypeRef v = CMGetAttachment(s, key, NULL);
    CMTime t = v != NULL && CFGetTypeID(v) == CFDictionaryGetTypeID() ? CMTimeMakeFromDictionary(v) : kCMTimeInvalid;
    return CMTIME_IS_NUMERIC(t) ? t : kCMTimeZero;
}

CMTime CMSampleBufferGetOutputDuration(CMSampleBufferRef s) {
    CMTime d = CMSampleBufferGetDuration(s);
    if (!CMTIME_IS_NUMERIC(d))
        return d;
    d = CMTimeSubtract(d, trim(s, kCMSampleBufferAttachmentKey_TrimDurationAtStart));
    d = CMTimeSubtract(d, trim(s, kCMSampleBufferAttachmentKey_TrimDurationAtEnd));
    return CMTimeCompare(d, kCMTimeZero) < 0 ? kCMTimeZero : d;
}

CMTime CMSampleBufferGetOutputPresentationTimeStamp(CMSampleBufferRef s) {
    if (s == NULL)
        return kCMTimeInvalid;
    pthread_mutex_lock(&s->lock);
    Boolean set = s->outputTimeSet;
    CMTime t = s->outputPTS;
    pthread_mutex_unlock(&s->lock);
    if (set)
        return t;
    CMTime pts = CMSampleBufferGetPresentationTimeStamp(s);
    return CMTIME_IS_NUMERIC(pts) ? CMTimeAdd(pts, trim(s, kCMSampleBufferAttachmentKey_TrimDurationAtStart)) : pts;
}

OSStatus CMSampleBufferSetOutputPresentationTimeStamp(CMSampleBufferRef s, CMTime t) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    pthread_mutex_lock(&s->lock);
    s->outputTimeSet = CMTIME_IS_VALID(t);
    s->outputPTS = t;
    pthread_mutex_unlock(&s->lock);
    return noErr;
}

CMTime CMSampleBufferGetOutputDecodeTimeStamp(CMSampleBufferRef s) {
    CMTime dts = CMSampleBufferGetDecodeTimeStamp(s);
    if (!CMTIME_IS_NUMERIC(dts))
        return dts;
    CMTime shift = CMTimeSubtract(CMSampleBufferGetOutputPresentationTimeStamp(s), CMSampleBufferGetPresentationTimeStamp(s));
    return CMTIME_IS_NUMERIC(shift) ? CMTimeAdd(dts, shift) : dts;
}

static OSStatus timingArray(CMSampleBufferRef s, Boolean output, CMItemCount entries, CMSampleTimingInfo *out,
                            CMItemCount *needed) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (needed != NULL)
        *needed = s->timingCount;
    if (s->timingCount == 0)
        return kCMSampleBufferError_BufferHasNoSampleTimingInfo;
    if (out == NULL)
        return noErr;
    if (entries < s->timingCount)
        return kCMSampleBufferError_ArrayTooSmall;
    memcpy(out, s->timing, (size_t)s->timingCount * sizeof(CMSampleTimingInfo));
    if (output) {
        CMTime shift = CMTimeSubtract(CMSampleBufferGetOutputPresentationTimeStamp(s), CMSampleBufferGetPresentationTimeStamp(s));
        for (CMItemCount i = 0; i < s->timingCount && CMTIME_IS_NUMERIC(shift); i++) {
            out[i].presentationTimeStamp = CMTimeAdd(out[i].presentationTimeStamp, shift);
            if (CMTIME_IS_NUMERIC(out[i].decodeTimeStamp))
                out[i].decodeTimeStamp = CMTimeAdd(out[i].decodeTimeStamp, shift);
        }
    }
    return noErr;
}

OSStatus CMSampleBufferGetSampleTimingInfoArray(CMSampleBufferRef s, CMItemCount entries, CMSampleTimingInfo *out,
                                                CMItemCount *needed) {
    return timingArray(s, false, entries, out, needed);
}

OSStatus CMSampleBufferGetOutputSampleTimingInfoArray(CMSampleBufferRef s, CMItemCount entries,
                                                      CMSampleTimingInfo *out, CMItemCount *needed) {
    return timingArray(s, true, entries, out, needed);
}

OSStatus CMSampleBufferGetSampleTimingInfo(CMSampleBufferRef s, CMItemIndex index, CMSampleTimingInfo *out) {
    if (s == NULL || out == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (index < 0 || index >= s->numSamples)
        return kCMSampleBufferError_SampleIndexOutOfRange;
    if (s->timingCount == 0)
        return kCMSampleBufferError_BufferHasNoSampleTimingInfo;
    *out = timingAt(s, index);
    return noErr;
}

OSStatus CMSampleBufferGetSampleSizeArray(CMSampleBufferRef s, CMItemCount entries, size_t *out, CMItemCount *needed) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (needed != NULL)
        *needed = s->sizeCount;
    if (s->sizeCount == 0)
        return kCMSampleBufferError_BufferHasNoSampleSizes;
    if (out == NULL)
        return noErr;
    if (entries < s->sizeCount)
        return kCMSampleBufferError_ArrayTooSmall;
    memcpy(out, s->sizes, (size_t)s->sizeCount * sizeof(size_t));
    return noErr;
}

size_t CMSampleBufferGetSampleSize(CMSampleBufferRef s, CMItemIndex index) {
    if (s == NULL || index < 0 || index >= s->numSamples)
        return 0;
    return sizeAt(s, index);
}

size_t CMSampleBufferGetTotalSampleSize(CMSampleBufferRef s) {
    if (s == NULL || s->sizeCount == 0)
        return 0;
    if (s->sizeCount == 1)
        return s->sizes[0] * (size_t)s->numSamples;
    size_t total = 0;
    for (CMItemCount i = 0; i < s->sizeCount; i++)
        total += s->sizes[i];
    return total;
}

CMFormatDescriptionRef CMSampleBufferGetFormatDescription(CMSampleBufferRef s) {
    return s != NULL ? s->format : NULL;
}

CFArrayRef CMSampleBufferGetSampleAttachmentsArray(CMSampleBufferRef s, Boolean create) {
    if (s == NULL)
        return NULL;
    pthread_mutex_lock(&s->lock);
    if (s->sampleAttachments == NULL && create && s->numSamples > 0) {
        s->sampleAttachments = CFArrayCreateMutable(NULL, s->numSamples, &kCFTypeArrayCallBacks);
        for (CMItemCount i = 0; i < s->numSamples; i++) {
            CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                 &kCFTypeDictionaryValueCallBacks);
            CFArrayAppendValue(s->sampleAttachments, d);
            CFRelease(d);
        }
    }
    CFArrayRef a = s->sampleAttachments;
    pthread_mutex_unlock(&s->lock);
    return a;
}

OSStatus CMSampleBufferCallForEachSample(CMSampleBufferRef s,
                                         OSStatus (*callback)(CMSampleBufferRef, CMItemCount, void *), void *refcon) {
    if (s == NULL || callback == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    for (CMItemCount i = 0; i < s->numSamples; i++) {
        CMSampleBufferRef one = NULL;
        OSStatus err = CMSampleBufferCopySampleBufferForRange(NULL, s, CFRangeMake(i, 1), &one);
        if (err != noErr)
            return err;
        err = callback(one, i, refcon);
        CFRelease(one);
        if (err != noErr)
            return err;
    }
    return noErr;
}

static OSStatus callBlock(CMSampleBufferRef one, CMItemCount i, void *refcon) {
    OSStatus (^handler)(CMSampleBufferRef, CMItemCount) = (OSStatus (^)(CMSampleBufferRef, CMItemCount))refcon;
    return handler(one, i);
}

OSStatus CMSampleBufferCallBlockForEachSample(CMSampleBufferRef s, OSStatus (^handler)(CMSampleBufferRef, CMItemCount)) {
    if (handler == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    return CMSampleBufferCallForEachSample(s, callBlock, (void *)handler);
}

/* ---- Audio data ---- */

OSStatus CMSampleBufferGetAudioStreamPacketDescriptionsPtr(CMSampleBufferRef s,
                                                           const AudioStreamPacketDescription **out, size_t *size) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (out != NULL)
        *out = s->packets;
    if (size != NULL)
        *size = (size_t)s->packetCount * sizeof(AudioStreamPacketDescription);
    return noErr;
}

OSStatus CMSampleBufferGetAudioStreamPacketDescriptions(CMSampleBufferRef s, size_t size,
                                                        AudioStreamPacketDescription *out, size_t *needed) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    size_t have = (size_t)s->packetCount * sizeof(AudioStreamPacketDescription);
    if (needed != NULL)
        *needed = have;
    if (out == NULL)
        return noErr;
    if (size < have)
        return kCMSampleBufferError_ArrayTooSmall;
    memcpy(out, s->packets, have);
    return noErr;
}

/* Interleaved linear PCM: one buffer; non-interleaved: one per channel,
   the data buffer holding them one after the other. */
static UInt32 bufferCount(const AudioStreamBasicDescription *asbd) {
    return (asbd->mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? asbd->mChannelsPerFrame : 1;
}

OSStatus CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(CMSampleBufferRef s, size_t *neededOut,
                                                                 AudioBufferList *list, size_t listSize,
                                                                 CFAllocatorRef structureAllocator,
                                                                 CFAllocatorRef blockAllocator, uint32_t flags,
                                                                 CMBlockBufferRef *blockOut) {
    if (s == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (!isLinearPCM(s->format))
        return kCMSampleBufferError_InvalidMediaTypeForOperation;
    const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription(s->format);
    UInt32 buffers = bufferCount(asbd);
    size_t needed = offsetof(AudioBufferList, mBuffers) + buffers * sizeof(AudioBuffer);
    if (neededOut != NULL)
        *neededOut = needed;
    if (list == NULL)
        return noErr;
    if (listSize < needed)
        return kCMSampleBufferError_ArrayTooSmall;
    if (blockOut == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (!CMSampleBufferDataIsReady(s))
        return kCMSampleBufferError_BufferNotReady;
    if (s->dataBuffer == NULL)
        return kCMSampleBufferError_InvalidSampleData;
    CMBlockBufferRef flat = NULL;
    OSStatus err = CMBlockBufferCreateContiguous(structureAllocator, s->dataBuffer, blockAllocator, NULL, 0, 0, 0, &flat);
    if (err != noErr)
        return err;
    char *bytes = NULL;
    size_t total = 0;
    CMBlockBufferGetDataPointer(flat, 0, NULL, &total, &bytes);
    size_t perBuffer = total / buffers;
    list->mNumberBuffers = buffers;
    for (UInt32 i = 0; i < buffers; i++) {
        list->mBuffers[i].mNumberChannels = buffers == 1 ? asbd->mChannelsPerFrame : 1;
        list->mBuffers[i].mDataByteSize = (UInt32)perBuffer;
        list->mBuffers[i].mData = bytes + i * perBuffer;
    }
    *blockOut = flat;
    return noErr;
}

OSStatus CMSampleBufferSetDataBufferFromAudioBufferList(CMSampleBufferRef s, CFAllocatorRef structureAllocator,
                                                        CFAllocatorRef blockAllocator, uint32_t flags,
                                                        const AudioBufferList *list) {
    if (s == NULL || list == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (!isLinearPCM(s->format))
        return kCMSampleBufferError_InvalidMediaTypeForOperation;
    if (s->dataBuffer != NULL)
        return kCMSampleBufferError_AlreadyHasDataBuffer;
    const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription(s->format);
    if (list->mNumberBuffers != bufferCount(asbd))
        return kCMSampleBufferError_InvalidEntryCount;
    size_t perBuffer = (size_t)s->numSamples * asbd->mBytesPerFrame, total = perBuffer * list->mNumberBuffers;
    for (UInt32 i = 0; i < list->mNumberBuffers; i++)
        if (list->mBuffers[i].mDataByteSize < perBuffer)
            return kCMSampleBufferError_InvalidSampleData;
    CMBlockBufferRef data = NULL;
    OSStatus err = CMBlockBufferCreateWithMemoryBlock(structureAllocator, NULL, total, blockAllocator, NULL, 0, total,
                                                      kCMBlockBufferAssureMemoryNowFlag, &data);
    if (err != noErr)
        return err;
    for (UInt32 i = 0; i < list->mNumberBuffers && err == noErr; i++)
        err = CMBlockBufferReplaceDataBytes(list->mBuffers[i].mData, data, i * perBuffer, perBuffer);
    if (err == noErr)
        err = CMSampleBufferSetDataBuffer(s, data);
    CFRelease(data);
    return err;
}

OSStatus CMSampleBufferCopyPCMDataIntoAudioBufferList(CMSampleBufferRef s, int32_t frameOffset, int32_t numFrames,
                                                      AudioBufferList *list) {
    if (s == NULL || list == NULL)
        return kCMSampleBufferError_RequiredParameterMissing;
    if (!isLinearPCM(s->format))
        return kCMSampleBufferError_InvalidMediaTypeForOperation;
    if (frameOffset < 0 || numFrames < 0 || frameOffset + numFrames > s->numSamples)
        return kCMSampleBufferError_SampleIndexOutOfRange;
    if (!CMSampleBufferDataIsReady(s))
        return kCMSampleBufferError_BufferNotReady;
    const AudioStreamBasicDescription *asbd = CMAudioFormatDescriptionGetStreamBasicDescription(s->format);
    UInt32 buffers = bufferCount(asbd);
    if (list->mNumberBuffers != buffers || s->dataBuffer == NULL)
        return kCMSampleBufferError_InvalidEntryCount;
    size_t perBuffer = (size_t)s->numSamples * asbd->mBytesPerFrame;
    size_t bytes = (size_t)numFrames * asbd->mBytesPerFrame, start = (size_t)frameOffset * asbd->mBytesPerFrame;
    for (UInt32 i = 0; i < buffers; i++) {
        if (list->mBuffers[i].mDataByteSize < bytes)
            return kCMSampleBufferError_ArrayTooSmall;
        OSStatus err = CMBlockBufferCopyDataBytes(s->dataBuffer, i * perBuffer + start, bytes, list->mBuffers[i].mData);
        if (err != noErr)
            return err;
        list->mBuffers[i].mDataByteSize = (UInt32)bytes;
    }
    return noErr;
}
