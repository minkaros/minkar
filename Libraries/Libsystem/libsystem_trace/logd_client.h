/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef _LOGD_CLIENT_H_
#define _LOGD_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>
#include <os/log.h>

/* One message to logd; false when there is no logd to take it. */
bool _logd_send(const char *subsystem, const char *category, os_log_type_t type,
    uint64_t activity, const char *text);
/* A subsystem's LOGD_LEVEL_*, or -1 when logd does not answer. */
int _logd_level(const char *subsystem);

/* Whether a subsystem keeps messages of this type (info and debug by its
 * level; the others always). */
bool _logd_keeps(const char *subsystem, os_log_type_t type);

#endif
