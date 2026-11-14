/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __AUDIOOUTPUTUNIT__
#define __AUDIOOUTPUTUNIT__

#include <AudioUnit/AUComponent.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kAudioOutputUnitRange = 0x0200,
    kAudioOutputUnitStartSelect = 0x0201,
    kAudioOutputUnitStopSelect = 0x0202,
};

typedef OSStatus (*AudioOutputUnitStartProc)(void *self);
typedef OSStatus (*AudioOutputUnitStopProc)(void *self);

OSStatus AudioOutputUnitStart(AudioUnit ci);
OSStatus AudioOutputUnitStop(AudioUnit ci);

#ifdef __cplusplus
}
#endif

#endif
