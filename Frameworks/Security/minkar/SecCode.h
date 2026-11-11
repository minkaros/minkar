/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Code signing. Minkar does not check code signatures yet: code on disk
 * and running code are reported as not signed (errSecCSUnsigned), and
 * requirements are kept as their text.
 */

#ifndef _SECURITY_SECCODE_H_
#define _SECURITY_SECCODE_H_

#include "SecBase.h"

#ifdef __cplusplus
extern "C" {
#endif

CF_IMPLICIT_BRIDGING_ENABLED

typedef uint32_t SecCSFlags;
enum {
    kSecCSDefaultFlags = 0,
    kSecCSConsiderExpiration = 1 << 31,
    kSecCSEnforceRevocationChecks = 1 << 30,
    kSecCSNoNetworkAccess = 1 << 29,
    kSecCSReportProgress = 1 << 28,
    kSecCSCheckTrustedAnchors = 1 << 27,
    kSecCSQuickCheck = 1 << 26,
    kSecCSCheckAllArchitectures = 1 << 0,
    kSecCSDoNotValidateExecutable = 1 << 1,
    kSecCSDoNotValidateResources = 1 << 2,
    kSecCSBasicValidateOnly = kSecCSDoNotValidateExecutable | kSecCSDoNotValidateResources,
    kSecCSCheckNestedCode = 1 << 3,
    kSecCSStrictValidate = 1 << 4,
    kSecCSInternalInformation = 1 << 0,
    kSecCSSigningInformation = 1 << 1,
    kSecCSRequirementInformation = 1 << 2,
    kSecCSDynamicInformation = 1 << 3,
    kSecCSContentInformation = 1 << 4
};

CF_EXPORT const CFStringRef kSecCodeInfoIdentifier;
CF_EXPORT const CFStringRef kSecCodeInfoTeamIdentifier;
CF_EXPORT const CFStringRef kSecCodeInfoMainExecutable;
CF_EXPORT const CFStringRef kSecCodeInfoCertificates;
CF_EXPORT const CFStringRef kSecCodeInfoFlags;
CF_EXPORT const CFStringRef kSecCodeInfoFormat;
CF_EXPORT const CFStringRef kSecCodeInfoPList;
CF_EXPORT const CFStringRef kSecCodeInfoUnique;
CF_EXPORT const CFStringRef kSecCodeInfoDesignatedRequirement;
CF_EXPORT const CFStringRef kSecCodeInfoEntitlementsDict;
CF_EXPORT const CFStringRef kSecGuestAttributePid;

CFTypeID SecCodeGetTypeID(void);
CFTypeID SecStaticCodeGetTypeID(void);
CFTypeID SecRequirementGetTypeID(void);

OSStatus SecCodeCopySelf(SecCSFlags flags, SecCodeRef *self);
OSStatus SecCodeCopyGuestWithAttributes(SecCodeRef host, CFDictionaryRef attributes, SecCSFlags flags, SecCodeRef *guest);
OSStatus SecCodeCopyStaticCode(SecCodeRef code, SecCSFlags flags, SecStaticCodeRef *staticCode);
OSStatus SecCodeCopyPath(SecStaticCodeRef staticCode, SecCSFlags flags, CFURLRef *path);
OSStatus SecCodeCopySigningInformation(SecStaticCodeRef code, SecCSFlags flags, CFDictionaryRef *information);
OSStatus SecCodeCopyDesignatedRequirement(SecStaticCodeRef code, SecCSFlags flags, SecRequirementRef *requirement);
OSStatus SecCodeCheckValidity(SecCodeRef code, SecCSFlags flags, SecRequirementRef requirement);
OSStatus SecCodeCheckValidityWithErrors(SecCodeRef code, SecCSFlags flags, SecRequirementRef requirement, CFErrorRef *errors);

OSStatus SecStaticCodeCreateWithPath(CFURLRef path, SecCSFlags flags, SecStaticCodeRef *staticCode);
OSStatus SecStaticCodeCheckValidity(SecStaticCodeRef staticCode, SecCSFlags flags, SecRequirementRef requirement);
OSStatus SecStaticCodeCheckValidityWithErrors(SecStaticCodeRef staticCode, SecCSFlags flags, SecRequirementRef requirement,
                                              CFErrorRef *errors);

OSStatus SecRequirementCreateWithString(CFStringRef text, SecCSFlags flags, SecRequirementRef *requirement);
OSStatus SecRequirementCreateWithStringAndErrors(CFStringRef text, SecCSFlags flags, CFErrorRef *errors, SecRequirementRef *requirement);
OSStatus SecRequirementCopyString(SecRequirementRef requirement, SecCSFlags flags, CFStringRef *text);


CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
