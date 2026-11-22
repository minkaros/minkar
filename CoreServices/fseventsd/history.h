/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * fseventsd's history on the volumes (history.c). Not thread-safe: the
 * callers hold fseventsd's lock, except for a replay, which only reads.
 */

#ifndef FSEVENTSD_HISTORY_H
#define FSEVENTSD_HISTORY_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* An event kept on its volume. */
void historyAppend(uint64_t id, uint32_t flags, const char *path);
/* What was appended, on the disks and the files closed (after each
 * batch of events, and before a replay reads them). */
void historyFlush(void);
/* The files of volumes no longer mounted, closed. */
void historyForgetUnmounted(void);
/* Whether a path is a history's own (its writes are not events). */
int historyIsOwnPath(const char *path);

/* The mount point of a path's volume, or of a device's. */
int historyMountOf(const char *path, char *mount, size_t length);
int historyMountOfDevice(dev_t device, char *mount, size_t length);

/* A volume's events after since and up to upto, oldest first, to fn
 * with absolute paths; stops when fn returns nonzero. */
int historyReplay(const char *mount, uint64_t since, uint64_t upto,
    int (*fn)(void *context, uint64_t id, uint32_t flags, const char *path), void *context);
/* The first event ID the volume still has, or 0 if it has none. */
uint64_t historyOldest(const char *mount);
/* The volume's history UUID (0), or -1 when it keeps none. */
int historyUUID(const char *mount, char uuid[37]);
/* The last event before a CFAbsoluteTime, or 0. */
uint64_t historyLastBefore(const char *mount, double absoluteTime);
/* The events up to an ID forgotten (whole files); -1 with no history. */
int historyPurge(const char *mount, uint64_t upto);

#endif
