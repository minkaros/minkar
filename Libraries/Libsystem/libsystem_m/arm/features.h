/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What the Arm sources here (pow_data.h, exp_data.h) want from musl's
 * <features.h>: "hidden" for symbols private to libsystem_m.
 */

#ifndef _MINKAR_ARM_FEATURES_H
#define _MINKAR_ARM_FEATURES_H

#define hidden __attribute__((__visibility__("hidden")))

#endif
