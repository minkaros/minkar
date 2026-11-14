/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio components: the units AudioToolbox has, found by description, and
 * the AudioUnit calls, checked and passed to each unit's class. A
 * component registry that loads outside bundles comes later (AU9).
 */

#include "AUInternal.h"
#include <AudioToolbox/AudioToolbox.h>
#include <stdlib.h>
#include <string.h>

static const struct OpaqueAudioComponent components[] = {
    { { kAudioUnitType_Output, kAudioUnitSubType_DefaultOutput, kAudioUnitManufacturer_Apple, 0, 0 },
      "Minkar: DefaultOutputUnit", 0x10000, &_AUOutputClass },
    { { kAudioUnitType_Output, kAudioUnitSubType_HALOutput, kAudioUnitManufacturer_Apple, 0, 0 },
      "Minkar: AUHAL", 0x10000, &_AUOutputClass },
    { { kAudioUnitType_Output, kAudioUnitSubType_SystemOutput, kAudioUnitManufacturer_Apple, 0, 0 },
      "Minkar: SystemOutputUnit", 0x10000, &_AUOutputClass },
};

#define kComponentCount (sizeof(components) / sizeof(components[0]))

/* A field of the description matches when the asked one is 0 (any). */
static Boolean matches(const AudioComponentDescription *have, const AudioComponentDescription *want) {
    if (want == NULL)
        return true;
    return (want->componentType == 0 || want->componentType == have->componentType) &&
           (want->componentSubType == 0 || want->componentSubType == have->componentSubType) &&
           (want->componentManufacturer == 0 || want->componentManufacturer == have->componentManufacturer);
}

AudioComponent AudioComponentFindNext(AudioComponent after, const AudioComponentDescription *desc) {
    size_t i = 0;

    if (after != NULL) {
        for (; i < kComponentCount; i++)
            if (&components[i] == after) {
                i++;
                break;
            }
    }
    for (; i < kComponentCount; i++)
        if (matches(&components[i].description, desc))
            return (AudioComponent)&components[i];
    return NULL;
}

UInt32 AudioComponentCount(const AudioComponentDescription *desc) {
    UInt32 n = 0;

    for (size_t i = 0; i < kComponentCount; i++)
        if (matches(&components[i].description, desc))
            n++;
    return n;
}

static Boolean isComponent(AudioComponent c) {
    for (size_t i = 0; i < kComponentCount; i++)
        if (&components[i] == c)
            return true;
    return false;
}

OSStatus AudioComponentCopyName(AudioComponent c, CFStringRef *outName) {
    if (!isComponent(c) || outName == NULL)
        return kAudio_ParamError;
    *outName = CFStringCreateWithCString(NULL, c->name, kCFStringEncodingUTF8);
    return noErr;
}

OSStatus AudioComponentGetDescription(AudioComponent c, AudioComponentDescription *outDesc) {
    if (!isComponent(c) || outDesc == NULL)
        return kAudio_ParamError;
    *outDesc = c->description;
    return noErr;
}

OSStatus AudioComponentGetVersion(AudioComponent c, UInt32 *outVersion) {
    if (!isComponent(c) || outVersion == NULL)
        return kAudio_ParamError;
    *outVersion = c->version;
    return noErr;
}

OSStatus AudioComponentInstanceNew(AudioComponent c, AudioComponentInstance *out) {
    AudioUnit unit;
    OSStatus status;

    if (!isComponent(c) || out == NULL)
        return kAudio_ParamError;
    unit = calloc(1, sizeof(*unit));
    if (unit == NULL)
        return kAudio_MemFullError;
    unit->magic = kAUInstanceMagic;
    unit->component = c;
    unit->cls = c->cls;
    pthread_mutex_init(&unit->lock, NULL);
    status = unit->cls->create(unit);
    if (status != noErr) {
        pthread_mutex_destroy(&unit->lock);
        free(unit);
        return status;
    }
    *out = unit;
    return noErr;
}

static Boolean alive(AudioUnit unit) {
    return unit != NULL && unit->magic == kAUInstanceMagic;
}

OSStatus AudioComponentInstanceDispose(AudioComponentInstance unit) {
    if (!alive(unit))
        return kAudio_ParamError;
    if (unit->cls->stop != NULL)
        unit->cls->stop(unit);
    unit->cls->uninitialize(unit);
    unit->cls->dispose(unit);
    unit->magic = 0;
    pthread_mutex_destroy(&unit->lock);
    free(unit);
    return noErr;
}

AudioComponent AudioComponentInstanceGetComponent(AudioComponentInstance unit) {
    return alive(unit) ? (AudioComponent)unit->component : NULL;
}

/* ---- Units ------------------------------------------------------------- */

OSStatus AudioUnitInitialize(AudioUnit unit) {
    return alive(unit) ? unit->cls->initialize(unit) : kAudio_ParamError;
}

OSStatus AudioUnitUninitialize(AudioUnit unit) {
    return alive(unit) ? unit->cls->uninitialize(unit) : kAudio_ParamError;
}

OSStatus AudioUnitGetPropertyInfo(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                  AudioUnitElement element, UInt32 *outSize, Boolean *outWritable) {
    UInt32 size = 0;
    Boolean writable = false;
    OSStatus status;

    if (!alive(unit))
        return kAudio_ParamError;
    status = unit->cls->getPropertyInfo(unit, id, scope, element, &size, &writable);
    if (status == noErr) {
        if (outSize != NULL)
            *outSize = size;
        if (outWritable != NULL)
            *outWritable = writable;
    }
    return status;
}

OSStatus AudioUnitGetProperty(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                              AudioUnitElement element, void *data, UInt32 *ioSize) {
    if (!alive(unit) || data == NULL || ioSize == NULL)
        return kAudio_ParamError;
    return unit->cls->getProperty(unit, id, scope, element, data, ioSize);
}

OSStatus AudioUnitSetProperty(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                              AudioUnitElement element, const void *data, UInt32 size) {
    OSStatus status;

    if (!alive(unit))
        return kAudio_ParamError;
    status = unit->cls->setProperty(unit, id, scope, element, data, size);
    if (status == noErr)
        _AUPropertyChanged(unit, id, scope, element);
    return status;
}

OSStatus AudioUnitAddPropertyListener(AudioUnit unit, AudioUnitPropertyID id,
                                      AudioUnitPropertyListenerProc proc, void *refCon) {
    OSStatus status = kAudio_MemFullError;

    if (!alive(unit) || proc == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&unit->lock);
    if (unit->listenerCount < (int)(sizeof(unit->listeners) / sizeof(unit->listeners[0]))) {
        unit->listeners[unit->listenerCount].id = id;
        unit->listeners[unit->listenerCount].proc = proc;
        unit->listeners[unit->listenerCount].refCon = refCon;
        unit->listenerCount++;
        status = noErr;
    }
    pthread_mutex_unlock(&unit->lock);
    return status;
}

OSStatus AudioUnitRemovePropertyListenerWithUserData(AudioUnit unit, AudioUnitPropertyID id,
                                                     AudioUnitPropertyListenerProc proc, void *refCon) {
    if (!alive(unit))
        return kAudio_ParamError;
    pthread_mutex_lock(&unit->lock);
    for (int i = 0; i < unit->listenerCount; i++)
        if (unit->listeners[i].id == id && unit->listeners[i].proc == proc &&
            unit->listeners[i].refCon == refCon) {
            unit->listeners[i] = unit->listeners[--unit->listenerCount];
            break;
        }
    pthread_mutex_unlock(&unit->lock);
    return noErr;
}

void _AUPropertyChanged(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element) {
    AudioUnitPropertyListenerProc procs[16];
    void *refCons[16];
    int n = 0;

    pthread_mutex_lock(&unit->lock);
    for (int i = 0; i < unit->listenerCount; i++)
        if (unit->listeners[i].id == id) {
            procs[n] = unit->listeners[i].proc;
            refCons[n++] = unit->listeners[i].refCon;
        }
    pthread_mutex_unlock(&unit->lock);
    for (int i = 0; i < n; i++)
        procs[i](refCons[i], unit, id, scope, element);
}

OSStatus AudioUnitAddRenderNotify(AudioUnit unit, AURenderCallback proc, void *refCon) {
    OSStatus status = kAudio_MemFullError;

    if (!alive(unit) || proc == NULL)
        return kAudio_ParamError;
    pthread_mutex_lock(&unit->lock);
    if (unit->notifyCount < (int)(sizeof(unit->notifies) / sizeof(unit->notifies[0]))) {
        unit->notifies[unit->notifyCount].proc = proc;
        unit->notifies[unit->notifyCount].refCon = refCon;
        unit->notifyCount++;
        status = noErr;
    }
    pthread_mutex_unlock(&unit->lock);
    return status;
}

OSStatus AudioUnitRemoveRenderNotify(AudioUnit unit, AURenderCallback proc, void *refCon) {
    if (!alive(unit))
        return kAudio_ParamError;
    pthread_mutex_lock(&unit->lock);
    for (int i = 0; i < unit->notifyCount; i++)
        if (unit->notifies[i].proc == proc && unit->notifies[i].refCon == refCon) {
            unit->notifies[i] = unit->notifies[--unit->notifyCount];
            break;
        }
    pthread_mutex_unlock(&unit->lock);
    return noErr;
}

/* On the render thread: no lock (the notifications are set before the
   unit runs). */
void _AURenderNotify(AudioUnit unit, AudioUnitRenderActionFlags flags, const AudioTimeStamp *time,
                     UInt32 bus, UInt32 frames, AudioBufferList *data) {
    for (int i = 0; i < unit->notifyCount; i++) {
        AudioUnitRenderActionFlags f = flags;
        unit->notifies[i].proc(unit->notifies[i].refCon, &f, time, bus, frames, data);
    }
}

OSStatus AudioUnitGetParameter(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                               AudioUnitElement element, AudioUnitParameterValue *value) {
    if (!alive(unit) || value == NULL)
        return kAudio_ParamError;
    if (unit->cls->getParameter == NULL)
        return kAudioUnitErr_InvalidParameter;
    return unit->cls->getParameter(unit, id, scope, element, value);
}

OSStatus AudioUnitSetParameter(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                               AudioUnitElement element, AudioUnitParameterValue value, UInt32 offset) {
    if (!alive(unit))
        return kAudio_ParamError;
    if (unit->cls->setParameter == NULL)
        return kAudioUnitErr_InvalidParameter;
    return unit->cls->setParameter(unit, id, scope, element, value);
}

OSStatus AudioUnitRender(AudioUnit unit, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                         UInt32 bus, UInt32 frames, AudioBufferList *data) {
    if (!alive(unit) || time == NULL || data == NULL)
        return kAudio_ParamError;
    if (unit->cls->render == NULL)
        return kAudioUnitErr_NoConnection;
    return unit->cls->render(unit, flags, time, bus, frames, data);
}

OSStatus AudioUnitReset(AudioUnit unit, AudioUnitScope scope, AudioUnitElement element) {
    if (!alive(unit))
        return kAudio_ParamError;
    return unit->cls->reset != NULL ? unit->cls->reset(unit, scope, element) : noErr;
}

OSStatus AudioOutputUnitStart(AudioUnit unit) {
    if (!alive(unit))
        return kAudio_ParamError;
    return unit->cls->start != NULL ? unit->cls->start(unit) : kAudioUnitErr_InvalidProperty;
}

OSStatus AudioOutputUnitStop(AudioUnit unit) {
    if (!alive(unit))
        return kAudio_ParamError;
    return unit->cls->stop != NULL ? unit->cls->stop(unit) : kAudioUnitErr_InvalidProperty;
}
