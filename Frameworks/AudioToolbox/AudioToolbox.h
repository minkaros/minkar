/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __AUDIOTOOLBOX__
#define __AUDIOTOOLBOX__

#include <AudioToolbox/AudioConverter.h>
#include <AudioToolbox/AudioFile.h>
#include <AudioToolbox/AudioFileStream.h>
#include <AudioToolbox/AudioFormat.h>
#include <AudioToolbox/AudioQueue.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#include <AudioToolbox/AudioServices.h>
#include <AudioUnit/AudioUnit.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
/* Shows what an AudioToolbox object is, for debugging. */
void CAShow(void *inObject);
void CAShowFile(void *inObject, FILE *inFile);
#ifdef __cplusplus
}
#endif

#endif
