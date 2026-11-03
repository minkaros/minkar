/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The pasteboard server's interface types (Pasteboard.defs): a pasteboard's
 * or a type's name, a C string of at most 255 bytes.
 */

#ifndef _PASTEBOARD_TYPES_H
#define _PASTEBOARD_TYPES_H

#include <stdint.h>

typedef char pb_name_t[256];
typedef uint8_t *pb_bytes_t;

#endif
