/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * System sounds: a sound file (up to 30 seconds) is read whole when its ID
 * is made, as float stereo at the output device's rate; playing it adds a
 * voice to a system output unit that mixes every voice being played, and
 * its completion runs, on the run loop asked for (the main one by default),
 * once the voice is done. The alert is the user's alert sound.
 */

#include "AUInternal.h"
#include "AudioServices.h"
#include <AudioToolbox/ExtendedAudioFile.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Block.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define kMaxSeconds     30
#define kMaxVoices      16
#define kAlertPath      "/System/Library/Sounds/Chime.aiff"
#define kAlertDomain    CFSTR("org.minkar.sound")
#define kAlertKey       CFSTR("AlertSound")

typedef struct Sound {
    SystemSoundID id;
    CFURLRef url;
    float *samples;             /* interleaved stereo */
    UInt32 frames;
    Boolean isUISound, completeIfAppDies;
    AudioServicesSystemSoundCompletionProc completion;
    void *clientData;
    CFRunLoopRef runLoop;
    CFStringRef mode;
} Sound;

typedef struct Voice {
    SystemSoundID id;
    const float *samples;
    UInt32 frames, at;
    float gain;                 /* the alert volume, for alerts and UI sounds */
    void (^block)(void);
} Voice;

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static Sound *sSounds;
static int sCount;
static SystemSoundID sNext = 0x2000;
static SystemSoundID sAlertID;
static CFStringRef sAlertPathInUse;     /* the file sAlertID was made from */

static AudioUnit sUnit;
static Float64 sRate;
static Voice sVoices[kMaxVoices];
static int sVoiceCount;
static Boolean sRunning;

static Sound *find(SystemSoundID id) {
    for (int i = 0; i < sCount; i++)
        if (sSounds[i].id == id)
            return &sSounds[i];
    return NULL;
}

/* ---- Output ------------------------------------------------------------------ */

static void voiceDone(Voice v);

static OSStatus render(void *refCon, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *time, UInt32 bus,
                       UInt32 frames, AudioBufferList *data) {
    float *out = data->mBuffers[0].mData;
    Voice done[kMaxVoices];
    int doneCount = 0;

    memset(out, 0, frames * 2 * sizeof(float));
    pthread_mutex_lock(&sLock);
    for (int i = 0; i < sVoiceCount;) {
        Voice *v = &sVoices[i];
        UInt32 n = v->frames - v->at < frames ? v->frames - v->at : frames;
        const float *src = v->samples + (size_t)v->at * 2;
        for (UInt32 k = 0; k < n * 2; k++)
            out[k] += src[k] * v->gain;
        v->at += n;
        if (v->at >= v->frames) {
            done[doneCount++] = *v;
            sVoices[i] = sVoices[--sVoiceCount];
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&sLock);
    for (int i = 0; i < doneCount; i++)
        voiceDone(done[i]);
    return noErr;
}

/* The output unit, made the first time a sound is read or played. */
static Boolean output(void) {
    AudioComponentDescription d = { kAudioUnitType_Output, kAudioUnitSubType_SystemOutput,
                                    kAudioUnitManufacturer_Apple, 0, 0 };
    AudioStreamBasicDescription f;
    AURenderCallbackStruct cb = { render, NULL };
    UInt32 size = sizeof(sRate);

    if (sUnit != NULL)
        return true;
    if (AudioComponentInstanceNew(AudioComponentFindNext(NULL, &d), &sUnit) != noErr)
        return false;
    AudioUnitGetProperty(sUnit, kAudioUnitProperty_SampleRate, kAudioUnitScope_Output, 0, &sRate, &size);
    if (sRate <= 0)
        sRate = 48000;
    memset(&f, 0, sizeof(f));
    f.mSampleRate = sRate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    f.mBytesPerPacket = f.mBytesPerFrame = 2 * sizeof(float);
    f.mFramesPerPacket = 1;
    f.mChannelsPerFrame = 2;
    f.mBitsPerChannel = 32;
    if (AudioUnitSetProperty(sUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &f, sizeof(f)) != noErr ||
        AudioUnitSetProperty(sUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &cb, sizeof(cb)) != noErr ||
        AudioUnitInitialize(sUnit) != noErr) {
        AudioComponentInstanceDispose(sUnit);
        sUnit = NULL;
        return false;
    }
    return true;
}

/* Stops the unit a moment after the last voice, unless another started. */
static void stopWhenIdle(void) {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 2), dispatch_get_global_queue(0, 0), ^{
        Boolean idle;
        pthread_mutex_lock(&sLock);
        idle = sVoiceCount == 0 && sRunning;
        if (idle)
            sRunning = false;
        pthread_mutex_unlock(&sLock);
        if (idle)
            AudioOutputUnitStop(sUnit);
    });
}

static void voiceDone(Voice v) {
    AudioServicesSystemSoundCompletionProc completion = NULL;
    void *clientData = NULL;
    CFRunLoopRef runLoop = NULL;
    CFStringRef mode = NULL;
    void (^block)(void) = v.block;
    SystemSoundID id = v.id;
    Boolean idle;

    pthread_mutex_lock(&sLock);
    Sound *s = find(id);
    if (s != NULL) {
        completion = s->completion;
        clientData = s->clientData;
        runLoop = s->runLoop != NULL ? (CFRunLoopRef)CFRetain(s->runLoop) : NULL;
        mode = s->mode != NULL ? (CFStringRef)CFRetain(s->mode) : NULL;
    }
    idle = sVoiceCount == 0;
    pthread_mutex_unlock(&sLock);
    if (completion != NULL || block != NULL) {
        void (^call)(void) = ^{
            if (completion != NULL)
                completion(id, clientData);
            if (block != NULL) {
                block();
                Block_release(block);
            }
        };
        if (runLoop != NULL) {
            CFRunLoopPerformBlock(runLoop, mode != NULL ? (CFTypeRef)mode : (CFTypeRef)kCFRunLoopCommonModes, call);
            CFRunLoopWakeUp(runLoop);
        } else {
            dispatch_async(dispatch_get_main_queue(), call);
        }
    }
    if (runLoop != NULL)
        CFRelease(runLoop);
    if (mode != NULL)
        CFRelease(mode);
    if (idle)
        stopWhenIdle();
}

/* ---- Sounds ----------------------------------------------------------------- */

/* The file, whole, as float stereo at the output's rate. */
static OSStatus readSound(CFURLRef url, float **outSamples, UInt32 *outFrames) {
    ExtAudioFileRef file;
    AudioStreamBasicDescription f;
    float *samples;
    UInt32 capacity, total = 0;
    OSStatus status;

    if (!output())
        return kAudioServicesSystemSoundUnspecifiedError;
    status = ExtAudioFileOpenURL(url, &file);
    if (status != noErr)
        return kAudioServicesSystemSoundUnspecifiedError;
    memset(&f, 0, sizeof(f));
    f.mSampleRate = sRate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagsNativeFloatPacked;
    f.mBytesPerPacket = f.mBytesPerFrame = 2 * sizeof(float);
    f.mFramesPerPacket = 1;
    f.mChannelsPerFrame = 2;
    f.mBitsPerChannel = 32;
    if (ExtAudioFileSetProperty(file, kExtAudioFileProperty_ClientDataFormat, sizeof(f), &f) != noErr) {
        ExtAudioFileDispose(file);
        return kAudioServicesSystemSoundUnspecifiedError;
    }
    capacity = (UInt32)(sRate * kMaxSeconds);
    samples = malloc((size_t)capacity * 2 * sizeof(float));
    if (samples == NULL) {
        ExtAudioFileDispose(file);
        return kAudioServicesSystemSoundUnspecifiedError;
    }
    while (total < capacity) {
        struct { UInt32 n; AudioBuffer b; } list = { 1, { 2, 0, samples + (size_t)total * 2 } };
        UInt32 n = capacity - total < 4096 ? capacity - total : 4096;
        list.b.mDataByteSize = n * 2 * sizeof(float);
        if (ExtAudioFileRead(file, &n, (AudioBufferList *)&list) != noErr || n == 0)
            break;
        total += n;
    }
    ExtAudioFileDispose(file);
    if (total == 0) {
        free(samples);
        return kAudioServicesSystemSoundUnspecifiedError;
    }
    *outSamples = realloc(samples, (size_t)total * 2 * sizeof(float));
    if (*outSamples == NULL)
        *outSamples = samples;
    *outFrames = total;
    return noErr;
}

OSStatus AudioServicesCreateSystemSoundID(CFURLRef inFileURL, SystemSoundID *outSystemSoundID) {
    float *samples = NULL;
    UInt32 frames = 0;
    OSStatus status;
    Sound *grown;

    if (inFileURL == NULL || outSystemSoundID == NULL)
        return kAudioServicesBadSpecifierError;
    status = readSound(inFileURL, &samples, &frames);
    if (status != noErr)
        return status;
    pthread_mutex_lock(&sLock);
    grown = realloc(sSounds, (size_t)(sCount + 1) * sizeof(Sound));
    if (grown == NULL) {
        pthread_mutex_unlock(&sLock);
        free(samples);
        return kAudioServicesSystemSoundUnspecifiedError;
    }
    sSounds = grown;
    memset(&sSounds[sCount], 0, sizeof(Sound));
    sSounds[sCount].id = sNext++;
    sSounds[sCount].url = CFRetain(inFileURL);
    sSounds[sCount].samples = samples;
    sSounds[sCount].frames = frames;
    sSounds[sCount].isUISound = true;
    *outSystemSoundID = sSounds[sCount++].id;
    pthread_mutex_unlock(&sLock);
    return kAudioServicesNoError;
}

static void releaseSound(Sound *s) {
    CFRelease(s->url);
    if (s->runLoop != NULL)
        CFRelease(s->runLoop);
    if (s->mode != NULL)
        CFRelease(s->mode);
}

OSStatus AudioServicesDisposeSystemSoundID(SystemSoundID inSystemSoundID) {
    OSStatus status = kAudioServicesBadSpecifierError;
    Sound *s;

    pthread_mutex_lock(&sLock);
    s = find(inSystemSoundID);
    if (s != NULL) {
        /* Its voices stop: their samples go with it. */
        for (int i = 0; i < sVoiceCount;)
            if (sVoices[i].id == inSystemSoundID) {
                if (sVoices[i].block != NULL)
                    Block_release(sVoices[i].block);
                sVoices[i] = sVoices[--sVoiceCount];
            } else {
                i++;
            }
        releaseSound(s);
        free(s->samples);
        *s = sSounds[--sCount];
        status = kAudioServicesNoError;
    }
    pthread_mutex_unlock(&sLock);
    return status;
}

/* A global (NSGlobalDomain) preference, read anew: the Sound pane changes
   them while apps run. */
static CFPropertyListRef globalValue(CFStringRef key) {
    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    return CFPreferencesCopyAppValue(key, kCFPreferencesAnyApplication);
}

/* The alert volume (com.apple.sound.beep.volume, 0 to 1; 1 when unset). */
static float alertVolume(void) {
    CFPropertyListRef v = globalValue(CFSTR("com.apple.sound.beep.volume"));
    float volume = 1;

    if (v != NULL && CFGetTypeID(v) == CFNumberGetTypeID())
        CFNumberGetValue(v, kCFNumberFloatType, &volume);
    if (v != NULL)
        CFRelease(v);
    return volume < 0 ? 0 : volume > 1 ? 1 : volume;
}

/* Whether user interface sound effects play (com.apple.sound.uiaudio.enabled,
   on when unset). */
static Boolean uiSoundsEnabled(void) {
    Boolean valid = false;
    Boolean on;

    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    on = CFPreferencesGetAppBooleanValue(CFSTR("com.apple.sound.uiaudio.enabled"),
                                         kCFPreferencesAnyApplication, &valid);
    return valid ? on : true;
}

/* The user's alert sound: com.apple.sound.beep.sound (a sound file's path,
   the reference's key), else Minkar's earlier org.minkar.sound AlertSound,
   else Chime. Made again when the choice changes. */
static SystemSoundID alertSound(void) {
    CFStringRef path;
    CFURLRef url;
    SystemSoundID id = 0, old = 0;

    path = globalValue(CFSTR("com.apple.sound.beep.sound"));
    if (path != NULL && CFGetTypeID(path) != CFStringGetTypeID()) {
        CFRelease(path);
        path = NULL;
    }
    if (path == NULL) {
        path = CFPreferencesCopyAppValue(kAlertKey, kAlertDomain);
        if (path != NULL && CFGetTypeID(path) != CFStringGetTypeID()) {
            CFRelease(path);
            path = NULL;
        }
    }
    if (path == NULL)
        path = CFStringCreateWithCString(NULL, kAlertPath, kCFStringEncodingUTF8);
    pthread_mutex_lock(&sLock);
    if (sAlertID != 0 && sAlertPathInUse != NULL && CFEqual(sAlertPathInUse, path)) {
        id = sAlertID;
        pthread_mutex_unlock(&sLock);
        CFRelease(path);
        return id;
    }
    pthread_mutex_unlock(&sLock);
    url = CFURLCreateWithFileSystemPath(NULL, path, kCFURLPOSIXPathStyle, false);
    if (url == NULL || AudioServicesCreateSystemSoundID(url, &id) != noErr)
        id = 0;
    if (url != NULL)
        CFRelease(url);
    if (id == 0) {
        CFRelease(path);
        pthread_mutex_lock(&sLock);
        id = sAlertID;
        pthread_mutex_unlock(&sLock);
        return id;
    }
    pthread_mutex_lock(&sLock);
    old = sAlertID;
    sAlertID = id;
    if (sAlertPathInUse != NULL)
        CFRelease(sAlertPathInUse);
    sAlertPathInUse = path;
    pthread_mutex_unlock(&sLock);
    /* The earlier one is kept: a voice may still be playing it. */
    (void)old;
    return id;
}

static void play(SystemSoundID id, Boolean alert, void (^block)(void)) {
    Sound *s;
    Boolean start = false, added = false, silent = false;
    void (^copy)(void) = block != NULL ? Block_copy(block) : NULL;
    float gain = 1;

    if (id == kSystemSoundID_UserPreferredAlert) {
        id = alertSound();
        alert = true;
    }
    pthread_mutex_lock(&sLock);
    s = find(id);
    Boolean ui = s != NULL && s->isUISound;
    pthread_mutex_unlock(&sLock);
    /* Alerts and user interface sounds play at the alert volume; the
       latter not at all when the user turned them off. */
    if (alert || ui)
        gain = alertVolume();
    if (ui && !alert && !uiSoundsEnabled())
        silent = true;
    pthread_mutex_lock(&sLock);
    s = find(id);
    if (s != NULL && !silent && sVoiceCount < kMaxVoices) {
        sVoices[sVoiceCount++] = (Voice){ id, s->samples, s->frames, 0, gain, copy };
        added = true;
        if (!sRunning)
            start = sRunning = true;
    }
    pthread_mutex_unlock(&sLock);
    if (start && AudioOutputUnitStart(sUnit) != noErr) {
        pthread_mutex_lock(&sLock);
        sRunning = false;
        pthread_mutex_unlock(&sLock);
    }
    if (!added) {
        /* Nothing to play: the completion still runs. */
        Voice v = { id, NULL, 0, 0, 1, copy };
        voiceDone(v);
    }
}

void AudioServicesPlaySystemSound(SystemSoundID inSystemSoundID) {
    if (inSystemSoundID == kSystemSoundID_FlashScreen)
        return;
    play(inSystemSoundID, false, NULL);
}

void AudioServicesPlayAlertSound(SystemSoundID inSystemSoundID) {
    play(inSystemSoundID, true, NULL);
}

void AudioServicesPlaySystemSoundWithCompletion(SystemSoundID inSystemSoundID, void (^inCompletionBlock)(void)) {
    play(inSystemSoundID, false, inCompletionBlock);
}

void AudioServicesPlayAlertSoundWithCompletion(SystemSoundID inSystemSoundID, void (^inCompletionBlock)(void)) {
    play(inSystemSoundID, true, inCompletionBlock);
}

OSStatus AudioServicesAddSystemSoundCompletion(SystemSoundID inSystemSoundID, CFRunLoopRef inRunLoop,
                                               CFStringRef inRunLoopMode,
                                               AudioServicesSystemSoundCompletionProc inCompletionRoutine,
                                               void *inClientData) {
    OSStatus status = kAudioServicesBadSpecifierError;
    Sound *s;

    pthread_mutex_lock(&sLock);
    s = find(inSystemSoundID);
    if (s != NULL) {
        s->completion = inCompletionRoutine;
        s->clientData = inClientData;
        if (s->runLoop != NULL)
            CFRelease(s->runLoop);
        if (s->mode != NULL)
            CFRelease(s->mode);
        s->runLoop = inRunLoop != NULL ? (CFRunLoopRef)CFRetain(inRunLoop) : NULL;
        s->mode = inRunLoopMode != NULL ? (CFStringRef)CFRetain(inRunLoopMode) : NULL;
        status = kAudioServicesNoError;
    }
    pthread_mutex_unlock(&sLock);
    return status;
}

void AudioServicesRemoveSystemSoundCompletion(SystemSoundID inSystemSoundID) {
    Sound *s;

    pthread_mutex_lock(&sLock);
    s = find(inSystemSoundID);
    if (s != NULL)
        s->completion = NULL;
    pthread_mutex_unlock(&sLock);
}

/* ---- Properties ------------------------------------------------------------ */

OSStatus AudioServicesGetPropertyInfo(AudioServicesPropertyID id, UInt32 specSize, const void *spec,
                                      UInt32 *outSize, Boolean *outWritable) {
    if (id != kAudioServicesPropertyIsUISound && id != kAudioServicesPropertyCompletePlaybackIfAppDies)
        return kAudioServicesUnsupportedPropertyError;
    if (specSize < sizeof(SystemSoundID) || spec == NULL)
        return kAudioServicesBadSpecifierError;
    if (outSize != NULL)
        *outSize = sizeof(UInt32);
    if (outWritable != NULL)
        *outWritable = true;
    return kAudioServicesNoError;
}

OSStatus AudioServicesGetProperty(AudioServicesPropertyID id, UInt32 specSize, const void *spec, UInt32 *ioSize,
                                  void *data) {
    OSStatus status = kAudioServicesBadSpecifierError;
    Sound *s;

    if (id != kAudioServicesPropertyIsUISound && id != kAudioServicesPropertyCompletePlaybackIfAppDies)
        return kAudioServicesUnsupportedPropertyError;
    if (specSize < sizeof(SystemSoundID) || spec == NULL || ioSize == NULL || data == NULL)
        return kAudioServicesBadSpecifierError;
    if (*ioSize < sizeof(UInt32))
        return kAudioServicesBadPropertySizeError;
    pthread_mutex_lock(&sLock);
    s = find(*(const SystemSoundID *)spec);
    if (s != NULL) {
        *(UInt32 *)data = id == kAudioServicesPropertyIsUISound ? s->isUISound : s->completeIfAppDies;
        *ioSize = sizeof(UInt32);
        status = kAudioServicesNoError;
    }
    pthread_mutex_unlock(&sLock);
    return status;
}

OSStatus AudioServicesSetProperty(AudioServicesPropertyID id, UInt32 specSize, const void *spec, UInt32 size,
                                  const void *data) {
    OSStatus status = kAudioServicesBadSpecifierError;
    Sound *s;

    if (id != kAudioServicesPropertyIsUISound && id != kAudioServicesPropertyCompletePlaybackIfAppDies)
        return kAudioServicesUnsupportedPropertyError;
    if (specSize < sizeof(SystemSoundID) || spec == NULL || data == NULL)
        return kAudioServicesBadSpecifierError;
    if (size < sizeof(UInt32))
        return kAudioServicesBadPropertySizeError;
    pthread_mutex_lock(&sLock);
    s = find(*(const SystemSoundID *)spec);
    if (s != NULL) {
        if (id == kAudioServicesPropertyIsUISound)
            s->isUISound = *(const UInt32 *)data != 0;
        else
            s->completeIfAppDies = *(const UInt32 *)data != 0;
        status = kAudioServicesNoError;
    }
    pthread_mutex_unlock(&sLock);
    return status;
}
