/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Host time: the clock audio time stamps use (mach_absolute_time's).
 */

#ifndef __HOSTTIME__
#define __HOSTTIME__

#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

UInt64 AudioGetCurrentHostTime(void);
Float64 AudioGetHostClockFrequency(void);
UInt32 AudioGetHostClockMinimumTimeDelta(void);
UInt64 AudioConvertHostTimeToNanos(UInt64 inHostTime);
UInt64 AudioConvertNanosToHostTime(UInt64 inNanos);

#ifdef __cplusplus
}
#endif

#endif
