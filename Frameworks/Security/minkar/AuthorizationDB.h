/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The rights database: each right's definition, kept in
 * /etc/authorization.plist (changed by administrators only).
 */

#ifndef _SECURITY_AUTHORIZATIONDB_H_
#define _SECURITY_AUTHORIZATIONDB_H_

#include "Authorization.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kAuthorizationRuleClassAllow "allow"
#define kAuthorizationRuleClassDeny "deny"
#define kAuthorizationRightRule "rule"
#define kAuthorizationRuleIsAdmin "is-admin"
#define kAuthorizationRuleAuthenticateAsSessionUser "authenticate-session-owner"
#define kAuthorizationRuleAuthenticateAsAdmin "authenticate-admin"
#define kAuthorizationComment "comment"

OSStatus AuthorizationRightGet(const char *rightName, CFDictionaryRef *rightDefinition);
OSStatus AuthorizationRightSet(AuthorizationRef authRef, const char *rightName, CFTypeRef rightDefinition,
                               CFStringRef descriptionKey, CFBundleRef bundle, CFStringRef localeTableName);
OSStatus AuthorizationRightRemove(AuthorizationRef authRef, const char *rightName);

#ifdef __cplusplus
}
#endif

#endif
