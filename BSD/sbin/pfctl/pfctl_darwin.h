/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * pfctl_darwin.h: what OpenBSD 4.3's pfctl uses under other names on
 * Darwin, included before each of its files.
 */

#ifndef _PFCTL_DARWIN_H
#define _PFCTL_DARWIN_H

#include <sys/types.h>
#include <sys/queue.h>

/* OpenBSD's SIMPLEQ is STAILQ elsewhere. */
#define SIMPLEQ_HEAD			STAILQ_HEAD
#define SIMPLEQ_HEAD_INITIALIZER	STAILQ_HEAD_INITIALIZER
#define SIMPLEQ_ENTRY			STAILQ_ENTRY
#define SIMPLEQ_INIT			STAILQ_INIT
#define SIMPLEQ_FIRST			STAILQ_FIRST
#define SIMPLEQ_NEXT			STAILQ_NEXT
#define SIMPLEQ_EMPTY			STAILQ_EMPTY
#define SIMPLEQ_FOREACH			STAILQ_FOREACH
#define SIMPLEQ_INSERT_HEAD		STAILQ_INSERT_HEAD
#define SIMPLEQ_INSERT_TAIL		STAILQ_INSERT_TAIL
#define SIMPLEQ_REMOVE_HEAD		STAILQ_REMOVE_HEAD

#define SIMPLEQ_END(head)		NULL

/* Darwin has one routing table: "rtable" takes only 0. */
#define RT_TABLEID_MAX			0

/* OpenBSD's TAILQ_END. */
#ifndef TAILQ_END
#define TAILQ_END(head)			NULL
#endif

#endif /* !_PFCTL_DARWIN_H */
