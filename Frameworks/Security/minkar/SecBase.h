/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Security's types and result codes.
 */

#ifndef _SECURITY_SECBASE_H_
#define _SECURITY_SECBASE_H_

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __SecKeychain *SecKeychainRef;
typedef struct __SecKeychainItem *SecKeychainItemRef;
typedef struct __SecKeychainSearch *SecKeychainSearchRef;
typedef struct __SecCertificate *SecCertificateRef;
typedef struct __SecKey *SecKeyRef;
typedef struct __SecIdentity *SecIdentityRef;
typedef struct __SecAccess *SecAccessRef;
typedef struct __SecPolicy *SecPolicyRef;
typedef struct __SecTrust *SecTrustRef;
typedef struct __SecCode *SecCodeRef;
typedef struct __SecCode *SecStaticCodeRef;
typedef struct __SecRequirement *SecRequirementRef;

typedef OSType SecKeychainAttrType;
typedef FourCharCode SecItemClass;

typedef struct SecKeychainAttribute {
    SecKeychainAttrType tag;
    UInt32 length;
    void *data;
} SecKeychainAttribute;
typedef SecKeychainAttribute *SecKeychainAttributePtr;

typedef struct SecKeychainAttributeList {
    UInt32 count;
    SecKeychainAttribute *attr;
} SecKeychainAttributeList;

typedef struct SecKeychainAttributeInfo {
    UInt32 count;
    UInt32 *tag;
    UInt32 *format;
} SecKeychainAttributeInfo;

enum {
    errSecSuccess = 0,
    errSecUnimplemented = -4,
    errSecDiskFull = -34,
    errSecIO = -36,
    errSecParam = -50,
    errSecWrPerm = -61,
    errSecAllocate = -108,
    errSecUserCanceled = -128,
    errSecBadReq = -909,
    errSecNotAvailable = -25291,
    errSecReadOnly = -25292,
    errSecAuthFailed = -25293,
    errSecNoSuchKeychain = -25294,
    errSecInvalidKeychain = -25295,
    errSecDuplicateItem = -25299,
    errSecItemNotFound = -25300,
    errSecInteractionNotAllowed = -25308,
    errSecDecode = -26275,
    errSecVerifyFailed = -67808,
    errSecUnsupportedFormat = -25256,
    errSecUnknownFormat = -25257,
    errSecInvalidAlgorithm = -67747,
    errSecCSUnimplemented = -67072,
    errSecCSInvalidObjectRef = -67071,
    errSecCSUnsigned = -67062,
    errSecCSSignatureFailed = -67061,
    errSecCSReqFailed = -67050,
    errSecCSReqInvalid = -67052,
    errSecCSStaticCodeNotFound = -67068
};

/* A sentence for the result code. */
CFStringRef SecCopyErrorMessageString(OSStatus status, void *reserved);

#ifdef __cplusplus
}
#endif

#endif
