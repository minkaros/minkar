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
    kAudioDevicePropertyBufferFrameSizeRange = 'fsz#',
    kAudioDevicePropertyLatency = 'ltnc',
    kAudioDevicePropertyDataSource = 'ssrc',
    kAudioDevicePropertyStreams = 'stm#',
    kAudioDevicePropertyPreferredChannelLayout = 'srnd',
    kAudioDevicePropertyTransportType = 'tran',
    kAudioDevicePropertyDeviceManufacturerCFString = kAudioObjectPropertyManufacturer,
    kAudioDevicePropertyDeviceIsAlive = 'livn',
    kAudioDevicePropertyDeviceIsRunning = 'goin',
    kAudioDevicePropertyVolumeScalar = 'volm',
    kAudioDevicePropertyVolumeDecibels = 'vold',
    kAudioDevicePropertyVolumeRangeDecibels = 'vdb#',
    kAudioDevicePropertyVolumeScalarToDecibels = 'v2db',
    kAudioDevicePropertyVolumeDecibelsToScalar = 'db2v',
    kAudioDevicePropertyMute = 'mute',
    kAudioDevicePropertyStreamFormat = 'sfmt',
    kAudioDevicePropertySafetyOffset = 'saft',
    kAudioDevicePropertyIOProcStreamUsage = 'suse',
    kAudioObjectPropertyBaseClass = 'bcls',
    kAudioObjectPropertyOwner = 'stdv',
    kAudioDevicePropertyPreferredChannelsForStereo = 'dch2',
    kAudioStreamPropertyIsActive = 'sact',
    kAudioStreamPropertyDirection = 'sdir',
    kAudioStreamPropertyTerminalType = 'term',
    kAudioStreamPropertyStartingChannel = 'schn',
    kAudioStreamPropertyLatency = kAudioDevicePropertyLatency,
    kAudioStreamPropertyVirtualFormat = 'sfmt',
    kAudioStreamPropertyAvailableVirtualFormats = 'sfma',
    kAudioStreamPropertyPhysicalFormat = 'pft ',
    kAudioStreamPropertyAvailablePhysicalFormats = 'pfta',

    /* Every object's. */
    kAudioObjectPropertyCreator = 'oplg',
    kAudioObjectPropertyModelName = 'lmod',
    kAudioObjectPropertyElementName = 'lchn',
    kAudioObjectPropertyControlList = 'ctrl',
    kAudioObjectPropertyCustomPropertyInfoList = 'cust',

    /* The system object's plug-ins. */
    kAudioHardwarePropertyPlugInList = 'plg#',
    kAudioHardwarePropertyTranslateBundleIDToPlugIn = 'bidp',

    /* A plug-in's. */
    kAudioPlugInPropertyBundleID = 'piid',
    kAudioPlugInPropertyDeviceList = 'dev#',
    kAudioPlugInPropertyTranslateUIDToDevice = 'uidd',
    kAudioPlugInPropertyBoxList = 'box#',
    kAudioPlugInPropertyClockDeviceList = 'clk#',
    kAudioPlugInCreateAggregateDevice = 'cagg',
    kAudioPlugInDestroyAggregateDevice = 'dagg',

    /* A device's, about its clock and its use. */
    kAudioDevicePropertyModelUID = 'muid',
    kAudioDevicePropertyRelatedDevices = 'akin',
    kAudioDevicePropertyClockDomain = 'clkd',
    kAudioDevicePropertyDeviceCanBeDefaultDevice = 'dflt',
    kAudioDevicePropertyDeviceCanBeDefaultSystemDevice = 'sflt',
    kAudioDevicePropertyDeviceIsRunningSomewhere = 'gone',
    kAudioDevicePropertyAvailableNominalSampleRates = 'nsr#',
    kAudioDevicePropertyIsHidden = 'hidn',
    kAudioDevicePropertyZeroTimeStampPeriod = 'ring',
    kAudioDevicePropertyClockIsStable = 'cstb',
    kAudioDevicePropertyIcon = 'icon',
    kAudioDevicePropertyActualSampleRate = 'asrt',

    /* Aggregate devices'. */
    kAudioAggregateDevicePropertyFullSubDeviceList = 'grup',
    kAudioAggregateDevicePropertyActiveSubDeviceList = 'agrp',
    kAudioAggregateDevicePropertyComposition = 'acom',
    kAudioAggregateDevicePropertyMainSubDevice = 'amst',
    kAudioAggregateDevicePropertyMasterSubDevice = kAudioAggregateDevicePropertyMainSubDevice,
    kAudioAggregateDevicePropertyClockDevice = 'apcd',
    kAudioAggregateDevicePropertyTapList = 'tap#',
    kAudioAggregateDevicePropertySubTapList = 'atap',
    kAudioSubDevicePropertyExtraLatency = 'xltc',
    kAudioSubDevicePropertyDriftCompensation = 'drft',
    kAudioSubDevicePropertyDriftCompensationQuality = 'drfq',

    /* Controls'. */
    kAudioControlPropertyScope = 'cscp',
    kAudioControlPropertyElement = 'celm',
    kAudioLevelControlPropertyScalarValue = 'lcsv',
    kAudioLevelControlPropertyDecibelValue = 'lcdv',
    kAudioLevelControlPropertyDecibelRange = 'lcdr',
    kAudioBooleanControlPropertyValue = 'bcvl',
};

/* An aggregate device's description (AudioHardwareCreateAggregateDevice). */
#define kAudioAggregateDeviceUIDKey "uid"
#define kAudioAggregateDeviceNameKey "name"
#define kAudioAggregateDeviceMainSubDeviceKey "master"
#define kAudioAggregateDeviceMasterSubDeviceKey kAudioAggregateDeviceMainSubDeviceKey
#define kAudioAggregateDeviceClockDeviceKey "clock"
#define kAudioAggregateDeviceIsPrivateKey "private"
#define kAudioAggregateDeviceIsStackedKey "stacked"
#define kAudioAggregateDeviceSubDeviceListKey "subdevices"
#define kAudioAggregateDeviceTapListKey "taps"
#define kAudioAggregateDeviceTapAutoStartKey "tapautostart"
#define kAudioSubDeviceUIDKey "uid"
#define kAudioSubDeviceNameKey "name"
#define kAudioSubDeviceInputChannelsKey "channels-in"
#define kAudioSubDeviceOutputChannelsKey "channels-out"
#define kAudioSubDeviceExtraInputLatencyKey "latency-in"
#define kAudioSubDeviceExtraOutputLatencyKey "latency-out"
#define kAudioSubDeviceDriftCompensationKey "drift"
#define kAudioSubDeviceDriftCompensationQualityKey "drift quality"
#define kAudioSubTapUIDKey "uid"
#define kAudioSubTapDriftCompensationKey "drift"

OSStatus AudioHardwareCreateAggregateDevice(CFDictionaryRef inDescription, AudioObjectID *outDeviceID);
OSStatus AudioHardwareDestroyAggregateDevice(AudioObjectID inDeviceID);

/* Process taps (AU10): a tap object made from a CATapDescription; its
   audio comes as an input of an aggregate device that lists it. */
enum {
    kAudioHardwarePropertyTapList = 'tps#',
    kAudioHardwarePropertyTranslateUIDToTap = 'uidt',
    kAudioHardwarePropertyProcessObjectList = 'prs#',
    kAudioHardwarePropertyTranslatePIDToProcessObject = 'id2p',
    kAudioTapPropertyUID = 'tuid',
    kAudioTapPropertyDescription = 'tdsc',
    kAudioTapPropertyFormat = 'tfmt',
    kAudioProcessPropertyPID = 'ppid',
    kAudioProcessPropertyBundleID = 'pbid',
    kAudioProcessPropertyDevices = 'pdv#',
    kAudioProcessPropertyIsRunning = 'pir?',
    kAudioProcessPropertyIsRunningInput = 'piri',
    kAudioProcessPropertyIsRunningOutput = 'piro',
    kAudioProcessClassID = 'clnt',
};

#ifdef __OBJC__
@class CATapDescription;
#else
typedef struct CATapDescription CATapDescription;
#endif

OSStatus AudioHardwareCreateProcessTap(CATapDescription *inDescription, AudioObjectID *outTapID);
OSStatus AudioHardwareDestroyProcessTap(AudioObjectID inTapID);

enum {
    kAudioObjectClassID = 'aobj',
    kAudioSystemObjectClassID = 'asys',
    kAudioDeviceClassID = 'adev',
    kAudioStreamClassID = 'astr',
    kAudioPlugInClassID = 'aplg',
    kAudioAggregateDeviceClassID = 'aagg',
    kAudioSubDeviceClassID = 'asub',
    kAudioEndPointDeviceClassID = 'edev',
    kAudioClockDeviceClassID = 'aclk',
    kAudioBoxClassID = 'abox',
    kAudioTapClassID = 'tcls',
    kAudioControlClassID = 'actl',
    kAudioLevelControlClassID = 'levl',
    kAudioVolumeControlClassID = 'vlme',
    kAudioBooleanControlClassID = 'togl',
    kAudioMuteControlClassID = 'mute',
};

enum {
    kAudioStreamTerminalTypeUnknown = 0,
    kAudioStreamTerminalTypeLine = 'line',
    kAudioStreamTerminalTypeDigitalAudioInterface = 'spdf',
    kAudioStreamTerminalTypeSpeaker = 'spkr',
    kAudioStreamTerminalTypeHeadphones = 'hdph',
    kAudioStreamTerminalTypeMicrophone = 'micr',
    kAudioStreamTerminalTypeHDMI = 'hdmi',
};

struct AudioStreamRangedDescription {
    AudioStreamBasicDescription mFormat;
    AudioValueRange mSampleRateRange;
};
typedef struct AudioStreamRangedDescription AudioStreamRangedDescription;

enum {
    kAudioDeviceTransportTypeUnknown = 0,
    kAudioDeviceTransportTypeBuiltIn = 'bltn',
    kAudioDeviceTransportTypePCI = 'pci ',
    kAudioDeviceTransportTypeUSB = 'usb ',
    kAudioDeviceTransportTypeHDMI = 'hdmi',
    kAudioDeviceTransportTypeDisplayPort = 'dprt',
    kAudioDeviceTransportTypeAggregate = 'grup',
    kAudioDeviceTransportTypeAutoAggregate = 'fgrp',
    kAudioDeviceTransportTypeVirtual = 'virt',
    kAudioDeviceTransportTypeBluetooth = 'blue',
    kAudioDeviceTransportTypeBluetoothLE = 'blea',
    kAudioDeviceTransportTypeFireWire = '1394',
    kAudioDeviceTransportTypeThunderbolt = 'thun',
    kAudioDeviceTransportTypeAirPlay = 'airp',
    kAudioDeviceTransportTypeAVB = 'eavb',
    kAudioDeviceTransportTypeContinuityCaptureWired = 'ccwd',
    kAudioDeviceTransportTypeContinuityCaptureWireless = 'ccwl',
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

/* IO: the app's procedure, called once a cycle on the device's real-time
   thread with the cycle's output buffer to fill. */
typedef OSStatus (*AudioDeviceIOProc)(AudioObjectID inDevice, const AudioTimeStamp *inNow,
    const AudioBufferList *inInputData, const AudioTimeStamp *inInputTime,
    AudioBufferList *outOutputData, const AudioTimeStamp *inOutputTime, void *inClientData);
typedef AudioDeviceIOProc AudioDeviceIOProcID;

OSStatus AudioDeviceCreateIOProcID(AudioObjectID inDevice, AudioDeviceIOProc inProc,
    void *inClientData, AudioDeviceIOProcID *outIOProcID);
OSStatus AudioDeviceDestroyIOProcID(AudioObjectID inDevice, AudioDeviceIOProcID inIOProcID);
OSStatus AudioDeviceStart(AudioObjectID inDevice, AudioDeviceIOProcID inProcID);
OSStatus AudioDeviceStop(AudioObjectID inDevice, AudioDeviceIOProcID inProcID);
OSStatus AudioDeviceGetCurrentTime(AudioObjectID inDevice, AudioTimeStamp *outTime);

/* Lowers other apps' sound while this device plays something important. */
OSStatus AudioDeviceDuck(AudioDeviceID inDevice, Float32 inDuckedLevel, const AudioTimeStamp *inStartTime,
    Float32 inRampDuration);

#ifdef __cplusplus
}
#endif

#endif
