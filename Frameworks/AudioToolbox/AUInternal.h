/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Audio units inside AudioToolbox: a component is a description and a
 * class; an instance is the class's state behind the AudioUnit handle.
 * Private.
 */

#ifndef _AUINTERNAL_H_
#define _AUINTERNAL_H_

#include <AudioUnit/AudioUnit.h>
#include <pthread.h>

typedef struct AUClass AUClass;

/* An instance (the AudioUnit handle points at one). */
struct ComponentInstanceRecord {
    uint32_t                magic;          /* kAUInstanceMagic while alive */
    const struct OpaqueAudioComponent *component;
    const AUClass          *cls;
    pthread_mutex_t         lock;
    struct {
        AudioUnitPropertyID id;
        AudioUnitPropertyListenerProc proc;
        void *refCon;
    } listeners[16];
    int                     listenerCount;
    struct {
        AURenderCallback proc;
        void *refCon;
    } notifies[8];
    int                     notifyCount;
    void                   *state;          /* the class's */
};

#define kAUInstanceMagic 0x41554e54u     /* 'AUNT' */

struct AUClass {
    OSStatus (*create)(AudioUnit unit);
    void     (*dispose)(AudioUnit unit);
    OSStatus (*initialize)(AudioUnit unit);
    OSStatus (*uninitialize)(AudioUnit unit);
    OSStatus (*getPropertyInfo)(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                                AudioUnitElement element, UInt32 *size, Boolean *writable);
    OSStatus (*getProperty)(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                            AudioUnitElement element, void *data, UInt32 *size);
    OSStatus (*setProperty)(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope,
                            AudioUnitElement element, const void *data, UInt32 size);
    OSStatus (*getParameter)(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                             AudioUnitElement element, AudioUnitParameterValue *value);
    OSStatus (*setParameter)(AudioUnit unit, AudioUnitParameterID id, AudioUnitScope scope,
                             AudioUnitElement element, AudioUnitParameterValue value);
    OSStatus (*render)(AudioUnit unit, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time,
                       UInt32 bus, UInt32 frames, AudioBufferList *data);
    OSStatus (*reset)(AudioUnit unit, AudioUnitScope scope, AudioUnitElement element);
    OSStatus (*start)(AudioUnit unit);
    OSStatus (*stop)(AudioUnit unit);
};

struct OpaqueAudioComponent {
    AudioComponentDescription description;
    const char *name;
    UInt32 version;
    const AUClass *cls;
};

/* Tells the unit's listeners a property changed. */
__attribute__((visibility("hidden")))
void _AUPropertyChanged(AudioUnit unit, AudioUnitPropertyID id, AudioUnitScope scope, AudioUnitElement element);

/* Calls the unit's render notifications (pre or post, in flags). */
__attribute__((visibility("hidden")))
void _AURenderNotify(AudioUnit unit, AudioUnitRenderActionFlags flags, const AudioTimeStamp *time,
                     UInt32 bus, UInt32 frames, AudioBufferList *data);

__attribute__((visibility("hidden"))) extern const AUClass _AUOutputClass;

/* Linear PCM to interleaved float, and the formats it reads. */
__attribute__((visibility("hidden")))
Boolean _AUFormatIsSupported(const AudioStreamBasicDescription *format);
__attribute__((visibility("hidden")))
void _AUToFloat(const AudioStreamBasicDescription *format, const AudioBufferList *in, UInt32 frames,
                float *out, UInt32 outChannels);
/* Interleaved float (the format's channel count) to the format, at a frame
   offset into the buffers. */
__attribute__((visibility("hidden")))
void _AUFromFloat(const AudioStreamBasicDescription *format, const float *in, UInt32 frames,
                  AudioBufferList *out, UInt32 offset);
__attribute__((visibility("hidden")))
void _AUMapChannels(const float *in, UInt32 inChannels, float *out, UInt32 outChannels, UInt32 frames,
                    const SInt32 *map);

#endif /* _AUINTERNAL_H_ */
