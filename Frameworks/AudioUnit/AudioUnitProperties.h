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
    kAudioUnitProperty_FactoryPresets = 24,
    kAudioUnitProperty_ContextName = 25,
    kAudioUnitProperty_RenderQuality = 26,
    kAudioUnitProperty_InPlaceProcessing = 29,
    kAudioUnitProperty_ElementName = 30,
    kAudioUnitProperty_CocoaUI = 31,
    kAudioUnitProperty_PresentPreset = 36,
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

/* kAudioUnitProperty_MakeConnection: the source's output feeds the
   destination's input. */
typedef struct AudioUnitConnection {
    AudioUnit sourceAudioUnit;
    UInt32 sourceOutputNumber;
    UInt32 destInputNumber;
} AudioUnitConnection;

typedef UInt32 AudioUnitParameterUnit;
enum {
    kAudioUnitParameterUnit_Generic = 0,
    kAudioUnitParameterUnit_Indexed = 1,
    kAudioUnitParameterUnit_Boolean = 2,
    kAudioUnitParameterUnit_Percent = 3,
    kAudioUnitParameterUnit_Seconds = 4,
    kAudioUnitParameterUnit_SampleFrames = 5,
    kAudioUnitParameterUnit_Phase = 6,
    kAudioUnitParameterUnit_Rate = 7,
    kAudioUnitParameterUnit_Hertz = 8,
    kAudioUnitParameterUnit_Cents = 9,
    kAudioUnitParameterUnit_RelativeSemiTones = 10,
    kAudioUnitParameterUnit_MIDINoteNumber = 11,
    kAudioUnitParameterUnit_MIDIController = 12,
    kAudioUnitParameterUnit_Decibels = 13,
    kAudioUnitParameterUnit_LinearGain = 14,
    kAudioUnitParameterUnit_Degrees = 15,
    kAudioUnitParameterUnit_EqualPowerCrossfade = 16,
    kAudioUnitParameterUnit_MixerFaderCurve1 = 17,
    kAudioUnitParameterUnit_Pan = 18,
    kAudioUnitParameterUnit_Meters = 19,
    kAudioUnitParameterUnit_AbsoluteCents = 20,
    kAudioUnitParameterUnit_Octaves = 21,
    kAudioUnitParameterUnit_BPM = 22,
    kAudioUnitParameterUnit_Beats = 23,
    kAudioUnitParameterUnit_Milliseconds = 24,
    kAudioUnitParameterUnit_Ratio = 25,
    kAudioUnitParameterUnit_CustomUnit = 26,
};

typedef UInt32 AudioUnitParameterOptions;
enum {
    kAudioUnitParameterFlag_CFNameRelease = (1UL << 4),
    kAudioUnitParameterFlag_MeterReadOnly = (1UL << 15),
    kAudioUnitParameterFlag_HasClump = (1UL << 20),
    kAudioUnitParameterFlag_ValuesHaveStrings = (1UL << 21),
    kAudioUnitParameterFlag_DisplayLogarithmic = (1UL << 22),
    kAudioUnitParameterFlag_IsHighResolution = (1UL << 23),
    kAudioUnitParameterFlag_NonRealTime = (1UL << 24),
    kAudioUnitParameterFlag_CanRamp = (1UL << 25),
    kAudioUnitParameterFlag_ExpertMode = (1UL << 26),
    kAudioUnitParameterFlag_HasCFNameString = (1UL << 27),
    kAudioUnitParameterFlag_IsGlobalMeta = (1UL << 28),
    kAudioUnitParameterFlag_IsElementMeta = (1UL << 29),
    kAudioUnitParameterFlag_IsReadable = (1UL << 30),
    kAudioUnitParameterFlag_IsWritable = (1UL << 31),
};

typedef struct AudioUnitParameterInfo {
    char name[52];
    CFStringRef unitName;
    UInt32 clumpID;
    CFStringRef cfNameString;
    AudioUnitParameterUnit unit;
    AudioUnitParameterValue minValue;
    AudioUnitParameterValue maxValue;
    AudioUnitParameterValue defaultValue;
    AudioUnitParameterOptions flags;
} AudioUnitParameterInfo;

/* The multichannel mixer's parameters (input and output scope). */
enum {
    kMultiChannelMixerParam_Volume = 0,
    kMultiChannelMixerParam_Enable = 1,
    kMultiChannelMixerParam_Pan = 2,
};

typedef struct AURenderCallbackStruct {
    AURenderCallback inputProc;
    void *inputProcRefCon;
} AURenderCallbackStruct;

#ifdef __cplusplus
}
#endif

#endif
