/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The output units (default, system and HAL output): a device IO procedure
 * that pulls the client's samples through its render callback, in the
 * client's format and rate, converts them to the device's interleaved
 * float, resamples them when the rates differ and applies the unit's
 * volume. Input (element 1) comes with the device's input (AU2).
 */

#include "AUInternal.h"
#include <CoreAudio/CoreAudio.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "speexdsp/speex_resampler.h"

#define kOutChannels        2
#define kDefaultMaxFrames   4096
#define kMaxDeviceFrames    8192
#define kResamplerQuality   4

typedef struct {
    AudioDeviceID   device;
    Boolean         deviceSet;          /* CurrentDevice was set */
    Float64         deviceRate;
    AudioStreamBasicDescription client; /* input scope, element 0 */
    AURenderCallbackStruct callback;
    UInt32          maxFrames;
    Float32         volume;
    Boolean         initialized;
    volatile Boolean running;
    Boolean         outputEnabled;
    Boolean         allocate;           /* ShouldAllocateBuffer */
    OSStatus        lastRenderError;
    AudioDeviceIOProcID procID;

    /* Built by initialize. */
    SpeexResamplerState *resampler;
    AudioBufferList *buffers;           /* the client's, maxFrames long */
    void           *bufferMemory;
    float          *scratch;            /* client frames as stereo float */
    float          *fifo;               /* device-rate stereo float */
    UInt32          fifoCount, fifoCapacity;
    Float64         clientSampleTime;
} OutputState;

#define S(unit) ((OutputState *)(unit)->state)

static void canonicalFormat(AudioStreamBasicDescription *f, Float64 rate) {
    memset(f, 0, sizeof(*f));
    f->mSampleRate = rate;
    f->mFormatID = kAudioFormatLinearPCM;
    f->mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    f->mBytesPerPacket = sizeof(float);
    f->mFramesPerPacket = 1;
    f->mBytesPerFrame = sizeof(float);
    f->mChannelsPerFrame = kOutChannels;
    f->mBitsPerChannel = 32;
}

static void deviceFormat(const OutputState *s, AudioStreamBasicDescription *f) {
    canonicalFormat(f, s->deviceRate);
    f->mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    f->mBytesPerPacket = f->mBytesPerFrame = sizeof(float) * kOutChannels;
}

static AudioDeviceID defaultDevice(void) {
    AudioObjectPropertyAddress a = { kAudioHardwarePropertyDefaultOutputDevice,
                                     kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    AudioDeviceID d = kAudioObjectUnknown;
    UInt32 size = sizeof(d);

    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, NULL, &size, &d) != noErr)
        return kAudioObjectUnknown;
    return d;
}

static Float64 rateOf(AudioDeviceID d) {
    AudioObjectPropertyAddress a = { kAudioDevicePropertyNominalSampleRate,
                                     kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    Float64 rate = 0;
    UInt32 size = sizeof(rate);

    if (d == kAudioObjectUnknown || AudioObjectGetPropertyData(d, &a, 0, NULL, &size, &rate) != noErr)
        return 0;
    return rate;
}

static UInt32 deviceUInt32(AudioDeviceID d, AudioObjectPropertySelector sel, AudioObjectPropertyScope scope) {
    AudioObjectPropertyAddress a = { sel, scope, kAudioObjectPropertyElementMain };
    UInt32 v = 0, size = sizeof(v);

    if (AudioObjectGetPropertyData(d, &a, 0, NULL, &size, &v) != noErr)
        return 0;
    return v;
}

static Boolean isHAL(AudioUnit unit) {
    return unit->component->description.componentSubType == kAudioUnitSubType_HALOutput;
}

/* ---- Life ---------------------------------------------------------------- */

static OSStatus outputCreate(AudioUnit unit) {
    OutputState *s = calloc(1, sizeof(*s));

    if (s == NULL)
        return kAudio_MemFullError;
    s->device = defaultDevice();
    s->deviceRate = rateOf(s->device);
    if (s->deviceRate <= 0)
        s->deviceRate = 48000;
    canonicalFormat(&s->client, s->deviceRate);
    s->maxFrames = kDefaultMaxFrames;
    s->volume = 1;
    s->outputEnabled = true;
    s->allocate = true;
    unit->state = s;
    return noErr;
}

static void freeBuffers(OutputState *s) {
    if (s->resampler != NULL)
        speex_resampler_destroy(s->resampler);
    free(s->buffers);
    free(s->bufferMemory);
    free(s->scratch);
    free(s->fifo);
    s->resampler = NULL;
    s->buffers = NULL;
    s->bufferMemory = s->scratch = NULL;
    s->fifo = NULL;
    s->fifoCount = s->fifoCapacity = 0;
}

static OSStatus outputIOProc(AudioObjectID device, const AudioTimeStamp *now, const AudioBufferList *input,
                             const AudioTimeStamp *inputTime, AudioBufferList *output,
                             const AudioTimeStamp *outputTime, void *refCon);

static OSStatus outputInitialize(AudioUnit unit) {
    OutputState *s = S(unit);
    UInt32 planes, planeBytes;
    OSStatus status = noErr;

    pthread_mutex_lock(&unit->lock);
    if (s->initialized)
        goto out;
    if (!s->deviceSet) {
        s->device = defaultDevice();
        s->deviceRate = rateOf(s->device);
    }
    if (s->device == kAudioObjectUnknown || s->deviceRate <= 0) {
        status = kAudioUnitErr_FailedInitialization;
        goto out;
    }

    /* The client's buffers: one per channel when planar. */
    planes = (s->client.mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? s->client.mChannelsPerFrame : 1;
    planeBytes = s->client.mBytesPerFrame * s->maxFrames;
    s->buffers = calloc(1, offsetof(AudioBufferList, mBuffers) + planes * sizeof(AudioBuffer));
    s->bufferMemory = calloc(planes, planeBytes);
    s->scratch = calloc(s->maxFrames, sizeof(float) * kOutChannels);
    s->fifoCapacity = (UInt32)ceil(s->maxFrames * s->deviceRate / s->client.mSampleRate) + kMaxDeviceFrames + 64;
    s->fifo = calloc(s->fifoCapacity, sizeof(float) * kOutChannels);
    if (s->buffers == NULL || s->bufferMemory == NULL || s->scratch == NULL || s->fifo == NULL) {
        freeBuffers(s);
        status = kAudio_MemFullError;
        goto out;
    }
    s->buffers->mNumberBuffers = planes;
    for (UInt32 i = 0; i < planes; i++) {
        s->buffers->mBuffers[i].mNumberChannels = planes == 1 ? s->client.mChannelsPerFrame : 1;
        s->buffers->mBuffers[i].mData = (char *)s->bufferMemory + i * planeBytes;
    }
    if (fabs(s->client.mSampleRate - s->deviceRate) > 0.5) {
        int err = 0;
        s->resampler = speex_resampler_init(kOutChannels, (spx_uint32_t)lround(s->client.mSampleRate),
                                            (spx_uint32_t)lround(s->deviceRate), kResamplerQuality, &err);
        if (s->resampler == NULL) {
            freeBuffers(s);
            status = kAudioUnitErr_FailedInitialization;
            goto out;
        }
        speex_resampler_skip_zeros(s->resampler);
    }
    if (AudioDeviceCreateIOProcID(s->device, outputIOProc, unit, &s->procID) != noErr) {
        freeBuffers(s);
        status = kAudioUnitErr_FailedInitialization;
        goto out;
    }
    s->clientSampleTime = 0;
    s->initialized = true;
out:
    pthread_mutex_unlock(&unit->lock);
    return status;
}

static OSStatus outputStop(AudioUnit unit);

static OSStatus outputUninitialize(AudioUnit unit) {
    OutputState *s = S(unit);

    outputStop(unit);
    pthread_mutex_lock(&unit->lock);
    if (s->initialized) {
        AudioDeviceDestroyIOProcID(s->device, s->procID);
        s->procID = NULL;
        freeBuffers(s);
        s->initialized = false;
    }
    pthread_mutex_unlock(&unit->lock);
    return noErr;
}

static void outputDispose(AudioUnit unit) {
    outputUninitialize(unit);
    free(unit->state);
    unit->state = NULL;
}

static OSStatus outputStart(AudioUnit unit) {
    OutputState *s = S(unit);
    OSStatus status;

    if (!s->initialized)
        return kAudioUnitErr_Uninitialized;
    pthread_mutex_lock(&unit->lock);
    if (s->running) {
        pthread_mutex_unlock(&unit->lock);
        return noErr;
    }
    s->running = true;
    status = AudioDeviceStart(s->device, s->procID);
    if (status != noErr)
        s->running = false;
    pthread_mutex_unlock(&unit->lock);
    if (status == noErr)
        _AUPropertyChanged(unit, kAudioOutputUnitProperty_IsRunning, kAudioUnitScope_Global, 0);
    return status;
}

static OSStatus outputStop(AudioUnit unit) {
    OutputState *s = S(unit);
    Boolean was;

    pthread_mutex_lock(&unit->lock);
    was = s->running;
    if (was) {
        /* Returns once the IO thread is out of the procedure. */
        AudioDeviceStop(s->device, s->procID);
        s->running = false;
    }
    pthread_mutex_unlock(&unit->lock);
    if (was)
        _AUPropertyChanged(unit, kAudioOutputUnitProperty_IsRunning, kAudioUnitScope_Global, 0);
    return noErr;
}

static OSStatus outputReset(AudioUnit unit, AudioUnitScope scope, AudioUnitElement element) {
    OutputState *s = S(unit);

    pthread_mutex_lock(&unit->lock);
    if (s->resampler != NULL && !s->running) {
        speex_resampler_reset_mem(s->resampler);
        speex_resampler_skip_zeros(s->resampler);
    }
    if (!s->running)
        s->fifoCount = 0;
    pthread_mutex_unlock(&unit->lock);
    return noErr;
}

/* ---- Rendering (the device's IO thread) ------------------------------------ */

/* Asks the client for frames into the unit's buffers and returns them as
   stereo float in scratch. */
static void pull(AudioUnit unit, OutputState *s, AURenderCallbackStruct cb, UInt32 frames,
                 const AudioTimeStamp *deviceTime) {
    AudioUnitRenderActionFlags flags = 0;
    AudioTimeStamp t;
    UInt32 planeBytes = s->client.mBytesPerFrame * frames;
    OSStatus status;

    memset(&t, 0, sizeof(t));
    t.mSampleTime = s->clientSampleTime;
    t.mHostTime = deviceTime->mHostTime;
    t.mRateScalar = 1;
    t.mFlags = kAudioTimeStampSampleHostTimeValid | kAudioTimeStampRateScalarValid;
    for (UInt32 i = 0; i < s->buffers->mNumberBuffers; i++) {
        s->buffers->mBuffers[i].mDataByteSize = planeBytes;
        s->buffers->mBuffers[i].mData = (char *)s->bufferMemory + i * s->client.mBytesPerFrame * s->maxFrames;
        memset(s->buffers->mBuffers[i].mData, 0, planeBytes);
    }

    _AURenderNotify(unit, kAudioUnitRenderAction_PreRender, &t, 0, frames, s->buffers);
    status = cb.inputProc != NULL ? cb.inputProc(cb.inputProcRefCon, &flags, &t, 0, frames, s->buffers) : noErr;
    _AURenderNotify(unit, kAudioUnitRenderAction_PostRender, &t, 0, frames, s->buffers);
    s->clientSampleTime += frames;

    if (status != noErr) {
        s->lastRenderError = status;
        memset(s->scratch, 0, frames * sizeof(float) * kOutChannels);
    } else if (cb.inputProc == NULL || (flags & kAudioUnitRenderAction_OutputIsSilence)) {
        memset(s->scratch, 0, frames * sizeof(float) * kOutChannels);
    } else {
        _AUToFloat(&s->client, s->buffers, frames, s->scratch, kOutChannels);
    }
}

static OSStatus outputIOProc(AudioObjectID device, const AudioTimeStamp *now, const AudioBufferList *input,
                             const AudioTimeStamp *inputTime, AudioBufferList *output,
                             const AudioTimeStamp *outputTime, void *refCon) {
    AudioUnit unit = refCon;
    OutputState *s = S(unit);
    AURenderCallbackStruct cb;
    float *out, gain;
    UInt32 channels, frames, guard = 0;

    if (output == NULL || output->mNumberBuffers == 0 || !s->running || !s->outputEnabled)
        return noErr;
    channels = output->mBuffers[0].mNumberChannels;
    if (channels == 0)
        return noErr;
    out = output->mBuffers[0].mData;
    frames = output->mBuffers[0].mDataByteSize / (channels * sizeof(float));
    if (frames > kMaxDeviceFrames)
        frames = kMaxDeviceFrames;

    /* The callback may change while running: take it under the lock, or
       keep this cycle silent if the lock is busy. */
    if (pthread_mutex_trylock(&unit->lock) != 0)
        return noErr;
    cb = s->callback;
    gain = s->volume;
    pthread_mutex_unlock(&unit->lock);

    while (s->fifoCount < frames && guard++ < 64) {
        UInt32 need = frames - s->fifoCount, chunk;

        if (s->resampler == NULL) {
            chunk = need < s->maxFrames ? need : s->maxFrames;
            pull(unit, s, cb, chunk, outputTime);
            memcpy(s->fifo + s->fifoCount * kOutChannels, s->scratch, chunk * sizeof(float) * kOutChannels);
            s->fifoCount += chunk;
        } else {
            spx_uint32_t inLen, outLen;

            chunk = (UInt32)ceil(need * s->client.mSampleRate / s->deviceRate);
            if (chunk == 0)
                chunk = 1;
            if (chunk > s->maxFrames)
                chunk = s->maxFrames;
            pull(unit, s, cb, chunk, outputTime);
            inLen = chunk;
            outLen = s->fifoCapacity - s->fifoCount;
            speex_resampler_process_interleaved_float(s->resampler, s->scratch, &inLen,
                                                      s->fifo + s->fifoCount * kOutChannels, &outLen);
            s->fifoCount += outLen;
        }
    }

    if (s->fifoCount < frames)  /* the client gave nothing usable */
        return noErr;
    for (UInt32 i = 0; i < frames; i++) {
        float l = s->fifo[i * 2] * gain, r = s->fifo[i * 2 + 1] * gain;
        if (channels == 1) {
            out[i] += (l + r) * 0.5f;
        } else {
            out[i * channels] += l;
            out[i * channels + 1] += r;
        }
    }
    s->fifoCount -= frames;
    memmove(s->fifo, s->fifo + frames * kOutChannels, s->fifoCount * sizeof(float) * kOutChannels);
    return noErr;
}

/* ---- Properties ------------------------------------------------------------ */

static OSStatus outputGetPropertyInfo(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                      AudioUnitElement element, UInt32 *size, Boolean *writable) {
    switch (id) {
    case kAudioUnitProperty_StreamFormat:
        *size = sizeof(AudioStreamBasicDescription);
        *writable = true;
        return noErr;
    case kAudioUnitProperty_SampleRate:
        *size = sizeof(Float64);
        *writable = true;
        return noErr;
    case kAudioUnitProperty_MaximumFramesPerSlice:
    case kAudioUnitProperty_ShouldAllocateBuffer:
    case kAudioOutputUnitProperty_EnableIO:
    case kAudioOutputUnitProperty_StartTimestampsAtZero:
        *size = sizeof(UInt32);
        *writable = true;
        return noErr;
    case kAudioUnitProperty_ElementCount:
    case kAudioOutputUnitProperty_IsRunning:
    case kAudioOutputUnitProperty_HasIO:
        *size = sizeof(UInt32);
        *writable = false;
        return noErr;
    case kAudioUnitProperty_LastRenderError:
        *size = sizeof(OSStatus);
        *writable = false;
        return noErr;
    case kAudioUnitProperty_Latency:
        *size = sizeof(Float64);
        *writable = false;
        return noErr;
    case kAudioUnitProperty_SetRenderCallback:
        *size = sizeof(AURenderCallbackStruct);
        *writable = true;
        return noErr;
    case kAudioOutputUnitProperty_CurrentDevice:
        *size = sizeof(AudioDeviceID);
        *writable = true;
        return noErr;
    default:
        return kAudioUnitErr_InvalidProperty;
    }
}

#define PUT(type, value)                          \
    do {                                          \
        if (*ioSize < sizeof(type))               \
            return kAudioUnitErr_InvalidPropertyValue; \
        *(type *)data = (value);                  \
        *ioSize = sizeof(type);                   \
        return noErr;                             \
    } while (0)

static OSStatus outputGetProperty(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                  AudioUnitElement element, void *data, UInt32 *ioSize) {
    OutputState *s = S(unit);
    AudioStreamBasicDescription f;

    switch (id) {
    case kAudioUnitProperty_StreamFormat:
        if (element != 0 && !(isHAL(unit) && element == 1))
            return kAudioUnitErr_InvalidElement;
        if (scope == kAudioUnitScope_Input && element == 0)
            f = s->client;
        else
            deviceFormat(s, &f);
        PUT(AudioStreamBasicDescription, f);
    case kAudioUnitProperty_SampleRate:
        PUT(Float64, scope == kAudioUnitScope_Input ? s->client.mSampleRate : s->deviceRate);
    case kAudioUnitProperty_MaximumFramesPerSlice:
        PUT(UInt32, s->maxFrames);
    case kAudioUnitProperty_ShouldAllocateBuffer:
        PUT(UInt32, s->allocate);
    case kAudioUnitProperty_ElementCount:
        PUT(UInt32, scope == kAudioUnitScope_Global ? 1 : (isHAL(unit) ? 2 : 1));
    case kAudioOutputUnitProperty_IsRunning:
        PUT(UInt32, s->running);
    case kAudioOutputUnitProperty_EnableIO:
        PUT(UInt32, element == 0 ? s->outputEnabled : 0);
    case kAudioOutputUnitProperty_HasIO:
        PUT(UInt32, element == 0 ? 1 : 0);
    case kAudioOutputUnitProperty_StartTimestampsAtZero:
        PUT(UInt32, 1);
    case kAudioUnitProperty_LastRenderError: {
        OSStatus e = s->lastRenderError;
        s->lastRenderError = noErr;
        PUT(OSStatus, e);
    }
    case kAudioUnitProperty_Latency: {
        /* From a render to the speaker: the service runs the safety
           offset and two cycles ahead, and the device adds its own. */
        UInt32 frames = deviceUInt32(s->device, kAudioDevicePropertyLatency, kAudioObjectPropertyScopeOutput) +
                        deviceUInt32(s->device, kAudioDevicePropertySafetyOffset, kAudioObjectPropertyScopeOutput) +
                        2 * deviceUInt32(s->device, kAudioDevicePropertyBufferFrameSize, kAudioObjectPropertyScopeGlobal);
        PUT(Float64, s->deviceRate > 0 ? frames / s->deviceRate : 0);
    }
    case kAudioUnitProperty_SetRenderCallback:
        PUT(AURenderCallbackStruct, s->callback);
    case kAudioOutputUnitProperty_CurrentDevice:
        PUT(AudioDeviceID, s->device);
    default:
        return kAudioUnitErr_InvalidProperty;
    }
}

static OSStatus outputSetProperty(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                  AudioUnitElement element, const void *data, UInt32 size) {
    OutputState *s = S(unit);
    OSStatus status = noErr;

    if (data == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&unit->lock);
    switch (id) {
    case kAudioUnitProperty_StreamFormat: {
        const AudioStreamBasicDescription *f = data;
        if (size < sizeof(*f)) {
            status = kAudioUnitErr_InvalidPropertyValue;
        } else if (!_AUFormatIsSupported(f)) {
            status = kAudioUnitErr_FormatNotSupported;
        } else if (scope == kAudioUnitScope_Input && element == 0) {
            if (s->initialized)
                status = kAudioUnitErr_Initialized;
            else
                s->client = *f;
        } else if (scope == kAudioUnitScope_Output && element == 0) {
            /* The device side: its rate is the device's. */
            if (fabs(f->mSampleRate - s->deviceRate) > 0.5)
                status = kAudioUnitErr_FormatNotSupported;
        } else {
            status = element > 1 ? kAudioUnitErr_InvalidElement : kAudioUnitErr_FormatNotSupported;
        }
        break;
    }
    case kAudioUnitProperty_SampleRate:
        if (size < sizeof(Float64) || *(const Float64 *)data <= 0)
            status = kAudioUnitErr_InvalidPropertyValue;
        else if (scope != kAudioUnitScope_Input)
            status = fabs(*(const Float64 *)data - s->deviceRate) > 0.5 ? kAudioUnitErr_FormatNotSupported : noErr;
        else if (s->initialized)
            status = kAudioUnitErr_Initialized;
        else
            s->client.mSampleRate = *(const Float64 *)data;
        break;
    case kAudioUnitProperty_MaximumFramesPerSlice:
        if (size < sizeof(UInt32) || *(const UInt32 *)data == 0 || *(const UInt32 *)data > 65536)
            status = kAudioUnitErr_InvalidPropertyValue;
        else if (s->initialized)
            status = kAudioUnitErr_Initialized;
        else
            s->maxFrames = *(const UInt32 *)data;
        break;
    case kAudioUnitProperty_ShouldAllocateBuffer:
        if (size < sizeof(UInt32))
            status = kAudioUnitErr_InvalidPropertyValue;
        else
            s->allocate = *(const UInt32 *)data != 0;
        break;
    case kAudioUnitProperty_SetRenderCallback:
        if (size < sizeof(AURenderCallbackStruct))
            status = kAudioUnitErr_InvalidPropertyValue;
        else
            s->callback = *(const AURenderCallbackStruct *)data;
        break;
    case kAudioOutputUnitProperty_EnableIO:
        if (size < sizeof(UInt32))
            status = kAudioUnitErr_InvalidPropertyValue;
        else if (s->initialized)
            status = kAudioUnitErr_Initialized;
        else if (element == 0 && scope == kAudioUnitScope_Output)
            s->outputEnabled = *(const UInt32 *)data != 0;
        else if (element == 1 && scope == kAudioUnitScope_Input && isHAL(unit))
            status = *(const UInt32 *)data != 0 ? kAudioUnitErr_InvalidPropertyValue : noErr;
        else
            status = kAudioUnitErr_InvalidElement;
        break;
    case kAudioOutputUnitProperty_StartTimestampsAtZero:
        break;
    case kAudioOutputUnitProperty_CurrentDevice: {
        AudioDeviceID d;
        if (size < sizeof(AudioDeviceID)) {
            status = kAudioUnitErr_InvalidPropertyValue;
            break;
        }
        d = *(const AudioDeviceID *)data;
        if (s->initialized) {
            status = d == s->device ? noErr : kAudioUnitErr_Initialized;
        } else if (d == kAudioObjectUnknown) {
            s->deviceSet = false;
            s->device = defaultDevice();
            s->deviceRate = rateOf(s->device);
        } else if (rateOf(d) <= 0) {
            status = kAudioUnitErr_InvalidPropertyValue;
        } else {
            s->deviceSet = true;
            s->device = d;
            s->deviceRate = rateOf(d);
        }
        break;
    }
    default:
        status = kAudioUnitErr_InvalidProperty;
        break;
    }
    pthread_mutex_unlock(&unit->lock);
    return status;
}

/* ---- Parameters ------------------------------------------------------------ */

static OSStatus outputGetParameter(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                                   AudioUnitElement element, AudioUnitParameterValue *value) {
    if (id != kHALOutputParam_Volume)
        return kAudioUnitErr_InvalidParameter;
    *value = S(unit)->volume;
    return noErr;
}

static OSStatus outputSetParameter(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                                   AudioUnitElement element, AudioUnitParameterValue value) {
    if (id != kHALOutputParam_Volume)
        return kAudioUnitErr_InvalidParameter;
    if (!(value >= 0 && value <= 1))
        return kAudioUnitErr_InvalidPropertyValue;
    pthread_mutex_lock(&unit->lock);
    S(unit)->volume = value;
    pthread_mutex_unlock(&unit->lock);
    return noErr;
}

const AUClass _AUOutputClass = {
    .create = outputCreate,
    .dispose = outputDispose,
    .initialize = outputInitialize,
    .uninitialize = outputUninitialize,
    .getPropertyInfo = outputGetPropertyInfo,
    .getProperty = outputGetProperty,
    .setProperty = outputSetProperty,
    .getParameter = outputGetParameter,
    .setParameter = outputSetParameter,
    .render = NULL,
    .reset = outputReset,
    .start = outputStart,
    .stop = outputStop,
};
