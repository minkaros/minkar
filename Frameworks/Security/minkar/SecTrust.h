/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Trust: whether a certificate chain is to be trusted for a use (the
 * policies), built to an anchor (the system's, or the app's own).
 */

#ifndef _SECURITY_SECTRUST_H_
#define _SECURITY_SECTRUST_H_

#include "SecBase.h"

CF_ASSUME_NONNULL_BEGIN

typedef uint32_t SecTrustResultType;
enum {
    kSecTrustResultInvalid = 0,
    kSecTrustResultProceed = 1,                 /* the user said so (exceptions) */
    kSecTrustResultConfirm CF_ENUM_DEPRECATED(10_0, 10_9, 2_0, 7_0) = 2,
    kSecTrustResultDeny = 3,
    kSecTrustResultUnspecified = 4,             /* trusted by its anchors */
    kSecTrustResultRecoverableTrustFailure = 5,
    kSecTrustResultFatalTrustFailure = 6,
    kSecTrustResultOtherError = 7
};

CF_EXPORT CFTypeID SecTrustGetTypeID(void);
/* certificates: one, or an array with the leaf first; policies: one or
   an array (nil: the basic X.509 checks). */
CF_EXPORT OSStatus SecTrustCreateWithCertificates(CFTypeRef certificates, CFTypeRef _Nullable policies,
                                                  SecTrustRef CF_RETURNS_RETAINED _Nullable *_Nonnull trust);
CF_EXPORT OSStatus SecTrustSetPolicies(SecTrustRef trust, CFTypeRef policies);
CF_EXPORT OSStatus SecTrustCopyPolicies(SecTrustRef trust, CFArrayRef CF_RETURNS_RETAINED _Nullable *_Nonnull policies);
/* Anchors of the app's own: only these, unless told to take the system's
   too (SecTrustSetAnchorCertificatesOnly(trust, false)). */
CF_EXPORT OSStatus SecTrustSetAnchorCertificates(SecTrustRef trust, CFArrayRef _Nullable anchorCertificates);
CF_EXPORT OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef trust, Boolean anchorCertificatesOnly);
CF_EXPORT OSStatus SecTrustCopyCustomAnchorCertificates(SecTrustRef trust,
                                                        CFArrayRef CF_RETURNS_RETAINED _Nullable *_Nonnull anchors);
CF_EXPORT OSStatus SecTrustSetVerifyDate(SecTrustRef trust, CFDateRef verifyDate);
CF_EXPORT OSStatus SecTrustSetNetworkFetchAllowed(SecTrustRef trust, Boolean allowFetch);
CF_EXPORT OSStatus SecTrustGetNetworkFetchAllowed(SecTrustRef trust, Boolean *allowFetch);
CF_EXPORT CFAbsoluteTime SecTrustGetVerifyTime(SecTrustRef trust);

CF_EXPORT bool SecTrustEvaluateWithError(SecTrustRef trust, CFErrorRef _Nullable *_Nullable CF_RETURNS_RETAINED error)
    API_AVAILABLE(macos(10.14));
CF_EXPORT OSStatus SecTrustEvaluate(SecTrustRef trust, SecTrustResultType *result)
    API_DEPRECATED_WITH_REPLACEMENT("SecTrustEvaluateWithError", macos(10.3, 10.15));
/* Evaluated first if it was not. */
CF_EXPORT OSStatus SecTrustGetTrustResult(SecTrustRef trust, SecTrustResultType *result);

/* The certificates: the chain built once evaluated and trusted, else
   those given. */
CF_EXPORT CFIndex SecTrustGetCertificateCount(SecTrustRef trust);
CF_EXPORT SecCertificateRef _Nullable SecTrustGetCertificateAtIndex(SecTrustRef trust, CFIndex ix);
CF_EXPORT SecKeyRef _Nullable SecTrustCopyPublicKey(SecTrustRef trust);

/* What failed, to be accepted the next time for this certificate (a
   user's "trust it anyway"); other failures still fail. */
CF_EXPORT CFDataRef _Nullable SecTrustCopyExceptions(SecTrustRef trust);
CF_EXPORT bool SecTrustSetExceptions(SecTrustRef trust, CFDataRef _Nullable exceptions);

CF_ASSUME_NONNULL_END

#endif
