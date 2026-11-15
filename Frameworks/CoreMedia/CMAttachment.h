/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Attachments: values kept on a sample buffer or a CoreVideo buffer by
 * key, each passed on to what is made from it, or not.
 */

#ifndef __CMATTACHMENT__
#define __CMATTACHMENT__

#include <CoreMedia/CMBase.h>

typedef CFTypeRef CMAttachmentBearerRef;

typedef CF_ENUM(uint32_t, CMAttachmentMode) {
    kCMAttachmentMode_ShouldNotPropagate = 0,
    kCMAttachmentMode_ShouldPropagate = 1,
};

CM_EXPORT void CMSetAttachment(CMAttachmentBearerRef target, CFStringRef key, CFTypeRef value,
                               CMAttachmentMode attachmentMode);
CM_EXPORT CFTypeRef CMGetAttachment(CMAttachmentBearerRef target, CFStringRef key, CMAttachmentMode *attachmentModeOut);
CM_EXPORT void CMRemoveAttachment(CMAttachmentBearerRef target, CFStringRef key);
CM_EXPORT void CMRemoveAllAttachments(CMAttachmentBearerRef target);
CM_EXPORT CFDictionaryRef CMCopyDictionaryOfAttachments(CFAllocatorRef allocator, CMAttachmentBearerRef target,
                                                        CMAttachmentMode attachmentMode);
CM_EXPORT void CMSetAttachments(CMAttachmentBearerRef target, CFDictionaryRef theAttachments,
                                CMAttachmentMode attachmentMode);
CM_EXPORT void CMPropagateAttachments(CMAttachmentBearerRef source, CMAttachmentBearerRef destination);

#endif
