/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * CoreMedia's own: the attachments every bearer of its types keeps
 * (right after its CF header), and the registration of those types.
 */

#ifndef __CMINTERNAL__
#define __CMINTERNAL__

#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>
#include "CMAttachment.h"

typedef struct {
    pthread_mutex_t lock;
    CFMutableDictionaryRef propagated;
    CFMutableDictionaryRef unpropagated;
} CMAttachmentStore;

/* A CoreMedia object that bears attachments: its store follows its CF
   header. */
typedef struct {
    CFRuntimeBase base;
    CMAttachmentStore attachments;
} CMBearerHeader;

__attribute__((visibility("hidden"))) void _CMAttachmentStoreInit(CMAttachmentStore *store);
__attribute__((visibility("hidden"))) void _CMAttachmentStoreFinalize(CMAttachmentStore *store);
/* A type whose objects begin with a CMBearerHeader. */
__attribute__((visibility("hidden"))) void _CMRegisterBearerType(CFTypeID type);

#endif
