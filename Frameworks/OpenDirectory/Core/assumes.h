/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * What OpenDirectory's sources take from the system's assumes.h: an
 * expression that should hold, its value given back and a log line when
 * it does not. Minkar's own.
 */

#ifndef OPENDIRECTORY_ASSUMES_H
#define OPENDIRECTORY_ASSUMES_H

#include <os/log.h>

#define osx_assumes(e) ({ \
	__typeof__(e) _osx_v = (e); \
	if (!_osx_v) \
		os_log_error(OS_LOG_DEFAULT, "OpenDirectory: %s failed (%s:%d)", #e, __FILE__, __LINE__); \
	_osx_v; })

#define osx_assumes_zero(e) ({ \
	__typeof__(e) _osx_v = (e); \
	if (_osx_v != 0) \
		os_log_error(OS_LOG_DEFAULT, "OpenDirectory: %s is not zero (%s:%d)", #e, __FILE__, __LINE__); \
	_osx_v; })

#endif
