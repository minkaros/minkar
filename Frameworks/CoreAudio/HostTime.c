/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "HostTime.h"
#include <mach/mach_time.h>

static mach_timebase_info_data_t timebase(void) {
    static mach_timebase_info_data_t info;
    if (info.denom == 0)
        mach_timebase_info(&info);
    return info;
}

UInt64 AudioGetCurrentHostTime(void) {
    return mach_absolute_time();
}

Float64 AudioGetHostClockFrequency(void) {
    mach_timebase_info_data_t info = timebase();
    return 1.0e9 * info.denom / info.numer;
}

UInt32 AudioGetHostClockMinimumTimeDelta(void) {
    return 1;
}

UInt64 AudioConvertHostTimeToNanos(UInt64 hostTime) {
    mach_timebase_info_data_t info = timebase();
    return (UInt64)((__uint128_t)hostTime * info.numer / info.denom);
}

UInt64 AudioConvertNanosToHostTime(UInt64 nanos) {
    mach_timebase_info_data_t info = timebase();
    return (UInt64)((__uint128_t)nanos * info.denom / info.numer);
}
