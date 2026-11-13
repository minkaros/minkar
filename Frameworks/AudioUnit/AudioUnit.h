/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio units: the components (AudioComponent) and their instances. The
 * functions are AudioToolbox's.
 */

#ifndef __AUDIOUNIT__
#define __AUDIOUNIT__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const double AudioUnitVersionNumber;
extern const unsigned char AudioUnitVersionString[];

typedef struct OpaqueAudioComponent *AudioComponent;
typedef struct ComponentInstanceRecord *AudioComponentInstance;
typedef AudioComponentInstance AudioUnit;

typedef struct AudioComponentDescription {
    OSType componentType;
    OSType componentSubType;
    OSType componentManufacturer;
    UInt32 componentFlags;
    UInt32 componentFlagsMask;
} AudioComponentDescription;

typedef UInt32 AudioUnitPropertyID;
typedef UInt32 AudioUnitScope;
typedef UInt32 AudioUnitElement;
typedef UInt32 AudioUnitParameterID;
typedef Float32 AudioUnitParameterValue;

enum {
    kAudioUnitType_Output = 'auou',
    kAudioUnitType_MusicDevice = 'aumu',
    kAudioUnitType_Effect = 'aufx',
    kAudioUnitType_Mixer = 'aumx',
    kAudioUnitType_FormatConverter = 'aufc',
    kAudioUnitSubType_HALOutput = 'ahal',
    kAudioUnitSubType_DefaultOutput = 'def ',
    kAudioUnitSubType_SystemOutput = 'sys ',
    kAudioUnitSubType_VoiceProcessingIO = 'vpio',
    kAudioUnitManufacturer_Apple = 'appl',
};

enum {
    kAudioUnitScope_Global = 0,
    kAudioUnitScope_Input = 1,
    kAudioUnitScope_Output = 2,
};

#ifdef __cplusplus
}
#endif

#endif
