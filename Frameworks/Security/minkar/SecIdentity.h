/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Identities: a certificate with its private key, for client
 * authentication. Identity preferences (which identity to offer to a
 * server) are not stored yet: none is preferred.
 */

#ifndef _SECURITY_SECIDENTITY_H_
#define _SECURITY_SECIDENTITY_H_

#include "SecBase.h"

#ifdef __cplusplus
extern "C" {
#endif

CF_IMPLICIT_BRIDGING_ENABLED

CFTypeID SecIdentityGetTypeID(void);
OSStatus SecIdentityCopyCertificate(SecIdentityRef identityRef, SecCertificateRef CF_RETURNS_RETAINED *certificateRef);
OSStatus SecIdentityCopyPrivateKey(SecIdentityRef identityRef, SecKeyRef CF_RETURNS_RETAINED *privateKeyRef);
SecIdentityRef _Nullable SecIdentityCopyPreferred(CFStringRef name, CFArrayRef _Nullable keyUsage,
                                                  CFArrayRef _Nullable validIssuers);

CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
