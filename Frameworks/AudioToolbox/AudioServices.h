/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * System sounds by ID. There is no audio output yet: sounds are accepted
 * and their completion runs, silently.
 */

#ifndef __AUDIOSERVICES__
#define __AUDIOSERVICES__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef UInt32 SystemSoundID;
typedef UInt32 AudioServicesPropertyID;
typedef void (*AudioServicesSystemSoundCompletionProc)(SystemSoundID ssID, void *clientData);

enum {
    kAudioServicesNoError = 0,
    kAudioServicesUnsupportedPropertyError = 'pty?',
    kAudioServicesBadPropertySizeError = '!siz',
    kAudioServicesBadSpecifierError = 'spec',
    kAudioServicesSystemSoundUnspecifiedError = -1500,
    kAudioServicesSystemSoundClientTimedOutError = -1501
};

enum {
    kSystemSoundID_UserPreferredAlert = 0x00001000,
    kSystemSoundID_FlashScreen = 0x00000FFE,
    kUserPreferredAlert = kSystemSoundID_UserPreferredAlert
};

OSStatus AudioServicesCreateSystemSoundID(CFURLRef inFileURL, SystemSoundID *outSystemSoundID);
OSStatus AudioServicesDisposeSystemSoundID(SystemSoundID inSystemSoundID);
void AudioServicesPlaySystemSound(SystemSoundID inSystemSoundID);
void AudioServicesPlayAlertSound(SystemSoundID inSystemSoundID);
#ifdef __BLOCKS__
void AudioServicesPlaySystemSoundWithCompletion(SystemSoundID inSystemSoundID, void (^inCompletionBlock)(void));
void AudioServicesPlayAlertSoundWithCompletion(SystemSoundID inSystemSoundID, void (^inCompletionBlock)(void));
#endif
OSStatus AudioServicesAddSystemSoundCompletion(SystemSoundID inSystemSoundID, CFRunLoopRef inRunLoop, CFStringRef inRunLoopMode,
                                               AudioServicesSystemSoundCompletionProc inCompletionRoutine, void *inClientData);
void AudioServicesRemoveSystemSoundCompletion(SystemSoundID inSystemSoundID);

#ifdef __cplusplus
}
#endif

#endif
