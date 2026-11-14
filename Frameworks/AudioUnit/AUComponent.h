/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio components and units (AudioToolbox): finding a component by its
 * description, making an instance of it, and the unit's properties,
 * parameters, rendering and listeners.
 */

#ifndef __AUCOMPONENT__
#define __AUCOMPONENT__

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudioTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 AudioUnitRenderActionFlags;
enum {
    kAudioUnitRenderAction_PreRender = (1U << 2),
    kAudioUnitRenderAction_PostRender = (1U << 3),
    kAudioUnitRenderAction_OutputIsSilence = (1U << 4),
    kAudioOfflineUnitRenderAction_Preflight = (1U << 5),
    kAudioOfflineUnitRenderAction_Render = (1U << 6),
    kAudioOfflineUnitRenderAction_Complete = (1U << 7),
    kAudioUnitRenderAction_PostRenderError = (1U << 8),
    kAudioUnitRenderAction_DoNotCheckRenderArgs = (1U << 9),
};

enum {
    kAudioUnitErr_InvalidProperty = -10879,
    kAudioUnitErr_InvalidParameter = -10878,
    kAudioUnitErr_InvalidElement = -10877,
    kAudioUnitErr_NoConnection = -10876,
    kAudioUnitErr_FailedInitialization = -10875,
    kAudioUnitErr_TooManyFramesToProcess = -10874,
    kAudioUnitErr_InvalidFile = -10871,
    kAudioUnitErr_UnknownFileType = -10870,
    kAudioUnitErr_FileNotSpecified = -10869,
    kAudioUnitErr_FormatNotSupported = -10868,
    kAudioUnitErr_Uninitialized = -10867,
    kAudioUnitErr_InvalidScope = -10866,
    kAudioUnitErr_PropertyNotWritable = -10865,
    kAudioUnitErr_CannotDoInCurrentContext = -10863,
    kAudioUnitErr_InvalidPropertyValue = -10851,
    kAudioUnitErr_PropertyNotInUse = -10850,
    kAudioUnitErr_Initialized = -10849,
    kAudioUnitErr_InvalidOfflineRender = -10848,
    kAudioUnitErr_Unauthorized = -10847,
    kAudioComponentErr_InstanceInvalidated = -66749,
};

typedef OSStatus (*AURenderCallback)(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames,
    AudioBufferList *ioData);

typedef void (*AudioUnitPropertyListenerProc)(void *inRefCon, AudioUnit inUnit,
    AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement);

typedef UInt32 AudioUnitParameterEventType;

/* Components. */
AudioComponent AudioComponentFindNext(AudioComponent inComponent, const AudioComponentDescription *inDesc);
UInt32 AudioComponentCount(const AudioComponentDescription *inDesc);
OSStatus AudioComponentCopyName(AudioComponent inComponent, CFStringRef *outName);
OSStatus AudioComponentGetDescription(AudioComponent inComponent, AudioComponentDescription *outDesc);
OSStatus AudioComponentGetVersion(AudioComponent inComponent, UInt32 *outVersion);
OSStatus AudioComponentInstanceNew(AudioComponent inComponent, AudioComponentInstance *outInstance);
OSStatus AudioComponentInstanceDispose(AudioComponentInstance inInstance);
AudioComponent AudioComponentInstanceGetComponent(AudioComponentInstance inInstance);

/* Units. */
OSStatus AudioUnitInitialize(AudioUnit inUnit);
OSStatus AudioUnitUninitialize(AudioUnit inUnit);
OSStatus AudioUnitGetPropertyInfo(AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope,
    AudioUnitElement inElement, UInt32 *outDataSize, Boolean *outWritable);
OSStatus AudioUnitGetProperty(AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope,
    AudioUnitElement inElement, void *outData, UInt32 *ioDataSize);
OSStatus AudioUnitSetProperty(AudioUnit inUnit, AudioUnitPropertyID inID, AudioUnitScope inScope,
    AudioUnitElement inElement, const void *inData, UInt32 inDataSize);
OSStatus AudioUnitAddPropertyListener(AudioUnit inUnit, AudioUnitPropertyID inID,
    AudioUnitPropertyListenerProc inProc, void *inProcUserData);
OSStatus AudioUnitRemovePropertyListenerWithUserData(AudioUnit inUnit, AudioUnitPropertyID inID,
    AudioUnitPropertyListenerProc inProc, void *inProcUserData);
OSStatus AudioUnitAddRenderNotify(AudioUnit inUnit, AURenderCallback inProc, void *inProcUserData);
OSStatus AudioUnitRemoveRenderNotify(AudioUnit inUnit, AURenderCallback inProc, void *inProcUserData);
OSStatus AudioUnitGetParameter(AudioUnit inUnit, AudioUnitParameterID inID, AudioUnitScope inScope,
    AudioUnitElement inElement, AudioUnitParameterValue *outValue);
OSStatus AudioUnitSetParameter(AudioUnit inUnit, AudioUnitParameterID inID, AudioUnitScope inScope,
    AudioUnitElement inElement, AudioUnitParameterValue inValue, UInt32 inBufferOffsetInFrames);
OSStatus AudioUnitRender(AudioUnit inUnit, AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp, UInt32 inOutputBusNumber, UInt32 inNumberFrames,
    AudioBufferList *ioData);
OSStatus AudioUnitReset(AudioUnit inUnit, AudioUnitScope inScope, AudioUnitElement inElement);

#ifdef __cplusplus
}
#endif

#endif
