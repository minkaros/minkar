/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Values as in the reference platform's open-source Security (APSL):
 * policies are named by object identifiers under 1.2.840.113635.100.1.
 */

#include "Security.h"
#include "SecInternal.h"

const CFStringRef kSecPolicyOid = CFSTR("SecPolicyOid");
const CFStringRef kSecPolicyName = CFSTR("SecPolicyName");
const CFStringRef kSecPolicyClient = CFSTR("SecPolicyClient");

const CFStringRef kSecPolicyAppleX509Basic = CFSTR("1.2.840.113635.100.1.2");
const CFStringRef kSecPolicyAppleSSL = CFSTR("1.2.840.113635.100.1.3");

/* A policy: its identifier, and for SSL whether it checks a server and
   which name. */
struct __SecPolicy {
    CFRuntimeBase base;
    CFStringRef oid;
    Boolean server;
    CFStringRef hostname;
};

static void policyFinalize(CFTypeRef cf) {
    struct __SecPolicy *p = (struct __SecPolicy *)cf;
    if (p->oid) CFRelease(p->oid);
    if (p->hostname) CFRelease(p->hostname);
}

SEC_TYPE(SecPolicy, "SecPolicy", policyFinalize)

static SecPolicyRef createPolicy(CFStringRef oid, Boolean server, CFStringRef hostname) {
    struct __SecPolicy *p = _SecCreateInstance(SecPolicyGetTypeID(), sizeof(struct __SecPolicy));
    if (p == NULL)
        return NULL;
    p->oid = CFRetain(oid);
    p->server = server;
    p->hostname = hostname != NULL ? CFStringCreateCopy(NULL, hostname) : NULL;
    return p;
}

SecPolicyRef SecPolicyCreateBasicX509(void) {
    return createPolicy(kSecPolicyAppleX509Basic, false, NULL);
}

SecPolicyRef SecPolicyCreateSSL(Boolean server, CFStringRef hostname) {
    return createPolicy(kSecPolicyAppleSSL, server, hostname);
}

CFDictionaryRef SecPolicyCopyProperties(SecPolicyRef policyRef) {
    struct __SecPolicy *p = (struct __SecPolicy *)policyRef;
    CFMutableDictionaryRef d;
    if (p == NULL)
        return NULL;
    d = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(d, kSecPolicyOid, p->oid);
    if (p->hostname)
        CFDictionarySetValue(d, kSecPolicyName, p->hostname);
    if (CFEqual(p->oid, kSecPolicyAppleSSL))
        CFDictionarySetValue(d, kSecPolicyClient, p->server ? kCFBooleanFalse : kCFBooleanTrue);
    return d;
}

Boolean _SecPolicyIsSSL(SecPolicyRef policy) {
    return policy != NULL && CFEqual(((struct __SecPolicy *)policy)->oid, kSecPolicyAppleSSL);
}

Boolean _SecPolicyIsServer(SecPolicyRef policy) {
    return policy != NULL && ((struct __SecPolicy *)policy)->server;
}

CFStringRef _SecPolicyGetHostname(SecPolicyRef policy) {
    return policy != NULL ? ((struct __SecPolicy *)policy)->hostname : NULL;
}
