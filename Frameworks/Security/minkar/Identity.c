/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Identities (SecIdentity.h) and trust settings (SecTrustSettings.h).
 */

#include "Security.h"
#include "SecInternal.h"

/* ---- Identities ---- */

struct __SecIdentity {
    CFRuntimeBase base;
    SecCertificateRef certificate;
    SecKeyRef privateKey;
};

static void identityFinalize(CFTypeRef cf) {
    struct __SecIdentity *i = (struct __SecIdentity *)cf;
    if (i->certificate != NULL)
        CFRelease(i->certificate);
    if (i->privateKey != NULL)
        CFRelease(i->privateKey);
}

SEC_TYPE(SecIdentity, "SecIdentity", identityFinalize)

SecIdentityRef _SecIdentityCreate(SecCertificateRef certificate, SecKeyRef privateKey) {
    struct __SecIdentity *i;
    if (certificate == NULL || privateKey == NULL)
        return NULL;
    i = _SecCreateInstance(SecIdentityGetTypeID(), sizeof(struct __SecIdentity));
    if (i == NULL)
        return NULL;
    i->certificate = (SecCertificateRef)CFRetain(certificate);
    i->privateKey = (SecKeyRef)CFRetain(privateKey);
    return i;
}

static struct __SecIdentity *identityOf(SecIdentityRef identity) {
    if (identity == NULL || CFGetTypeID(identity) != SecIdentityGetTypeID())
        return NULL;
    return (struct __SecIdentity *)identity;
}

OSStatus SecIdentityCopyCertificate(SecIdentityRef identityRef, SecCertificateRef *certificateRef) {
    struct __SecIdentity *i = identityOf(identityRef);
    if (i == NULL || certificateRef == NULL)
        return errSecParam;
    *certificateRef = (SecCertificateRef)CFRetain(i->certificate);
    return errSecSuccess;
}

OSStatus SecIdentityCopyPrivateKey(SecIdentityRef identityRef, SecKeyRef *privateKeyRef) {
    struct __SecIdentity *i = identityOf(identityRef);
    if (i == NULL || privateKeyRef == NULL)
        return errSecParam;
    *privateKeyRef = (SecKeyRef)CFRetain(i->privateKey);
    return errSecSuccess;
}

/* No identity preferences are kept: none is preferred for any name. */
SecIdentityRef SecIdentityCopyPreferred(CFStringRef name, CFArrayRef keyUsage, CFArrayRef validIssuers) {
    (void)name; (void)keyUsage; (void)validIssuers;
    return NULL;
}

/* ---- Trust settings ---- */

static Boolean validDomain(SecTrustSettingsDomain domain) {
    return domain == kSecTrustSettingsDomainUser || domain == kSecTrustSettingsDomainAdmin ||
           domain == kSecTrustSettingsDomainSystem;
}

OSStatus SecTrustSettingsCopyTrustSettings(SecCertificateRef certRef, SecTrustSettingsDomain domain,
                                           CFArrayRef *trustSettings) {
    if (certRef == NULL || trustSettings == NULL || !validDomain(domain))
        return errSecParam;
    *trustSettings = NULL;
    return errSecNoTrustSettings;
}

OSStatus SecTrustSettingsCopyCertificates(SecTrustSettingsDomain domain, CFArrayRef *certArray) {
    if (!validDomain(domain))
        return errSecParam;
    if (certArray != NULL)
        *certArray = NULL;
    return errSecNoTrustSettings;
}
