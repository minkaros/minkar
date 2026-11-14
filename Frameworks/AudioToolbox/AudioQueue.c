/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Output audio queues for linear PCM, over a default output unit: the
 * unit's render callback takes samples from the buffers in the order they
 * were enqueued (through a processing tap when there is one), and each
 * played buffer goes back to the app through its callback, on the run
 * loop or dispatch queue it gave or on the queue's own. Offline, the app
 * pulls the samples itself, converted to a format of its choosing.
 * The unit converts the queue's format and rate to the device's. An input
 * queue records from the default input through a HAL unit, converted to
 * its format, into the buffers in turn. Compressed formats come with the
 * codecs (AU4).
 */

#include "AUInternal.h"
#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioQueue.h>
#include <Block.h>
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#include <mach/mach_time.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define kQueueMagic     0x41517565u    /* 'AQue' */
#define kMaxListeners   8
#define kMaxMeterChannels 8

typedef struct QueueBuffer {
    AudioQueueBuffer buffer;    /* first: the app's AudioQueueBufferRef */
    struct QueueBuffer *nextAllocated;
    struct QueueBuffer *nextQueued;
    UInt32 offset;              /* bytes played */
    UInt32 trimEnd;             /* bytes not to play at the end */
    Float64 startTime;          /* an input buffer's first frame, in the queue's time */
    Boolean queued;
} QueueBuffer;

struct OpaqueAudioQueue {
    uint32_t magic;
    AudioStreamBasicDescription format;
    AudioQueueOutputCallback callback;
    void *userData;
    CFRunLoopRef runLoop;
    CFStringRef mode;
    dispatch_queue_t own;       /* when there is no run loop */
    dispatch_queue_t userQueue; /* the app's, for its block */
    AudioQueueOutputCallbackBlock outputBlock;

    AudioUnit unit;
    pthread_mutex_t lock;       /* the buffers and state */
    QueueBuffer *allocated;
    QueueBuffer *head, *tail;
    Boolean running, paused, stopping, metering;
    volatile Boolean disposed;
    int refs;                   /* the app's, and one per pending block */
    Float64 playedFrames;       /* since start, in the queue's rate */
    Float64 latency;            /* seconds from a render to the speaker */
    SInt64 tailFrames;          /* silence still to give once drained */
    Boolean draining;
    UInt64 startHostTime;
    UInt32 decodeFrames;
    Float32 volume;

    struct {
        AudioQueuePropertyID id;
        AudioQueuePropertyListenerProc proc;
        void *userData;
    } listeners[kMaxListeners];
    int listenerCount;

    AudioQueueLevelMeterState meters[kMaxMeterChannels];

    /* An input queue: the HAL unit records, the converter takes its frames
       to the queue's format, into the buffers in turn. */
    Boolean isInput;
    AudioQueueInputCallback inputCallback;
    AudioQueueInputCallbackBlock inputBlock;
    AudioConverterRef inConverter;
    float *inFrames;            /* a cycle's recording, float stereo at the device's rate */
    UInt32 inFrameCount;
    Boolean inFramesGiven;
    Float64 recordedFrames;     /* in the queue's rate, since start */
    UInt64 overruns;            /* cycles with no buffer to fill */

    struct OpaqueAudioQueueProcessingTap *tap;
    Boolean offline;            /* rendered by AudioQueueOfflineRender, not the device */
    AudioStreamBasicDescription offlineFormat;
    AudioConverterRef offlineConverter;
    unsigned char *offlineScratch;
    UInt32 offlineScratchFrames;
};

/* A processing tap: the queue's samples, as float planar, go through the
   app's callback between the buffers and the output. */
struct OpaqueAudioQueueProcessingTap {
    AudioQueueRef queue;
    AudioQueueProcessingTapCallback callback;
    void *clientData;
    AudioQueueProcessingTapFlags flags;
    UInt32 maxFrames;
    AudioStreamBasicDescription format;     /* float, planar, the queue's rate and channels */
    AudioBufferList *list;      /* the callback's buffers */
    float *planes;
    unsigned char *scratch;     /* the queue's samples, in its format */
    float *interleaved;
    UInt32 frames;              /* this cycle's */
    Boolean drained;
    Boolean siphoned;           /* a siphon's source, already taken */
    UInt32 siphonFrames;
};

struct OpaqueAudioQueueTimeline {
    AudioQueueRef queue;
};

static Boolean alive(AudioQueueRef q) {
    return q != NULL && q->magic == kQueueMagic && !q->disposed;
}

static void release(AudioQueueRef q) {
    if (__atomic_sub_fetch(&q->refs, 1, __ATOMIC_ACQ_REL) == 0) {
        if (q->runLoop != NULL) {
            CFRelease(q->runLoop);
            CFRelease(q->mode);
        }
        if (q->own != NULL)
            dispatch_release(q->own);
        if (q->userQueue != NULL)
            dispatch_release(q->userQueue);
        if (q->outputBlock != NULL)
            Block_release(q->outputBlock);
        if (q->inputBlock != NULL)
            Block_release(q->inputBlock);
        q->magic = 0;
        pthread_mutex_destroy(&q->lock);
        free(q);
    }
}

/* Runs a block where the app's callbacks run; the queue's memory stays
   until it has run (it looks at disposed first). */
static void onCallbackThread(AudioQueueRef q, void (^block)(void)) {
    __atomic_add_fetch(&q->refs, 1, __ATOMIC_ACQ_REL);
    if (q->userQueue != NULL) {
        dispatch_async(q->userQueue, ^{
            block();
            release(q);
        });
    } else if (q->runLoop != NULL) {
        CFRunLoopPerformBlock(q->runLoop, q->mode, ^{
            block();
            release(q);
        });
        CFRunLoopWakeUp(q->runLoop);
    } else {
        dispatch_async(q->own, ^{
            block();
            release(q);
        });
    }
}

static void notifyListeners(AudioQueueRef q, AudioQueuePropertyID id) {
    AudioQueuePropertyListenerProc procs[kMaxListeners];
    void *data[kMaxListeners];
    int n = 0;

    pthread_mutex_lock(&q->lock);
    for (int i = 0; i < q->listenerCount; i++)
        if (q->listeners[i].id == id) {
            procs[n] = q->listeners[i].proc;
            data[n++] = q->listeners[i].userData;
        }
    pthread_mutex_unlock(&q->lock);
    for (int i = 0; i < n; i++)
        procs[i](data[i], q, id);
}

static void returnBuffer(AudioQueueRef q, QueueBuffer *b) {
    onCallbackThread(q, ^{
        if (q->disposed)
            return;
        if (q->isInput) {
            AudioTimeStamp start;
            memset(&start, 0, sizeof(start));
            start.mSampleTime = b->startTime;
            start.mFlags = kAudioTimeStampSampleTimeValid;
            if (q->inputBlock != NULL)
                q->inputBlock(q, &b->buffer, &start, 0, NULL);
            else if (q->inputCallback != NULL)
                q->inputCallback(q->userData, q, &b->buffer, &start, 0, NULL);
            return;
        }
        if (q->outputBlock != NULL)
            q->outputBlock(q, &b->buffer);
        else if (q->callback != NULL)
            q->callback(q->userData, q, &b->buffer);
    });
}

static void stopUnit(AudioQueueRef q);

static void finishStopping(AudioQueueRef q) {
    onCallbackThread(q, ^{
        Boolean was;
        if (q->disposed)
            return;
        AudioOutputUnitStop(q->unit);
        pthread_mutex_lock(&q->lock);
        was = q->running;
        q->running = false;
        q->stopping = false;
        pthread_mutex_unlock(&q->lock);
        if (was)
            notifyListeners(q, kAudioQueueProperty_IsRunning);
    });
}

/* ---- Rendering (the device's IO thread) ------------------------------------ */

static void meter(AudioQueueRef q, const AudioBufferList *data, UInt32 frames) {
    UInt32 ch = q->format.mChannelsPerFrame < kMaxMeterChannels ? q->format.mChannelsPerFrame : kMaxMeterChannels;
    float *f = malloc((size_t)frames * q->format.mChannelsPerFrame * sizeof(float));

    if (f == NULL || frames == 0) {
        free(f);
        return;
    }
    _AUToFloat(&q->format, data, frames, f, q->format.mChannelsPerFrame);
    for (UInt32 c = 0; c < ch; c++) {
        double sum = 0;
        float peak = 0;
        for (UInt32 i = 0; i < frames; i++) {
            float v = fabsf(f[i * q->format.mChannelsPerFrame + c]);
            sum += (double)v * v;
            if (v > peak)
                peak = v;
        }
        q->meters[c].mAveragePower = (Float32)sqrt(sum / frames);
        q->meters[c].mPeakPower = peak;
    }
    free(f);
}

/* Samples from the queued buffers into out (the queue's format), silence
   after them; buffers used up go back. Returns the frames taken; *drained
   when a stop that plays out has played out. */
static UInt32 source(AudioQueueRef q, unsigned char *out, UInt32 frames, Boolean *drained) {
    UInt32 bpf = q->format.mBytesPerFrame, want = frames * bpf, done = 0;
    QueueBuffer *finished = NULL, **finishedTail = &finished;

    pthread_mutex_lock(&q->lock);
    if (!q->paused) {
        while (done < want && q->head != NULL) {
            QueueBuffer *b = q->head;
            UInt32 end = b->buffer.mAudioDataByteSize > b->trimEnd ? b->buffer.mAudioDataByteSize - b->trimEnd : 0;
            UInt32 n = end > b->offset ? end - b->offset : 0;

            if (n > want - done)
                n = want - done;
            n -= n % bpf;
            memcpy(out + done, (unsigned char *)b->buffer.mAudioData + b->offset, n);
            b->offset += n;
            done += n;
            if (end - b->offset < bpf) {
                q->head = b->nextQueued;
                if (q->head == NULL)
                    q->tail = NULL;
                b->queued = false;
                b->nextQueued = NULL;
                *finishedTail = b;
                finishedTail = &b->nextQueued;
            }
        }
        q->playedFrames += done / bpf;
        if (q->stopping && q->head == NULL) {
            /* What was handed out last is still on its way to the
               speaker: keep the device running until it has played. */
            if (!q->draining) {
                q->draining = true;
                q->tailFrames = q->offline ? 0
                                : (SInt64)(q->latency * q->format.mSampleRate) + 2 * (SInt64)frames;
            }
            q->tailFrames -= frames - done / bpf;
            *drained = q->tailFrames <= 0;
        }
    }
    pthread_mutex_unlock(&q->lock);

    if (done < want)
        memset(out + done, 0, want - done);
    while (finished != NULL) {
        QueueBuffer *next = finished->nextQueued;
        finished->nextQueued = NULL;
        returnBuffer(q, finished);
        finished = next;
    }
    return done / bpf;
}

/* The queue's samples, interleaved in its format, to float planes. */
static void toPlanes(struct OpaqueAudioQueueProcessingTap *t, const unsigned char *in, UInt32 frames,
                     AudioBufferList *out) {
    AudioQueueRef q = t->queue;
    UInt32 ch = q->format.mChannelsPerFrame;
    AudioBufferList in1 = { 1, { { ch, frames * q->format.mBytesPerFrame, (void *)in } } };

    _AUToFloat(&q->format, &in1, frames, t->interleaved, ch);
    for (UInt32 c = 0; c < ch && c < out->mNumberBuffers; c++) {
        float *plane = out->mBuffers[c].mData;
        for (UInt32 i = 0; i < frames; i++)
            plane[i] = t->interleaved[i * ch + c];
        out->mBuffers[c].mDataByteSize = frames * sizeof(float);
    }
}

/* Float planes back to the queue's format. */
static void fromPlanes(struct OpaqueAudioQueueProcessingTap *t, const AudioBufferList *in, UInt32 frames,
                       unsigned char *out) {
    AudioQueueRef q = t->queue;
    UInt32 ch = q->format.mChannelsPerFrame;
    AudioBufferList out1 = { 1, { { ch, frames * q->format.mBytesPerFrame, out } } };

    for (UInt32 c = 0; c < ch; c++) {
        const float *plane = c < in->mNumberBuffers ? in->mBuffers[c].mData : NULL;
        for (UInt32 i = 0; i < frames; i++)
            t->interleaved[i * ch + c] = plane != NULL ? plane[i] : 0;
    }
    _AUFromFloat(&q->format, t->interleaved, frames, &out1, 0);
}

/* A cycle's frames in the queue's format: from the buffers, through the
   tap when there is one. Returns the frames that were the app's. */
static UInt32 produce(AudioQueueRef q, unsigned char *out, UInt32 frames, const AudioTimeStamp *time,
                      Boolean *drained) {
    struct OpaqueAudioQueueProcessingTap *t = q->tap;
    AudioTimeStamp ts;
    AudioQueueProcessingTapFlags flags = 0;
    UInt32 got = 0, n;

    if (t == NULL || frames > t->maxFrames)
        return source(q, out, frames, drained);
    memset(&ts, 0, sizeof(ts));
    ts.mSampleTime = q->playedFrames;
    ts.mHostTime = time != NULL ? time->mHostTime : 0;
    ts.mFlags = kAudioTimeStampSampleHostTimeValid;
    t->frames = frames;
    t->drained = false;
    t->siphoned = false;
    for (UInt32 c = 0; c < t->list->mNumberBuffers; c++) {
        t->list->mBuffers[c].mData = t->planes + (size_t)c * t->maxFrames;
        t->list->mBuffers[c].mDataByteSize = frames * sizeof(float);
    }
    if (t->flags & kAudioQueueProcessingTap_Siphon) {
        /* The app looks, the samples go on unchanged. */
        got = source(q, out, frames, drained);
        toPlanes(t, out, frames, t->list);
        t->siphoned = true;
        t->siphonFrames = frames;
        n = frames;
        t->callback(t->clientData, t, frames, &ts, &flags, &n, t->list);
        return got;
    }
    n = frames;
    t->callback(t->clientData, t, frames, &ts, &flags, &n, t->list);
    if (n > frames)
        n = frames;
    fromPlanes(t, t->list, n, out);
    if (n < frames)
        memset(out + n * q->format.mBytesPerFrame, 0, (frames - n) * q->format.mBytesPerFrame);
    *drained = t->drained;
    return n;
}

static OSStatus render(void *refCon, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                       UInt32 bus, UInt32 frames, AudioBufferList *data) {
    AudioQueueRef q = refCon;
    Boolean drained = false;
    UInt32 done = produce(q, data->mBuffers[0].mData, frames, time, &drained);

    if (done == 0)
        *flags |= kAudioUnitRenderAction_OutputIsSilence;
    if (q->metering)
        meter(q, data, frames);
    if (drained) {
        pthread_mutex_lock(&q->lock);
        q->stopping = false;    /* once */
        q->draining = false;
        pthread_mutex_unlock(&q->lock);
        finishStopping(q);
    }
    return noErr;
}

/* ---- Life -------------------------------------------------------------------- */

OSStatus AudioQueueNewOutput(const AudioStreamBasicDescription *format, AudioQueueOutputCallback callback,
                             void *userData, CFRunLoopRef runLoop, CFStringRef mode, UInt32 flags,
                             AudioQueueRef *outAQ) {
    AudioComponentDescription d = { kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput,
                                    kAudioUnitManufacturer_Apple, 0, 0 };
    AURenderCallbackStruct cb;
    AudioQueueRef q;
    OSStatus status;

    if (format == NULL || outAQ == NULL)
        return kAudio_ParamError;
    *outAQ = NULL;
    if (format->mFormatID != kAudioFormatLinearPCM)
        return kAudioQueueErr_CodecNotFound;
    if (!_AUFormatIsSupported(format) || (format->mFormatFlags & kAudioFormatFlagIsNonInterleaved))
        return kAudioQueueErr_InvalidPropertyValue;
    q = calloc(1, sizeof(*q));
    if (q == NULL)
        return kAudio_MemFullError;
    q->magic = kQueueMagic;
    q->refs = 1;
    q->format = *format;
    q->callback = callback;
    q->userData = userData;
    q->volume = 1;
    q->decodeFrames = 2048;
    pthread_mutex_init(&q->lock, NULL);
    if (runLoop != NULL) {
        q->runLoop = (CFRunLoopRef)CFRetain(runLoop);
        q->mode = mode != NULL ? CFStringCreateCopy(NULL, mode) : (CFStringRef)CFRetain(kCFRunLoopCommonModes);
    } else {
        q->own = dispatch_queue_create("org.minkar.AudioQueue", DISPATCH_QUEUE_SERIAL);
    }

    status = AudioComponentInstanceNew(AudioComponentFindNext(NULL, &d), &q->unit);
    if (status == noErr)
        status = AudioUnitSetProperty(q->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                                      format, sizeof(*format));
    cb.inputProc = render;
    cb.inputProcRefCon = q;
    if (status == noErr)
        status = AudioUnitSetProperty(q->unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                                      &cb, sizeof(cb));
    if (status == noErr)
        status = AudioUnitInitialize(q->unit);
    if (status != noErr) {
        if (q->unit != NULL)
            AudioComponentInstanceDispose(q->unit);
        if (q->runLoop != NULL) {
            CFRelease(q->runLoop);
            CFRelease(q->mode);
        }
        if (q->own != NULL)
            dispatch_release(q->own);
        pthread_mutex_destroy(&q->lock);
        free(q);
        return status == kAudioUnitErr_FormatNotSupported ? kAudioQueueErr_InvalidPropertyValue
                                                         : kAudioQueueErr_InvalidDevice;
    }
    *outAQ = q;
    return noErr;
}

/* The converter's input: the cycle's recording, once. */
static OSStatus recordingInput(AudioConverterRef c, UInt32 *ioPackets, AudioBufferList *data,
                               AudioStreamPacketDescription **descriptions, void *userData) {
    AudioQueueRef q = userData;

    if (q->inFramesGiven) {
        *ioPackets = 0;
        return 'nomo';      /* used up, not ended */
    }
    data->mNumberBuffers = 1;
    data->mBuffers[0].mNumberChannels = 2;
    data->mBuffers[0].mDataByteSize = q->inFrameCount * 2 * sizeof(float);
    data->mBuffers[0].mData = q->inFrames;
    *ioPackets = q->inFrameCount;
    q->inFramesGiven = true;
    return noErr;
}

/* The HAL unit has recorded a cycle: into the buffers, converted; each
   full one goes back to the app. */
static OSStatus recorded(void *refCon, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                         UInt32 bus, UInt32 frames, AudioBufferList *unused) {
    AudioQueueRef q = refCon;
    struct { UInt32 n; AudioBuffer b; } list = { 1, { 2, frames * 2 * sizeof(float), q->inFrames } };
    UInt32 bpf = q->format.mBytesPerFrame;
    OSStatus status;

    if (frames > 8192)
        return noErr;
    status = AudioUnitRender(q->unit, flags, time, 1, frames, (AudioBufferList *)&list);
    if (status != noErr)
        return status;
    q->inFrameCount = frames;
    q->inFramesGiven = false;
    for (;;) {
        QueueBuffer *b;
        UInt32 room, produced;
        AudioBufferList out;

        pthread_mutex_lock(&q->lock);
        b = q->head;
        pthread_mutex_unlock(&q->lock);
        if (b == NULL || q->paused) {
            q->overruns += b == NULL;
            AudioConverterReset(q->inConverter);
            break;
        }
        if (b->buffer.mAudioDataByteSize == 0)
            b->startTime = q->recordedFrames;
        room = (b->buffer.mAudioDataBytesCapacity - b->buffer.mAudioDataByteSize) / bpf;
        out.mNumberBuffers = 1;
        out.mBuffers[0].mNumberChannels = q->format.mChannelsPerFrame;
        out.mBuffers[0].mDataByteSize = room * bpf;
        out.mBuffers[0].mData = (char *)b->buffer.mAudioData + b->buffer.mAudioDataByteSize;
        produced = room;
        status = AudioConverterFillComplexBuffer(q->inConverter, recordingInput, q, &produced, &out, NULL);
        b->buffer.mAudioDataByteSize += produced * bpf;
        q->recordedFrames += produced;
        if (b->buffer.mAudioDataBytesCapacity - b->buffer.mAudioDataByteSize < bpf) {
            pthread_mutex_lock(&q->lock);
            q->head = b->nextQueued;
            if (q->head == NULL)
                q->tail = NULL;
            b->queued = false;
            b->nextQueued = NULL;
            pthread_mutex_unlock(&q->lock);
            returnBuffer(q, b);
            continue;       /* the rest into the next buffer */
        }
        if (status != noErr || produced == 0)
            break;          /* the cycle is used up */
    }
    return noErr;
}

static OSStatus newInput(const AudioStreamBasicDescription *format, AudioQueueRef *outAQ) {
    AudioComponentDescription d = { kAudioUnitType_Output, kAudioUnitSubType_HALOutput,
                                    kAudioUnitManufacturer_Apple, 0, 0 };
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDefaultInputDevice, kAudioObjectPropertyScopeGlobal,
                                     kAudioObjectPropertyElementMain };
    AudioStreamBasicDescription device;
    AURenderCallbackStruct cb;
    AudioDeviceID input = kAudioObjectUnknown;
    UInt32 one = 1, zero = 0, size = sizeof(input);
    Float64 rate = 0;
    AudioQueueRef q;
    OSStatus status;

    if (format == NULL || outAQ == NULL)
        return kAudio_ParamError;
    *outAQ = NULL;
    if (format->mFormatID != kAudioFormatLinearPCM)
        return kAudioQueueErr_CodecNotFound;
    if (!_AUFormatIsSupported(format) || (format->mFormatFlags & kAudioFormatFlagIsNonInterleaved))
        return kAudioQueueErr_InvalidPropertyValue;
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &input) != noErr ||
        input == kAudioObjectUnknown)
        return kAudioQueueErr_InvalidDevice;
    a.mSelector = kAudioDevicePropertyNominalSampleRate;
    size = sizeof(rate);
    AudioObjectGetPropertyData(input, &a, 0, NULL, &size, &rate);
    if (rate <= 0)
        return kAudioQueueErr_InvalidDevice;
    q = calloc(1, sizeof(*q));
    if (q == NULL)
        return kAudio_MemFullError;
    q->magic = kQueueMagic;
    q->refs = 1;
    q->isInput = true;
    q->format = *format;
    q->volume = 1;
    pthread_mutex_init(&q->lock, NULL);
    q->inFrames = calloc(8192, 2 * sizeof(float));

    memset(&device, 0, sizeof(device));
    device.mSampleRate = rate;
    device.mFormatID = kAudioFormatLinearPCM;
    device.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    device.mBytesPerPacket = device.mBytesPerFrame = 2 * sizeof(float);
    device.mFramesPerPacket = 1;
    device.mChannelsPerFrame = 2;
    device.mBitsPerChannel = 32;
    status = q->inFrames != NULL ? AudioConverterNew(&device, format, &q->inConverter) : kAudio_MemFullError;
    if (status == noErr)
        status = AudioComponentInstanceNew(AudioComponentFindNext(NULL, &d), &q->unit);
    if (status == noErr)
        status = AudioUnitSetProperty(q->unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1,
                                      &one, sizeof(one));
    if (status == noErr)
        status = AudioUnitSetProperty(q->unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0,
                                      &zero, sizeof(zero));
    if (status == noErr)
        status = AudioUnitSetProperty(q->unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0,
                                      &input, sizeof(input));
    if (status == noErr)
        status = AudioUnitSetProperty(q->unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1,
                                      &device, sizeof(device));
    cb.inputProc = recorded;
    cb.inputProcRefCon = q;
    if (status == noErr)
        status = AudioUnitSetProperty(q->unit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0,
                                      &cb, sizeof(cb));
    if (status == noErr)
        status = AudioUnitInitialize(q->unit);
    if (status != noErr) {
        if (q->unit != NULL)
            AudioComponentInstanceDispose(q->unit);
        if (q->inConverter != NULL)
            AudioConverterDispose(q->inConverter);
        free(q->inFrames);
        pthread_mutex_destroy(&q->lock);
        free(q);
        return status == kAudioConverterErr_FormatNotSupported ? kAudioQueueErr_InvalidPropertyValue
                                                               : kAudioQueueErr_InvalidDevice;
    }
    *outAQ = q;
    return noErr;
}

/* An input queue: what the default input records, in the queue's format,
   into the buffers the app enqueues; each comes back full, with the time
   of its first frame. */
OSStatus AudioQueueNewInput(const AudioStreamBasicDescription *format, AudioQueueInputCallback callback,
                            void *userData, CFRunLoopRef runLoop, CFStringRef mode, UInt32 flags,
                            AudioQueueRef *outAQ) {
    OSStatus status = newInput(format, outAQ);
    AudioQueueRef q;

    if (status != noErr)
        return status;
    q = *outAQ;
    q->inputCallback = callback;
    q->userData = userData;
    if (runLoop != NULL) {
        q->runLoop = (CFRunLoopRef)CFRetain(runLoop);
        q->mode = mode != NULL ? CFStringCreateCopy(NULL, mode) : (CFStringRef)CFRetain(kCFRunLoopCommonModes);
    } else {
        q->own = dispatch_queue_create("org.minkar.AudioQueue", DISPATCH_QUEUE_SERIAL);
    }
    return noErr;
}

static void stopUnit(AudioQueueRef q) {
    AudioOutputUnitStop(q->unit);
}

OSStatus AudioQueueDispose(AudioQueueRef q, Boolean immediate) {
    QueueBuffer *b;

    if (!alive(q))
        return kAudio_ParamError;
    if (!immediate) {
        /* Let what is queued play out first. */
        Float64 seconds = 0;
        pthread_mutex_lock(&q->lock);
        if (q->running && !q->paused)
            for (b = q->head; b != NULL; b = b->nextQueued)
                seconds += (b->buffer.mAudioDataByteSize - b->offset) / (Float64)q->format.mBytesPerFrame /
                           q->format.mSampleRate;
        pthread_mutex_unlock(&q->lock);
        for (int waited = 0; waited < (int)(seconds * 100) + 50; waited++) {
            Boolean empty;
            pthread_mutex_lock(&q->lock);
            empty = q->head == NULL;
            pthread_mutex_unlock(&q->lock);
            if (empty)
                break;
            usleep(10000);
        }
    }
    q->disposed = true;
    stopUnit(q);
    AudioComponentInstanceDispose(q->unit);
    q->unit = NULL;
    if (q->tap != NULL) {
        struct OpaqueAudioQueueProcessingTap *t = q->tap;
        q->tap = NULL;
        free(t->list);
        free(t->planes);
        free(t->interleaved);
        free(t->scratch);
        free(t);
    }
    if (q->offlineConverter != NULL)
        AudioConverterDispose(q->offlineConverter);
    free(q->offlineScratch);
    if (q->inConverter != NULL)
        AudioConverterDispose(q->inConverter);
    free(q->inFrames);

    /* Blocks on the queue's own dispatch queue are let finish before the
       buffers go; any on the app's run loop find disposed set. */
    if (q->own != NULL)
        dispatch_sync(q->own, ^{});
    for (b = q->allocated; b != NULL;) {
        QueueBuffer *next = b->nextAllocated;
        free(b->buffer.mAudioData);
        free(b->buffer.mPacketDescriptions);
        free(b);
        b = next;
    }
    q->allocated = q->head = q->tail = NULL;
    release(q);
    return noErr;
}

/* ---- Buffers ----------------------------------------------------------------- */

OSStatus AudioQueueAllocateBufferWithPacketDescriptions(AudioQueueRef q, UInt32 size, UInt32 descriptions,
                                                        AudioQueueBufferRef *out) {
    QueueBuffer *b;
    void *data, *pd = NULL;

    if (!alive(q) || out == NULL)
        return kAudio_ParamError;
    if (size == 0)
        return kAudioQueueErr_BufferEmpty;
    b = calloc(1, sizeof(*b));
    data = calloc(1, size);
    if (descriptions > 0)
        pd = calloc(descriptions, sizeof(AudioStreamPacketDescription));
    if (b == NULL || data == NULL || (descriptions > 0 && pd == NULL)) {
        free(b);
        free(data);
        free(pd);
        return kAudio_MemFullError;
    }
    /* The buffer's const fields are set once, here. */
    *(UInt32 *)&b->buffer.mAudioDataBytesCapacity = size;
    *(void **)&b->buffer.mAudioData = data;
    *(UInt32 *)&b->buffer.mPacketDescriptionCapacity = descriptions;
    *(AudioStreamPacketDescription **)&b->buffer.mPacketDescriptions = pd;
    pthread_mutex_lock(&q->lock);
    b->nextAllocated = q->allocated;
    q->allocated = b;
    pthread_mutex_unlock(&q->lock);
    *out = &b->buffer;
    return noErr;
}

OSStatus AudioQueueAllocateBuffer(AudioQueueRef q, UInt32 size, AudioQueueBufferRef *out) {
    return AudioQueueAllocateBufferWithPacketDescriptions(q, size, 0, out);
}

static QueueBuffer *findBuffer(AudioQueueRef q, AudioQueueBufferRef ref) {
    for (QueueBuffer *b = q->allocated; b != NULL; b = b->nextAllocated)
        if (&b->buffer == ref)
            return b;
    return NULL;
}

OSStatus AudioQueueFreeBuffer(AudioQueueRef q, AudioQueueBufferRef ref) {
    QueueBuffer *b, **link;

    if (!alive(q))
        return kAudio_ParamError;
    pthread_mutex_lock(&q->lock);
    b = findBuffer(q, ref);
    if (b == NULL || b->queued) {
        pthread_mutex_unlock(&q->lock);
        return b == NULL ? kAudioQueueErr_InvalidBuffer : kAudioQueueErr_BufferInQueue;
    }
    for (link = &q->allocated; *link != b; link = &(*link)->nextAllocated)
        ;
    *link = b->nextAllocated;
    pthread_mutex_unlock(&q->lock);
    free(b->buffer.mAudioData);
    free(b->buffer.mPacketDescriptions);
    free(b);
    return noErr;
}

OSStatus AudioQueueEnqueueBufferWithParameters(AudioQueueRef q, AudioQueueBufferRef ref, UInt32 numDescs,
                                               const AudioStreamPacketDescription *descs, UInt32 trimStart,
                                               UInt32 trimEnd, UInt32 numParams,
                                               const AudioQueueParameterEvent *params,
                                               const AudioTimeStamp *startTime, AudioTimeStamp *actualStart) {
    QueueBuffer *b;
    UInt32 bpf;

    if (!alive(q))
        return kAudio_ParamError;
    bpf = q->format.mBytesPerFrame;
    for (UInt32 i = 0; i < numParams; i++)
        AudioQueueSetParameter(q, params[i].mID, params[i].mValue);
    pthread_mutex_lock(&q->lock);
    b = findBuffer(q, ref);
    if (b == NULL || b->queued) {
        pthread_mutex_unlock(&q->lock);
        return b == NULL ? kAudioQueueErr_InvalidBuffer : kAudioQueueErr_BufferEnqueuedTwice;
    }
    if (q->isInput) {
        /* To be filled: from its start. */
        ref->mAudioDataByteSize = 0;
    } else if (ref->mAudioDataByteSize == 0 || ref->mAudioDataByteSize > ref->mAudioDataBytesCapacity) {
        pthread_mutex_unlock(&q->lock);
        return kAudioQueueErr_BufferEmpty;
    }
    b->offset = trimStart * bpf < ref->mAudioDataByteSize ? trimStart * bpf : ref->mAudioDataByteSize;
    b->trimEnd = trimEnd * bpf;
    b->queued = true;
    b->nextQueued = NULL;
    if (q->tail != NULL)
        q->tail->nextQueued = b;
    else
        q->head = b;
    q->tail = b;
    pthread_mutex_unlock(&q->lock);
    if (actualStart != NULL) {
        memset(actualStart, 0, sizeof(*actualStart));
        actualStart->mSampleTime = q->playedFrames;
        actualStart->mFlags = kAudioTimeStampSampleTimeValid;
    }
    return noErr;
}

OSStatus AudioQueueEnqueueBuffer(AudioQueueRef q, AudioQueueBufferRef ref, UInt32 numDescs,
                                 const AudioStreamPacketDescription *descs) {
    return AudioQueueEnqueueBufferWithParameters(q, ref, numDescs, descs, 0, 0, 0, NULL, NULL, NULL);
}

/* ---- Running ----------------------------------------------------------------- */

OSStatus AudioQueueStart(AudioQueueRef q, const AudioTimeStamp *startTime) {
    Boolean was;
    OSStatus status = noErr;

    if (!alive(q))
        return kAudio_ParamError;
    pthread_mutex_lock(&q->lock);
    was = q->running;
    q->paused = false;
    q->stopping = false;
    if (!was) {
        q->playedFrames = 0;
        q->recordedFrames = 0;
        q->startHostTime = mach_absolute_time();
    }
    q->draining = false;
    pthread_mutex_unlock(&q->lock);
    if (!was) {
        UInt32 size = sizeof(q->latency);
        if (AudioUnitGetProperty(q->unit, kAudioUnitProperty_Latency, kAudioUnitScope_Global, 0, &q->latency,
                                 &size) != noErr)
            q->latency = 0;
        /* Offline, AudioQueueOfflineRender pulls the samples, not the device. */
        status = q->offline ? noErr : AudioOutputUnitStart(q->unit);
        if (status != noErr)
            return kAudioQueueErr_CannotStart;
        pthread_mutex_lock(&q->lock);
        q->running = true;
        pthread_mutex_unlock(&q->lock);
        onCallbackThread(q, ^{
            if (!q->disposed)
                notifyListeners(q, kAudioQueueProperty_IsRunning);
        });
    }
    return status;
}

OSStatus AudioQueuePrime(AudioQueueRef q, UInt32 frames, UInt32 *outPrepared) {
    UInt32 queued = 0;

    if (!alive(q))
        return kAudio_ParamError;
    pthread_mutex_lock(&q->lock);
    for (QueueBuffer *b = q->head; b != NULL; b = b->nextQueued)
        queued += (b->buffer.mAudioDataByteSize - b->offset) / q->format.mBytesPerFrame;
    pthread_mutex_unlock(&q->lock);
    if (outPrepared != NULL)
        *outPrepared = frames != 0 && frames < queued ? frames : queued;
    return noErr;
}

/* Gives back every queued buffer, in order. */
static void returnAll(AudioQueueRef q) {
    QueueBuffer *b;

    pthread_mutex_lock(&q->lock);
    b = q->head;
    q->head = q->tail = NULL;
    pthread_mutex_unlock(&q->lock);
    while (b != NULL) {
        QueueBuffer *next = b->nextQueued;
        b->queued = false;
        b->nextQueued = NULL;
        returnBuffer(q, b);
        b = next;
    }
}

OSStatus AudioQueueStop(AudioQueueRef q, Boolean immediate) {
    Boolean was;

    if (!alive(q))
        return kAudio_ParamError;
    if (!immediate && !q->isInput) {
        pthread_mutex_lock(&q->lock);
        if (q->running && q->head != NULL && !q->paused) {
            q->stopping = true;     /* render stops the queue when drained */
            pthread_mutex_unlock(&q->lock);
            return noErr;
        }
        pthread_mutex_unlock(&q->lock);
    }
    stopUnit(q);
    pthread_mutex_lock(&q->lock);
    if (q->isInput)
        for (QueueBuffer *b = q->head; b != NULL; b = b->nextQueued)
            if (b->buffer.mAudioDataByteSize == 0)
                b->startTime = q->recordedFrames;  /* empty: nothing recorded into it */
    was = q->running;
    q->running = q->paused = q->stopping = q->draining = false;
    pthread_mutex_unlock(&q->lock);
    returnAll(q);
    if (was)
        onCallbackThread(q, ^{
            if (!q->disposed)
                notifyListeners(q, kAudioQueueProperty_IsRunning);
        });
    return noErr;
}

OSStatus AudioQueuePause(AudioQueueRef q) {
    if (!alive(q))
        return kAudio_ParamError;
    pthread_mutex_lock(&q->lock);
    q->paused = true;
    pthread_mutex_unlock(&q->lock);
    return noErr;
}

OSStatus AudioQueueFlush(AudioQueueRef q) {
    /* Nothing is held back from the device: the buffers play as given. */
    return alive(q) ? noErr : kAudio_ParamError;
}

OSStatus AudioQueueReset(AudioQueueRef q) {
    if (!alive(q))
        return kAudio_ParamError;
    returnAll(q);
    AudioUnitReset(q->unit, kAudioUnitScope_Global, 0);
    return noErr;
}

/* ---- Parameters and properties ------------------------------------------------ */

OSStatus AudioQueueGetParameter(AudioQueueRef q, AudioQueueParameterID id, AudioQueueParameterValue *value) {
    if (!alive(q) || value == NULL)
        return kAudio_ParamError;
    switch (id) {
    case kAudioQueueParam_Volume:
        *value = q->volume;
        return noErr;
    case kAudioQueueParam_PlayRate:
        *value = 1;
        return noErr;
    case kAudioQueueParam_Pitch:
    case kAudioQueueParam_VolumeRampTime:
    case kAudioQueueParam_Pan:
        *value = 0;
        return noErr;
    default:
        return kAudioQueueErr_InvalidParameter;
    }
}

OSStatus AudioQueueSetParameter(AudioQueueRef q, AudioQueueParameterID id, AudioQueueParameterValue value) {
    if (!alive(q))
        return kAudio_ParamError;
    switch (id) {
    case kAudioQueueParam_Volume:
        if (!(value >= 0 && value <= 1))
            return kAudioQueueErr_InvalidParameter;
        q->volume = value;
        return AudioUnitSetParameter(q->unit, kHALOutputParam_Volume, kAudioUnitScope_Global, 0, value, 0);
    case kAudioQueueParam_VolumeRampTime:
    case kAudioQueueParam_Pan:
        return noErr;
    case kAudioQueueParam_PlayRate:
    case kAudioQueueParam_Pitch:
        /* Time and pitch changes are not there yet: only the normal rate. */
        return id == kAudioQueueParam_PlayRate && value == 1 ? noErr : kAudioQueueErr_InvalidParameter;
    default:
        return kAudioQueueErr_InvalidParameter;
    }
}

static UInt32 propertySize(AudioQueueRef q, AudioQueuePropertyID id) {
    switch (id) {
    case kAudioQueueProperty_IsRunning:
    case kAudioQueueDeviceProperty_NumberChannels:
    case kAudioQueueProperty_MaximumOutputPacketSize:
    case kAudioQueueProperty_EnableLevelMetering:
    case kAudioQueueProperty_DecodeBufferSizeFrames:
    case kAudioQueueProperty_ConverterError:
    case kAudioQueueProperty_EnableTimePitch:
    case kAudioQueueProperty_TimePitchAlgorithm:
    case kAudioQueueProperty_TimePitchBypass:
        return sizeof(UInt32);
    case kAudioQueueDeviceProperty_SampleRate:
        return sizeof(Float64);
    case kAudioQueueProperty_StreamDescription:
        return sizeof(AudioStreamBasicDescription);
    case kAudioQueueProperty_CurrentDevice:
        return sizeof(CFStringRef);
    case kAudioQueueProperty_CurrentLevelMeter:
    case kAudioQueueProperty_CurrentLevelMeterDB:
        return q->format.mChannelsPerFrame * sizeof(AudioQueueLevelMeterState);
    default:
        return 0;
    }
}

OSStatus AudioQueueGetPropertySize(AudioQueueRef q, AudioQueuePropertyID id, UInt32 *outSize) {
    UInt32 size;

    if (!alive(q) || outSize == NULL)
        return kAudio_ParamError;
    size = propertySize(q, id);
    if (size == 0)
        return kAudioQueueErr_InvalidProperty;
    *outSize = size;
    return noErr;
}

static AudioDeviceID queueDevice(AudioQueueRef q) {
    AudioDeviceID d = kAudioObjectUnknown;
    UInt32 size = sizeof(d);

    AudioUnitGetProperty(q->unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &d, &size);
    return d;
}

static Float32 decibels(Float32 v) {
    return v > 0 ? 20 * log10f(v) : -120;
}

OSStatus AudioQueueGetProperty(AudioQueueRef q, AudioQueuePropertyID id, void *data, UInt32 *ioSize) {
    UInt32 size;

    if (!alive(q) || data == NULL || ioSize == NULL)
        return kAudio_ParamError;
    size = propertySize(q, id);
    if (size == 0)
        return kAudioQueueErr_InvalidProperty;
    if (*ioSize < size && id != kAudioQueueProperty_CurrentLevelMeter && id != kAudioQueueProperty_CurrentLevelMeterDB)
        return kAudioQueueErr_InvalidPropertySize;
    switch (id) {
    case kAudioQueueProperty_IsRunning:
        *(UInt32 *)data = q->running;
        break;
    case kAudioQueueDeviceProperty_NumberChannels:
        *(UInt32 *)data = 2;
        break;
    case kAudioQueueProperty_MaximumOutputPacketSize:
        *(UInt32 *)data = q->format.mBytesPerPacket;
        break;
    case kAudioQueueProperty_EnableLevelMetering:
        *(UInt32 *)data = q->metering;
        break;
    case kAudioQueueProperty_DecodeBufferSizeFrames:
        *(UInt32 *)data = q->decodeFrames;
        break;
    case kAudioQueueProperty_ConverterError:
    case kAudioQueueProperty_EnableTimePitch:
    case kAudioQueueProperty_TimePitchAlgorithm:
        *(UInt32 *)data = 0;
        break;
    case kAudioQueueProperty_TimePitchBypass:
        *(UInt32 *)data = 1;
        break;
    case kAudioQueueDeviceProperty_SampleRate: {
        Float64 rate = 0;
        UInt32 s = sizeof(rate);
        AudioUnitGetProperty(q->unit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &rate, &s);
        *(Float64 *)data = rate;
        break;
    }
    case kAudioQueueProperty_StreamDescription:
        *(AudioStreamBasicDescription *)data = q->format;
        break;
    case kAudioQueueProperty_CurrentDevice: {
        AudioObjectPropertyAddress a = { kAudioDevicePropertyDeviceUID, kAudioObjectPropertyScopeGlobal,
                                         kAudioObjectPropertyElementMain };
        CFStringRef uid = NULL;
        UInt32 s = sizeof(uid);
        if (AudioObjectGetPropertyData(queueDevice(q), &a, 0, NULL, &s, &uid) != noErr)
            uid = NULL;
        *(CFStringRef *)data = uid;
        break;
    }
    case kAudioQueueProperty_CurrentLevelMeter:
    case kAudioQueueProperty_CurrentLevelMeterDB: {
        UInt32 n = *ioSize / sizeof(AudioQueueLevelMeterState);
        AudioQueueLevelMeterState *m = data;
        if (n > q->format.mChannelsPerFrame)
            n = q->format.mChannelsPerFrame;
        for (UInt32 c = 0; c < n; c++) {
            AudioQueueLevelMeterState s = c < kMaxMeterChannels ? q->meters[c] : (AudioQueueLevelMeterState){ 0, 0 };
            if (id == kAudioQueueProperty_CurrentLevelMeterDB) {
                s.mAveragePower = decibels(s.mAveragePower);
                s.mPeakPower = decibels(s.mPeakPower);
            }
            m[c] = s;
        }
        size = n * sizeof(AudioQueueLevelMeterState);
        break;
    }
    }
    *ioSize = size;
    return noErr;
}

OSStatus AudioQueueSetProperty(AudioQueueRef q, AudioQueuePropertyID id, const void *data, UInt32 size) {
    if (!alive(q) || data == NULL)
        return kAudio_ParamError;
    switch (id) {
    case kAudioQueueProperty_EnableLevelMetering:
        if (size < sizeof(UInt32))
            return kAudioQueueErr_InvalidPropertySize;
        q->metering = *(const UInt32 *)data != 0;
        return noErr;
    case kAudioQueueProperty_DecodeBufferSizeFrames:
        if (size < sizeof(UInt32))
            return kAudioQueueErr_InvalidPropertySize;
        q->decodeFrames = *(const UInt32 *)data;
        return noErr;
    case kAudioQueueProperty_MagicCookie:
        return noErr;   /* linear PCM has none */
    case kAudioQueueProperty_EnableTimePitch:
    case kAudioQueueProperty_TimePitchAlgorithm:
    case kAudioQueueProperty_TimePitchBypass:
        return noErr;
    case kAudioQueueProperty_CurrentDevice: {
        CFStringRef uid;
        AudioDeviceID d = kAudioObjectUnknown;
        if (size < sizeof(CFStringRef))
            return kAudioQueueErr_InvalidPropertySize;
        uid = *(const CFStringRef *)data;
        if (uid != NULL) {
            AudioObjectPropertyAddress a = { kAudioHardwarePropertyTranslateUIDToDevice,
                                             kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
            UInt32 s = sizeof(d);
            if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, sizeof(uid), &uid, &s, &d) != noErr ||
                d == kAudioObjectUnknown)
                return kAudioQueueErr_InvalidDevice;
        }
        if (d == queueDevice(q) || (uid == NULL))
            return noErr;
        /* Moving to another device: the unit is made again for it. */
        if (q->running)
            return kAudioQueueErr_InvalidRunState;
        AudioUnitUninitialize(q->unit);
        AudioUnitSetProperty(q->unit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &d,
                             sizeof(d));
        return AudioUnitInitialize(q->unit) == noErr ? noErr : kAudioQueueErr_InvalidDevice;
    }
    default:
        return propertySize(q, id) != 0 ? kAudioQueueErr_InvalidPropertyValue : kAudioQueueErr_InvalidProperty;
    }
}

OSStatus AudioQueueAddPropertyListener(AudioQueueRef q, AudioQueuePropertyID id,
                                       AudioQueuePropertyListenerProc proc, void *userData) {
    OSStatus status = kAudio_MemFullError;

    if (!alive(q) || proc == NULL)
        return kAudio_ParamError;
    if (id != kAudioQueueProperty_IsRunning && propertySize(q, id) == 0)
        return kAudioQueueErr_InvalidProperty;
    pthread_mutex_lock(&q->lock);
    if (q->listenerCount < kMaxListeners) {
        q->listeners[q->listenerCount].id = id;
        q->listeners[q->listenerCount].proc = proc;
        q->listeners[q->listenerCount].userData = userData;
        q->listenerCount++;
        status = noErr;
    }
    pthread_mutex_unlock(&q->lock);
    return status;
}

OSStatus AudioQueueRemovePropertyListener(AudioQueueRef q, AudioQueuePropertyID id,
                                          AudioQueuePropertyListenerProc proc, void *userData) {
    if (!alive(q))
        return kAudio_ParamError;
    pthread_mutex_lock(&q->lock);
    for (int i = 0; i < q->listenerCount; i++)
        if (q->listeners[i].id == id && q->listeners[i].proc == proc && q->listeners[i].userData == userData) {
            q->listeners[i] = q->listeners[--q->listenerCount];
            break;
        }
    pthread_mutex_unlock(&q->lock);
    return noErr;
}

/* ---- Time --------------------------------------------------------------------- */

OSStatus AudioQueueCreateTimeline(AudioQueueRef q, AudioQueueTimelineRef *out) {
    AudioQueueTimelineRef t;

    if (!alive(q) || out == NULL)
        return kAudio_ParamError;
    t = calloc(1, sizeof(*t));
    if (t == NULL)
        return kAudio_MemFullError;
    t->queue = q;
    *out = t;
    return noErr;
}

OSStatus AudioQueueDisposeTimeline(AudioQueueRef q, AudioQueueTimelineRef t) {
    if (!alive(q) || t == NULL || t->queue != q)
        return kAudio_ParamError;
    free(t);
    return noErr;
}

/* The queue's time: frames played since it started, in its rate. */
OSStatus AudioQueueGetCurrentTime(AudioQueueRef q, AudioQueueTimelineRef t, AudioTimeStamp *out,
                                  Boolean *discontinuity) {
    if (!alive(q) || out == NULL)
        return kAudio_ParamError;
    if (!q->running)
        return kAudioQueueErr_InvalidRunState;
    memset(out, 0, sizeof(*out));
    out->mSampleTime = q->isInput ? q->recordedFrames : q->playedFrames;
    out->mHostTime = mach_absolute_time();
    out->mRateScalar = 1;
    out->mFlags = kAudioTimeStampSampleHostTimeValid | kAudioTimeStampRateScalarValid;
    if (discontinuity != NULL)
        *discontinuity = false;
    return noErr;
}

OSStatus AudioQueueDeviceGetCurrentTime(AudioQueueRef q, AudioTimeStamp *out) {
    if (!alive(q) || out == NULL)
        return kAudio_ParamError;
    if (!q->running)
        return kAudioQueueErr_InvalidRunState;
    return AudioDeviceGetCurrentTime(queueDevice(q), out);
}

/* ---- Dispatch queues ------------------------------------------------------------ */

OSStatus AudioQueueNewOutputWithDispatchQueue(AudioQueueRef *outAQ, const AudioStreamBasicDescription *format,
                                              UInt32 flags, dispatch_queue_t queue,
                                              AudioQueueOutputCallbackBlock block) {
    OSStatus status;

    if (outAQ == NULL || queue == NULL || block == NULL)
        return kAudio_ParamError;
    status = AudioQueueNewOutput(format, NULL, NULL, NULL, NULL, flags, outAQ);
    if (status != noErr)
        return status;
    dispatch_retain(queue);
    (*outAQ)->userQueue = queue;
    (*outAQ)->outputBlock = Block_copy(block);
    return noErr;
}

OSStatus AudioQueueNewInputWithDispatchQueue(AudioQueueRef *outAQ, const AudioStreamBasicDescription *format,
                                             UInt32 flags, dispatch_queue_t queue,
                                             AudioQueueInputCallbackBlock block) {
    OSStatus status;

    if (outAQ == NULL || queue == NULL || block == NULL)
        return kAudio_ParamError;
    status = newInput(format, outAQ);
    if (status != noErr)
        return status;
    dispatch_retain(queue);
    (*outAQ)->userQueue = queue;
    (*outAQ)->inputBlock = Block_copy(block);
    return noErr;
}

/* ---- The device's time ------------------------------------------------------------ */

/* A time in the device's sample time, its host time, or both, from now's
   pair and the device's rate. */
OSStatus AudioQueueDeviceTranslateTime(AudioQueueRef q, const AudioTimeStamp *in, AudioTimeStamp *out) {
    AudioTimeStamp now;
    mach_timebase_info_data_t tb;
    Float64 rate = 0, ticksPerSecond;
    UInt32 size = sizeof(rate);
    OSStatus status;

    if (!alive(q) || in == NULL || out == NULL)
        return kAudio_ParamError;
    status = AudioQueueDeviceGetCurrentTime(q, &now);
    if (status != noErr)
        return status;
    AudioUnitGetProperty(q->unit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &rate, &size);
    if (rate <= 0)
        return kAudioQueueErr_InvalidDevice;
    mach_timebase_info(&tb);
    ticksPerSecond = 1e9 * tb.denom / tb.numer;
    memset(out, 0, sizeof(*out));
    if (in->mFlags & kAudioTimeStampSampleTimeValid) {
        out->mSampleTime = in->mSampleTime;
        out->mHostTime = (UInt64)((Float64)now.mHostTime + (in->mSampleTime - now.mSampleTime) / rate * ticksPerSecond);
    } else if (in->mFlags & kAudioTimeStampHostTimeValid) {
        out->mHostTime = in->mHostTime;
        out->mSampleTime = now.mSampleTime + ((Float64)in->mHostTime - (Float64)now.mHostTime) / ticksPerSecond * rate;
    } else {
        return kAudio_ParamError;
    }
    out->mRateScalar = 1;
    out->mFlags = kAudioTimeStampSampleHostTimeValid | kAudioTimeStampRateScalarValid;
    return noErr;
}

/* The soonest a start can be heard: no sooner than the output's latency
   from now. */
OSStatus AudioQueueDeviceGetNearestStartTime(AudioQueueRef q, AudioTimeStamp *ioRequested, UInt32 flags) {
    AudioTimeStamp now, asked;
    Float64 rate = 0, earliest;
    UInt32 size = sizeof(rate);
    OSStatus status;

    if (!alive(q) || ioRequested == NULL)
        return kAudio_ParamError;
    status = AudioQueueDeviceGetCurrentTime(q, &now);
    if (status != noErr)
        return status;
    AudioUnitGetProperty(q->unit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &rate, &size);
    earliest = now.mSampleTime + q->latency * rate;
    if (ioRequested->mFlags & (kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid)) {
        status = AudioQueueDeviceTranslateTime(q, ioRequested, &asked);
        if (status != noErr)
            return status;
        if (asked.mSampleTime >= earliest) {
            *ioRequested = asked;
            return noErr;
        }
    }
    asked.mSampleTime = earliest;
    asked.mFlags = kAudioTimeStampSampleTimeValid;
    return AudioQueueDeviceTranslateTime(q, &asked, ioRequested);
}

/* ---- Offline rendering ----------------------------------------------------------- */

OSStatus AudioQueueSetOfflineRenderFormat(AudioQueueRef q, const AudioStreamBasicDescription *format,
                                          const AudioChannelLayout *layout) {
    AudioConverterRef converter = NULL;
    OSStatus status;

    if (!alive(q))
        return kAudio_ParamError;
    if (q->running && !q->offline)
        return kAudioQueueErr_InvalidRunState;
    if (format != NULL) {
        status = AudioConverterNew(&q->format, format, &converter);
        if (status != noErr)
            return kAudioQueueErr_InvalidPropertyValue;
    }
    if (q->offlineConverter != NULL)
        AudioConverterDispose(q->offlineConverter);
    q->offlineConverter = converter;
    q->offline = format != NULL;
    if (format != NULL)
        q->offlineFormat = *format;
    return noErr;
}

static OSStatus offlineInput(AudioConverterRef c, UInt32 *ioPackets, AudioBufferList *data,
                             AudioStreamPacketDescription **descriptions, void *userData) {
    AudioQueueRef q = userData;
    UInt32 frames = *ioPackets;
    Boolean drained = false;

    if (frames > 4096)
        frames = 4096;
    if (frames > q->offlineScratchFrames) {
        unsigned char *grown = realloc(q->offlineScratch, (size_t)frames * q->format.mBytesPerFrame);
        if (grown == NULL)
            return kAudio_MemFullError;
        q->offlineScratch = grown;
        q->offlineScratchFrames = frames;
    }
    produce(q, q->offlineScratch, frames, NULL, &drained);
    if (drained) {
        pthread_mutex_lock(&q->lock);
        q->stopping = false;
        q->draining = false;
        pthread_mutex_unlock(&q->lock);
        finishStopping(q);
    }
    data->mNumberBuffers = 1;
    data->mBuffers[0].mNumberChannels = q->format.mChannelsPerFrame;
    data->mBuffers[0].mDataByteSize = frames * q->format.mBytesPerFrame;
    data->mBuffers[0].mData = q->offlineScratch;
    *ioPackets = frames;
    return noErr;
}

/* The next frames, in the offline format, into a buffer: what the queue
   would have played. The first call, with a sample time of 0, starts the
   rendering and gives no frames. */
OSStatus AudioQueueOfflineRender(AudioQueueRef q, const AudioTimeStamp *timestamp, AudioQueueBufferRef buffer,
                                 UInt32 frames) {
    AudioBufferList list;
    UInt32 n = frames;
    OSStatus status;

    if (!alive(q) || timestamp == NULL || buffer == NULL)
        return kAudio_ParamError;
    if (!q->offline)
        return kAudioQueueErr_InvalidOfflineMode;
    if (!q->running)
        return kAudioQueueErr_InvalidRunState;
    if ((UInt64)frames * q->offlineFormat.mBytesPerFrame > buffer->mAudioDataBytesCapacity)
        return kAudioQueueErr_InvalidBuffer;
    list.mNumberBuffers = 1;
    list.mBuffers[0].mNumberChannels = q->offlineFormat.mChannelsPerFrame;
    list.mBuffers[0].mDataByteSize = buffer->mAudioDataBytesCapacity;
    list.mBuffers[0].mData = buffer->mAudioData;
    if (frames == 0) {
        buffer->mAudioDataByteSize = 0;
        return noErr;
    }
    status = AudioConverterFillComplexBuffer(q->offlineConverter, offlineInput, q, &n, &list, NULL);
    buffer->mAudioDataByteSize = n * q->offlineFormat.mBytesPerFrame;
    return status;
}

/* ---- Processing taps --------------------------------------------------------------- */

OSStatus AudioQueueProcessingTapNew(AudioQueueRef q, AudioQueueProcessingTapCallback callback, void *clientData,
                                    AudioQueueProcessingTapFlags flags, UInt32 *outMaxFrames,
                                    AudioStreamBasicDescription *outFormat, AudioQueueProcessingTapRef *outTap) {
    struct OpaqueAudioQueueProcessingTap *t;
    UInt32 ch, maxFrames = 4096;

    if (!alive(q) || callback == NULL || outMaxFrames == NULL || outFormat == NULL || outTap == NULL)
        return kAudio_ParamError;
    if (q->tap != NULL)
        return kAudioQueueErr_TooManyTaps;
    if (q->running)
        return kAudioQueueErr_InvalidRunState;
    ch = q->format.mChannelsPerFrame;
    t = calloc(1, sizeof(*t));
    if (t == NULL)
        return kAudio_MemFullError;
    t->queue = q;
    t->callback = callback;
    t->clientData = clientData;
    t->flags = flags;
    t->maxFrames = maxFrames;
    t->format.mSampleRate = q->format.mSampleRate;
    t->format.mFormatID = kAudioFormatLinearPCM;
    t->format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    t->format.mBytesPerPacket = t->format.mBytesPerFrame = sizeof(float);
    t->format.mFramesPerPacket = 1;
    t->format.mChannelsPerFrame = ch;
    t->format.mBitsPerChannel = 32;
    t->list = calloc(1, offsetof(AudioBufferList, mBuffers) + ch * sizeof(AudioBuffer));
    t->planes = calloc((size_t)ch * maxFrames, sizeof(float));
    t->interleaved = calloc((size_t)ch * maxFrames, sizeof(float));
    t->scratch = calloc(maxFrames, q->format.mBytesPerFrame);
    if (t->list == NULL || t->planes == NULL || t->interleaved == NULL || t->scratch == NULL) {
        free(t->list);
        free(t->planes);
        free(t->interleaved);
        free(t->scratch);
        free(t);
        return kAudio_MemFullError;
    }
    t->list->mNumberBuffers = ch;
    for (UInt32 c = 0; c < ch; c++)
        t->list->mBuffers[c].mNumberChannels = 1;
    q->tap = t;
    *outMaxFrames = maxFrames;
    *outFormat = t->format;
    *outTap = t;
    return noErr;
}

OSStatus AudioQueueProcessingTapDispose(AudioQueueProcessingTapRef t) {
    AudioQueueRef q;

    if (t == NULL || !alive(t->queue))
        return kAudio_ParamError;
    q = t->queue;
    if (q->running)
        return kAudioQueueErr_InvalidRunState;
    q->tap = NULL;
    free(t->list);
    free(t->planes);
    free(t->interleaved);
    free(t->scratch);
    free(t);
    return noErr;
}

/* Called from the tap's callback: the queue's next frames, as float planes. */
OSStatus AudioQueueProcessingTapGetSourceAudio(AudioQueueProcessingTapRef t, UInt32 frames, AudioTimeStamp *ioTime,
                                               AudioQueueProcessingTapFlags *outFlags, UInt32 *outFrames,
                                               AudioBufferList *data) {
    AudioQueueRef q;
    Boolean drained = false;
    UInt32 got;

    if (t == NULL || data == NULL || outFrames == NULL)
        return kAudio_ParamError;
    q = t->queue;
    if (frames > t->maxFrames)
        frames = t->maxFrames;
    if (t->siphoned) {
        /* A siphon's samples were taken already: the same again. */
        if (frames > t->siphonFrames)
            frames = t->siphonFrames;
        for (UInt32 c = 0; c < data->mNumberBuffers && c < t->list->mNumberBuffers; c++) {
            if (data->mBuffers[c].mData != t->list->mBuffers[c].mData)
                memcpy(data->mBuffers[c].mData, t->list->mBuffers[c].mData, frames * sizeof(float));
            data->mBuffers[c].mDataByteSize = frames * sizeof(float);
        }
        *outFrames = frames;
    } else {
        got = source(q, t->scratch, frames, &drained);
        toPlanes(t, t->scratch, frames, data);
        t->drained = drained;
        *outFrames = frames;
        if (got == 0 && outFlags != NULL)
            *outFlags |= kAudioQueueProcessingTap_EndOfStream;
    }
    if (ioTime != NULL) {
        memset(ioTime, 0, sizeof(*ioTime));
        ioTime->mSampleTime = q->playedFrames;
        ioTime->mFlags = kAudioTimeStampSampleTimeValid;
    }
    return noErr;
}

OSStatus AudioQueueProcessingTapGetQueueTime(AudioQueueProcessingTapRef t, Float64 *outSampleTime,
                                             UInt32 *outFrameCount) {
    if (t == NULL || !alive(t->queue))
        return kAudio_ParamError;
    if (!t->queue->running)
        return kAudioQueueErr_InvalidRunState;
    if (outSampleTime != NULL)
        *outSampleTime = t->queue->playedFrames;
    if (outFrameCount != NULL)
        *outFrameCount = t->frames;
    return noErr;
}
