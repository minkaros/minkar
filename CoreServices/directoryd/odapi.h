/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * OpenDirectory's service in directoryd (odapi.c).
 */

#ifndef DIRECTORYD_ODAPI_H
#define DIRECTORYD_ODAPI_H

/* Starts answering on OpenDirectory's service, on a queue of its own. */
void odapi_start(void);

#endif
