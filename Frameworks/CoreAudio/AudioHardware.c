/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The system object and the devices it owns (AudioDevices.c): until the
 * audio service (AU2) owns them, the kernel's output engines. Listeners
 * of a device's volume and mute are called when the service tells they
 * changed.
 */

#include "AudioHardware.h"
#include "AudioDevices.h"
#include <Block.h>
#include <dispatch/dispatch.h>
#include <notify.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define kMaxListed 16

static Boolean systemHas(AudioObjectPropertySelector selector) {
    switch (selector) {
    case kAudioHardwarePropertyDevices:
    case kAudioHardwarePropertyDefaultInputDevice:
    case kAudioHardwarePropertyDefaultOutputDevice:
    case kAudioHardwarePropertyDefaultSystemOutputDevice:
    case kAudioObjectPropertyOwnedObjects:
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer:
    case kAudioObjectPropertyClass:
    case kAudioHardwarePropertyTranslateUIDToDevice:
    case kAudioHardwarePropertyRunLoop:
        return true;
    default:
        return false;
    }
}

/* The run loop the app asked notifications on. They come on this
   library's own thread whatever it is (as with NULL, the reference's
   choice for apps that ask it): kept to be read back. */
static CFRunLoopRef notificationRunLoop;

static UInt32 systemSize(AudioObjectPropertySelector selector) {
    switch (selector) {
    case kAudioHardwarePropertyDevices:
    case kAudioObjectPropertyOwnedObjects: {
        AudioDeviceID ids[kMaxListed];
        UInt32 n = _AudioDeviceCopyIDs(ids, kMaxListed);
        return (n < kMaxListed ? n : kMaxListed) * (UInt32)sizeof(AudioDeviceID);
    }
    case kAudioObjectPropertyName:
    case kAudioObjectPropertyManufacturer:
        return sizeof(CFStringRef);
    case kAudioHardwarePropertyRunLoop:
        return sizeof(CFRunLoopRef);
    default:
        return sizeof(AudioObjectID);
    }
}

Boolean AudioObjectHasProperty(AudioObjectID object, const AudioObjectPropertyAddress *address) {
    if (address == NULL)
        return false;
    if (object != kAudioObjectSystemObject)
        return _AudioDeviceHasProperty(object, address);
    return systemHas(address->mSelector);
}

OSStatus AudioObjectIsPropertySettable(AudioObjectID object, const AudioObjectPropertyAddress *address,
                                       Boolean *outIsSettable) {
    if (object != kAudioObjectSystemObject && !_AudioDeviceExists(object))
        return kAudioHardwareBadObjectError;
    if (!AudioObjectHasProperty(object, address))
        return kAudioHardwareUnknownPropertyError;
    if (outIsSettable != NULL)
        *outIsSettable = object != kAudioObjectSystemObject ? _AudioDeviceIsPropertySettable(object, address)
                                                            : address->mSelector == kAudioHardwarePropertyRunLoop;
    return noErr;
}

OSStatus AudioObjectGetPropertyDataSize(AudioObjectID object, const AudioObjectPropertyAddress *address,
    UInt32 qualifierSize, const void *qualifier, UInt32 *outDataSize) {
    if (address == NULL || outDataSize == NULL)
        return kAudioHardwareIllegalOperationError;
    if (object != kAudioObjectSystemObject) {
        if (!_AudioDeviceExists(object))
            return kAudioHardwareBadObjectError;
        if (!_AudioDeviceHasProperty(object, address))
            return kAudioHardwareUnknownPropertyError;
        *outDataSize = _AudioDevicePropertySize(object, address);
        return noErr;
    }
    if (!systemHas(address->mSelector))
        return kAudioHardwareUnknownPropertyError;
    *outDataSize = systemSize(address->mSelector);
    return noErr;
}

OSStatus AudioObjectGetPropertyData(AudioObjectID object, const AudioObjectPropertyAddress *address,
    UInt32 qualifierSize, const void *qualifier, UInt32 *ioDataSize, void *outData) {
    UInt32 size;

    if (object != kAudioObjectSystemObject) {
        if (address == NULL)
            return kAudioHardwareIllegalOperationError;
        return _AudioDeviceGetProperty(object, address, ioDataSize, outData);
    }
    OSStatus status = AudioObjectGetPropertyDataSize(object, address, qualifierSize, qualifier, &size);
    if (status != noErr)
        return status;
    if (ioDataSize == NULL || (*ioDataSize < size && address->mSelector != kAudioHardwarePropertyDevices &&
                               address->mSelector != kAudioObjectPropertyOwnedObjects))
        return kAudioHardwareBadPropertySizeError;
    switch (address->mSelector) {
    case kAudioObjectPropertyName:
        *(CFStringRef *)outData = CFSTR("Minkar Audio");
        break;
    case kAudioObjectPropertyManufacturer:
        *(CFStringRef *)outData = CFSTR("Minkar");
        break;
    case kAudioObjectPropertyClass:
        *(AudioClassID *)outData = kAudioSystemObjectClassID;
        break;
    case kAudioHardwarePropertyDevices:
    case kAudioObjectPropertyOwnedObjects: {
        /* As many as fit. */
        AudioDeviceID ids[kMaxListed];
        UInt32 n = _AudioDeviceCopyIDs(ids, kMaxListed);
        UInt32 fit = *ioDataSize / (UInt32)sizeof(AudioDeviceID);
        if (n > kMaxListed)
            n = kMaxListed;
        if (n > fit)
            n = fit;
        memcpy(outData, ids, n * sizeof(AudioDeviceID));
        size = n * (UInt32)sizeof(AudioDeviceID);
        break;
    }
    case kAudioHardwarePropertyDefaultOutputDevice:
    case kAudioHardwarePropertyDefaultSystemOutputDevice:
        *(AudioObjectID *)outData = _AudioDeviceDefaultOutput();
        break;
    case kAudioHardwarePropertyDefaultInputDevice:
        *(AudioObjectID *)outData = _AudioDeviceDefaultInput();
        break;
    case kAudioHardwarePropertyTranslateUIDToDevice:
        *(AudioObjectID *)outData = qualifierSize >= sizeof(CFStringRef) && qualifier != NULL ?
            _AudioDeviceWithUID(*(const CFStringRef *)qualifier) : kAudioObjectUnknown;
        break;
    case kAudioHardwarePropertyRunLoop:
        *(CFRunLoopRef *)outData = notificationRunLoop;
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
    if (address == NULL)
        return kAudioHardwareIllegalOperationError;
    if (object != kAudioObjectSystemObject)
        return _AudioDeviceSetProperty(object, address, dataSize, data);
    if (!systemHas(address->mSelector))
        return kAudioHardwareUnknownPropertyError;
    if (address->mSelector == kAudioHardwarePropertyRunLoop) {
        if (dataSize < sizeof(CFRunLoopRef) || data == NULL)
            return kAudioHardwareBadPropertySizeError;
        notificationRunLoop = *(const CFRunLoopRef *)data;
        return noErr;
    }
    /* The default output is the first device, the only one for now. */
    if ((address->mSelector == kAudioHardwarePropertyDefaultOutputDevice ||
         address->mSelector == kAudioHardwarePropertyDefaultSystemOutputDevice) &&
        dataSize >= sizeof(AudioObjectID) && data != NULL &&
        *(const AudioObjectID *)data == _AudioDeviceDefaultOutput())
        return noErr;
    return kAudioHardwareIllegalOperationError;
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

static void watchChanges(void);

static OSStatus addListener(Listener entry) {
    if (entry.object != kAudioObjectSystemObject && !_AudioDeviceExists(entry.object))
        return kAudioHardwareBadObjectError;
    watchChanges();
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

/* The audio service tells when a device's volume or mute changed (by any
   app or by the menu bar): the listeners of those properties are called,
   each with the address it listens to. */
#define kVolumeChangedNotification "org.minkar.audio.volume"

static Boolean listensTo(const Listener *l, AudioObjectPropertySelector selector) {
    return l->address.mSelector == selector || l->address.mSelector == kAudioObjectPropertySelectorWildcard;
}

static void volumeChanged(void) {
    static const AudioObjectPropertySelector changed[] = {
        kAudioDevicePropertyVolumeScalar, kAudioDevicePropertyVolumeDecibels, kAudioDevicePropertyMute
    };

    pthread_mutex_lock(&lock);
    for (size_t i = 0; i < listenerCount; i++) {
        Listener l = listeners[i];
        if (l.object == kAudioObjectSystemObject)
            continue;
        for (size_t s = 0; s < sizeof(changed) / sizeof(changed[0]); s++) {
            if (!listensTo(&l, changed[s]))
                continue;
            AudioObjectPropertyAddress address = l.address;
            if (address.mSelector == kAudioObjectPropertySelectorWildcard)
                address.mSelector = changed[s];
            if (l.block != NULL) {
                AudioObjectPropertyListenerBlock block = (AudioObjectPropertyListenerBlock)_Block_copy(l.block);
                dispatch_queue_t queue = l.queue != NULL ? (dispatch_queue_t)l.queue
                                                         : dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
                dispatch_async(queue, ^{
                    block(1, &address);
                    _Block_release(block);
                });
            } else if (l.proc != NULL) {
                AudioObjectPropertyListenerProc proc = l.proc;
                void *clientData = l.clientData;
                AudioObjectID object = l.object;
                dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                    proc(object, 1, &address, clientData);
                });
            }
            if (l.address.mSelector != kAudioObjectPropertySelectorWildcard)
                break;
        }
    }
    pthread_mutex_unlock(&lock);
}

static void watchChanges(void) {
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        int token;
        notify_register_dispatch(kVolumeChangedNotification, &token,
                                 dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                                 ^(int t) { volumeChanged(); });
    });
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

/* Ducking comes with the audio service's mixer. */
OSStatus AudioDeviceDuck(AudioDeviceID device, Float32 level, const AudioTimeStamp *startTime, Float32 rampDuration) {
    return _AudioDeviceExists(device) ? kAudioHardwareUnsupportedOperationError : kAudioHardwareBadDeviceError;
}
