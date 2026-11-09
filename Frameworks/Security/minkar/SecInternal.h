/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef _SECURITY_SECINTERNAL_H_
#define _SECURITY_SECINTERNAL_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

CFTypeID _SecRegisterClass(CFTypeID *storage, const char *name, void (*finalize)(CFTypeRef));
void *_SecCreateInstance(CFTypeID type, size_t size);

/* A type registered on first use, with its GetTypeID function. */
#define SEC_TYPE(Name, label, finalizer) \
    static CFTypeID Name##TypeID; \
    static pthread_once_t Name##Once = PTHREAD_ONCE_INIT; \
    static void Name##Register(void) { _SecRegisterClass(&Name##TypeID, label, finalizer); } \
    CFTypeID Name##GetTypeID(void) { pthread_once(&Name##Once, Name##Register); return Name##TypeID; }

/* libcrypto, loaded when first needed (it is built after the frameworks). */
typedef struct evp_pkey_st EVP_PKEY;
EVP_PKEY *_SecKeyGetPKey(SecKeyRef key);
SecKeyRef _SecKeyCreateWithPKey(EVP_PKEY *pkey, Boolean isPrivate);
int _SecCryptoVerify(EVP_PKEY *pkey, const char *digestName, const uint8_t *data, size_t length, Boolean isDigest,
                     const uint8_t *signature, size_t signatureLength);
Boolean _SecCryptoAvailable(void);

#endif
