/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio objects and their properties: the system object (its devices, the
 * default ones), devices and streams.
 */

#ifndef __AUDIOHARDWARE__
#define __AUDIOHARDWARE__

#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 AudioObjectID;
typedef UInt32 AudioClassID;
typedef UInt32 AudioObjectPropertySelector;
typedef UInt32 AudioObjectPropertyScope;
typedef UInt32 AudioObjectPropertyElement;
typedef AudioObjectID AudioDeviceID;
typedef AudioObjectID AudioStreamID;

struct AudioObjectPropertyAddress {
    AudioObjectPropertySelector mSelector;
    AudioObjectPropertyScope mScope;
    AudioObjectPropertyElement mElement;
};
typedef struct AudioObjectPropertyAddress AudioObjectPropertyAddress;

typedef OSStatus (*AudioObjectPropertyListenerProc)(AudioObjectID inObjectID, UInt32 inNumberAddresses,
    const AudioObjectPropertyAddress *inAddresses, void *inClientData);
#ifdef __BLOCKS__
typedef void (^AudioObjectPropertyListenerBlock)(UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses);
#endif

enum {
    kAudioObjectUnknown = 0,
    kAudioObjectSystemObject = 1,
    kAudioDeviceUnknown = kAudioObjectUnknown,
};

enum {
    kAudioHardwareNoError = 0,
    kAudioHardwareNotRunningError = 'stop',
    kAudioHardwareUnspecifiedError = 'what',
    kAudioHardwareUnknownPropertyError = 'who?',
    kAudioHardwareBadPropertySizeError = '!siz',
    kAudioHardwareIllegalOperationError = 'nope',
    kAudioHardwareBadObjectError = '!obj',
    kAudioHardwareBadDeviceError = '!dev',
    kAudioHardwareBadStreamError = '!str',
    kAudioHardwareUnsupportedOperationError = 'unop',
    kAudioDevicePermissionsError = '!hog',
};

enum {
    kAudioObjectPropertyScopeGlobal = 'glob',
    kAudioObjectPropertyScopeInput = 'inpt',
    kAudioObjectPropertyScopeOutput = 'outp',
    kAudioObjectPropertyScopePlayThrough = 'ptru',
    kAudioObjectPropertyElementMain = 0,
    kAudioObjectPropertyElementMaster = kAudioObjectPropertyElementMain,
    kAudioObjectPropertyElementWildcard = 0xFFFFFFFF,
    kAudioObjectPropertySelectorWildcard = '****',
    kAudioObjectPropertyScopeWildcard = '****',
};

enum {
    kAudioObjectPropertyName = 'lnam',
    kAudioObjectPropertyManufacturer = 'lmak',
    kAudioObjectPropertyOwnedObjects = 'ownd',
    kAudioObjectPropertyClass = 'clas',
    kAudioHardwarePropertyDevices = 'dev#',
    kAudioHardwarePropertyDefaultInputDevice = 'dIn ',
    kAudioHardwarePropertyDefaultOutputDevice = 'dOut',
    kAudioHardwarePropertyDefaultSystemOutputDevice = 'sOut',
    kAudioHardwarePropertyRunLoop = 'rnlp',
    kAudioHardwarePropertyTranslateUIDToDevice = 'uidd',
    kAudioDevicePropertyDeviceUID = 'uid ',
    kAudioDevicePropertyDeviceNameCFString = kAudioObjectPropertyName,
    kAudioDevicePropertyNominalSampleRate = 'nsrt',
    kAudioDevicePropertyStreamConfiguration = 'slay',
    kAudioDevicePropertyBufferFrameSize = 'fsiz',
    kAudioDevicePropertyLatency = 'ltnc',
    kAudioDevicePropertyDataSource = 'ssrc',
    kAudioDevicePropertyStreams = 'stm#',
    kAudioDevicePropertyTransportType = 'tran',
};

OSStatus AudioObjectGetPropertyDataSize(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
    UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 *outDataSize);
OSStatus AudioObjectGetPropertyData(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
    UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 *ioDataSize, void *outData);
OSStatus AudioObjectSetPropertyData(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
    UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 inDataSize, const void *inData);
Boolean AudioObjectHasProperty(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress);
OSStatus AudioObjectIsPropertySettable(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
    Boolean *outIsSettable);
OSStatus AudioObjectAddPropertyListener(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
    AudioObjectPropertyListenerProc inListener, void *inClientData);
OSStatus AudioObjectRemovePropertyListener(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
    AudioObjectPropertyListenerProc inListener, void *inClientData);
#ifdef __BLOCKS__
OSStatus AudioObjectAddPropertyListenerBlock(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
    void *inDispatchQueue, AudioObjectPropertyListenerBlock inListener);
OSStatus AudioObjectRemovePropertyListenerBlock(AudioObjectID inObjectID, const AudioObjectPropertyAddress *inAddress,
    void *inDispatchQueue, AudioObjectPropertyListenerBlock inListener);
#endif

/* Lowers other apps' sound while this device plays something important. */
OSStatus AudioDeviceDuck(AudioDeviceID inDevice, Float32 inDuckedLevel, const AudioTimeStamp *inStartTime,
    Float32 inRampDuration);

#ifdef __cplusplus
}
#endif

#endif
