/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio units' properties and parameters: the ones Minkar's units have.
 */

#ifndef __AUDIOUNITPROPERTIES__
#define __AUDIOUNITPROPERTIES__

#include <AudioUnit/AUComponent.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kAudioUnitProperty_ClassInfo = 0,
    kAudioUnitProperty_MakeConnection = 1,
    kAudioUnitProperty_SampleRate = 2,
    kAudioUnitProperty_ParameterList = 3,
    kAudioUnitProperty_ParameterInfo = 4,
    kAudioUnitProperty_StreamFormat = 8,
    kAudioUnitProperty_ElementCount = 11,
    kAudioUnitProperty_Latency = 12,
    kAudioUnitProperty_SupportedNumChannels = 13,
    kAudioUnitProperty_MaximumFramesPerSlice = 14,
    kAudioUnitProperty_AudioChannelLayout = 19,
    kAudioUnitProperty_TailTime = 20,
    kAudioUnitProperty_BypassEffect = 21,
    kAudioUnitProperty_LastRenderError = 22,
    kAudioUnitProperty_SetRenderCallback = 23,
    kAudioUnitProperty_ShouldAllocateBuffer = 51,
    kAudioUnitProperty_IsInterAppConnected = 1048576 + 1,

    kAudioOutputUnitProperty_CurrentDevice = 2000,
    kAudioOutputUnitProperty_IsRunning = 2001,
    kAudioOutputUnitProperty_ChannelMap = 2002,
    kAudioOutputUnitProperty_EnableIO = 2003,
    kAudioOutputUnitProperty_StartTime = 2004,
    kAudioOutputUnitProperty_SetInputCallback = 2005,
    kAudioOutputUnitProperty_HasIO = 2006,
    kAudioOutputUnitProperty_StartTimestampsAtZero = 2007,

    kAUVoiceIOProperty_BypassVoiceProcessing = 2100,
    kAUVoiceIOProperty_VoiceProcessingEnableAGC = 2101,
    kAUVoiceIOProperty_MuteOutput = 2104,
};

/* The output units' volume (a software gain, 0 to 1). */
enum {
    kHALOutputParam_Volume = 14,
};

typedef struct AURenderCallbackStruct {
    AURenderCallback inputProc;
    void *inputProcRefCon;
} AURenderCallbackStruct;

#ifdef __cplusplus
}
#endif

#endif
