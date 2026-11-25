/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio devices: one for each output engine the kernel publishes
 * (IOAudioFamily's IOAudioEngine), with its name, maker, UID, transport and
 * rate, and its volume and mute, which are the engine's level and toggle
 * controls, set through the registry. The audio service (AU2) will own the
 * devices and mix; until then the system object lists the engines here.
 */

#include "AudioDevices.h"
#include "AudioAggregate.h"
#include "AudioServerTypes.h"
#include "AudioServerUser.h"
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <IOKit/IOKitLib.h>
#include <math.h>
#include <stddef.h>
#include <pthread.h>
#include <string.h>

#define kMaxDevices 16
#define kFirstDeviceID 0x100

typedef struct Device {
    AudioDeviceID   id;
    io_service_t    engine;         /* retained */
    io_service_t    volume;         /* IOAudioLevelControl, or 0 */
    io_service_t    mute;           /* IOAudioToggleControl, or 0 */
    io_service_t    inputVolume;    /* the input's level, or 0 */
    Boolean         hasInput;
    UInt32          framesPerCycle; /* asked of the audio service */
    CFStringRef     uid;
    CFDictionaryRef plugIn;         /* a plug-in's device (engine 0): its description from the service */
} Device;

static pthread_mutex_t devicesLock = PTHREAD_MUTEX_INITIALIZER;
static Device devices[kMaxDevices];
static UInt32 deviceCount;
static Boolean devicesFound;

static CFTypeRef copyProperty(io_registry_entry_t entry, const char *key) {
    CFStringRef name = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    CFTypeRef value = IORegistryEntryCreateCFProperty(entry, name, NULL, 0);
    CFRelease(name);
    return value;
}

/* A plug-in device's number (AudioServerTypes.h's keys), or fallback. */
static double plugInNumber(const void *device, const char *key, double fallback);

static SInt64 numberProperty(io_registry_entry_t entry, const char *key, SInt64 fallback) {
    CFTypeRef value = copyProperty(entry, key);
    SInt64 n = fallback;
    if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID())
        CFNumberGetValue(value, kCFNumberSInt64Type, &n);
    if (value != NULL)
        CFRelease(value);
    return n;
}

/* The engine's control of a type and subtype for a usage (output or
   input), or 0. */
static io_service_t findControl(io_service_t engine, UInt32 type, UInt32 subType, UInt32 usage) {
    io_iterator_t children;
    io_registry_entry_t child;
    io_service_t found = 0;

    if (IORegistryEntryGetChildIterator(engine, kIOServicePlane, &children) != KERN_SUCCESS)
        return 0;
    while (found == 0 && (child = IOIteratorNext(children)) != 0) {
        if ((UInt32)numberProperty(child, "IOAudioControlType", 0) == type &&
            (UInt32)numberProperty(child, "IOAudioControlSubType", 0) == subType &&
            (UInt32)numberProperty(child, "IOAudioControlUsage", 'outp') == usage)
            found = child;
        else
            IOObjectRelease(child);
    }
    IOObjectRelease(children);
    return found;
}

/* Whether an engine has a stream of a direction (0 output, 1 input). */
static Boolean hasStream(io_service_t engine, SInt64 direction) {
    io_iterator_t children;
    io_registry_entry_t child;
    Boolean found = false;

    if (IORegistryEntryGetChildIterator(engine, kIOServicePlane, &children) != KERN_SUCCESS)
        return false;
    while (!found && (child = IOIteratorNext(children)) != 0) {
        if (IOObjectConformsTo(child, "IOAudioStream") &&
            numberProperty(child, "IOAudioStreamDirection", -1) == direction)
            found = true;
        IOObjectRelease(child);
    }
    IOObjectRelease(children);
    return found;
}

/* The devices of the audio service's plug-ins (AU10), after the engines:
   their descriptions, asked of the service. */
static void findPlugInDevices(void) {
    mach_port_t server = MACH_PORT_NULL;
    vm_offset_t data = 0;
    mach_msg_type_number_t count = 0;

    if (bootstrap_look_up(bootstrap_port, kAudioServerName, &server) != KERN_SUCCESS)
        return;
    if (as_copy_devices(server, &data, &count) == KERN_SUCCESS && data != 0) {
        CFDataRef bytes = CFDataCreate(NULL, (const UInt8 *)data, (CFIndex)count);
        CFPropertyListRef list = CFPropertyListCreateWithData(NULL, bytes, 0, NULL, NULL);
        CFRelease(bytes);
        vm_deallocate(mach_task_self(), data, count);
        if (list != NULL && CFGetTypeID(list) == CFArrayGetTypeID())
            for (CFIndex i = 0; i < CFArrayGetCount(list) && deviceCount < kMaxDevices; i++) {
                CFDictionaryRef description = CFArrayGetValueAtIndex(list, i);
                CFStringRef uid = CFGetTypeID(description) == CFDictionaryGetTypeID() ?
                    CFDictionaryGetValue(description, CFSTR(kAudioServerDeviceUID)) : NULL;
                if (uid == NULL || CFGetTypeID(uid) != CFStringGetTypeID())
                    continue;
                Device *d = &devices[deviceCount];
                memset(d, 0, sizeof(*d));
                d->id = kFirstDeviceID + deviceCount;
                d->plugIn = CFRetain(description);
                d->uid = CFRetain(uid);
                d->hasInput = plugInNumber(d, kAudioServerDeviceInChannels, 0) > 0;
                d->framesPerCycle = kAudioServerFramesPerCycle;
                deviceCount++;
            }
        if (list != NULL)
            CFRelease(list);
    }
    mach_port_deallocate(mach_task_self(), server);
}

static double plugInNumber(const void *device, const char *key, double fallback) {
    const Device *d = device;
    CFStringRef k;
    CFTypeRef v;
    double n = fallback;
    if (d->plugIn == NULL)
        return fallback;
    k = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
    v = CFDictionaryGetValue(d->plugIn, k);
    CFRelease(k);
    if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberDoubleType, &n);
    return n;
}

/* A device's number: the engine's registry property, or the plug-in
   device's description key. */
static SInt64 deviceNumber(const Device *d, const char *engineKey, const char *plugInKey, SInt64 fallback) {
    if (d->plugIn != NULL)
        return (SInt64)plugInNumber(d, plugInKey, (double)fallback);
    return numberProperty(d->engine, engineKey, fallback);
}

/* The engines, once: each output engine is a device, in the registry's
   order. Its UID is its path in the registry, the same from boot to boot
   for the same hardware. */
static void findDevices(void) {
    io_iterator_t engines;
    io_service_t engine;

    if (devicesFound)
        return;
    devicesFound = true;
    if (IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching("IOAudioEngine"),
                                     &engines) != KERN_SUCCESS)
        return;
    while ((engine = IOIteratorNext(engines)) != 0) {
        io_string_t path;

        if (deviceCount == kMaxDevices || !hasStream(engine, 0)) {
            IOObjectRelease(engine);
            continue;
        }
        Device *d = &devices[deviceCount];
        d->id = kFirstDeviceID + deviceCount;
        d->engine = engine;
        d->volume = findControl(engine, 'levl', 'vlme', 'outp');
        d->mute = findControl(engine, 'togl', 'mute', 'outp');
        d->hasInput = hasStream(engine, 1);
        d->inputVolume = d->hasInput ? findControl(engine, 'levl', 'vlme', 'inpt') : 0;
        d->framesPerCycle = kAudioServerFramesPerCycle;
        if (IORegistryEntryGetPath(engine, kIOServicePlane, path) == KERN_SUCCESS)
            d->uid = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
        else
            d->uid = CFStringCreateWithFormat(NULL, NULL, CFSTR("MinkarAudioDevice:%u"), (unsigned)deviceCount);
        deviceCount++;
    }
    IOObjectRelease(engines);
    findPlugInDevices();
}

static Device *deviceWithID(AudioObjectID id) {
    Device *found = NULL;

    pthread_mutex_lock(&devicesLock);
    findDevices();
    if (id >= kFirstDeviceID && id < kFirstDeviceID + deviceCount)
        found = &devices[id - kFirstDeviceID];
    pthread_mutex_unlock(&devicesLock);
    return found;
}

UInt32 _AudioDeviceCopyIDs(AudioDeviceID *ids, UInt32 capacity) {
    UInt32 n;

    pthread_mutex_lock(&devicesLock);
    findDevices();
    for (n = 0; n < deviceCount && n < capacity; n++)
        ids[n] = devices[n].id;
    n = deviceCount;
    pthread_mutex_unlock(&devicesLock);
    /* Then this process's aggregate devices. */
    return n + _AudioAggregateCopyIDs(ids + (n < capacity ? n : capacity), n < capacity ? capacity - n : 0);
}

/* Whether a device plays through a display (HDMI, DisplayPort): a
   graphics card's audio, which a display may not even have speakers for. */
static Boolean isDisplayAudio(const Device *d) {
    io_registry_entry_t parent;
    UInt32 transport = kAudioDeviceTransportTypeUnknown;

    if (d->plugIn != NULL)
        return false;
    if (IORegistryEntryGetParentEntry(d->engine, kIOServicePlane, &parent) == KERN_SUCCESS) {
        transport = (UInt32)numberProperty(parent, "IOAudioDeviceTransportType", 0);
        IOObjectRelease(parent);
    }
    return transport == kAudioDeviceTransportTypeHDMI ||
           transport == kAudioDeviceTransportTypeDisplayPort;
}

/* The default output: the first device that is not a display's (the
   computer's own outputs), else the first device. */
AudioDeviceID _AudioDeviceDefaultOutput(void) {
    AudioDeviceID id = kAudioObjectUnknown;

    pthread_mutex_lock(&devicesLock);
    findDevices();
    for (UInt32 i = 0; i < deviceCount && id == kAudioObjectUnknown; i++)
        if (!isDisplayAudio(&devices[i]) && devices[i].plugIn == NULL)
            id = devices[i].id;
    for (UInt32 i = 0; i < deviceCount && id == kAudioObjectUnknown; i++)
        if (devices[i].plugIn != NULL && plugInNumber(&devices[i], kAudioServerDeviceCanBeDefault, 1) != 0)
            id = devices[i].id;
    if (id == kAudioObjectUnknown && deviceCount > 0)
        id = devices[0].id;
    pthread_mutex_unlock(&devicesLock);
    return id;
}

/* The first device with an input. */
AudioDeviceID _AudioDeviceDefaultInput(void) {
    AudioDeviceID id = kAudioObjectUnknown;

    pthread_mutex_lock(&devicesLock);
    findDevices();
    for (UInt32 i = 0; i < deviceCount && id == kAudioObjectUnknown; i++)
        if (devices[i].hasInput)
            id = devices[i].id;
    pthread_mutex_unlock(&devicesLock);
    return id;
}

UInt32 _AudioDeviceFramesPerCycle(AudioObjectID id) {
    Device *d = deviceWithID(id);
    return d != NULL ? d->framesPerCycle : kAudioServerFramesPerCycle;
}

AudioDeviceID _AudioDeviceWithUID(CFStringRef uid) {
    AudioDeviceID id = kAudioObjectUnknown;

    if (uid == NULL)
        return id;
    if ((id = _AudioAggregateWithUID(uid)) != kAudioObjectUnknown)
        return id;
    pthread_mutex_lock(&devicesLock);
    findDevices();
    for (UInt32 i = 0; i < deviceCount; i++)
        if (CFEqual(devices[i].uid, uid))
            id = devices[i].id;
    pthread_mutex_unlock(&devicesLock);
    return id;
}

/* ---- Streams ------------------------------------------------------------ */

/* Each device's streams, objects of their own: its output, and its input
   when it has one. IDs from kFirstStreamID, two a device (even: output). */
#define kFirstStreamID 0x200

static AudioStreamID streamID(const Device *d, Boolean input) {
    return kFirstStreamID + (AudioStreamID)(d - devices) * 2 + (input ? 1 : 0);
}

/* The device a stream ID is of, and the stream's direction; NULL if none. */
static Device *streamOwner(AudioObjectID id, Boolean *input) {
    Device *found = NULL;

    pthread_mutex_lock(&devicesLock);
    findDevices();
    if (id >= kFirstStreamID && id < kFirstStreamID + deviceCount * 2) {
        found = &devices[(id - kFirstStreamID) / 2];
        *input = ((id - kFirstStreamID) & 1) != 0;
        if (*input && !found->hasInput)
            found = NULL;
    }
    pthread_mutex_unlock(&devicesLock);
    return found;
}

/* The format the IOProc and the hardware use: interleaved float. */
static void deviceFormat(Device *d, AudioStreamBasicDescription *f) {
    memset(f, 0, sizeof(*f));
    f->mSampleRate = (Float64)plugInNumber(d, kAudioServerDeviceSampleRate,
                                           (double)numberProperty(d->engine, "IOAudioEngineSampleRate", 0));
    f->mFormatID = kAudioFormatLinearPCM;
    f->mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    f->mChannelsPerFrame = kAudioServerChannels;
    f->mBitsPerChannel = 32;
    f->mBytesPerFrame = kAudioServerChannels * sizeof(Float32);
    f->mFramesPerPacket = 1;
    f->mBytesPerPacket = f->mBytesPerFrame;
}

static Boolean streamHas(AudioObjectPropertySelector selector) {
    switch (selector) {
    case kAudioObjectPropertyClass:
    case kAudioObjectPropertyBaseClass:
    case kAudioObjectPropertyOwner:
    case kAudioStreamPropertyIsActive:
    case kAudioStreamPropertyDirection:
    case kAudioStreamPropertyTerminalType:
    case kAudioStreamPropertyStartingChannel:
    case kAudioStreamPropertyLatency:
    case kAudioStreamPropertyVirtualFormat:
    case kAudioStreamPropertyPhysicalFormat:
    case kAudioStreamPropertyAvailableVirtualFormats:
    case kAudioStreamPropertyAvailablePhysicalFormats:
        return true;
    default:
        return false;
    }
}

static UInt32 streamSize(AudioObjectPropertySelector selector) {
    switch (selector) {
    case kAudioStreamPropertyVirtualFormat:
    case kAudioStreamPropertyPhysicalFormat:
        return sizeof(AudioStreamBasicDescription);
    case kAudioStreamPropertyAvailableVirtualFormats:
    case kAudioStreamPropertyAvailablePhysicalFormats:
        return sizeof(AudioStreamRangedDescription);
    default:
        return sizeof(UInt32);
    }
}

static OSStatus streamGet(Device *d, Boolean input, const AudioObjectPropertyAddress *a, UInt32 *ioSize, void *out) {
    UInt32 size = streamSize(a->mSelector);

    if (!streamHas(a->mSelector))
        return kAudioHardwareUnknownPropertyError;
    if (ioSize == NULL || *ioSize < size || out == NULL)
        return kAudioHardwareBadPropertySizeError;
    switch (a->mSelector) {
    case kAudioObjectPropertyClass:
        *(UInt32 *)out = kAudioStreamClassID;
        break;
    case kAudioObjectPropertyBaseClass:
        *(UInt32 *)out = kAudioObjectClassID;
        break;
    case kAudioObjectPropertyOwner:
        *(AudioObjectID *)out = d->id;
        break;
    case kAudioStreamPropertyIsActive:
        *(UInt32 *)out = 1;
        break;
    case kAudioStreamPropertyDirection:
        *(UInt32 *)out = input ? 1 : 0;
        break;
    case kAudioStreamPropertyTerminalType:
        *(UInt32 *)out = input ? kAudioStreamTerminalTypeMicrophone : kAudioStreamTerminalTypeSpeaker;
        break;
    case kAudioStreamPropertyStartingChannel:
        *(UInt32 *)out = 1;
        break;
    case kAudioStreamPropertyLatency:
        *(UInt32 *)out = 0;     /* the device's own latency holds it all */
        break;
    case kAudioStreamPropertyVirtualFormat:
    case kAudioStreamPropertyPhysicalFormat:
        deviceFormat(d, out);
        break;
    case kAudioStreamPropertyAvailableVirtualFormats:
    case kAudioStreamPropertyAvailablePhysicalFormats: {
        AudioStreamRangedDescription *r = out;
        deviceFormat(d, &r->mFormat);
        r->mSampleRateRange.mMinimum = r->mSampleRateRange.mMaximum = r->mFormat.mSampleRate;
        break;
    }
    }
    *ioSize = size;
    return noErr;
}

/* A device's streams in a scope: output, input, or both (global). */
static UInt32 streamsIn(Device *d, AudioObjectPropertyScope scope, AudioStreamID *ids) {
    UInt32 n = 0;

    if (scope != kAudioObjectPropertyScopeInput) {
        if (ids)
            ids[n] = streamID(d, false);
        n++;
    }
    if (d->hasInput && scope != kAudioObjectPropertyScopeOutput) {
        if (ids)
            ids[n] = streamID(d, true);
        n++;
    }
    return n;
}

Boolean _AudioDeviceExists(AudioObjectID id) {
    Boolean input;

    if (_AudioAggregateIs(id) || _AudioTapIs(id) || _AudioProcessIs(id))
        return true;
    return deviceWithID(id) != NULL || streamOwner(id, &input) != NULL;
}

/* ---- Volume ------------------------------------------------------------ */

typedef struct Level {
    SInt32 value, min, max;
    Float64 minDB, maxDB;
} Level;

static Boolean readControlLevel(io_service_t control, Level *l) {
    if (control == 0)
        return false;
    l->value = (SInt32)numberProperty(control, "IOAudioControlValue", 0);
    l->min = (SInt32)numberProperty(control, "IOAudioLevelControlMinValue", 0);
    l->max = (SInt32)numberProperty(control, "IOAudioLevelControlMaxValue", 0);
    l->minDB = (SInt32)numberProperty(control, "IOAudioLevelControlMinDB", 0) / 65536.0;
    l->maxDB = (SInt32)numberProperty(control, "IOAudioLevelControlMaxDB", 0) / 65536.0;
    return l->max > l->min;
}

static Boolean readLevel(Device *d, Level *l) {
    return readControlLevel(d->volume, l);
}

static Boolean isInput(const AudioObjectPropertyAddress *a) {
    return a->mScope == kAudioObjectPropertyScopeInput;
}

/* The level a volume property is about: the input's in the input scope. */
static Boolean readScopedLevel(Device *d, const AudioObjectPropertyAddress *a, Level *l) {
    return readControlLevel(isInput(a) ? d->inputVolume : d->volume, l);
}

static Float32 scalarOf(const Level *l, SInt32 value) {
    return (Float32)(value - l->min) / (Float32)(l->max - l->min);
}

static SInt32 valueOfScalar(const Level *l, Float32 scalar) {
    if (scalar < 0)
        scalar = 0;
    if (scalar > 1)
        scalar = 1;
    return l->min + (SInt32)lroundf(scalar * (Float32)(l->max - l->min));
}

static Float32 decibelsOf(const Level *l, SInt32 value) {
    return (Float32)(l->minDB + (l->maxDB - l->minDB) * scalarOf(l, value));
}

static OSStatus setControlValue(io_service_t control, SInt32 value) {
    CFNumberRef number = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
    CFDictionaryRef properties = CFDictionaryCreate(NULL, (const void **)&(CFStringRef){ CFSTR("IOAudioControlValue") },
        (const void **)&number, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    kern_return_t kr = IORegistryEntrySetCFProperties(control, properties);

    CFRelease(properties);
    CFRelease(number);
    return kr == KERN_SUCCESS ? noErr : kAudioHardwareIllegalOperationError;
}

/* ---- Properties -------------------------------------------------------- */

static Boolean isOutputOrGlobal(const AudioObjectPropertyAddress *a) {
    return a->mScope == kAudioObjectPropertyScopeOutput || a->mScope == kAudioObjectPropertyScopeGlobal ||
           a->mScope == kAudioObjectPropertyScopeWildcard;
}

/* The volume's elements: the main one, and each of two channels (one
   level for all here). */
static Boolean isVolumeElement(const AudioObjectPropertyAddress *a) {
    return a->mElement <= 2 || a->mElement == kAudioObjectPropertyElementWildcard;
}

Boolean _AudioDeviceHasProperty(AudioObjectID id, const AudioObjectPropertyAddress *a) {
    Device *d = deviceWithID(id);
    Boolean input;

    if (a == NULL)
        return false;
    if (d == NULL && _AudioAggregateIs(id))
        return _AudioAggregateHasProperty(id, a);
    if (d == NULL && _AudioTapIs(id))
        return _AudioTapHasProperty(id, a);
    if (d == NULL && _AudioProcessIs(id))
        return _AudioProcessHasProperty(id, a);
    if (d == NULL)
        return streamOwner(id, &input) != NULL && streamHas(a->mSelector);
    switch (a->mSelector) {
    case kAudioDevicePropertyStreams:
    case kAudioObjectPropertyOwnedObjects:
    case kAudioDevicePropertyPreferredChannelsForStereo:
    case kAudioDevicePropertyPreferredChannelLayout:
        return true;
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer:
    case kAudioObjectPropertyClass:
    case kAudioObjectPropertyBaseClass:
    case kAudioDevicePropertyDeviceUID:
    case kAudioDevicePropertyTransportType:
    case kAudioDevicePropertyNominalSampleRate:
    case kAudioDevicePropertyDeviceIsAlive:
    case kAudioDevicePropertyDeviceIsRunning:
    case kAudioDevicePropertyBufferFrameSize:
    case kAudioDevicePropertyBufferFrameSizeRange:
        return true;
    case kAudioDevicePropertyStreamConfiguration:
    case kAudioDevicePropertyStreamFormat:
    case kAudioDevicePropertyLatency:
    case kAudioDevicePropertySafetyOffset:
        return a->mScope == kAudioObjectPropertyScopeOutput || a->mScope == kAudioObjectPropertyScopeWildcard ||
               (isInput(a) && d->hasInput);
    case kAudioDevicePropertyVolumeScalar:
    case kAudioDevicePropertyVolumeDecibels:
    case kAudioDevicePropertyVolumeRangeDecibels:
    case kAudioDevicePropertyVolumeScalarToDecibels:
    case kAudioDevicePropertyVolumeDecibelsToScalar:
        if (isInput(a))
            return d->inputVolume != 0 && isVolumeElement(a);
        return d->volume != 0 && isOutputOrGlobal(a) && isVolumeElement(a);
    case kAudioDevicePropertyMute:
        return (d->mute != 0 || d->volume != 0) && isOutputOrGlobal(a) && isVolumeElement(a);
    default:
        return false;
    }
}

Boolean _AudioDeviceIsPropertySettable(AudioObjectID id, const AudioObjectPropertyAddress *a) {
    if (_AudioTapIs(id) || _AudioProcessIs(id))
        return false;
    if (_AudioAggregateIs(id))
        return _AudioAggregateIsPropertySettable(id, a);
    switch (a->mSelector) {
    case kAudioDevicePropertyVolumeScalar:
    case kAudioDevicePropertyVolumeDecibels:
    case kAudioDevicePropertyMute:
    case kAudioDevicePropertyBufferFrameSize:
        return true;
    default:
        return false;
    }
}

UInt32 _AudioDevicePropertySize(AudioObjectID id, const AudioObjectPropertyAddress *a) {
    Device *d = deviceWithID(id);
    Boolean input;

    if (d == NULL && _AudioAggregateIs(id))
        return _AudioAggregatePropertySize(id, a);
    if (d == NULL && _AudioTapIs(id))
        return _AudioTapPropertySize(id, a);
    if (d == NULL && _AudioProcessIs(id))
        return _AudioProcessPropertySize(id, a);
    if (d == NULL && streamOwner(id, &input) != NULL)
        return streamSize(a->mSelector);
    switch (a->mSelector) {
    case kAudioDevicePropertyStreams:
    case kAudioObjectPropertyOwnedObjects:
        return d != NULL ? streamsIn(d, a->mSelector == kAudioObjectPropertyOwnedObjects ?
                                     kAudioObjectPropertyScopeGlobal : a->mScope, NULL) * sizeof(AudioStreamID) : 0;
    case kAudioDevicePropertyPreferredChannelsForStereo:
        return 2 * sizeof(UInt32);
    case kAudioDevicePropertyPreferredChannelLayout:
        return (UInt32)(offsetof(AudioChannelLayout, mChannelDescriptions) +
                        kAudioServerChannels * sizeof(AudioChannelDescription));
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer:
    case kAudioDevicePropertyDeviceUID:
        return sizeof(CFStringRef);
    case kAudioDevicePropertyNominalSampleRate:
        return sizeof(Float64);
    case kAudioDevicePropertyVolumeScalar:
    case kAudioDevicePropertyVolumeDecibels:
    case kAudioDevicePropertyVolumeScalarToDecibels:
    case kAudioDevicePropertyVolumeDecibelsToScalar:
        return sizeof(Float32);
    case kAudioDevicePropertyVolumeRangeDecibels:
    case kAudioDevicePropertyBufferFrameSizeRange:
        return sizeof(AudioValueRange);
    case kAudioDevicePropertyStreamConfiguration:
        return sizeof(AudioBufferList);
    case kAudioDevicePropertyStreamFormat:
        return sizeof(AudioStreamBasicDescription);
    default:
        return sizeof(UInt32);
    }
}

/* Without a mute control, mute is the level at its lowest, and unmuting
   restores the level it had. */
static SInt32 levelBeforeMute[kMaxDevices];
static Boolean mutedByLevel[kMaxDevices];

OSStatus _AudioDeviceGetProperty(AudioObjectID id, const AudioObjectPropertyAddress *a,
                                 UInt32 *ioSize, void *out) {
    Device *d = deviceWithID(id);
    UInt32 size;
    Level l;
    Boolean input;

    if (d == NULL && _AudioAggregateIs(id))
        return _AudioAggregateGetProperty(id, a, ioSize, out);
    if (d == NULL && _AudioTapIs(id))
        return _AudioTapGetProperty(id, a, ioSize, out);
    if (d == NULL && _AudioProcessIs(id))
        return _AudioProcessGetProperty(id, a, ioSize, out);
    if (d == NULL) {
        Device *owner = streamOwner(id, &input);
        return owner != NULL ? streamGet(owner, input, a, ioSize, out) : kAudioHardwareBadObjectError;
    }
    if (!_AudioDeviceHasProperty(id, a))
        return kAudioHardwareUnknownPropertyError;
    size = _AudioDevicePropertySize(id, a);
    if (ioSize == NULL || *ioSize < size || out == NULL)
        return kAudioHardwareBadPropertySizeError;
    io_registry_entry_t parent = 0;
    switch (a->mSelector) {
    case kAudioDevicePropertyStreams:
    case kAudioObjectPropertyOwnedObjects:
        streamsIn(d, a->mSelector == kAudioObjectPropertyOwnedObjects ? kAudioObjectPropertyScopeGlobal : a->mScope,
                  out);
        break;
    case kAudioDevicePropertyPreferredChannelsForStereo:
        ((UInt32 *)out)[0] = 1;
        ((UInt32 *)out)[1] = 2;
        break;
    case kAudioDevicePropertyPreferredChannelLayout: {
        AudioChannelLayout *l2 = out;
        memset(l2, 0, size);
        l2->mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelDescriptions;
        l2->mNumberChannelDescriptions = kAudioServerChannels;
        for (UInt32 i = 0; i < kAudioServerChannels; i++)
            l2->mChannelDescriptions[i].mChannelLabel = kAudioServerChannels == 2 ?
                (i == 0 ? kAudioChannelLabel_Left : kAudioChannelLabel_Right) : kAudioChannelLabel_Discrete_0 + i;
        break;
    }
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer: {
        CFTypeRef value = NULL;
        if (d->plugIn != NULL) {
            value = CFDictionaryGetValue(d->plugIn, a->mSelector == kAudioObjectPropertyName ?
                                         CFSTR(kAudioServerDeviceName) : CFSTR(kAudioServerDeviceManufacturer));
            if (value != NULL)
                CFRetain(value);
        } else if (IORegistryEntryGetParentEntry(d->engine, kIOServicePlane, &parent) == KERN_SUCCESS) {
            value = copyProperty(parent, a->mSelector == kAudioObjectPropertyName ?
                                 "IOAudioDeviceName" : "IOAudioDeviceManufacturerName");
            IOObjectRelease(parent);
        }
        if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID()) {
            if (value != NULL)
                CFRelease(value);
            value = CFRetain(a->mSelector == kAudioObjectPropertyName ? CFSTR("Audio Output") : CFSTR("Unknown"));
        }
        *(CFStringRef *)out = value;
        break;
    }
    case kAudioDevicePropertyDeviceUID:
        *(CFStringRef *)out = CFRetain(d->uid);
        break;
    case kAudioObjectPropertyClass:
        *(UInt32 *)out = kAudioDeviceClassID;
        break;
    case kAudioObjectPropertyBaseClass:
        *(UInt32 *)out = kAudioObjectClassID;
        break;
    case kAudioDevicePropertyTransportType: {
        UInt32 transport = (UInt32)plugInNumber(d, kAudioServerDeviceTransport, kAudioDeviceTransportTypeUnknown);
        if (d->plugIn == NULL && IORegistryEntryGetParentEntry(d->engine, kIOServicePlane, &parent) == KERN_SUCCESS) {
            transport = (UInt32)numberProperty(parent, "IOAudioDeviceTransportType", 0);
            IOObjectRelease(parent);
        }
        *(UInt32 *)out = transport;
        break;
    }
    case kAudioDevicePropertyNominalSampleRate:
        *(Float64 *)out = (Float64)plugInNumber(d, kAudioServerDeviceSampleRate,
                                                (double)numberProperty(d->engine, "IOAudioEngineSampleRate", 0));
        break;
    case kAudioDevicePropertyDeviceIsAlive:
        *(UInt32 *)out = 1;
        break;
    case kAudioDevicePropertyDeviceIsRunning:
        *(UInt32 *)out = numberProperty(d->engine, "IOAudioEngineState", 0) == 1;
        break;
    case kAudioDevicePropertyBufferFrameSize:
        /* The audio service's cycle, as this process asks for it. */
        *(UInt32 *)out = d->framesPerCycle;
        break;
    case kAudioDevicePropertyBufferFrameSizeRange:
        ((AudioValueRange *)out)->mMinimum = kAudioServerMinFramesPerCycle;
        ((AudioValueRange *)out)->mMaximum = 1024;
        break;
    case kAudioDevicePropertyLatency:
        *(UInt32 *)out = (UInt32)deviceNumber(d, isInput(a) ? "IOAudioEngineInputSampleLatency" :
                                              "IOAudioEngineOutputSampleLatency",
                                              isInput(a) ? kAudioServerDeviceInLatency : kAudioServerDeviceOutLatency, 0);
        break;
    case kAudioDevicePropertySafetyOffset:
        *(UInt32 *)out = (UInt32)deviceNumber(d, "IOAudioEngineSampleOffset",
                                              isInput(a) ? kAudioServerDeviceInSafety : kAudioServerDeviceOutSafety, 0);
        break;
    case kAudioDevicePropertyStreamConfiguration: {
        /* One buffer of interleaved channels, as the IOProc gets it. */
        AudioBufferList *list = out;
        list->mNumberBuffers = 1;
        list->mBuffers[0].mNumberChannels = kAudioServerChannels;
        list->mBuffers[0].mDataByteSize = d->framesPerCycle * kAudioServerChannels * sizeof(Float32);
        list->mBuffers[0].mData = NULL;
        break;
    }
    case kAudioDevicePropertyStreamFormat: {
        AudioStreamBasicDescription *f = out;
        memset(f, 0, sizeof(*f));
        f->mSampleRate = (Float64)plugInNumber(d, kAudioServerDeviceSampleRate,
                                               (double)numberProperty(d->engine, "IOAudioEngineSampleRate", 0));
        f->mFormatID = kAudioFormatLinearPCM;
        f->mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
        f->mChannelsPerFrame = kAudioServerChannels;
        f->mBitsPerChannel = 32;
        f->mBytesPerFrame = kAudioServerChannels * sizeof(Float32);
        f->mFramesPerPacket = 1;
        f->mBytesPerPacket = f->mBytesPerFrame;
        break;
    }
    case kAudioDevicePropertyVolumeScalar:
    case kAudioDevicePropertyVolumeDecibels:
        if (!readScopedLevel(d, a, &l))
            return kAudioHardwareUnspecifiedError;
        {
            UInt32 n = (UInt32)(d - devices);
            SInt32 value = !isInput(a) && mutedByLevel[n] ? levelBeforeMute[n] : l.value;
            *(Float32 *)out = a->mSelector == kAudioDevicePropertyVolumeScalar ?
                scalarOf(&l, value) : decibelsOf(&l, value);
        }
        break;
    case kAudioDevicePropertyVolumeRangeDecibels:
        if (!readScopedLevel(d, a, &l))
            return kAudioHardwareUnspecifiedError;
        ((AudioValueRange *)out)->mMinimum = l.minDB;
        ((AudioValueRange *)out)->mMaximum = l.maxDB;
        break;
    case kAudioDevicePropertyVolumeScalarToDecibels:
        if (!readScopedLevel(d, a, &l))
            return kAudioHardwareUnspecifiedError;
        *(Float32 *)out = decibelsOf(&l, valueOfScalar(&l, *(Float32 *)out));
        break;
    case kAudioDevicePropertyVolumeDecibelsToScalar:
        if (!readScopedLevel(d, a, &l))
            return kAudioHardwareUnspecifiedError;
        *(Float32 *)out = l.maxDB > l.minDB ?
            (Float32)((*(Float32 *)out - l.minDB) / (l.maxDB - l.minDB)) : 0;
        if (*(Float32 *)out < 0) *(Float32 *)out = 0;
        if (*(Float32 *)out > 1) *(Float32 *)out = 1;
        break;
    case kAudioDevicePropertyMute:
        if (d->mute != 0)
            *(UInt32 *)out = numberProperty(d->mute, "IOAudioControlValue", 0) != 0;
        else
            *(UInt32 *)out = mutedByLevel[d - devices];
        break;
    default:
        return kAudioHardwareUnknownPropertyError;
    }
    *ioSize = size;
    return noErr;
}

OSStatus _AudioDeviceSetProperty(AudioObjectID id, const AudioObjectPropertyAddress *a,
                                 UInt32 size, const void *data) {
    Device *d = deviceWithID(id);
    UInt32 n;
    Level l;

    if (d == NULL && _AudioAggregateIs(id))
        return _AudioAggregateSetProperty(id, a, size, data);
    if (d == NULL)
        return kAudioHardwareBadObjectError;
    if (!_AudioDeviceHasProperty(id, a) || !_AudioDeviceIsPropertySettable(id, a))
        return kAudioHardwareUnknownPropertyError;
    if (data == NULL || size < _AudioDevicePropertySize(id, a))
        return kAudioHardwareBadPropertySizeError;
    n = (UInt32)(d - devices);
    switch (a->mSelector) {
    case kAudioDevicePropertyBufferFrameSize: {
        UInt32 frames = *(const UInt32 *)data;
        if (frames < kAudioServerMinFramesPerCycle || frames > 1024)
            return kAudioHardwareIllegalOperationError;
        d->framesPerCycle = frames;     /* for this process's next start */
        return noErr;
    }
    case kAudioDevicePropertyVolumeScalar:
    case kAudioDevicePropertyVolumeDecibels: {
        if (!readScopedLevel(d, a, &l))
            return kAudioHardwareUnspecifiedError;
        Float32 scalar = *(const Float32 *)data;
        if (a->mSelector == kAudioDevicePropertyVolumeDecibels)
            scalar = l.maxDB > l.minDB ? (Float32)((scalar - l.minDB) / (l.maxDB - l.minDB)) : 0;
        SInt32 value = valueOfScalar(&l, scalar);
        if (isInput(a))
            return setControlValue(d->inputVolume, value);
        if (mutedByLevel[n]) {
            levelBeforeMute[n] = value;     /* applied when unmuted */
            return noErr;
        }
        return setControlValue(d->volume, value);
    }
    case kAudioDevicePropertyMute: {
        Boolean mute = *(const UInt32 *)data != 0;
        if (d->mute != 0)
            return setControlValue(d->mute, mute);
        if (!readLevel(d, &l))
            return kAudioHardwareUnspecifiedError;
        if (mute && !mutedByLevel[n]) {
            levelBeforeMute[n] = l.value;
            mutedByLevel[n] = true;
            return setControlValue(d->volume, l.min);
        }
        if (!mute && mutedByLevel[n]) {
            mutedByLevel[n] = false;
            return setControlValue(d->volume, levelBeforeMute[n]);
        }
        return noErr;
    }
    default:
        return kAudioHardwareUnknownPropertyError;
    }
}

/* The device's UID as a C string (for the audio service). */
Boolean _AudioDeviceGetUIDCString(AudioObjectID id, char *buffer, size_t size) {
    Device *d = deviceWithID(id);

    return d != NULL && CFStringGetCString(d->uid, buffer, (CFIndex)size, kCFStringEncodingUTF8);
}
