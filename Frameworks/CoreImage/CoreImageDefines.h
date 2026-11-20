/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef COREIMAGE_DEFINES_H
#define COREIMAGE_DEFINES_H

#ifdef __cplusplus
#define CORE_IMAGE_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define CORE_IMAGE_EXPORT extern __attribute__((visibility("default")))
#endif

#define CORE_IMAGE_CLASS_EXPORT __attribute__((visibility("default")))

#endif
