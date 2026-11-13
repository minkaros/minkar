/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The keys of a file's quarantine properties (NSURLQuarantinePropertiesKey,
 * kCFURLQuarantinePropertiesKey): who downloaded it, from where, how.
 */

#ifndef __LSQUARANTINE__
#define __LSQUARANTINE__

#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

CF_EXPORT const CFStringRef kLSQuarantineAgentNameKey;
CF_EXPORT const CFStringRef kLSQuarantineAgentBundleIdentifierKey;
CF_EXPORT const CFStringRef kLSQuarantineTimeStampKey;
CF_EXPORT const CFStringRef kLSQuarantineTypeKey;
CF_EXPORT const CFStringRef kLSQuarantineDataURLKey;
CF_EXPORT const CFStringRef kLSQuarantineOriginURLKey;

/* kLSQuarantineTypeKey's values. */
CF_EXPORT const CFStringRef kLSQuarantineTypeWebDownload;
CF_EXPORT const CFStringRef kLSQuarantineTypeOtherDownload;
CF_EXPORT const CFStringRef kLSQuarantineTypeEmailAttachment;
CF_EXPORT const CFStringRef kLSQuarantineTypeInstantMessageAttachment;
CF_EXPORT const CFStringRef kLSQuarantineTypeCalendarEventAttachment;
CF_EXPORT const CFStringRef kLSQuarantineTypeOtherAttachment;

CF_EXTERN_C_END

#endif
