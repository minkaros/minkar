/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The system object. Until the audio service (AU2) owns devices there are
 * none: the device list is empty and the default devices are unknown.
 * Listeners are kept; nothing changes yet to call them.
 */

#include "AudioHardware.h"
#include <Block.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static Boolean systemHas(AudioObjectPropertySelector selector) {
    switch (selector) {
    case kAudioHardwarePropertyDevices:
    case kAudioHardwarePropertyDefaultInputDevice:
    case kAudioHardwarePropertyDefaultOutputDevice:
    case kAudioHardwarePropertyDefaultSystemOutputDevice:
    case kAudioObjectPropertyOwnedObjects:
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer:
    case kAudioHardwarePropertyTranslateUIDToDevice:
        return true;
    default:
        return false;
    }
}

static UInt32 systemSize(AudioObjectPropertySelector selector) {
    switch (selector) {
    case kAudioHardwarePropertyDevices:
    case kAudioObjectPropertyOwnedObjects:
        return 0;
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer:
        return sizeof(CFStringRef);
    default:
        return sizeof(AudioObjectID);
    }
}

Boolean AudioObjectHasProperty(AudioObjectID object, const AudioObjectPropertyAddress *address) {
    return object == kAudioObjectSystemObject && address != NULL && systemHas(address->mSelector);
}

OSStatus AudioObjectIsPropertySettable(AudioObjectID object, const AudioObjectPropertyAddress *address,
                                       Boolean *outIsSettable) {
    if (!AudioObjectHasProperty(object, address))
        return object == kAudioObjectSystemObject ? kAudioHardwareUnknownPropertyError : kAudioHardwareBadObjectError;
    if (outIsSettable != NULL)
        *outIsSettable = false;
    return noErr;
}

OSStatus AudioObjectGetPropertyDataSize(AudioObjectID object, const AudioObjectPropertyAddress *address,
    UInt32 qualifierSize, const void *qualifier, UInt32 *outDataSize) {
    if (object != kAudioObjectSystemObject)
        return kAudioHardwareBadObjectError;
    if (address == NULL || outDataSize == NULL)
        return kAudioHardwareIllegalOperationError;
    if (!systemHas(address->mSelector))
        return kAudioHardwareUnknownPropertyError;
    *outDataSize = systemSize(address->mSelector);
    return noErr;
}

OSStatus AudioObjectGetPropertyData(AudioObjectID object, const AudioObjectPropertyAddress *address,
    UInt32 qualifierSize, const void *qualifier, UInt32 *ioDataSize, void *outData) {
    UInt32 size;
    OSStatus status = AudioObjectGetPropertyDataSize(object, address, qualifierSize, qualifier, &size);
    if (status != noErr)
        return status;
    if (ioDataSize == NULL || *ioDataSize < size)
        return kAudioHardwareBadPropertySizeError;
    switch (address->mSelector) {
    case kAudioObjectPropertyName:
        *(CFStringRef *)outData = CFSTR("Minkar Audio");
        break;
    case kAudioObjectPropertyManufacturer:
        *(CFStringRef *)outData = CFSTR("Minkar");
        break;
    case kAudioHardwarePropertyDevices:
    case kAudioObjectPropertyOwnedObjects:
        break;
    default:
        *(AudioObjectID *)outData = kAudioObjectUnknown;
        break;
    }
    *ioDataSize = size;
    return noErr;
}

OSStatus AudioObjectSetPropertyData(AudioObjectID object, const AudioObjectPropertyAddress *address,
    UInt32 qualifierSize, const void *qualifier, UInt32 dataSize, const void *data) {
    if (object != kAudioObjectSystemObject)
        return kAudioHardwareBadObjectError;
    if (address == NULL || !systemHas(address->mSelector))
        return kAudioHardwareUnknownPropertyError;
    /* The defaults can only be set to a device, and there is none. */
    return kAudioHardwareBadDeviceError;
}

/* Listeners: kept so they can be removed, and called when the service
   reports changes. */
typedef struct Listener {
    AudioObjectID object;
    AudioObjectPropertyAddress address;
    AudioObjectPropertyListenerProc proc;
    void *clientData;
    void *block;
    void *queue;
} Listener;

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static Listener *listeners;
static size_t listenerCount;

static OSStatus addListener(Listener entry) {
    if (entry.object != kAudioObjectSystemObject)
        return kAudioHardwareBadObjectError;
    pthread_mutex_lock(&lock);
    Listener *grown = realloc(listeners, (listenerCount + 1) * sizeof(Listener));
    if (grown != NULL) {
        listeners = grown;
        listeners[listenerCount++] = entry;
    }
    pthread_mutex_unlock(&lock);
    return grown != NULL ? noErr : kAudioHardwareUnspecifiedError;
}

static OSStatus removeListener(AudioObjectID object, const AudioObjectPropertyAddress *address,
                               AudioObjectPropertyListenerProc proc, void *clientData, void *block) {
    OSStatus status = kAudioHardwareIllegalOperationError;
    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < listenerCount; i++) {
        Listener *l = &listeners[i];
        if (l->object == object && memcmp(&l->address, address, sizeof(*address)) == 0 &&
            l->proc == proc && l->clientData == clientData && l->block == block) {
            if (l->block != NULL)
                _Block_release(l->block);
            listeners[i] = listeners[--listenerCount];
            status = noErr;
            break;
        }
    }
    pthread_mutex_unlock(&lock);
    return status;
}

OSStatus AudioObjectAddPropertyListener(AudioObjectID object, const AudioObjectPropertyAddress *address,
    AudioObjectPropertyListenerProc proc, void *clientData) {
    if (address == NULL || proc == NULL)
        return kAudioHardwareIllegalOperationError;
    return addListener((Listener){ object, *address, proc, clientData, NULL, NULL });
}

OSStatus AudioObjectRemovePropertyListener(AudioObjectID object, const AudioObjectPropertyAddress *address,
    AudioObjectPropertyListenerProc proc, void *clientData) {
    if (address == NULL)
        return kAudioHardwareIllegalOperationError;
    return removeListener(object, address, proc, clientData, NULL);
}

OSStatus AudioObjectAddPropertyListenerBlock(AudioObjectID object, const AudioObjectPropertyAddress *address,
    void *queue, AudioObjectPropertyListenerBlock block) {
    if (address == NULL || block == NULL)
        return kAudioHardwareIllegalOperationError;
    return addListener((Listener){ object, *address, NULL, NULL, _Block_copy(block), queue });
}

OSStatus AudioObjectRemovePropertyListenerBlock(AudioObjectID object, const AudioObjectPropertyAddress *address,
    void *queue, AudioObjectPropertyListenerBlock block) {
    if (address == NULL)
        return kAudioHardwareIllegalOperationError;
    return removeListener(object, address, NULL, NULL, block);
}

/* No device: nothing to duck. */
OSStatus AudioDeviceDuck(AudioDeviceID device, Float32 level, const AudioTimeStamp *startTime, Float32 rampDuration) {
    return kAudioHardwareBadDeviceError;
}
