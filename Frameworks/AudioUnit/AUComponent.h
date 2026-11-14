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
enum {
    kParameterEvent_Immediate = 1,
    kParameterEvent_Ramped = 2,
};

struct AudioUnitParameterEvent {
    AudioUnitScope scope;
    AudioUnitElement element;
    AudioUnitParameterID parameter;
    AudioUnitParameterEventType eventType;
    union {
        struct {
            SInt32 startBufferOffset;
            UInt32 durationInFrames;
            AudioUnitParameterValue startValue;
            AudioUnitParameterValue endValue;
        } ramp;
        struct {
            UInt32 bufferOffset;
            AudioUnitParameterValue value;
        } immediate;
    } eventValues;
};
typedef struct AudioUnitParameterEvent AudioUnitParameterEvent;

/* Components an app registers: a factory gives the plug-in interface, and
   its Lookup gives each call's method by selector. */
enum {
    kAudioUnitRange = 0x0000,
    kAudioUnitInitializeSelect = 0x0001,
    kAudioUnitUninitializeSelect = 0x0002,
    kAudioUnitGetPropertyInfoSelect = 0x0003,
    kAudioUnitGetPropertySelect = 0x0004,
    kAudioUnitSetPropertySelect = 0x0005,
    kAudioUnitAddPropertyListenerSelect = 0x000A,
    kAudioUnitRemovePropertyListenerSelect = 0x000B,
    kAudioUnitRemovePropertyListenerWithUserDataSelect = 0x0012,
    kAudioUnitAddRenderNotifySelect = 0x000F,
    kAudioUnitRemoveRenderNotifySelect = 0x0010,
    kAudioUnitGetParameterSelect = 0x0006,
    kAudioUnitSetParameterSelect = 0x0007,
    kAudioUnitScheduleParametersSelect = 0x0011,
    kAudioUnitRenderSelect = 0x000E,
    kAudioUnitResetSelect = 0x0009,
    kAudioUnitComplexRenderSelect = 0x0013,
    kAudioUnitProcessSelect = 0x0014,
    kAudioUnitProcessMultipleSelect = 0x0015,
};

typedef void (*AudioComponentMethod)(void);

typedef struct AudioComponentPlugInInterface {
    OSStatus (*Open)(void *self, AudioComponentInstance mInstance);
    OSStatus (*Close)(void *self);
    AudioComponentMethod (*Lookup)(SInt16 selector);
    void *reserved;
} AudioComponentPlugInInterface;

typedef AudioComponentPlugInInterface *(*AudioComponentFactoryFunction)(const AudioComponentDescription *inDesc);

typedef OSStatus (*AudioUnitInitializeProc)(void *self);
typedef OSStatus (*AudioUnitUninitializeProc)(void *self);
typedef OSStatus (*AudioUnitGetPropertyInfoProc)(void *self, AudioUnitPropertyID prop, AudioUnitScope scope,
    AudioUnitElement elem, UInt32 *outDataSize, Boolean *outWritable);
typedef OSStatus (*AudioUnitGetPropertyProc)(void *self, AudioUnitPropertyID inID, AudioUnitScope inScope,
    AudioUnitElement inElement, void *outData, UInt32 *ioDataSize);
typedef OSStatus (*AudioUnitSetPropertyProc)(void *self, AudioUnitPropertyID inID, AudioUnitScope inScope,
    AudioUnitElement inElement, const void *inData, UInt32 inDataSize);
typedef OSStatus (*AudioUnitAddPropertyListenerProc)(void *self, AudioUnitPropertyID prop,
    AudioUnitPropertyListenerProc proc, void *userData);
typedef OSStatus (*AudioUnitRemovePropertyListenerProc)(void *self, AudioUnitPropertyID prop,
    AudioUnitPropertyListenerProc proc);
typedef OSStatus (*AudioUnitRemovePropertyListenerWithUserDataProc)(void *self, AudioUnitPropertyID prop,
    AudioUnitPropertyListenerProc proc, void *userData);
typedef OSStatus (*AudioUnitAddRenderNotifyProc)(void *self, AURenderCallback proc, void *userData);
typedef OSStatus (*AudioUnitRemoveRenderNotifyProc)(void *self, AURenderCallback proc, void *userData);
typedef OSStatus (*AudioUnitScheduleParametersProc)(void *self, const AudioUnitParameterEvent *events,
    UInt32 numEvents);
typedef OSStatus (*AudioUnitResetProc)(void *self, AudioUnitScope inScope, AudioUnitElement inElement);
typedef OSStatus (*AudioUnitGetParameterProc)(void *inComponentStorage, AudioUnitParameterID inID,
    AudioUnitScope inScope, AudioUnitElement inElement, AudioUnitParameterValue *outValue);
typedef OSStatus (*AudioUnitSetParameterProc)(void *inComponentStorage, AudioUnitParameterID inID,
    AudioUnitScope inScope, AudioUnitElement inElement, AudioUnitParameterValue inValue,
    UInt32 inBufferOffsetInFrames);
typedef OSStatus (*AudioUnitRenderProc)(void *inComponentStorage, AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp, UInt32 inOutputBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData);
typedef OSStatus (*AudioUnitProcessProc)(void *self, AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp, UInt32 inNumberFrames, AudioBufferList *ioData);
typedef OSStatus (*AudioUnitProcessMultipleProc)(void *self, AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp, UInt32 inNumberFrames, UInt32 inNumberInputBufferLists,
    const AudioBufferList **inInputBufferLists, UInt32 inNumberOutputBufferLists,
    AudioBufferList **ioOutputBufferLists);

enum {
    kAudioComponentFlag_Unsearchable = 1,
    kAudioComponentFlag_SandboxSafe = 2,
    kAudioComponentFlag_IsV3AudioUnit = 4,
    kAudioComponentFlag_RequiresAsyncInstantiation = 8,
    kAudioComponentFlag_CanLoadInProcess = 0x10,
};

typedef UInt32 AudioComponentInstantiationOptions;
enum {
    kAudioComponentInstantiation_LoadOutOfProcess = 1,
    kAudioComponentInstantiation_LoadInProcess = 2,
};

typedef UInt32 AudioComponentValidationResult;
enum {
    kAudioComponentValidationResult_Unknown = 0,
    kAudioComponentValidationResult_Passed,
    kAudioComponentValidationResult_Failed,
    kAudioComponentValidationResult_TimedOut,
    kAudioComponentValidationResult_UnauthorizedError_Open,
    kAudioComponentValidationResult_UnauthorizedError_Init,
};

#define kAudioComponentConfigurationInfo_ValidationResult "ValidationResult"
#define kAudioComponentValidationParameter_TimeOut "TimeOut"
#define kAudioComponentValidationParameter_ForceValidation "ForceValidation"

/* Components. */
AudioComponent AudioComponentFindNext(AudioComponent inComponent, const AudioComponentDescription *inDesc);
UInt32 AudioComponentCount(const AudioComponentDescription *inDesc);
OSStatus AudioComponentCopyName(AudioComponent inComponent, CFStringRef *outName);
OSStatus AudioComponentGetDescription(AudioComponent inComponent, AudioComponentDescription *outDesc);
OSStatus AudioComponentGetVersion(AudioComponent inComponent, UInt32 *outVersion);
OSStatus AudioComponentInstanceNew(AudioComponent inComponent, AudioComponentInstance *outInstance);
OSStatus AudioComponentInstanceDispose(AudioComponentInstance inInstance);
AudioComponent AudioComponentInstanceGetComponent(AudioComponentInstance inInstance);
#ifdef __BLOCKS__
void AudioComponentInstantiate(AudioComponent inComponent, AudioComponentInstantiationOptions inOptions,
    void (^inCompletionHandler)(AudioComponentInstance, OSStatus));
OSStatus AudioComponentValidate(AudioComponent inComponent, CFDictionaryRef inValidationParameters,
    AudioComponentValidationResult *outValidationResult);
#endif
Boolean AudioComponentInstanceCanDo(AudioComponentInstance inInstance, SInt16 inSelectorID);
AudioComponent AudioComponentRegister(const AudioComponentDescription *inDesc, CFStringRef inName,
    UInt32 inVersion, AudioComponentFactoryFunction inFactory);
OSStatus AudioComponentCopyConfigurationInfo(AudioComponent inComponent, CFDictionaryRef *outConfigurationInfo);

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
OSStatus AudioUnitScheduleParameters(AudioUnit inUnit, const AudioUnitParameterEvent *inParameterEvent,
    UInt32 inNumParamEvents);
OSStatus AudioUnitProcess(AudioUnit inUnit, AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp, UInt32 inNumberFrames, AudioBufferList *ioData);
OSStatus AudioUnitProcessMultiple(AudioUnit inUnit, AudioUnitRenderActionFlags *ioActionFlags,
    const AudioTimeStamp *inTimeStamp, UInt32 inNumberFrames, UInt32 inNumberInputBufferLists,
    const AudioBufferList **inInputBufferLists, UInt32 inNumberOutputBufferLists,
    AudioBufferList **ioOutputBufferLists);

#ifdef __cplusplus
}
#endif

#endif
