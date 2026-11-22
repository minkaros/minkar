/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CFOpenDirectory's private constants that its sources use and the
 * published sources do not define. Minkar's own.
 */

#ifndef __CFOPENDIRECTORY_CONSTANTSPRIV_H
#define __CFOPENDIRECTORY_CONSTANTSPRIV_H

#include <uuid/uuid.h>

/* The UUID of no session, node or query: all zeros. */
static const uuid_t kODNullUUID = { 0 };

#endif
