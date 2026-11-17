/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Date and time conversions between the classic seconds-since-1904
 * forms and CFAbsoluteTime. Seconds and LongDateTime count local time;
 * UTCDateTime counts UTC, with a 1/65536 s fraction.
 */

#ifndef _CORESERVICES_DATETIMEUTILS_H_
#define _CORESERVICES_DATETIMEUTILS_H_

#include <MacTypes.h>
#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 2)
struct UTCDateTime {
	UInt16	highSeconds;
	UInt32	lowSeconds;
	UInt16	fraction;
};
#pragma pack(pop)
typedef struct UTCDateTime UTCDateTime;
typedef UTCDateTime *UTCDateTimePtr;
typedef UTCDateTimePtr *UTCDateTimeHandle;
typedef UTCDateTime LocalDateTime;
typedef LocalDateTime *LocalDateTimePtr;
typedef SInt64 LongDateTime;

OSStatus UCConvertSecondsToCFAbsoluteTime(UInt32 iSeconds, CFAbsoluteTime *oCFTime);
OSStatus UCConvertCFAbsoluteTimeToSeconds(CFAbsoluteTime iCFTime, UInt32 *oSeconds);
OSStatus UCConvertLongDateTimeToCFAbsoluteTime(LongDateTime iLongTime, CFAbsoluteTime *oCFTime);
OSStatus UCConvertCFAbsoluteTimeToLongDateTime(CFAbsoluteTime iCFTime, LongDateTime *oLongDate);
OSStatus UCConvertUTCDateTimeToCFAbsoluteTime(const UTCDateTime *iUTCDate, CFAbsoluteTime *oCFTime);
OSStatus UCConvertCFAbsoluteTimeToUTCDateTime(CFAbsoluteTime iCFTime, UTCDateTime *oUTCDate);

#ifdef __cplusplus
}
#endif

#endif
