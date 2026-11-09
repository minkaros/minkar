/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Keys and certificates from PEM or DER data (not added to a keychain).
 */

#ifndef _SECURITY_SECIMPORTEXPORT_H_
#define _SECURITY_SECIMPORTEXPORT_H_

#include "SecBase.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t SecExternalFormat;
enum {
    kSecFormatUnknown = 0,
    kSecFormatOpenSSL,
    kSecFormatSSH,
    kSecFormatBSAFE,
    kSecFormatRawKey,
    kSecFormatWrappedPKCS8,
    kSecFormatWrappedOpenSSL,
    kSecFormatWrappedSSH,
    kSecFormatWrappedLSH,
    kSecFormatX509Cert,
    kSecFormatPEMSequence,
    kSecFormatPKCS7,
    kSecFormatPKCS12,
    kSecFormatNetscapeCertSequence,
    kSecFormatSSHv2
};

typedef uint32_t SecExternalItemType;
enum {
    kSecItemTypeUnknown = 0,
    kSecItemTypePrivateKey,
    kSecItemTypePublicKey,
    kSecItemTypeSessionKey,
    kSecItemTypeCertificate,
    kSecItemTypeAggregate
};

typedef uint32_t SecItemImportExportFlags;
enum { kSecItemPemArmour = 0x00000001 };

typedef struct {
    uint32_t version;
    uint32_t flags;
    CFTypeRef passphrase;
    CFStringRef alertTitle;
    CFStringRef alertPrompt;
    SecAccessRef accessRef;
    CFArrayRef keyUsage;
    CFArrayRef keyAttributes;
} SecItemImportExportKeyParameters;

OSStatus SecItemImport(CFDataRef importedData, CFStringRef fileNameOrExtension, SecExternalFormat *inputFormat,
                       SecExternalItemType *itemType, SecItemImportExportFlags flags,
                       const SecItemImportExportKeyParameters *keyParams, SecKeychainRef importKeychain, CFArrayRef *outItems);

#ifdef __cplusplus
}
#endif

#endif
