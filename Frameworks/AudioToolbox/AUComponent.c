/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio components: the units AudioToolbox has and those the app
 * registers (a factory and its plug-in interface), found by description,
 * and the AudioUnit calls, checked and passed to each unit's class. A
 * registry that loads outside bundles comes later (AU9).
 */

#include "AUInternal.h"
#include <AudioToolbox/AudioToolbox.h>
#include <Block.h>
#include <dispatch/dispatch.h>
#include <stdio.h>
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

#define kBuiltIn (sizeof(components) / sizeof(components[0]))

/* Components the app registered, after the built-in ones. */
static pthread_mutex_t registryLock = PTHREAD_MUTEX_INITIALIZER;
static struct OpaqueAudioComponent **registered;
static size_t registeredCount;

static size_t componentCount(void) {
    size_t n;
    pthread_mutex_lock(&registryLock);
    n = kBuiltIn + registeredCount;
    pthread_mutex_unlock(&registryLock);
    return n;
}

static const struct OpaqueAudioComponent *componentAt(size_t i) {
    const struct OpaqueAudioComponent *c = NULL;
    if (i < kBuiltIn)
        return &components[i];
    pthread_mutex_lock(&registryLock);
    if (i - kBuiltIn < registeredCount)
        c = registered[i - kBuiltIn];
    pthread_mutex_unlock(&registryLock);
    return c;
}

/* A field of the description matches when the asked one is 0 (any). */
static Boolean matches(const AudioComponentDescription *have, const AudioComponentDescription *want) {
    if (want == NULL)
        return true;
    return (want->componentType == 0 || want->componentType == have->componentType) &&
           (want->componentSubType == 0 || want->componentSubType == have->componentSubType) &&
           (want->componentManufacturer == 0 || want->componentManufacturer == have->componentManufacturer);
}

/* Unsearchable components are found only when asked for exactly. */
static Boolean findable(const struct OpaqueAudioComponent *c, const AudioComponentDescription *desc) {
    if (!matches(&c->description, desc))
        return false;
    if (!(c->description.componentFlags & kAudioComponentFlag_Unsearchable))
        return true;
    return desc != NULL && desc->componentType != 0 && desc->componentSubType != 0 &&
           desc->componentManufacturer != 0;
}

AudioComponent AudioComponentFindNext(AudioComponent after, const AudioComponentDescription *desc) {
    size_t i = 0, count = componentCount();

    if (after != NULL) {
        for (; i < count; i++)
            if (componentAt(i) == after) {
                i++;
                break;
            }
    }
    for (; i < count; i++) {
        const struct OpaqueAudioComponent *c = componentAt(i);
        if (c != NULL && findable(c, desc))
            return (AudioComponent)c;
    }
    return NULL;
}

UInt32 AudioComponentCount(const AudioComponentDescription *desc) {
    UInt32 n = 0;
    size_t count = componentCount();

    for (size_t i = 0; i < count; i++) {
        const struct OpaqueAudioComponent *c = componentAt(i);
        if (c != NULL && findable(c, desc))
            n++;
    }
    return n;
}

static Boolean isComponent(AudioComponent c) {
    size_t count = componentCount();

    for (size_t i = 0; i < count; i++)
        if (componentAt(i) == c)
            return true;
    return false;
}

AudioComponent AudioComponentRegister(const AudioComponentDescription *desc, CFStringRef name, UInt32 version,
                                      AudioComponentFactoryFunction factory) {
    struct OpaqueAudioComponent *c, **grown;
    char buf[256] = "";

    if (desc == NULL || factory == NULL)
        return NULL;
    c = calloc(1, sizeof(*c));
    if (c == NULL)
        return NULL;
    if (name != NULL)
        CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8);
    c->description = *desc;
    c->name = strdup(buf);
    c->version = version;
    c->cls = &_AUPlugInClass;
    c->factory = factory;
    pthread_mutex_lock(&registryLock);
    grown = realloc(registered, (registeredCount + 1) * sizeof(*registered));
    if (grown == NULL) {
        pthread_mutex_unlock(&registryLock);
        free((void *)c->name);
        free(c);
        return NULL;
    }
    registered = grown;
    registered[registeredCount++] = c;
    pthread_mutex_unlock(&registryLock);
    return c;
}

OSStatus AudioComponentCopyConfigurationInfo(AudioComponent c, CFDictionaryRef *out) {
    CFMutableDictionaryRef info;
    SInt32 result = kAudioComponentValidationResult_Passed;
    CFNumberRef number;

    if (!isComponent(c) || out == NULL)
        return kAudio_ParamError;
    info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    number = CFNumberCreate(NULL, kCFNumberSInt32Type, &result);
    CFDictionarySetValue(info, CFSTR(kAudioComponentConfigurationInfo_ValidationResult), number);
    CFRelease(number);
    *out = info;
    return noErr;
}

/* A component validates when an instance of it opens and initializes. */
OSStatus AudioComponentValidate(AudioComponent c, CFDictionaryRef parameters,
                                AudioComponentValidationResult *outResult) {
    AudioComponentInstance unit = NULL;
    OSStatus status;

    if (!isComponent(c) || outResult == NULL)
        return kAudio_ParamError;
    status = AudioComponentInstanceNew(c, &unit);
    if (status != noErr) {
        *outResult = kAudioComponentValidationResult_UnauthorizedError_Open;
        return noErr;
    }
    status = AudioUnitInitialize(unit);
    AudioComponentInstanceDispose(unit);
    *outResult = status == noErr ? kAudioComponentValidationResult_Passed
                                 : kAudioComponentValidationResult_UnauthorizedError_Init;
    return noErr;
}

/* Instances are made in this process; the handler is called on another
   thread, as the reference platform's may be. */
void AudioComponentInstantiate(AudioComponent c, AudioComponentInstantiationOptions options,
                               void (^handler)(AudioComponentInstance, OSStatus)) {
    void (^copy)(AudioComponentInstance, OSStatus);

    if (handler == NULL)
        return;
    copy = Block_copy(handler);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        AudioComponentInstance unit = NULL;
        OSStatus status = AudioComponentInstanceNew(c, &unit);
        copy(status == noErr ? unit : NULL, status);
        Block_release(copy);
    });
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

static Boolean isPlugIn(AudioUnit unit) {
    return unit->cls == &_AUPlugInClass;
}

OSStatus AudioUnitAddPropertyListener(AudioUnit unit, AudioUnitPropertyID id,
                                      AudioUnitPropertyListenerProc proc, void *refCon) {
    OSStatus status = kAudio_MemFullError;

    if (!alive(unit) || proc == NULL)
        return kAudio_ParamError;
    if (isPlugIn(unit)) {
        AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitAddPropertyListenerSelect);
        return m != NULL ? ((AudioUnitAddPropertyListenerProc)m)(unit->state, id, proc, refCon)
                         : kAudioUnitErr_InvalidProperty;
    }
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
    if (isPlugIn(unit)) {
        AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitRemovePropertyListenerWithUserDataSelect);
        if (m != NULL)
            return ((AudioUnitRemovePropertyListenerWithUserDataProc)m)(unit->state, id, proc, refCon);
        m = _AUPlugInMethod(unit, kAudioUnitRemovePropertyListenerSelect);
        return m != NULL ? ((AudioUnitRemovePropertyListenerProc)m)(unit->state, id, proc)
                         : kAudioUnitErr_InvalidProperty;
    }
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
    if (isPlugIn(unit)) {
        AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitAddRenderNotifySelect);
        return m != NULL ? ((AudioUnitAddRenderNotifyProc)m)(unit->state, proc, refCon)
                         : kAudioUnitErr_InvalidProperty;
    }
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
    if (isPlugIn(unit)) {
        AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitRemoveRenderNotifySelect);
        return m != NULL ? ((AudioUnitRemoveRenderNotifyProc)m)(unit->state, proc, refCon)
                         : kAudioUnitErr_InvalidProperty;
    }
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

/* Parameter changes at offsets in the next buffer: the units here render
   whole buffers, so each takes its value (a ramp's end) at once. */
OSStatus AudioUnitScheduleParameters(AudioUnit unit, const AudioUnitParameterEvent *events, UInt32 count) {
    if (!alive(unit) || (events == NULL && count > 0))
        return kAudio_ParamError;
    if (isPlugIn(unit)) {
        AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitScheduleParametersSelect);
        if (m != NULL)
            return ((AudioUnitScheduleParametersProc)m)(unit->state, events, count);
    }
    for (UInt32 i = 0; i < count; i++) {
        const AudioUnitParameterEvent *e = &events[i];
        AudioUnitParameterValue v = e->eventType == kParameterEvent_Ramped ? e->eventValues.ramp.endValue
                                                                           : e->eventValues.immediate.value;
        OSStatus status;
        if (unit->cls->setParameter == NULL)
            return kAudioUnitErr_InvalidParameter;
        status = unit->cls->setParameter(unit, e->parameter, e->scope, e->element, v);
        if (status != noErr)
            return status;
    }
    return noErr;
}

/* In-place processing, for units that do it (a registered effect, say);
   the output units do not. */
OSStatus AudioUnitProcess(AudioUnit unit, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                          UInt32 frames, AudioBufferList *data) {
    AudioComponentMethod m;

    if (!alive(unit) || time == NULL || data == NULL)
        return kAudio_ParamError;
    m = isPlugIn(unit) ? _AUPlugInMethod(unit, kAudioUnitProcessSelect) : NULL;
    return m != NULL ? ((AudioUnitProcessProc)m)(unit->state, flags, time, frames, data)
                     : kAudio_UnimplementedError;
}

OSStatus AudioUnitProcessMultiple(AudioUnit unit, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                                  UInt32 frames, UInt32 inputs, const AudioBufferList **inLists, UInt32 outputs,
                                  AudioBufferList **outLists) {
    AudioComponentMethod m;

    if (!alive(unit) || time == NULL)
        return kAudio_ParamError;
    m = isPlugIn(unit) ? _AUPlugInMethod(unit, kAudioUnitProcessMultipleSelect) : NULL;
    return m != NULL ? ((AudioUnitProcessMultipleProc)m)(unit->state, flags, time, frames, inputs, inLists,
                                                         outputs, outLists)
                     : kAudio_UnimplementedError;
}

Boolean AudioComponentInstanceCanDo(AudioComponentInstance unit, SInt16 selector) {
    if (!alive(unit))
        return false;
    if (isPlugIn(unit))
        return _AUPlugInMethod(unit, selector) != NULL;
    switch (selector) {
    case kAudioUnitInitializeSelect:
    case kAudioUnitUninitializeSelect:
    case kAudioUnitGetPropertyInfoSelect:
    case kAudioUnitGetPropertySelect:
    case kAudioUnitSetPropertySelect:
    case kAudioUnitAddPropertyListenerSelect:
    case kAudioUnitRemovePropertyListenerWithUserDataSelect:
    case kAudioUnitAddRenderNotifySelect:
    case kAudioUnitRemoveRenderNotifySelect:
    case kAudioUnitScheduleParametersSelect:
    case kAudioUnitResetSelect:
        return true;
    case kAudioUnitGetParameterSelect:
    case kAudioUnitSetParameterSelect:
        return unit->cls->setParameter != NULL;
    case kAudioUnitRenderSelect:
        return unit->cls->render != NULL;
    case kAudioOutputUnitStartSelect:
    case kAudioOutputUnitStopSelect:
        return unit->cls->start != NULL;
    default:
        return false;
    }
}

/* What an object is, on a stream (CAShow's debugging aid): the units and
   components here; graphs and sequences come with them (AU9). */
void CAShowFile(void *object, FILE *file) {
    AudioUnit unit = object;
    AudioComponentDescription d;
    char t[5], st[5], m[5];

    if (file == NULL)
        return;
    if (object == NULL) {
        fprintf(file, "(null)\n");
        return;
    }
    if (alive(unit)) {
        d = unit->component->description;
        for (int i = 0; i < 4; i++) {
            t[i] = (char)(d.componentType >> (24 - 8 * i));
            st[i] = (char)(d.componentSubType >> (24 - 8 * i));
            m[i] = (char)(d.componentManufacturer >> (24 - 8 * i));
        }
        t[4] = st[4] = m[4] = 0;
        fprintf(file, "AudioUnit %p: '%s' '%s' '%s' \"%s\"\n", object, t, st, m, unit->component->name);
        return;
    }
    if (isComponent(object)) {
        fprintf(file, "AudioComponent %p: \"%s\"\n", object, ((AudioComponent)object)->name);
        return;
    }
    fprintf(file, "%p: not an object CAShow knows\n", object);
}

void CAShow(void *object) {
    CAShowFile(object, stdout);
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
