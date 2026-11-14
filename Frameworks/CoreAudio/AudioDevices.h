/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The devices behind the system object (AudioDevices.c). Private.
 */

#ifndef __AUDIODEVICES_PRIVATE__
#define __AUDIODEVICES_PRIVATE__

#include "AudioHardware.h"

__attribute__((visibility("hidden"))) UInt32 _AudioDeviceCopyIDs(AudioDeviceID *ids, UInt32 capacity);
__attribute__((visibility("hidden"))) AudioDeviceID _AudioDeviceDefaultOutput(void);
__attribute__((visibility("hidden"))) AudioDeviceID _AudioDeviceWithUID(CFStringRef uid);
__attribute__((visibility("hidden"))) Boolean _AudioDeviceExists(AudioObjectID id);
__attribute__((visibility("hidden"))) Boolean _AudioDeviceHasProperty(AudioObjectID id, const AudioObjectPropertyAddress *a);
__attribute__((visibility("hidden"))) Boolean _AudioDeviceIsPropertySettable(AudioObjectID id, const AudioObjectPropertyAddress *a);
__attribute__((visibility("hidden"))) UInt32 _AudioDevicePropertySize(AudioObjectID id, const AudioObjectPropertyAddress *a);
__attribute__((visibility("hidden"))) OSStatus _AudioDeviceGetProperty(AudioObjectID id, const AudioObjectPropertyAddress *a, UInt32 *ioSize, void *out);
__attribute__((visibility("hidden"))) OSStatus _AudioDeviceSetProperty(AudioObjectID id, const AudioObjectPropertyAddress *a, UInt32 size, const void *data);

__attribute__((visibility("hidden"))) Boolean _AudioDeviceGetUIDCString(AudioObjectID id, char *buffer, size_t size);

#endif
