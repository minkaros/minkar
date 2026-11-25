/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * audiod's audio server plug-ins (plugins.c): the drivers in the HAL
 * plug-in folders, loaded at start, and their devices' IO.
 */

#ifndef _AUDIOD_PLUGINS_H_
#define _AUDIOD_PLUGINS_H_

#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>

typedef struct PlugInDevice PlugInDevice;

/* Loads every driver; the number of devices they have. */
int PlugInsLoad(void);
int PlugInDeviceCount(void);
PlugInDevice *PlugInDeviceAt(int index);

const char *PlugInDeviceUID(const PlugInDevice *d);
/* For copy_devices (AudioServerTypes.h's keys). */
CFDictionaryRef PlugInDeviceCopyDescription(const PlugInDevice *d);
double PlugInDeviceSampleRate(const PlugInDevice *d);
int PlugInDeviceHasInput(const PlugInDevice *d);
uint32_t PlugInDeviceSafetyOffset(const PlugInDevice *d, int input);

int PlugInDeviceStartIO(PlugInDevice *d);
void PlugInDeviceStopIO(PlugInDevice *d);
/* The device's frame at a host time, from its zero time stamps. */
double PlugInDeviceFrameAt(PlugInDevice *d, uint64_t host);
/* One IO cycle: frames recorded from readFrame into recorded (stereo
   float, when the device has an input), the mix (stereo float) written
   at writeFrame. */
void PlugInDeviceCycle(PlugInDevice *d, uint64_t cycle, uint32_t frames, uint64_t now, double nowFrame,
                       double readFrame, uint64_t inputHost, float *recorded,
                       double writeFrame, uint64_t outputHost, const float *mix);

#endif
