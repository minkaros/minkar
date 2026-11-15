/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An audio device's clock: the time its samples are played at, which may
 * drift from the host's. With no device given, the default output's,
 * following it when it changes.
 */

#ifndef __CMAUDIODEVICECLOCK__
#define __CMAUDIODEVICECLOCK__

#include <CoreMedia/CMBase.h>
#include <CoreMedia/CMSync.h>
#include <CoreAudio/CoreAudio.h>

CM_EXPORT OSStatus CMAudioDeviceClockCreate(CFAllocatorRef allocator, CFStringRef deviceUID, CMClockRef *clockOut);
CM_EXPORT OSStatus CMAudioDeviceClockCreateFromAudioDeviceID(CFAllocatorRef allocator, AudioDeviceID deviceID,
                                                             CMClockRef *clockOut);
CM_EXPORT OSStatus CMAudioDeviceClockSetAudioDeviceUID(CMClockRef clock, CFStringRef deviceUID);
CM_EXPORT OSStatus CMAudioDeviceClockSetAudioDeviceID(CMClockRef clock, AudioDeviceID deviceID);
CM_EXPORT OSStatus CMAudioDeviceClockGetAudioDevice(CMClockRef clock, CFStringRef *deviceUIDOut,
                                                    AudioDeviceID *deviceIDOut, Boolean *trackingDefaultDeviceOut);

#endif
