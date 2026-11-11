/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Transforms: a read stream, digests and signature verification, joined
 * into a chain and run in one go (SecTransformExecute). Each transform
 * reads its input attribute and gives its output attribute to the next.
 */

#ifndef _SECURITY_SECTRANSFORM_H_
#define _SECURITY_SECTRANSFORM_H_

#include "SecBase.h"

#ifdef __cplusplus
extern "C" {
#endif

CF_IMPLICIT_BRIDGING_ENABLED

typedef CFTypeRef SecTransformRef;
typedef CFTypeRef SecGroupTransformRef;

CF_EXPORT const CFStringRef kSecTransformErrorDomain;
CF_EXPORT const CFStringRef kSecTransformInputAttributeName;
CF_EXPORT const CFStringRef kSecTransformOutputAttributeName;
CF_EXPORT const CFStringRef kSecTransformDebugAttributeName;
CF_EXPORT const CFStringRef kSecTransformTransformName;
CF_EXPORT const CFStringRef kSecTransformAbortAttributeName;

enum {
    kSecTransformErrorAttributeNotFound = 1,
    kSecTransformErrorInvalidOperation = 2,
    kSecTransformErrorNotInitializedCorrectly = 3,
    kSecTransformErrorMoreThanOneOutput = 4,
    kSecTransformErrorInvalidInputDictionary = 5,
    kSecTransformErrorInvalidAlgorithm = 6,
    kSecTransformErrorInvalidLength = 7,
    kSecTransformErrorInvalidType = 8,
    kSecTransformErrorInvalidInput = 10,
    kSecTransformErrorNameAlreadyRegistered = 11,
    kSecTransformErrorUnsupportedAttribute = 12,
    kSecTransformOperationNotSupportedOnGroup = 13,
    kSecTransformErrorMissingParameter = 14,
    kSecTransformErrorInvalidConnection = 15,
    kSecTransformTransformIsExecuting = 16,
    kSecTransformInvalidOverride = 17,
    kSecTransformTransformIsNotRegistered = 18,
    kSecTransformErrorAbortInProgress = 19,
    kSecTransformErrorAborted = 20,
    kSecTransformInvalidArgument = 21
};

CFTypeID SecTransformGetTypeID(void);
CFTypeID SecGroupTransformGetTypeID(void);
SecGroupTransformRef SecTransformCreateGroupTransform(void);
SecGroupTransformRef SecTransformConnectTransforms(SecTransformRef sourceTransformRef, CFStringRef sourceAttributeName,
                                                   SecTransformRef destinationTransformRef, CFStringRef destinationAttributeName,
                                                   SecGroupTransformRef group, CFErrorRef *error);
Boolean SecTransformSetAttribute(SecTransformRef transformRef, CFStringRef key, CFTypeRef value, CFErrorRef *error);
CFTypeRef SecTransformGetAttribute(SecTransformRef transformRef, CFStringRef key);
CFTypeRef SecTransformExecute(SecTransformRef transformRef, CFErrorRef *errorRef);

/* Reading a stream. */
SecTransformRef SecTransformCreateReadTransformWithReadStream(CFReadStreamRef inputStream);

/* Digests. */
CF_EXPORT const CFStringRef kSecDigestMD5;
CF_EXPORT const CFStringRef kSecDigestSHA1;
CF_EXPORT const CFStringRef kSecDigestSHA2;
CF_EXPORT const CFStringRef kSecDigestTypeAttribute;
CF_EXPORT const CFStringRef kSecDigestLengthAttribute;
SecTransformRef SecDigestTransformCreate(CFTypeRef digestType, CFIndex digestLength, CFErrorRef *error);

/* Signature verification: the input is the signed data (or its digest,
   with kSecInputIsAttributeName set to kSecInputIsDigest); the output is
   kCFBooleanTrue or kCFBooleanFalse. */
CF_EXPORT const CFStringRef kSecKeyAttributeName;
CF_EXPORT const CFStringRef kSecSignatureAttributeName;
CF_EXPORT const CFStringRef kSecInputIsAttributeName;
CF_EXPORT const CFStringRef kSecInputIsPlainText;
CF_EXPORT const CFStringRef kSecInputIsDigest;
CF_EXPORT const CFStringRef kSecInputIsRaw;
SecTransformRef SecVerifyTransformCreate(SecKeyRef key, CFDataRef signature, CFErrorRef *error);


CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
