/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Seconds since 1 January 1904 and CFAbsoluteTime (since 1 January
 * 2001, UTC). The UInt32 seconds and LongDateTime count local time, in
 * the system time zone at that moment; UTCDateTime counts UTC, its
 * fraction in 1/65536 s (as the reference system converts them,
 * checked against it on 16 Nov 2026).
 */

#include <math.h>
#include "DateTimeUtils.h"
#include "MacErrors.h"

/* Seconds from 1904 to 2001. */
static const double kEpochGap = 3061152000.0;

/* The local time zone's offset from UTC at abs, in seconds. */
static double
local_offset(CFAbsoluteTime abs)
{
	CFTimeZoneRef tz = CFTimeZoneCopySystem();
	double off = 0;

	if (tz != NULL) {
		off = CFTimeZoneGetSecondsFromGMT(tz, abs);
		CFRelease(tz);
	}
	return off;
}

/* Local seconds since 1904 to an absolute time: the offset taken at the
 * moment itself (a guess first, then corrected, around a change). */
static CFAbsoluteTime
from_local(double local)
{
	CFAbsoluteTime guess = local - kEpochGap;

	guess -= local_offset(guess);
	return local - kEpochGap - local_offset(guess);
}

static double
to_local(CFAbsoluteTime abs)
{
	return abs + kEpochGap + local_offset(abs);
}

OSStatus
UCConvertSecondsToCFAbsoluteTime(UInt32 iSeconds, CFAbsoluteTime *oCFTime)
{
	if (oCFTime == NULL)
		return paramErr;
	*oCFTime = from_local((double)iSeconds);
	return noErr;
}

OSStatus
UCConvertCFAbsoluteTimeToSeconds(CFAbsoluteTime iCFTime, UInt32 *oSeconds)
{
	double s;

	if (oSeconds == NULL)
		return paramErr;
	s = floor(to_local(iCFTime));
	if (s < 0 || s > 4294967295.0)
		return kUTCOverflowErr;
	*oSeconds = (UInt32)s;
	return noErr;
}

OSStatus
UCConvertLongDateTimeToCFAbsoluteTime(LongDateTime iLongTime, CFAbsoluteTime *oCFTime)
{
	if (oCFTime == NULL)
		return paramErr;
	*oCFTime = from_local((double)iLongTime);
	return noErr;
}

OSStatus
UCConvertCFAbsoluteTimeToLongDateTime(CFAbsoluteTime iCFTime, LongDateTime *oLongDate)
{
	if (oLongDate == NULL)
		return paramErr;
	*oLongDate = (LongDateTime)floor(to_local(iCFTime));
	return noErr;
}

OSStatus
UCConvertUTCDateTimeToCFAbsoluteTime(const UTCDateTime *iUTCDate, CFAbsoluteTime *oCFTime)
{
	double s;

	if (iUTCDate == NULL || oCFTime == NULL)
		return paramErr;
	s = (double)iUTCDate->highSeconds * 4294967296.0 + iUTCDate->lowSeconds;
	*oCFTime = s - kEpochGap + iUTCDate->fraction / 65536.0;
	return noErr;
}

OSStatus
UCConvertCFAbsoluteTimeToUTCDateTime(CFAbsoluteTime iCFTime, UTCDateTime *oUTCDate)
{
	double s, whole;

	if (oUTCDate == NULL)
		return paramErr;
	s = iCFTime + kEpochGap;
	if (s < 0 || s >= 281474976710656.0)	/* 48 bits of seconds */
		return kUTCOverflowErr;
	whole = floor(s);
	oUTCDate->highSeconds = (UInt16)(whole / 4294967296.0);
	oUTCDate->lowSeconds = (UInt32)fmod(whole, 4294967296.0);
	oUTCDate->fraction = (UInt16)((s - whole) * 65536.0);
	return noErr;
}
