/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Responsibility for a spawned process. A parent may disclaim a child it
 * starts (a browser its helpers, a terminal its shell) so that the
 * child answers for its own privacy permissions. Minkar keeps no
 * per-app privacy permissions yet, so there is nothing to pass to the
 * child: the call is accepted and the process starts as it would have.
 */

#include <stdbool.h>
#include <stddef.h>
#include <errno.h>

/* The attributes as posix_spawnattr_t * (a pointer to an opaque pointer):
   <spawn.h> is not the C library's one on this file's include path. */
int responsibility_spawnattrs_setdisclaim(void **attrs, bool disclaim);

int
responsibility_spawnattrs_setdisclaim(void **attrs, bool disclaim)
{
	(void)disclaim;
	if (attrs == NULL || *attrs == NULL)
		return EINVAL;
	return 0;
}
