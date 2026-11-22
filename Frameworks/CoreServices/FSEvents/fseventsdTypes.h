/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * C types for fseventsd.defs: the watched paths, NUL-separated, and a
 * volume's history UUID as a string.
 */

#ifndef FSEVENTSD_TYPES_H
#define FSEVENTSD_TYPES_H

typedef char *fsed_paths_t;
typedef char fsed_uuid_t[37];	/* a UUID string and its NUL */

#endif
