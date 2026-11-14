/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The audio service's shared area for one client's IO (AudioServer.defs).
 * Private to CoreAudio and audiod.
 */

#ifndef _AUDIOSERVERTYPES_H_
#define _AUDIOSERVERTYPES_H_

#include <stdint.h>
#include <mach/mach_types.h>

#define kAudioServerName                "org.minkar.audiod"
#define kAudioServerInterfaceVersion    1
#define kAudioServerFramesPerCycle      512
#define kAudioServerChannels            2

typedef char as_name_t[512];

/*
 * At the start of the area. The service writes the cycle's times before it
 * signals wake: the frame of the device's timeline the samples will be
 * played at (outputSampleTime) and its host time, and now. The client
 * renders framesPerCycle frames of channels interleaved float32 samples at
 * samples, then signals done. Version, channels, rate and cycle size are
 * set once, at start.
 */
typedef struct AudioServerArea {
    uint32_t version;
    uint32_t framesPerCycle;
    uint32_t channels;
    uint32_t samplesOffset;         /* from the area's start */
    double   sampleRate;
    volatile uint64_t cycle;        /* counts up from 1 */
    volatile double   nowSampleTime;
    volatile uint64_t nowHostTime;
    volatile double   outputSampleTime;
    volatile uint64_t outputHostTime;
    volatile uint32_t late;         /* cycles the client missed */
} AudioServerArea;

#endif /* _AUDIOSERVERTYPES_H_ */
