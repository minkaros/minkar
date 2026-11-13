/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Compiling sandbox profiles (SBPL) to the kernel's form (sbblob.h).
 */

#ifndef MINKAR_SBPL_H
#define MINKAR_SBPL_H

#include "sbblob.h"

/* The profile's blob (free it), or NULL with the reason in error.
   params: key, value, ..., NULL (the profile's (param "KEY")). */
uint8_t *sbpl_compile(const char *source, const char *const *params, size_t *size, char *error, size_t errorsize);

#endif
