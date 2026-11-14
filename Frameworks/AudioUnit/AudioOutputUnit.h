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

OSStatus AudioOutputUnitStart(AudioUnit ci);
OSStatus AudioOutputUnitStop(AudioUnit ci);

#ifdef __cplusplus
}
#endif

#endif
