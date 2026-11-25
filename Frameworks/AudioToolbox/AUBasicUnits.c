/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * AudioToolbox's own processing units, for hosts that build graphs:
 *
 * - AUConverter ('aufc' 'conv'): its input in one format, its output in
 *   another (sample format, channels, rate), through an AudioConverter.
 * - GenericOutput ('auou' 'genr'): the same, an output unit with no device:
 *   the host renders it (offline rendering, tests).
 * - MultiChannelMixer ('aumx' 'mcmx'): inputs (8 by default, ElementCount),
 *   each converted to the output's rate and channels, scaled by its volume,
 *   panned, enabled or not, summed and scaled by the output volume.
 *
 * Each input comes from a render callback or a connection (MakeConnection,
 * which AUComponent.c turns into a render callback); the formats default to
 * the canonical one, 32-bit float, non-interleaved, stereo, 44.1 kHz.
 */

#include "AUInternal.h"
#include <AudioToolbox/AudioToolbox.h>
#include <stdlib.h>
#include <string.h>

#define kMaxInputs 16
#define kDefaultMaxFrames 4096

enum { kKindConverter, kKindGenericOutput, kKindMixer };

typedef struct Input {
    AudioStreamBasicDescription format;
    AURenderCallbackStruct callback;
    AudioConverterRef converter;        /* to the output's format (the mixer: interleaved float) */
    AudioBufferList *list;              /* what the callback renders into */
    void *memory;
    AudioTimeStamp time;
    OSStatus status;
    Float32 volume, enable, pan;
} Input;

typedef struct BasicState {
    int kind;
    UInt32 inputCount;
    Input inputs[kMaxInputs];
    AudioStreamBasicDescription output;
    UInt32 maxFrames;
    Float32 outputVolume;
    Boolean initialized, running;
    OSStatus lastError;
    void *outMemory;                    /* when the host's buffers have no memory */
    float *mix, *scratch;               /* the mixer's, interleaved float */
    AudioBufferList scratchList;
} BasicState;

#define S(unit) ((BasicState *)(unit)->state)

static void canonical(AudioStreamBasicDescription *f, Float64 rate, UInt32 channels) {
    memset(f, 0, sizeof(*f));
    f->mSampleRate = rate;
    f->mFormatID = kAudioFormatLinearPCM;
    f->mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    f->mBytesPerPacket = f->mBytesPerFrame = sizeof(float);
    f->mFramesPerPacket = 1;
    f->mChannelsPerFrame = channels;
    f->mBitsPerChannel = 32;
}

static void interleavedFloat(AudioStreamBasicDescription *f, Float64 rate, UInt32 channels) {
    canonical(f, rate, channels);
    f->mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    f->mBytesPerPacket = f->mBytesPerFrame = sizeof(float) * channels;
}

static Boolean nonInterleaved(const AudioStreamBasicDescription *f) {
    return (f->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
}

/* A buffer list for frames of the format, its memory in *memory. */
static AudioBufferList *newList(const AudioStreamBasicDescription *f, UInt32 frames, void **memory) {
    UInt32 buffers = nonInterleaved(f) ? f->mChannelsPerFrame : 1;
    size_t bytes = (size_t)f->mBytesPerFrame * frames;
    AudioBufferList *l = calloc(1, offsetof(AudioBufferList, mBuffers) + buffers * sizeof(AudioBuffer));
    *memory = calloc(buffers, bytes > 0 ? bytes : 1);
    if (l == NULL || *memory == NULL) {
        free(l);
        free(*memory);
        *memory = NULL;
        return NULL;
    }
    l->mNumberBuffers = buffers;
    for (UInt32 i = 0; i < buffers; i++) {
        l->mBuffers[i].mNumberChannels = nonInterleaved(f) ? 1 : f->mChannelsPerFrame;
        l->mBuffers[i].mData = (char *)*memory + i * bytes;
        l->mBuffers[i].mDataByteSize = (UInt32)bytes;
    }
    return l;
}

static OSStatus basicCreate(AudioUnit unit, int kind) {
    BasicState *s = calloc(1, sizeof(BasicState));
    if (s == NULL)
        return kAudio_MemFullError;
    s->kind = kind;
    s->inputCount = kind == kKindMixer ? 8 : 1;
    s->maxFrames = kDefaultMaxFrames;
    s->outputVolume = 1;
    canonical(&s->output, 44100, 2);
    for (int i = 0; i < kMaxInputs; i++) {
        canonical(&s->inputs[i].format, 44100, 2);
        s->inputs[i].volume = s->inputs[i].enable = 1;
    }
    unit->state = s;
    return noErr;
}

static OSStatus converterCreate(AudioUnit unit) { return basicCreate(unit, kKindConverter); }
static OSStatus genericCreate(AudioUnit unit) { return basicCreate(unit, kKindGenericOutput); }
static OSStatus mixerCreate(AudioUnit unit) { return basicCreate(unit, kKindMixer); }

static void freeBuffers(BasicState *s) {
    for (int i = 0; i < kMaxInputs; i++) {
        Input *in = &s->inputs[i];
        if (in->converter != NULL)
            AudioConverterDispose(in->converter);
        in->converter = NULL;
        free(in->list);
        free(in->memory);
        in->list = NULL;
        in->memory = NULL;
    }
    free(s->outMemory);
    free(s->mix);
    free(s->scratch);
    s->outMemory = NULL;
    s->mix = s->scratch = NULL;
}

static void basicDispose(AudioUnit unit) {
    BasicState *s = S(unit);
    if (s != NULL) {
        freeBuffers(s);
        free(s);
    }
    unit->state = NULL;
}

static OSStatus basicInitialize(AudioUnit unit) {
    BasicState *s = S(unit);
    AudioStreamBasicDescription target;
    OSStatus status = noErr;

    if (s->initialized)
        return noErr;
    if (s->kind == kKindMixer)
        interleavedFloat(&target, s->output.mSampleRate, s->output.mChannelsPerFrame);
    else
        target = s->output;
    if (!_AUFormatIsSupported(&s->output))
        return kAudioUnitErr_FormatNotSupported;
    for (UInt32 i = 0; i < s->inputCount && status == noErr; i++) {
        Input *in = &s->inputs[i];
        if (!_AUFormatIsSupported(&in->format)) {
            status = kAudioUnitErr_FormatNotSupported;
            break;
        }
        in->list = newList(&in->format, s->maxFrames, &in->memory);
        if (in->list == NULL)
            status = kAudio_MemFullError;
        else if (memcmp(&in->format, &target, sizeof(target)) != 0)
            status = AudioConverterNew(&in->format, &target, &in->converter);
        memset(&in->time, 0, sizeof(in->time));
        in->time.mFlags = kAudioTimeStampSampleTimeValid;
    }
    if (status == noErr && s->kind == kKindMixer) {
        size_t bytes = (size_t)s->maxFrames * s->output.mChannelsPerFrame * sizeof(float);
        s->mix = malloc(bytes);
        s->scratch = malloc(bytes);
        if (s->mix == NULL || s->scratch == NULL)
            status = kAudio_MemFullError;
    }
    if (status == noErr) {
        UInt32 buffers = nonInterleaved(&s->output) ? s->output.mChannelsPerFrame : 1;
        s->outMemory = calloc(buffers, (size_t)s->output.mBytesPerFrame * s->maxFrames);
        if (s->outMemory == NULL)
            status = kAudio_MemFullError;
    }
    if (status != noErr) {
        freeBuffers(s);
        return status;
    }
    s->initialized = true;
    return noErr;
}

static OSStatus basicUninitialize(AudioUnit unit) {
    BasicState *s = S(unit);
    freeBuffers(s);
    s->initialized = false;
    return noErr;
}

/* ---- Rendering ---- */

/* frames of input i, from its callback, into its own buffers. */
static OSStatus pullInput(AudioUnit unit, BasicState *s, UInt32 i, UInt32 frames, const AudioTimeStamp *time) {
    Input *in = &s->inputs[i];
    AudioUnitRenderActionFlags flags = 0;
    AudioTimeStamp t = time != NULL ? *time : in->time;
    for (UInt32 b = 0; b < in->list->mNumberBuffers; b++) {
        in->list->mBuffers[b].mDataByteSize = in->format.mBytesPerFrame * frames;
        in->list->mBuffers[b].mData = (char *)in->memory + (size_t)b * in->format.mBytesPerFrame * s->maxFrames;
        memset(in->list->mBuffers[b].mData, 0, in->list->mBuffers[b].mDataByteSize);
    }
    if (in->callback.inputProc == NULL)
        return kAudioUnitErr_NoConnection;
    return in->callback.inputProc(in->callback.inputProcRefCon, &flags, &t, i, frames, in->list);
}

typedef struct Pull {
    AudioUnit unit;
    UInt32 input;
} Pull;

/* The converter's input: as many frames as it asks (at most a slice). */
static OSStatus converterInput(AudioConverterRef converter, UInt32 *ioPackets, AudioBufferList *io,
                               AudioStreamPacketDescription **descriptions, void *user) {
    Pull *p = user;
    BasicState *s = S(p->unit);
    Input *in = &s->inputs[p->input];
    UInt32 n = *ioPackets < s->maxFrames ? *ioPackets : s->maxFrames;
    OSStatus status = pullInput(p->unit, s, p->input, n, NULL);
    in->time.mSampleTime += n;
    if (status != noErr) {
        in->status = status;
        *ioPackets = 0;
        return status;
    }
    for (UInt32 b = 0; b < io->mNumberBuffers && b < in->list->mNumberBuffers; b++)
        io->mBuffers[b] = in->list->mBuffers[b];
    *ioPackets = n;
    return noErr;
}

/* frames of input i in the target format, into out. */
static OSStatus renderInput(AudioUnit unit, BasicState *s, UInt32 i, UInt32 frames, const AudioTimeStamp *time,
                            AudioBufferList *out) {
    Input *in = &s->inputs[i];
    if (in->converter == NULL) {
        OSStatus status = pullInput(unit, s, i, frames, time);
        if (status != noErr)
            return status;
        for (UInt32 b = 0; b < out->mNumberBuffers && b < in->list->mNumberBuffers; b++)
            memcpy(out->mBuffers[b].mData, in->list->mBuffers[b].mData, in->list->mBuffers[b].mDataByteSize);
        return noErr;
    }
    Pull p = { unit, i };
    UInt32 got = frames;
    in->status = noErr;
    OSStatus status = AudioConverterFillComplexBuffer(in->converter, converterInput, &p, &got, out, NULL);
    if (in->status != noErr)
        return in->status;
    if (status != noErr)
        return status;
    return noErr;
}

/* The host's buffers, given memory of the unit's if they have none. */
static void prepareOutput(BasicState *s, UInt32 frames, AudioBufferList *data) {
    size_t plane = (size_t)s->output.mBytesPerFrame * s->maxFrames;
    for (UInt32 b = 0; b < data->mNumberBuffers; b++) {
        if (data->mBuffers[b].mData == NULL)
            data->mBuffers[b].mData = (char *)s->outMemory + b * plane;
        data->mBuffers[b].mDataByteSize = s->output.mBytesPerFrame * frames;
    }
}

static OSStatus mixerRender(AudioUnit unit, BasicState *s, UInt32 frames, const AudioTimeStamp *time,
                            AudioBufferList *data) {
    UInt32 channels = s->output.mChannelsPerFrame;
    memset(s->mix, 0, (size_t)frames * channels * sizeof(float));
    for (UInt32 i = 0; i < s->inputCount; i++) {
        Input *in = &s->inputs[i];
        if (in->callback.inputProc == NULL || in->enable == 0)
            continue;
        s->scratchList.mNumberBuffers = 1;
        s->scratchList.mBuffers[0].mNumberChannels = channels;
        s->scratchList.mBuffers[0].mData = s->scratch;
        s->scratchList.mBuffers[0].mDataByteSize = frames * channels * sizeof(float);
        memset(s->scratch, 0, (size_t)frames * channels * sizeof(float));
        OSStatus status = renderInput(unit, s, i, frames, time, &s->scratchList);
        if (status != noErr)
            return status;
        /* Pan: -1 left, +1 right, the other side lowered linearly. */
        float left = in->pan > 0 ? 1 - in->pan : 1, right = in->pan < 0 ? 1 + in->pan : 1;
        for (UInt32 f = 0; f < frames; f++)
            for (UInt32 c = 0; c < channels; c++) {
                float g = in->volume;
                if (channels == 2)
                    g *= c == 0 ? left : right;
                s->mix[f * channels + c] += s->scratch[f * channels + c] * g;
            }
    }
    for (size_t k = 0; k < (size_t)frames * channels; k++)
        s->mix[k] *= s->outputVolume;
    _AUFromFloat(&s->output, s->mix, frames, data, 0);
    return noErr;
}

static OSStatus basicRender(AudioUnit unit, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                            UInt32 bus, UInt32 frames, AudioBufferList *data) {
    BasicState *s = S(unit);
    OSStatus status;

    if (!s->initialized)
        return kAudioUnitErr_Uninitialized;
    if (bus != 0)
        return kAudioUnitErr_InvalidElement;
    if (frames > s->maxFrames)
        return kAudioUnitErr_TooManyFramesToProcess;
    if (data->mNumberBuffers != (nonInterleaved(&s->output) ? s->output.mChannelsPerFrame : 1))
        return kAudio_ParamError;
    prepareOutput(s, frames, data);
    _AURenderNotify(unit, kAudioUnitRenderAction_PreRender, time, bus, frames, data);
    if (s->kind == kKindMixer)
        status = mixerRender(unit, s, frames, time, data);
    else
        status = renderInput(unit, s, 0, frames, time, data);
    _AURenderNotify(unit, kAudioUnitRenderAction_PostRender, time, bus, frames, data);
    if (status != noErr)
        s->lastError = status;
    return status;
}

static OSStatus basicReset(AudioUnit unit, AudioUnitScope scope, AudioUnitElement element) {
    BasicState *s = S(unit);
    for (UInt32 i = 0; i < s->inputCount; i++)
        if (s->inputs[i].converter != NULL)
            AudioConverterReset(s->inputs[i].converter);
    return noErr;
}

static OSStatus genericStart(AudioUnit unit) {
    S(unit)->running = true;
    return noErr;
}

static OSStatus genericStop(AudioUnit unit) {
    S(unit)->running = false;
    return noErr;
}

/* ---- Properties ---- */

static Boolean validElement(BasicState *s, AudioUnitScope scope, AudioUnitElement element) {
    if (scope == kAudioUnitScope_Input)
        return element < s->inputCount;
    if (scope == kAudioUnitScope_Output || scope == kAudioUnitScope_Global)
        return element == 0;
    return false;
}

static OSStatus basicGetPropertyInfo(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                     AudioUnitElement element, UInt32 *size, Boolean *writable) {
    BasicState *s = S(unit);
    switch (id) {
    case kAudioUnitProperty_StreamFormat:
        if (!validElement(s, scope, element) || scope == kAudioUnitScope_Global)
            return kAudioUnitErr_InvalidElement;
        *size = sizeof(AudioStreamBasicDescription);
        *writable = true;
        return noErr;
    case kAudioUnitProperty_SampleRate:
    case kAudioUnitProperty_Latency:
    case kAudioUnitProperty_TailTime:
        *size = sizeof(Float64);
        *writable = id == kAudioUnitProperty_SampleRate;
        return noErr;
    case kAudioUnitProperty_MaximumFramesPerSlice:
    case kAudioUnitProperty_ElementCount:
    case kAudioUnitProperty_ShouldAllocateBuffer:
        *size = sizeof(UInt32);
        *writable = id != kAudioUnitProperty_ElementCount ||
                    (s->kind == kKindMixer && scope == kAudioUnitScope_Input);
        return noErr;
    case kAudioUnitProperty_LastRenderError:
        *size = sizeof(OSStatus);
        *writable = false;
        return noErr;
    case kAudioUnitProperty_SetRenderCallback:
        *size = sizeof(AURenderCallbackStruct);
        *writable = true;
        return noErr;
    case kAudioUnitProperty_MakeConnection:
        *size = sizeof(AudioUnitConnection);
        *writable = true;
        return noErr;
    case kAudioOutputUnitProperty_IsRunning:
        if (s->kind != kKindGenericOutput)
            return kAudioUnitErr_InvalidProperty;
        *size = sizeof(UInt32);
        *writable = false;
        return noErr;
    case kAudioUnitProperty_ParameterList:
        *size = s->kind == kKindMixer ? 3 * sizeof(AudioUnitParameterID) : 0;
        *writable = false;
        return noErr;
    case kAudioUnitProperty_ParameterInfo:
        if (s->kind != kKindMixer)
            return kAudioUnitErr_InvalidProperty;
        *size = sizeof(AudioUnitParameterInfo);
        *writable = false;
        return noErr;
    default:
        return kAudioUnitErr_InvalidProperty;
    }
}

#define PUT(type, value)                                    \
    do {                                                    \
        if (*ioSize < sizeof(type))                         \
            return kAudioUnitErr_InvalidPropertyValue;      \
        *(type *)data = (value);                            \
        *ioSize = sizeof(type);                             \
        return noErr;                                       \
    } while (0)

static OSStatus basicGetProperty(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                 AudioUnitElement element, void *data, UInt32 *ioSize) {
    BasicState *s = S(unit);
    switch (id) {
    case kAudioUnitProperty_StreamFormat:
        if (!validElement(s, scope, element) || scope == kAudioUnitScope_Global)
            return kAudioUnitErr_InvalidElement;
        PUT(AudioStreamBasicDescription, scope == kAudioUnitScope_Input ? s->inputs[element].format : s->output);
    case kAudioUnitProperty_SampleRate:
        PUT(Float64, scope == kAudioUnitScope_Input && element < s->inputCount
                         ? s->inputs[element].format.mSampleRate : s->output.mSampleRate);
    case kAudioUnitProperty_Latency:
    case kAudioUnitProperty_TailTime:
        PUT(Float64, 0);
    case kAudioUnitProperty_MaximumFramesPerSlice:
        PUT(UInt32, s->maxFrames);
    case kAudioUnitProperty_ShouldAllocateBuffer:
        PUT(UInt32, 1);
    case kAudioUnitProperty_ElementCount:
        PUT(UInt32, scope == kAudioUnitScope_Input ? s->inputCount : scope == kAudioUnitScope_Output ? 1 : 1);
    case kAudioUnitProperty_LastRenderError:
        PUT(OSStatus, s->lastError);
    case kAudioUnitProperty_SetRenderCallback:
        if (scope != kAudioUnitScope_Input || element >= s->inputCount)
            return kAudioUnitErr_InvalidElement;
        PUT(AURenderCallbackStruct, s->inputs[element].callback);
    case kAudioUnitProperty_MakeConnection:
        if (element >= kMaxInputs)
            return kAudioUnitErr_InvalidElement;
        PUT(AudioUnitConnection, unit->connections[element]);
    case kAudioOutputUnitProperty_IsRunning:
        if (s->kind != kKindGenericOutput)
            return kAudioUnitErr_InvalidProperty;
        PUT(UInt32, s->running);
    case kAudioUnitProperty_ParameterList: {
        static const AudioUnitParameterID ids[] = { kMultiChannelMixerParam_Volume, kMultiChannelMixerParam_Enable,
                                                    kMultiChannelMixerParam_Pan };
        UInt32 n = s->kind == kKindMixer ? sizeof(ids) : 0;
        if (*ioSize < n)
            return kAudioUnitErr_InvalidPropertyValue;
        memcpy(data, ids, n);
        *ioSize = n;
        return noErr;
    }
    case kAudioUnitProperty_ParameterInfo: {
        AudioUnitParameterInfo info;
        if (s->kind != kKindMixer || *ioSize < sizeof(info))
            return kAudioUnitErr_InvalidProperty;
        memset(&info, 0, sizeof(info));
        info.flags = kAudioUnitParameterFlag_IsReadable | kAudioUnitParameterFlag_IsWritable;
        switch (element) {
        case kMultiChannelMixerParam_Volume:
            strcpy(info.name, "Volume");
            info.unit = kAudioUnitParameterUnit_LinearGain;
            info.maxValue = info.defaultValue = 1;
            break;
        case kMultiChannelMixerParam_Enable:
            strcpy(info.name, "Enable");
            info.unit = kAudioUnitParameterUnit_Boolean;
            info.maxValue = info.defaultValue = 1;
            break;
        case kMultiChannelMixerParam_Pan:
            strcpy(info.name, "Pan");
            info.unit = kAudioUnitParameterUnit_Pan;
            info.minValue = -1;
            info.maxValue = 1;
            break;
        default:
            return kAudioUnitErr_InvalidParameter;
        }
        memcpy(data, &info, sizeof(info));
        *ioSize = sizeof(info);
        return noErr;
    }
    default:
        return kAudioUnitErr_InvalidProperty;
    }
}

static OSStatus basicSetProperty(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                 AudioUnitElement element, const void *data, UInt32 size) {
    BasicState *s = S(unit);
    switch (id) {
    case kAudioUnitProperty_StreamFormat: {
        AudioStreamBasicDescription f;
        if (!validElement(s, scope, element) || scope == kAudioUnitScope_Global)
            return kAudioUnitErr_InvalidElement;
        if (data == NULL || size < sizeof(f))
            return kAudioUnitErr_InvalidPropertyValue;
        if (s->initialized)
            return kAudioUnitErr_Initialized;
        memcpy(&f, data, sizeof(f));
        if (!_AUFormatIsSupported(&f))
            return kAudioUnitErr_FormatNotSupported;
        if (scope == kAudioUnitScope_Input)
            s->inputs[element].format = f;
        else
            s->output = f;
        return noErr;
    }
    case kAudioUnitProperty_SampleRate: {
        Float64 rate;
        if (data == NULL || size < sizeof(rate))
            return kAudioUnitErr_InvalidPropertyValue;
        if (s->initialized)
            return kAudioUnitErr_Initialized;
        memcpy(&rate, data, sizeof(rate));
        if (scope == kAudioUnitScope_Input && element < s->inputCount)
            s->inputs[element].format.mSampleRate = rate;
        else
            s->output.mSampleRate = rate;
        return noErr;
    }
    case kAudioUnitProperty_MaximumFramesPerSlice: {
        UInt32 n;
        if (data == NULL || size < sizeof(n))
            return kAudioUnitErr_InvalidPropertyValue;
        if (s->initialized)
            return kAudioUnitErr_Initialized;
        memcpy(&n, data, sizeof(n));
        if (n == 0)
            return kAudioUnitErr_InvalidPropertyValue;
        s->maxFrames = n;
        return noErr;
    }
    case kAudioUnitProperty_ShouldAllocateBuffer:
        return noErr;
    case kAudioUnitProperty_ElementCount: {
        UInt32 n;
        if (s->kind != kKindMixer || scope != kAudioUnitScope_Input)
            return kAudioUnitErr_PropertyNotWritable;
        if (data == NULL || size < sizeof(n))
            return kAudioUnitErr_InvalidPropertyValue;
        if (s->initialized)
            return kAudioUnitErr_Initialized;
        memcpy(&n, data, sizeof(n));
        if (n == 0 || n > kMaxInputs)
            return kAudioUnitErr_InvalidPropertyValue;
        s->inputCount = n;
        return noErr;
    }
    case kAudioUnitProperty_SetRenderCallback: {
        AURenderCallbackStruct cb = { NULL, NULL };
        if (scope != kAudioUnitScope_Input && scope != kAudioUnitScope_Global)
            return kAudioUnitErr_InvalidScope;
        if (element >= s->inputCount)
            return kAudioUnitErr_InvalidElement;
        if (data != NULL && size >= sizeof(cb))
            memcpy(&cb, data, sizeof(cb));
        pthread_mutex_lock(&unit->lock);
        s->inputs[element].callback = cb;
        pthread_mutex_unlock(&unit->lock);
        return noErr;
    }
    default:
        return kAudioUnitErr_InvalidProperty;
    }
}

/* ---- Parameters (the mixer's) ---- */

static Float32 *parameter(BasicState *s, AudioUnitParameterID id, AudioUnitScope scope, AudioUnitElement element) {
    if (s->kind != kKindMixer)
        return NULL;
    if (scope == kAudioUnitScope_Output && element == 0 && id == kMultiChannelMixerParam_Volume)
        return &s->outputVolume;
    if (scope != kAudioUnitScope_Input || element >= s->inputCount)
        return NULL;
    switch (id) {
    case kMultiChannelMixerParam_Volume:
        return &s->inputs[element].volume;
    case kMultiChannelMixerParam_Enable:
        return &s->inputs[element].enable;
    case kMultiChannelMixerParam_Pan:
        return &s->inputs[element].pan;
    default:
        return NULL;
    }
}

static OSStatus basicGetParameter(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                                  AudioUnitElement element, AudioUnitParameterValue *value) {
    Float32 *p = parameter(S(unit), id, scope, element);
    if (p == NULL)
        return kAudioUnitErr_InvalidParameter;
    *value = *p;
    return noErr;
}

static OSStatus basicSetParameter(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                                  AudioUnitElement element, AudioUnitParameterValue value) {
    Float32 *p = parameter(S(unit), id, scope, element);
    if (p == NULL)
        return kAudioUnitErr_InvalidParameter;
    if (id == kMultiChannelMixerParam_Pan)
        value = value < -1 ? -1 : value > 1 ? 1 : value;
    *p = value;
    return noErr;
}

#define BASIC_CLASS(NAME, CREATE, START, STOP)                                                               \
    const AUClass NAME = {                                                                                    \
        .create = CREATE,                                                                                     \
        .dispose = basicDispose,                                                                              \
        .initialize = basicInitialize,                                                                        \
        .uninitialize = basicUninitialize,                                                                    \
        .getPropertyInfo = basicGetPropertyInfo,                                                              \
        .getProperty = basicGetProperty,                                                                      \
        .setProperty = basicSetProperty,                                                                      \
        .getParameter = basicGetParameter,                                                                    \
        .setParameter = basicSetParameter,                                                                    \
        .render = basicRender,                                                                                \
        .reset = basicReset,                                                                                  \
        .start = START,                                                                                       \
        .stop = STOP,                                                                                         \
    };
BASIC_CLASS(_AUConverterClass, converterCreate, NULL, NULL)
BASIC_CLASS(_AUGenericOutputClass, genericCreate, genericStart, genericStop)
BASIC_CLASS(_AUMixerClass, mixerCreate, NULL, NULL)
