/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The compression types this kext gives decmpfs (see
 * MinkarFSCompressionTypes.c).
 */

#ifndef _MINKAR_FS_COMPRESSION_TYPES_H_
#define _MINKAR_FS_COMPRESSION_TYPES_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
extern const uint32_t MinkarFSCompressionTypes[];
extern const unsigned MinkarFSCompressionTypeCount;
int MinkarFSCompressionRegister(uint32_t type);
void MinkarFSCompressionUnregister(uint32_t type);
#ifdef __cplusplus
}
#endif

#endif
