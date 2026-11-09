/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SHORTCUT(Atlas): there is no audio output, so playing a sound only runs
 * its completion. Sounds are kept by ID with their file's URL.
 */

#include "AudioServices.h"
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct Sound {
    SystemSoundID id;
    CFURLRef url;
    AudioServicesSystemSoundCompletionProc completion;
    void *clientData;
} Sound;

static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static Sound *sSounds;
static int sCount;
static SystemSoundID sNext = 0x2000;

static Sound *find(SystemSoundID id) {
    for (int i = 0; i < sCount; i++)
        if (sSounds[i].id == id)
            return &sSounds[i];
    return NULL;
}

OSStatus AudioServicesCreateSystemSoundID(CFURLRef inFileURL, SystemSoundID *outSystemSoundID) {
    if (inFileURL == NULL || outSystemSoundID == NULL)
        return kAudioServicesBadSpecifierError;
    if (!CFURLResourceIsReachable(inFileURL, NULL))
        return kAudioServicesSystemSoundUnspecifiedError;
    pthread_mutex_lock(&sLock);
    Sound *grown = realloc(sSounds, (size_t)(sCount + 1) * sizeof(Sound));
    if (grown == NULL) {
        pthread_mutex_unlock(&sLock);
        return kAudioServicesSystemSoundUnspecifiedError;
    }
    sSounds = grown;
    sSounds[sCount] = (Sound){ sNext++, CFRetain(inFileURL), NULL, NULL };
    *outSystemSoundID = sSounds[sCount++].id;
    pthread_mutex_unlock(&sLock);
    return kAudioServicesNoError;
}

OSStatus AudioServicesDisposeSystemSoundID(SystemSoundID inSystemSoundID) {
    OSStatus status = kAudioServicesBadSpecifierError;
    pthread_mutex_lock(&sLock);
    Sound *s = find(inSystemSoundID);
    if (s != NULL) {
        CFRelease(s->url);
        *s = sSounds[--sCount];
        status = kAudioServicesNoError;
    }
    pthread_mutex_unlock(&sLock);
    return status;
}

static void finished(SystemSoundID id, void (^block)(void)) {
    AudioServicesSystemSoundCompletionProc completion = NULL;
    void *clientData = NULL;
    pthread_mutex_lock(&sLock);
    Sound *s = find(id);
    if (s != NULL) {
        completion = s->completion;
        clientData = s->clientData;
    }
    pthread_mutex_unlock(&sLock);
    if (completion != NULL || block != NULL)
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completion != NULL)
                completion(id, clientData);
            if (block != NULL)
                block();
        });
}

void AudioServicesPlaySystemSound(SystemSoundID inSystemSoundID) {
    finished(inSystemSoundID, NULL);
}

void AudioServicesPlayAlertSound(SystemSoundID inSystemSoundID) {
    finished(inSystemSoundID, NULL);
}

void AudioServicesPlaySystemSoundWithCompletion(SystemSoundID inSystemSoundID, void (^inCompletionBlock)(void)) {
    finished(inSystemSoundID, inCompletionBlock);
}

void AudioServicesPlayAlertSoundWithCompletion(SystemSoundID inSystemSoundID, void (^inCompletionBlock)(void)) {
    finished(inSystemSoundID, inCompletionBlock);
}

OSStatus AudioServicesAddSystemSoundCompletion(SystemSoundID inSystemSoundID, CFRunLoopRef inRunLoop, CFStringRef inRunLoopMode,
                                               AudioServicesSystemSoundCompletionProc inCompletionRoutine, void *inClientData) {
    OSStatus status = kAudioServicesBadSpecifierError;
    pthread_mutex_lock(&sLock);
    Sound *s = find(inSystemSoundID);
    if (s != NULL) {
        s->completion = inCompletionRoutine;
        s->clientData = inClientData;
        status = kAudioServicesNoError;
    }
    pthread_mutex_unlock(&sLock);
    return status;
}

void AudioServicesRemoveSystemSoundCompletion(SystemSoundID inSystemSoundID) {
    pthread_mutex_lock(&sLock);
    Sound *s = find(inSystemSoundID);
    if (s != NULL)
        s->completion = NULL;
    pthread_mutex_unlock(&sLock);
}
