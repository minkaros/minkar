/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SecTask: a process (its audit token, or this one) as the holder of
 * entitlements, for daemons that check their clients (SCHelper, ...).
 *
 * SHORTCUT(19.6.0): code signatures are not read yet (Code.c), so no
 * process holds an entitlement: every value is absent, which callers
 * take as "not entitled" and fall back to an authorization. The static
 * trust cache (the kernel entitlements decision) will give them.
 */

#include "Security.h"
#include "SecInternal.h"
#include "SecTask.h"
#include <unistd.h>

struct __SecTask {
    CFRuntimeBase base;
    audit_token_t token;
    pid_t pid;
};

SEC_TYPE(SecTask, "SecTask", NULL)

static SecTaskRef createTask(audit_token_t token, pid_t pid) {
    struct __SecTask *t = _SecCreateInstance(SecTaskGetTypeID(), sizeof(struct __SecTask));
    if (t == NULL)
        return NULL;
    t->token = token;
    t->pid = pid;
    return t;
}

SecTaskRef SecTaskCreateWithAuditToken(CFAllocatorRef allocator, audit_token_t token) {
    return createTask(token, (pid_t)token.val[5]);   /* the audit token's pid */
}

SecTaskRef SecTaskCreateFromSelf(CFAllocatorRef allocator) {
    audit_token_t token;
    memset(&token, 0, sizeof(token));
    return createTask(token, getpid());
}

CFTypeRef SecTaskCopyValueForEntitlement(SecTaskRef task, CFStringRef entitlement, CFErrorRef *error) {
    if (error != NULL)
        *error = NULL;
    return NULL;
}

CFDictionaryRef SecTaskCopyValuesForEntitlements(SecTaskRef task, CFArrayRef entitlements, CFErrorRef *error) {
    if (error != NULL)
        *error = NULL;
    return CFDictionaryCreate(NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

CFStringRef SecTaskCopySigningIdentifier(SecTaskRef task, CFErrorRef *error) {
    if (error != NULL)
        *error = NULL;
    return NULL;
}

uint32_t SecTaskGetCodeSignStatus(SecTaskRef task) {
    return 0;
}
