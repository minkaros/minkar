/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef __CMSAMPLEBUFFER__
#define __CMSAMPLEBUFFER__

#include <CoreMedia/CMTime.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

typedef struct {
    CMTime duration;
    CMTime presentationTimeStamp;
    CMTime decodeTimeStamp;
} CMSampleTimingInfo;

CM_EXPORT const CMSampleTimingInfo kCMTimingInfoInvalid;

/* A sample's attachments (CMSampleBufferGetSampleAttachmentsArray). */
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_NotSync;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_PartialSync;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_HasRedundantCoding;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_IsDependedOnByOthers;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_DependsOnOthers;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_EarlierDisplayTimesAllowed;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_DisplayImmediately;
CM_EXPORT const CFStringRef kCMSampleAttachmentKey_DoNotDisplay;

#endif
