/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Trust policies: what a certificate chain is checked against (SSL server
 * names, code signing, ...), named by object identifiers.
 */

#ifndef _SECURITY_SECPOLICY_H_
#define _SECURITY_SECPOLICY_H_

#include "SecBase.h"

/* A policy's properties (SecPolicyCopyProperties). */
CF_EXPORT const CFStringRef kSecPolicyOid;
CF_EXPORT const CFStringRef kSecPolicyName;
CF_EXPORT const CFStringRef kSecPolicyClient;

/* Policy identifiers (kSecPolicyOid's values). */
CF_EXPORT const CFStringRef kSecPolicyAppleX509Basic;
CF_EXPORT const CFStringRef kSecPolicyAppleSSL;

CF_EXPORT CFTypeID SecPolicyGetTypeID(void);
/* The basic checks: a chain to a trusted anchor, each certificate valid
   now. */
CF_EXPORT SecPolicyRef SecPolicyCreateBasicX509(void);
/* A TLS server's (server true) or client's: the basic checks, and for a
   server its name (hostname, if given) in the leaf's subject alternative
   names, serverAuth in its extended key usage, and a validity of at most
   825 days, as the reference requires of certificates issued since 1 July
   2019. */
CF_EXPORT SecPolicyRef SecPolicyCreateSSL(Boolean server, CFStringRef hostname);
CF_EXPORT CFDictionaryRef SecPolicyCopyProperties(SecPolicyRef policyRef);

#endif
