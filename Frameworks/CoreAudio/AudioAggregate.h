/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Aggregate devices (AudioAggregate.c). Private.
 */

#ifndef __AUDIOAGGREGATE_PRIVATE__
#define __AUDIOAGGREGATE_PRIVATE__

#include "AudioHardware.h"

#define _AU_HIDDEN __attribute__((visibility("hidden")))

_AU_HIDDEN Boolean _AudioAggregateIs(AudioObjectID id);
_AU_HIDDEN UInt32 _AudioAggregateCopyIDs(AudioObjectID *ids, UInt32 capacity);
_AU_HIDDEN AudioObjectID _AudioAggregateWithUID(CFStringRef uid);
_AU_HIDDEN Boolean _AudioAggregateHasProperty(AudioObjectID id, const AudioObjectPropertyAddress *a);
_AU_HIDDEN Boolean _AudioAggregateIsPropertySettable(AudioObjectID id, const AudioObjectPropertyAddress *a);
_AU_HIDDEN UInt32 _AudioAggregatePropertySize(AudioObjectID id, const AudioObjectPropertyAddress *a);
_AU_HIDDEN OSStatus _AudioAggregateGetProperty(AudioObjectID id, const AudioObjectPropertyAddress *a, UInt32 *ioSize, void *out);
_AU_HIDDEN OSStatus _AudioAggregateSetProperty(AudioObjectID id, const AudioObjectPropertyAddress *a, UInt32 size, const void *data);
/* The aggregate's IO for an app's procedure: its sub-devices started (a
   run to stop), or NULL with *status set. */
_AU_HIDDEN void *_AudioAggregateStart(AudioObjectID id, AudioDeviceIOProc proc, void *clientData, OSStatus *status);
_AU_HIDDEN void _AudioAggregateStop(void *run);

/* The system object's device list changed: its listeners are told. */
_AU_HIDDEN void _AudioHardwareDevicesChanged(void);

#endif
