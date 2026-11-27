/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Trust settings: a user's or an administrator's own trust in particular
 * certificates. None are stored yet, in any domain: every certificate is
 * trusted only as its chain to the system's anchors makes it.
 */

#ifndef _SECURITY_SECTRUSTSETTINGS_H_
#define _SECURITY_SECTRUSTSETTINGS_H_

#include "SecBase.h"

#ifdef __cplusplus
extern "C" {
#endif

CF_IMPLICIT_BRIDGING_ENABLED

typedef CF_ENUM(uint32_t, SecTrustSettingsDomain) {
    kSecTrustSettingsDomainUser = 0,
    kSecTrustSettingsDomainAdmin,
    kSecTrustSettingsDomainSystem
};

OSStatus SecTrustSettingsCopyTrustSettings(SecCertificateRef certRef, SecTrustSettingsDomain domain,
                                           CFArrayRef CF_RETURNS_RETAINED _Nullable *_Nonnull trustSettings);
OSStatus SecTrustSettingsCopyCertificates(SecTrustSettingsDomain domain,
                                          CFArrayRef CF_RETURNS_RETAINED _Nullable *_Nullable certArray);

CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
