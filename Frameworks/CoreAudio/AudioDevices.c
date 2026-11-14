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
#include <IOKit/IOKitLib.h>
#include <math.h>
#include <pthread.h>
#include <string.h>

#define kMaxDevices 16
#define kFirstDeviceID 0x100

typedef struct Device {
    AudioDeviceID   id;
    io_service_t    engine;         /* retained */
    io_service_t    volume;         /* IOAudioLevelControl, or 0 */
    io_service_t    mute;           /* IOAudioToggleControl, or 0 */
    CFStringRef     uid;
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

static SInt64 numberProperty(io_registry_entry_t entry, const char *key, SInt64 fallback) {
    CFTypeRef value = copyProperty(entry, key);
    SInt64 n = fallback;
    if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID())
        CFNumberGetValue(value, kCFNumberSInt64Type, &n);
    if (value != NULL)
        CFRelease(value);
    return n;
}

/* The engine's control of a type and subtype for output, or 0. */
static io_service_t findControl(io_service_t engine, UInt32 type, UInt32 subType) {
    io_iterator_t children;
    io_registry_entry_t child;
    io_service_t found = 0;

    if (IORegistryEntryGetChildIterator(engine, kIOServicePlane, &children) != KERN_SUCCESS)
        return 0;
    while (found == 0 && (child = IOIteratorNext(children)) != 0) {
        if ((UInt32)numberProperty(child, "IOAudioControlType", 0) == type &&
            (UInt32)numberProperty(child, "IOAudioControlSubType", 0) == subType &&
            (UInt32)numberProperty(child, "IOAudioControlUsage", 'outp') == 'outp')
            found = child;
        else
            IOObjectRelease(child);
    }
    IOObjectRelease(children);
    return found;
}

/* An engine with an output stream. */
static Boolean hasOutput(io_service_t engine) {
    io_iterator_t children;
    io_registry_entry_t child;
    Boolean output = false;

    if (IORegistryEntryGetChildIterator(engine, kIOServicePlane, &children) != KERN_SUCCESS)
        return false;
    while (!output && (child = IOIteratorNext(children)) != 0) {
        if (IOObjectConformsTo(child, "IOAudioStream") &&
            numberProperty(child, "IOAudioStreamDirection", -1) == 0)
            output = true;
        IOObjectRelease(child);
    }
    IOObjectRelease(children);
    return output;
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

        if (deviceCount == kMaxDevices || !hasOutput(engine)) {
            IOObjectRelease(engine);
            continue;
        }
        Device *d = &devices[deviceCount];
        d->id = kFirstDeviceID + deviceCount;
        d->engine = engine;
        d->volume = findControl(engine, 'levl', 'vlme');
        d->mute = findControl(engine, 'togl', 'mute');
        if (IORegistryEntryGetPath(engine, kIOServicePlane, path) == KERN_SUCCESS)
            d->uid = CFStringCreateWithCString(NULL, path, kCFStringEncodingUTF8);
        else
            d->uid = CFStringCreateWithFormat(NULL, NULL, CFSTR("MinkarAudioDevice:%u"), (unsigned)deviceCount);
        deviceCount++;
    }
    IOObjectRelease(engines);
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
    return n;
}

AudioDeviceID _AudioDeviceDefaultOutput(void) {
    AudioDeviceID id = kAudioObjectUnknown;

    pthread_mutex_lock(&devicesLock);
    findDevices();
    if (deviceCount > 0)
        id = devices[0].id;
    pthread_mutex_unlock(&devicesLock);
    return id;
}

AudioDeviceID _AudioDeviceWithUID(CFStringRef uid) {
    AudioDeviceID id = kAudioObjectUnknown;

    if (uid == NULL)
        return id;
    pthread_mutex_lock(&devicesLock);
    findDevices();
    for (UInt32 i = 0; i < deviceCount; i++)
        if (CFEqual(devices[i].uid, uid))
            id = devices[i].id;
    pthread_mutex_unlock(&devicesLock);
    return id;
}

Boolean _AudioDeviceExists(AudioObjectID id) {
    return deviceWithID(id) != NULL;
}

/* ---- Volume ------------------------------------------------------------ */

typedef struct Level {
    SInt32 value, min, max;
    Float64 minDB, maxDB;
} Level;

static Boolean readLevel(Device *d, Level *l) {
    if (d->volume == 0)
        return false;
    l->value = (SInt32)numberProperty(d->volume, "IOAudioControlValue", 0);
    l->min = (SInt32)numberProperty(d->volume, "IOAudioLevelControlMinValue", 0);
    l->max = (SInt32)numberProperty(d->volume, "IOAudioLevelControlMaxValue", 0);
    l->minDB = (SInt32)numberProperty(d->volume, "IOAudioLevelControlMinDB", 0) / 65536.0;
    l->maxDB = (SInt32)numberProperty(d->volume, "IOAudioLevelControlMaxDB", 0) / 65536.0;
    return l->max > l->min;
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

    if (d == NULL || a == NULL)
        return false;
    switch (a->mSelector) {
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
        return true;
    case kAudioDevicePropertyVolumeScalar:
    case kAudioDevicePropertyVolumeDecibels:
    case kAudioDevicePropertyVolumeRangeDecibels:
    case kAudioDevicePropertyVolumeScalarToDecibels:
    case kAudioDevicePropertyVolumeDecibelsToScalar:
        return d->volume != 0 && isOutputOrGlobal(a) && isVolumeElement(a);
    case kAudioDevicePropertyMute:
        return (d->mute != 0 || d->volume != 0) && isOutputOrGlobal(a) && isVolumeElement(a);
    default:
        return false;
    }
}

Boolean _AudioDeviceIsPropertySettable(AudioObjectID id, const AudioObjectPropertyAddress *a) {
    switch (a->mSelector) {
    case kAudioDevicePropertyVolumeScalar:
    case kAudioDevicePropertyVolumeDecibels:
    case kAudioDevicePropertyMute:
        return true;
    default:
        return false;
    }
}

UInt32 _AudioDevicePropertySize(AudioObjectID id, const AudioObjectPropertyAddress *a) {
    switch (a->mSelector) {
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
        return sizeof(AudioValueRange);
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

    if (d == NULL)
        return kAudioHardwareBadObjectError;
    if (!_AudioDeviceHasProperty(id, a))
        return kAudioHardwareUnknownPropertyError;
    size = _AudioDevicePropertySize(id, a);
    if (ioSize == NULL || *ioSize < size || out == NULL)
        return kAudioHardwareBadPropertySizeError;
    io_registry_entry_t parent = 0;
    switch (a->mSelector) {
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer: {
        CFTypeRef value = NULL;
        if (IORegistryEntryGetParentEntry(d->engine, kIOServicePlane, &parent) == KERN_SUCCESS) {
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
        UInt32 transport = kAudioDeviceTransportTypeUnknown;
        if (IORegistryEntryGetParentEntry(d->engine, kIOServicePlane, &parent) == KERN_SUCCESS) {
            transport = (UInt32)numberProperty(parent, "IOAudioDeviceTransportType", 0);
            IOObjectRelease(parent);
        }
        *(UInt32 *)out = transport;
        break;
    }
    case kAudioDevicePropertyNominalSampleRate:
        *(Float64 *)out = (Float64)numberProperty(d->engine, "IOAudioEngineSampleRate", 0);
        break;
    case kAudioDevicePropertyDeviceIsAlive:
        *(UInt32 *)out = 1;
        break;
    case kAudioDevicePropertyDeviceIsRunning:
        *(UInt32 *)out = numberProperty(d->engine, "IOAudioEngineState", 0) == 1;
        break;
    case kAudioDevicePropertyBufferFrameSize:
        *(UInt32 *)out = (UInt32)numberProperty(d->engine, "IOAudioEngineNumSampleFramesPerBuffer", 0) / 2;
        break;
    case kAudioDevicePropertyVolumeScalar:
    case kAudioDevicePropertyVolumeDecibels:
        if (!readLevel(d, &l))
            return kAudioHardwareUnspecifiedError;
        {
            UInt32 n = (UInt32)(d - devices);
            SInt32 value = mutedByLevel[n] ? levelBeforeMute[n] : l.value;
            *(Float32 *)out = a->mSelector == kAudioDevicePropertyVolumeScalar ?
                scalarOf(&l, value) : decibelsOf(&l, value);
        }
        break;
    case kAudioDevicePropertyVolumeRangeDecibels:
        if (!readLevel(d, &l))
            return kAudioHardwareUnspecifiedError;
        ((AudioValueRange *)out)->mMinimum = l.minDB;
        ((AudioValueRange *)out)->mMaximum = l.maxDB;
        break;
    case kAudioDevicePropertyVolumeScalarToDecibels:
        if (!readLevel(d, &l))
            return kAudioHardwareUnspecifiedError;
        *(Float32 *)out = decibelsOf(&l, valueOfScalar(&l, *(Float32 *)out));
        break;
    case kAudioDevicePropertyVolumeDecibelsToScalar:
        if (!readLevel(d, &l))
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

    if (d == NULL)
        return kAudioHardwareBadObjectError;
    if (!_AudioDeviceHasProperty(id, a) || !_AudioDeviceIsPropertySettable(id, a))
        return kAudioHardwareUnknownPropertyError;
    if (data == NULL || size < _AudioDevicePropertySize(id, a))
        return kAudioHardwareBadPropertySizeError;
    n = (UInt32)(d - devices);
    switch (a->mSelector) {
    case kAudioDevicePropertyVolumeScalar:
    case kAudioDevicePropertyVolumeDecibels: {
        if (!readLevel(d, &l))
            return kAudioHardwareUnspecifiedError;
        Float32 scalar = *(const Float32 *)data;
        if (a->mSelector == kAudioDevicePropertyVolumeDecibels)
            scalar = l.maxDB > l.minDB ? (Float32)((scalar - l.minDB) / (l.maxDB - l.minDB)) : 0;
        SInt32 value = valueOfScalar(&l, scalar);
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
