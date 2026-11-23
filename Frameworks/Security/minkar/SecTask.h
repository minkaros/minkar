/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef _SECURITY_SECTASK_H_
#define _SECURITY_SECTASK_H_

#include <CoreFoundation/CoreFoundation.h>
#include <mach/message.h>

CF_EXTERN_C_BEGIN

/* A running process, as its entitlements' holder. */
typedef struct CF_BRIDGED_TYPE(id) __SecTask *SecTaskRef;

CFTypeID SecTaskGetTypeID(void);
SecTaskRef SecTaskCreateWithAuditToken(CFAllocatorRef allocator, audit_token_t token);
SecTaskRef SecTaskCreateFromSelf(CFAllocatorRef allocator);
CFTypeRef SecTaskCopyValueForEntitlement(SecTaskRef task, CFStringRef entitlement, CFErrorRef *error);
CFDictionaryRef SecTaskCopyValuesForEntitlements(SecTaskRef task, CFArrayRef entitlements, CFErrorRef *error);
CFStringRef SecTaskCopySigningIdentifier(SecTaskRef task, CFErrorRef *error);
uint32_t SecTaskGetCodeSignStatus(SecTaskRef task);

CF_EXTERN_C_END

#endif
