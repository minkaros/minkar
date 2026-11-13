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

#endif
