/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * launchd jobs from an application: submitted, looked up and removed, and
 * login items (a helper application inside the app's bundle).
 */

#ifndef __SERVICEMANAGEMENT__
#define __SERVICEMANAGEMENT__

#include <CoreFoundation/CoreFoundation.h>
#if __has_include(<Security/Authorization.h>)
#include <Security/Authorization.h>
#else
/* Security is not built yet: its authorization reference type. */
typedef const struct AuthorizationOpaqueRef *AuthorizationRef;
#endif

#ifdef __cplusplus
extern "C" {
#endif

CF_EXPORT const CFStringRef kSMDomainSystemLaunchd;
CF_EXPORT const CFStringRef kSMDomainUserLaunchd;
CF_EXPORT const CFStringRef kSMErrorDomainIPC;
CF_EXPORT const CFStringRef kSMErrorDomainFramework;
CF_EXPORT const CFStringRef kSMErrorDomainLaunchd;

enum {
    kSMErrorInternalFailure = 2,
    kSMErrorInvalidSignature,
    kSMErrorAuthorizationFailure,
    kSMErrorToolNotValid,
    kSMErrorJobNotFound,
    kSMErrorServiceUnavailable,
    kSMErrorJobPlistNotFound,
    kSMErrorJobMustBeEnabled,
    kSMErrorInvalidPlist
};

/* The job (its launchd dictionary), submitted to launchd. */
Boolean SMJobSubmit(CFStringRef domain, CFDictionaryRef job, AuthorizationRef auth, CFErrorRef *outError);
Boolean SMJobRemove(CFStringRef domain, CFStringRef jobLabel, AuthorizationRef auth, Boolean wait, CFErrorRef *outError);
CFDictionaryRef SMJobCopyDictionary(CFStringRef domain, CFStringRef jobLabel);
/* The helper in Contents/Library/LoginItems with this bundle identifier,
   started (as a launchd job of that label) or stopped. */
Boolean SMLoginItemSetEnabled(CFStringRef identifier, Boolean enabled);

#ifdef __cplusplus
}
#endif

#ifdef __OBJC__
#import <ServiceManagement/SMAppService.h>
#endif

#endif
