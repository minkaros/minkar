/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __CMBASE__
#define __CMBASE__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>
#include <stdint.h>

#ifdef __cplusplus
#define CM_EXPORT extern "C"
#else
#define CM_EXPORT extern
#endif

typedef int32_t CMItemCount;
typedef intptr_t CMItemIndex;
typedef int32_t CMPersistentTrackID;

#endif
