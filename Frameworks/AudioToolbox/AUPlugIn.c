/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Components an app registers (AudioComponentRegister): an instance is
 * the plug-in interface its factory gives, opened with the instance, and
 * each AudioUnit call is the method its Lookup gives for the call's
 * selector, called with the interface as self.
 */

#include "AUInternal.h"
#include <AudioToolbox/AudioToolbox.h>

#define IFACE(unit) ((AudioComponentPlugInInterface *)(unit)->state)

AudioComponentMethod _AUPlugInMethod(AudioUnit unit, SInt16 selector) {
    AudioComponentPlugInInterface *iface = IFACE(unit);
    return iface != NULL && iface->Lookup != NULL ? iface->Lookup(selector) : NULL;
}

static OSStatus plugInCreate(AudioUnit unit) {
    AudioComponentFactoryFunction factory = _AUComponentFactory(unit->component);
    AudioComponentPlugInInterface *iface = factory != NULL ? factory(&unit->component->description) : NULL;
    OSStatus status;

    if (iface == NULL)
        return kAudioUnitErr_FailedInitialization;
    status = iface->Open != NULL ? iface->Open(iface, unit) : noErr;
    if (status != noErr)
        return status;
    unit->state = iface;
    return noErr;
}

static void plugInDispose(AudioUnit unit) {
    AudioComponentPlugInInterface *iface = IFACE(unit);

    if (iface != NULL && iface->Close != NULL)
        iface->Close(iface);
    unit->state = NULL;
}

static OSStatus plugInInitialize(AudioUnit unit) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitInitializeSelect);
    return m != NULL ? ((AudioUnitInitializeProc)m)(IFACE(unit)) : noErr;
}

static OSStatus plugInUninitialize(AudioUnit unit) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitUninitializeSelect);
    return m != NULL ? ((AudioUnitUninitializeProc)m)(IFACE(unit)) : noErr;
}

static OSStatus plugInGetPropertyInfo(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                      AudioUnitElement element, UInt32 *size, Boolean *writable) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitGetPropertyInfoSelect);
    return m != NULL ? ((AudioUnitGetPropertyInfoProc)m)(IFACE(unit), id, scope, element, size, writable)
                     : kAudioUnitErr_InvalidProperty;
}

static OSStatus plugInGetProperty(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                  AudioUnitElement element, void *data, UInt32 *size) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitGetPropertySelect);
    return m != NULL ? ((AudioUnitGetPropertyProc)m)(IFACE(unit), id, scope, element, data, size)
                     : kAudioUnitErr_InvalidProperty;
}

static OSStatus plugInSetProperty(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                  AudioUnitElement element, const void *data, UInt32 size) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitSetPropertySelect);
    return m != NULL ? ((AudioUnitSetPropertyProc)m)(IFACE(unit), id, scope, element, data, size)
                     : kAudioUnitErr_InvalidProperty;
}

static OSStatus plugInGetParameter(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                                   AudioUnitElement element, AudioUnitParameterValue *value) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitGetParameterSelect);
    return m != NULL ? ((AudioUnitGetParameterProc)m)(IFACE(unit), id, scope, element, value)
                     : kAudioUnitErr_InvalidParameter;
}

static OSStatus plugInSetParameter(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                                   AudioUnitElement element, AudioUnitParameterValue value) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitSetParameterSelect);
    return m != NULL ? ((AudioUnitSetParameterProc)m)(IFACE(unit), id, scope, element, value, 0)
                     : kAudioUnitErr_InvalidParameter;
}

static OSStatus plugInRender(AudioUnit unit, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                             UInt32 bus, UInt32 frames, AudioBufferList *data) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitRenderSelect);
    AudioUnitRenderActionFlags none = 0;
    return m != NULL ? ((AudioUnitRenderProc)m)(IFACE(unit), flags != NULL ? flags : &none, time, bus, frames, data)
                     : kAudioUnitErr_NoConnection;
}

static OSStatus plugInReset(AudioUnit unit, AudioUnitScope scope, AudioUnitElement element) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioUnitResetSelect);
    return m != NULL ? ((AudioUnitResetProc)m)(IFACE(unit), scope, element) : noErr;
}

static OSStatus plugInStart(AudioUnit unit) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioOutputUnitStartSelect);
    return m != NULL ? ((AudioOutputUnitStartProc)m)(IFACE(unit)) : kAudioUnitErr_InvalidProperty;
}

static OSStatus plugInStop(AudioUnit unit) {
    AudioComponentMethod m = _AUPlugInMethod(unit, kAudioOutputUnitStopSelect);
    return m != NULL ? ((AudioOutputUnitStopProc)m)(IFACE(unit)) : kAudioUnitErr_InvalidProperty;
}

const AUClass _AUPlugInClass = {
    .create = plugInCreate,
    .dispose = plugInDispose,
    .initialize = plugInInitialize,
    .uninitialize = plugInUninitialize,
    .getPropertyInfo = plugInGetPropertyInfo,
    .getProperty = plugInGetProperty,
    .setProperty = plugInSetProperty,
    .getParameter = plugInGetParameter,
    .setParameter = plugInSetParameter,
    .render = plugInRender,
    .reset = plugInReset,
    .start = plugInStart,
    .stop = plugInStop,
};
