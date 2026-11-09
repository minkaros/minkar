/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#ifndef _SECURITY_SECCERTIFICATE_H_
#define _SECURITY_SECCERTIFICATE_H_

#include "SecBase.h"

#ifdef __cplusplus
extern "C" {
#endif

CFTypeID SecCertificateGetTypeID(void);
/* A DER-encoded X.509 certificate. */
SecCertificateRef SecCertificateCreateWithData(CFAllocatorRef allocator, CFDataRef data);
CFDataRef SecCertificateCopyData(SecCertificateRef certificate);
CFStringRef SecCertificateCopySubjectSummary(SecCertificateRef certificate);
OSStatus SecCertificateCopyCommonName(SecCertificateRef certificate, CFStringRef *commonName);
SecKeyRef SecCertificateCopyKey(SecCertificateRef certificate);
OSStatus SecCertificateCopyPublicKey(SecCertificateRef certificate, SecKeyRef *key);

typedef uint32_t CSSM_CERT_TYPE, CSSM_CERT_ENCODING;
typedef struct cssm_data { size_t Length; uint8_t *Data; } CSSM_DATA;
enum { CSSM_CERT_X_509v3 = 3, CSSM_CERT_ENCODING_DER = 3 };
/* The older form. */
OSStatus SecCertificateCreateFromData(const CSSM_DATA *data, CSSM_CERT_TYPE type, CSSM_CERT_ENCODING encoding,
                                      SecCertificateRef *certificate);

#ifdef __cplusplus
}
#endif

#endif
