/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Values as in the reference platform's open-source Security (APSL):
 * policies are named by object identifiers under 1.2.840.113635.100.1.
 */

#include "SecPolicy.h"

const CFStringRef kSecPolicyOid = CFSTR("SecPolicyOid");
const CFStringRef kSecPolicyName = CFSTR("SecPolicyName");
const CFStringRef kSecPolicyClient = CFSTR("SecPolicyClient");

const CFStringRef kSecPolicyAppleX509Basic = CFSTR("1.2.840.113635.100.1.2");
const CFStringRef kSecPolicyAppleSSL = CFSTR("1.2.840.113635.100.1.3");
