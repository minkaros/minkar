/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef METALKIT_DEFINES_H
#define METALKIT_DEFINES_H

#ifdef __cplusplus
#define MTK_EXTERN extern "C" __attribute__((visibility("default")))
#else
#define MTK_EXTERN extern __attribute__((visibility("default")))
#endif

#define MTK_EXPORT __attribute__((visibility("default")))

#endif
