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
#define kAudioServerInterfaceVersion    3
#define kAudioServerFramesPerCycle      512     /* unless the first client asks otherwise */
#define kAudioServerMinFramesPerCycle   64
#define kAudioServerMaxFramesPerCycle   4096
#define kAudioServerChannels            2

typedef char as_name_t[512];

/* A plug-in device's description (copy_devices): its UID (the key the
   service knows it by), name, manufacturer, transport type, nominal rate,
   channels of its output and input, latency and safety offset in frames
   (output, input), whether it can be the default device. */
#define kAudioServerDeviceUID           "uid"
#define kAudioServerDeviceName          "name"
#define kAudioServerDeviceManufacturer  "manufacturer"
#define kAudioServerDeviceTransport     "transport"
#define kAudioServerDeviceSampleRate    "rate"
#define kAudioServerDeviceOutChannels   "out"
#define kAudioServerDeviceInChannels    "in"
#define kAudioServerDeviceOutLatency    "latency-out"
#define kAudioServerDeviceInLatency     "latency-in"
#define kAudioServerDeviceOutSafety     "safety-out"
#define kAudioServerDeviceInSafety      "safety-in"
#define kAudioServerDeviceCanBeDefault  "default"

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
    /* The device's input, when it has one (inputChannels 0 when not): the
       cycle's recorded frames, as float, and when they were recorded. */
    uint32_t inputChannels;
    uint32_t inputSamplesOffset;    /* from the area's start */
    volatile double   inputSampleTime;
    volatile uint64_t inputHostTime;
} AudioServerArea;

#endif /* _AUDIOSERVERTYPES_H_ */
