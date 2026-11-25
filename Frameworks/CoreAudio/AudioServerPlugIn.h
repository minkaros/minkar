/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio server plug-ins (device drivers in user space, AU10): a CFPlugIn
 * bundle in /Library/Audio/Plug-Ins/HAL (.driver) whose factory, for the
 * plug-in type below, gives a driver: a reference to the driver interface.
 * The audio service (audiod) is its host: it asks the driver for its
 * objects and their properties, starts and stops its devices' IO, and
 * runs each IO cycle through the driver's operations. As the public
 * documentation describes it.
 */

#ifndef __AUDIOSERVERPLUGIN__
#define __AUDIOSERVERPLUGIN__

#include <CoreAudio/AudioHardware.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 443ABAB8-E7B3-491A-B985-BEB9187030DB */
#define kAudioServerPlugInTypeUUID                                                                          \
    CFUUIDGetConstantUUIDWithBytes(NULL, 0x44, 0x3A, 0xBA, 0xB8, 0xE7, 0xB3, 0x49, 0x1A, 0xB9, 0x85, 0xBE, \
                                   0xB9, 0x18, 0x70, 0x30, 0xDB)
/* EEA5773D-CC43-49F1-8E00-8F96E7D23B17 */
#define kAudioServerPlugInDriverInterfaceUUID                                                               \
    CFUUIDGetConstantUUIDWithBytes(NULL, 0xEE, 0xA5, 0x77, 0x3D, 0xCC, 0x43, 0x49, 0xF1, 0x8E, 0x00, 0x8F, \
                                   0x96, 0xE7, 0xD2, 0x3B, 0x17)

/* In the driver's own object space, its plug-in object. */
enum { kAudioObjectPlugInObject = 1 };

enum {
    kAudioServerPlugInIOOperationThread = 'thrd',
    kAudioServerPlugInIOOperationCycle = 'cycl',
    kAudioServerPlugInIOOperationReadInput = 'read',
    kAudioServerPlugInIOOperationConvertInput = 'cinp',
    kAudioServerPlugInIOOperationProcessInput = 'pinp',
    kAudioServerPlugInIOOperationProcessOutput = 'pout',
    kAudioServerPlugInIOOperationMixOutput = 'mixo',
    kAudioServerPlugInIOOperationProcessMix = 'pmix',
    kAudioServerPlugInIOOperationConvertMix = 'cmix',
    kAudioServerPlugInIOOperationWriteMix = 'rite',
};

enum {
    kAudioServerPlugInCustomPropertyDataTypeNone = 0,
    kAudioServerPlugInCustomPropertyDataTypeCFString = 'cfst',
    kAudioServerPlugInCustomPropertyDataTypeCFPropertyList = 'plst',
};

typedef struct AudioServerPlugInCustomPropertyInfo {
    AudioObjectPropertySelector mSelector;
    UInt32 mPropertyDataType;
    UInt32 mQualifierDataType;
} AudioServerPlugInCustomPropertyInfo;

typedef struct AudioServerPlugInClientInfo {
    UInt32 mClientID;
    pid_t mProcessID;
    Boolean mIsNativeEndian;
    CFStringRef mBundleID;
} AudioServerPlugInClientInfo;

typedef struct AudioServerPlugInIOCycleInfo {
    UInt64 mIOCycleCounter;
    UInt32 mNominalIOBufferFrameSize;
    AudioTimeStamp mCurrentTime;
    AudioTimeStamp mInputTime;
    AudioTimeStamp mOutputTime;
    AudioTimeStamp mMainTime;
    Float64 mDeviceHostTicksPerFrame;
} AudioServerPlugInIOCycleInfo;

typedef struct AudioServerPlugInHostInterface AudioServerPlugInHostInterface;
typedef const AudioServerPlugInHostInterface *AudioServerPlugInHostRef;

struct AudioServerPlugInHostInterface {
    OSStatus (*PropertiesChanged)(AudioServerPlugInHostRef inHost, AudioObjectID inObjectID,
                                  UInt32 inNumberAddresses, const AudioObjectPropertyAddress *inAddresses);
    OSStatus (*CopyFromStorage)(AudioServerPlugInHostRef inHost, CFStringRef inKey, CFPropertyListRef *outData);
    OSStatus (*WriteToStorage)(AudioServerPlugInHostRef inHost, CFStringRef inKey, CFPropertyListRef inData);
    OSStatus (*DeleteFromStorage)(AudioServerPlugInHostRef inHost, CFStringRef inKey);
    OSStatus (*RequestDeviceConfigurationChange)(AudioServerPlugInHostRef inHost, AudioObjectID inDeviceObjectID,
                                                 UInt64 inChangeAction, void *inChangeInfo);
};

typedef struct AudioServerPlugInDriverInterface AudioServerPlugInDriverInterface;
typedef AudioServerPlugInDriverInterface **AudioServerPlugInDriverRef;

struct AudioServerPlugInDriverInterface {
    void *_reserved;
    HRESULT (*QueryInterface)(void *inDriver, REFIID inUUID, LPVOID *outInterface);
    ULONG (*AddRef)(void *inDriver);
    ULONG (*Release)(void *inDriver);
    OSStatus (*Initialize)(AudioServerPlugInDriverRef inDriver, AudioServerPlugInHostRef inHost);
    OSStatus (*CreateDevice)(AudioServerPlugInDriverRef inDriver, CFDictionaryRef inDescription,
                             const AudioServerPlugInClientInfo *inClientInfo, AudioObjectID *outDeviceObjectID);
    OSStatus (*DestroyDevice)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID);
    OSStatus (*AddDeviceClient)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                const AudioServerPlugInClientInfo *inClientInfo);
    OSStatus (*RemoveDeviceClient)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                   const AudioServerPlugInClientInfo *inClientInfo);
    OSStatus (*PerformDeviceConfigurationChange)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                                 UInt64 inChangeAction, void *inChangeInfo);
    OSStatus (*AbortDeviceConfigurationChange)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                               UInt64 inChangeAction, void *inChangeInfo);
    Boolean (*HasProperty)(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID, pid_t inClientProcessID,
                           const AudioObjectPropertyAddress *inAddress);
    OSStatus (*IsPropertySettable)(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID,
                                   pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress,
                                   Boolean *outIsSettable);
    OSStatus (*GetPropertyDataSize)(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID,
                                    pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress,
                                    UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 *outDataSize);
    OSStatus (*GetPropertyData)(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID,
                                pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress,
                                UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 inDataSize,
                                UInt32 *outDataSize, void *outData);
    OSStatus (*SetPropertyData)(AudioServerPlugInDriverRef inDriver, AudioObjectID inObjectID,
                                pid_t inClientProcessID, const AudioObjectPropertyAddress *inAddress,
                                UInt32 inQualifierDataSize, const void *inQualifierData, UInt32 inDataSize,
                                const void *inData);
    OSStatus (*StartIO)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID);
    OSStatus (*StopIO)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID, UInt32 inClientID);
    OSStatus (*GetZeroTimeStamp)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                 UInt32 inClientID, Float64 *outSampleTime, UInt64 *outHostTime, UInt64 *outSeed);
    OSStatus (*WillDoIOOperation)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                  UInt32 inClientID, UInt32 inOperationID, Boolean *outWillDo,
                                  Boolean *outWillDoInPlace);
    OSStatus (*BeginIOOperation)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                                 UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize,
                                 const AudioServerPlugInIOCycleInfo *inIOCycleInfo);
    OSStatus (*DoIOOperation)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                              AudioObjectID inStreamObjectID, UInt32 inClientID, UInt32 inOperationID,
                              UInt32 inIOBufferFrameSize, const AudioServerPlugInIOCycleInfo *inIOCycleInfo,
                              void *ioMainBuffer, void *ioSecondaryBuffer);
    OSStatus (*EndIOOperation)(AudioServerPlugInDriverRef inDriver, AudioObjectID inDeviceObjectID,
                               UInt32 inClientID, UInt32 inOperationID, UInt32 inIOBufferFrameSize,
                               const AudioServerPlugInIOCycleInfo *inIOCycleInfo);
};

#ifdef __cplusplus
}
#endif

#endif
