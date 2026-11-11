/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Keys: RSA, DSA and EC keys, public or private, imported with
 * SecItemImport or taken from a certificate. The cryptography is
 * libcrypto's, loaded when a key is first used.
 */

#ifndef _SECURITY_SECKEY_H_
#define _SECURITY_SECKEY_H_

#include "SecBase.h"

#ifdef __cplusplus
extern "C" {
#endif

CF_IMPLICIT_BRIDGING_ENABLED

typedef CFStringRef SecKeyAlgorithm;

CF_EXPORT const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionRaw;
CF_EXPORT const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionPKCS1;
CF_EXPORT const SecKeyAlgorithm kSecKeyAlgorithmRSAEncryptionOAEPSHA1;
CF_EXPORT const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256;
CF_EXPORT const SecKeyAlgorithm kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256;
CF_EXPORT const SecKeyAlgorithm kSecKeyAlgorithmECDSASignatureMessageX962SHA256;

CF_EXPORT const CFStringRef kSecAttrKeyType;
CF_EXPORT const CFStringRef kSecAttrKeyTypeRSA;
CF_EXPORT const CFStringRef kSecAttrKeyTypeDSA;
CF_EXPORT const CFStringRef kSecAttrKeyTypeEC;
CF_EXPORT const CFStringRef kSecAttrKeyTypeECSECPrimeRandom;
CF_EXPORT const CFStringRef kSecAttrKeyClass;
CF_EXPORT const CFStringRef kSecAttrKeyClassPublic;
CF_EXPORT const CFStringRef kSecAttrKeyClassPrivate;
CF_EXPORT const CFStringRef kSecAttrKeySizeInBits;

CFTypeID SecKeyGetTypeID(void);
size_t SecKeyGetBlockSize(SecKeyRef key);
CFDictionaryRef SecKeyCopyAttributes(SecKeyRef key);
SecKeyRef SecKeyCopyPublicKey(SecKeyRef key);
CFDataRef SecKeyCreateEncryptedData(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef plaintext, CFErrorRef *error);
CFDataRef SecKeyCreateDecryptedData(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef ciphertext, CFErrorRef *error);
Boolean SecKeyVerifySignature(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef signedData, CFDataRef signature, CFErrorRef *error);
Boolean SecKeyIsAlgorithmSupported(SecKeyRef key, int operation, SecKeyAlgorithm algorithm);


CF_IMPLICIT_BRIDGING_DISABLED

#ifdef __cplusplus
}
#endif

#endif
