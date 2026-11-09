/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Keys and certificates on libcrypto (OpenSSL 3), which is built after
 * the frameworks: it is opened when a key or certificate is first made,
 * and its functions looked up by name.
 */

#include "Security.h"
#include "SecInternal.h"
#include <dlfcn.h>

#define kLibCrypto "/usr/lib/libcrypto.3.dylib"

typedef struct bio_st BIO;
typedef struct x509_st X509;
typedef struct X509_name_st X509_NAME;
typedef struct evp_pkey_ctx_st EVP_PKEY_CTX;
typedef struct evp_md_st EVP_MD;
typedef struct evp_md_ctx_st EVP_MD_CTX;

enum { EVP_PKEY_RSA = 6, EVP_PKEY_DSA = 116, EVP_PKEY_EC = 408 };
enum { RSA_PKCS1_PADDING = 1, RSA_NO_PADDING = 3, RSA_PKCS1_OAEP_PADDING = 4 };
enum { NID_commonName = 13 };

static struct {
    void *image;
    BIO *(*BIO_new_mem_buf)(const void *, int);
    int (*BIO_free)(BIO *);
    EVP_PKEY *(*PEM_read_bio_PrivateKey)(BIO *, EVP_PKEY **, void *, void *);
    EVP_PKEY *(*PEM_read_bio_PUBKEY)(BIO *, EVP_PKEY **, void *, void *);
    X509 *(*PEM_read_bio_X509)(BIO *, X509 **, void *, void *);
    EVP_PKEY *(*d2i_PUBKEY)(EVP_PKEY **, const unsigned char **, long);
    EVP_PKEY *(*d2i_AutoPrivateKey)(EVP_PKEY **, const unsigned char **, long);
    X509 *(*d2i_X509)(X509 **, const unsigned char **, long);
    int (*i2d_X509)(X509 *, unsigned char **);
    int (*i2d_PUBKEY)(EVP_PKEY *, unsigned char **);
    void (*X509_free)(X509 *);
    EVP_PKEY *(*X509_get_pubkey)(X509 *);
    X509_NAME *(*X509_get_subject_name)(const X509 *);
    int (*X509_NAME_get_text_by_NID)(X509_NAME *, int, char *, int);
    void (*EVP_PKEY_free)(EVP_PKEY *);
    int (*EVP_PKEY_get_id)(const EVP_PKEY *);
    int (*EVP_PKEY_get_bits)(const EVP_PKEY *);
    int (*EVP_PKEY_get_size)(const EVP_PKEY *);
    EVP_PKEY_CTX *(*EVP_PKEY_CTX_new)(EVP_PKEY *, void *);
    void (*EVP_PKEY_CTX_free)(EVP_PKEY_CTX *);
    int (*EVP_PKEY_decrypt_init)(EVP_PKEY_CTX *);
    int (*EVP_PKEY_decrypt)(EVP_PKEY_CTX *, unsigned char *, size_t *, const unsigned char *, size_t);
    int (*EVP_PKEY_encrypt_init)(EVP_PKEY_CTX *);
    int (*EVP_PKEY_encrypt)(EVP_PKEY_CTX *, unsigned char *, size_t *, const unsigned char *, size_t);
    int (*EVP_PKEY_CTX_set_rsa_padding)(EVP_PKEY_CTX *, int);
    int (*EVP_PKEY_verify_init)(EVP_PKEY_CTX *);
    int (*EVP_PKEY_verify)(EVP_PKEY_CTX *, const unsigned char *, size_t, const unsigned char *, size_t);
    int (*EVP_PKEY_CTX_set_signature_md)(EVP_PKEY_CTX *, const EVP_MD *);
    const EVP_MD *(*EVP_get_digestbyname)(const char *);
    EVP_MD_CTX *(*EVP_MD_CTX_new)(void);
    void (*EVP_MD_CTX_free)(EVP_MD_CTX *);
    int (*EVP_DigestVerifyInit)(EVP_MD_CTX *, EVP_PKEY_CTX **, const EVP_MD *, void *, EVP_PKEY *);
    int (*EVP_DigestVerify)(EVP_MD_CTX *, const unsigned char *, size_t, const unsigned char *, size_t);
} C;

static pthread_once_t sCryptoOnce = PTHREAD_ONCE_INIT;

#define LOAD(name) if ((*(void **)&C.name = dlsym(C.image, #name)) == NULL) ok = 0

static void loadCrypto(void) {
    int ok = 1;
    C.image = dlopen(kLibCrypto, RTLD_LAZY | RTLD_LOCAL);
    if (C.image == NULL)
        return;
    LOAD(BIO_new_mem_buf); LOAD(BIO_free); LOAD(PEM_read_bio_PrivateKey); LOAD(PEM_read_bio_PUBKEY); LOAD(PEM_read_bio_X509);
    LOAD(d2i_PUBKEY); LOAD(d2i_AutoPrivateKey); LOAD(d2i_X509); LOAD(i2d_X509); LOAD(i2d_PUBKEY); LOAD(X509_free);
    LOAD(X509_get_pubkey); LOAD(X509_get_subject_name); LOAD(X509_NAME_get_text_by_NID); LOAD(EVP_PKEY_free);
    LOAD(EVP_PKEY_get_id); LOAD(EVP_PKEY_get_bits); LOAD(EVP_PKEY_get_size); LOAD(EVP_PKEY_CTX_new); LOAD(EVP_PKEY_CTX_free);
    LOAD(EVP_PKEY_decrypt_init); LOAD(EVP_PKEY_decrypt); LOAD(EVP_PKEY_encrypt_init); LOAD(EVP_PKEY_encrypt);
    LOAD(EVP_PKEY_CTX_set_rsa_padding); LOAD(EVP_PKEY_verify_init); LOAD(EVP_PKEY_verify); LOAD(EVP_PKEY_CTX_set_signature_md);
    LOAD(EVP_get_digestbyname); LOAD(EVP_MD_CTX_new); LOAD(EVP_MD_CTX_free); LOAD(EVP_DigestVerifyInit); LOAD(EVP_DigestVerify);
    if (!ok) {
        dlclose(C.image);
        C.image = NULL;
    }
}

Boolean _SecCryptoAvailable(void) {
    pthread_once(&sCryptoOnce, loadCrypto);
    return C.image != NULL;
}

/* ---- Keys ---- */

const CFStringRef kSecKeyAlgorithmRSAEncryptionRaw = CFSTR("algid:encrypt:RSA:raw");
const CFStringRef kSecKeyAlgorithmRSAEncryptionPKCS1 = CFSTR("algid:encrypt:RSA:PKCS1");
const CFStringRef kSecKeyAlgorithmRSAEncryptionOAEPSHA1 = CFSTR("algid:encrypt:RSA:OAEP:SHA1");
const CFStringRef kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256 = CFSTR("algid:sign:RSA:message-PKCS1v15:SHA256");
const CFStringRef kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256 = CFSTR("algid:sign:RSA:digest-PKCS1v15:SHA256");
const CFStringRef kSecKeyAlgorithmECDSASignatureMessageX962SHA256 = CFSTR("algid:sign:ECDSA:message-X962:SHA256");
const CFStringRef kSecAttrKeyType = CFSTR("type");
const CFStringRef kSecAttrKeyTypeRSA = CFSTR("42");
const CFStringRef kSecAttrKeyTypeDSA = CFSTR("43");
const CFStringRef kSecAttrKeyTypeEC = CFSTR("73");
const CFStringRef kSecAttrKeyTypeECSECPrimeRandom = CFSTR("73");
const CFStringRef kSecAttrKeyClass = CFSTR("kcls");
const CFStringRef kSecAttrKeyClassPublic = CFSTR("0");
const CFStringRef kSecAttrKeyClassPrivate = CFSTR("1");
const CFStringRef kSecAttrKeySizeInBits = CFSTR("bsiz");

struct __SecKey {
    CFRuntimeBase base;
    EVP_PKEY *pkey;
    Boolean isPrivate;
};

static void keyFinalize(CFTypeRef cf) {
    struct __SecKey *k = (struct __SecKey *)cf;
    if (k->pkey != NULL)
        C.EVP_PKEY_free(k->pkey);
}

SEC_TYPE(SecKey, "SecKey", keyFinalize)

SecKeyRef _SecKeyCreateWithPKey(EVP_PKEY *pkey, Boolean isPrivate) {
    struct __SecKey *k = _SecCreateInstance(SecKeyGetTypeID(), sizeof(struct __SecKey));
    if (k == NULL) {
        C.EVP_PKEY_free(pkey);
        return NULL;
    }
    k->pkey = pkey;
    k->isPrivate = isPrivate;
    return k;
}

EVP_PKEY *_SecKeyGetPKey(SecKeyRef key) {
    return key != NULL ? ((struct __SecKey *)key)->pkey : NULL;
}

size_t SecKeyGetBlockSize(SecKeyRef key) {
    return key != NULL ? (size_t)C.EVP_PKEY_get_size(_SecKeyGetPKey(key)) : 0;
}

CFDictionaryRef SecKeyCopyAttributes(SecKeyRef key) {
    struct __SecKey *k = (struct __SecKey *)key;
    int id, bits;
    CFStringRef type;
    if (k == NULL)
        return NULL;
    id = C.EVP_PKEY_get_id(k->pkey);
    bits = C.EVP_PKEY_get_bits(k->pkey);
    type = id == EVP_PKEY_RSA ? kSecAttrKeyTypeRSA : id == EVP_PKEY_DSA ? kSecAttrKeyTypeDSA : kSecAttrKeyTypeEC;
    CFNumberRef size = CFNumberCreate(NULL, kCFNumberIntType, &bits);
    const void *keys[] = { kSecAttrKeyType, kSecAttrKeyClass, kSecAttrKeySizeInBits };
    const void *values[] = { type, k->isPrivate ? kSecAttrKeyClassPrivate : kSecAttrKeyClassPublic, size };
    CFDictionaryRef attributes = CFDictionaryCreate(NULL, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(size);
    return attributes;
}

/* The public half, through its DER encoding. */
SecKeyRef SecKeyCopyPublicKey(SecKeyRef key) {
    struct __SecKey *k = (struct __SecKey *)key;
    unsigned char *der = NULL;
    if (k == NULL)
        return NULL;
    int length = C.i2d_PUBKEY(k->pkey, &der);
    if (length <= 0)
        return NULL;
    const unsigned char *p = der;
    EVP_PKEY *pub = C.d2i_PUBKEY(NULL, &p, length);
    free(der);
    return pub != NULL ? _SecKeyCreateWithPKey(pub, false) : NULL;
}

static void setError(CFErrorRef *error, OSStatus status) {
    if (error != NULL)
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, status, NULL);
}

static int paddingFor(SecKeyAlgorithm algorithm) {
    if (CFEqual(algorithm, kSecKeyAlgorithmRSAEncryptionRaw)) return RSA_NO_PADDING;
    if (CFEqual(algorithm, kSecKeyAlgorithmRSAEncryptionPKCS1)) return RSA_PKCS1_PADDING;
    if (CFEqual(algorithm, kSecKeyAlgorithmRSAEncryptionOAEPSHA1)) return RSA_PKCS1_OAEP_PADDING;
    return -1;
}

static CFDataRef keyCrypt(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef input, CFErrorRef *error, Boolean decrypt) {
    struct __SecKey *k = (struct __SecKey *)key;
    int padding = algorithm != NULL ? paddingFor(algorithm) : -1;
    EVP_PKEY_CTX *ctx;
    size_t length = 0;
    CFMutableDataRef output = NULL;

    if (k == NULL || input == NULL || padding < 0 || C.EVP_PKEY_get_id(k->pkey) != EVP_PKEY_RSA || (decrypt && !k->isPrivate)) {
        setError(error, padding < 0 ? errSecInvalidAlgorithm : errSecParam);
        return NULL;
    }
    ctx = C.EVP_PKEY_CTX_new(k->pkey, NULL);
    if (ctx != NULL && (decrypt ? C.EVP_PKEY_decrypt_init(ctx) : C.EVP_PKEY_encrypt_init(ctx)) > 0 &&
        C.EVP_PKEY_CTX_set_rsa_padding(ctx, padding) > 0 &&
        (decrypt ? C.EVP_PKEY_decrypt : C.EVP_PKEY_encrypt)(ctx, NULL, &length, CFDataGetBytePtr(input), (size_t)CFDataGetLength(input)) > 0) {
        output = CFDataCreateMutable(NULL, 0);
        CFDataSetLength(output, (CFIndex)length);
        if ((decrypt ? C.EVP_PKEY_decrypt : C.EVP_PKEY_encrypt)(ctx, CFDataGetMutableBytePtr(output), &length,
                                                              CFDataGetBytePtr(input), (size_t)CFDataGetLength(input)) > 0)
            CFDataSetLength(output, (CFIndex)length);
        else {
            CFRelease(output);
            output = NULL;
        }
    }
    if (ctx != NULL)
        C.EVP_PKEY_CTX_free(ctx);
    if (output == NULL)
        setError(error, errSecDecode);
    return output;
}

CFDataRef SecKeyCreateDecryptedData(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef ciphertext, CFErrorRef *error) {
    return keyCrypt(key, algorithm, ciphertext, error, true);
}

CFDataRef SecKeyCreateEncryptedData(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef plaintext, CFErrorRef *error) {
    return keyCrypt(key, algorithm, plaintext, error, false);
}

/* 1 verified, 0 not, -1 could not be checked. */
int _SecCryptoVerify(EVP_PKEY *pkey, const char *digestName, const uint8_t *data, size_t length, Boolean isDigest,
                     const uint8_t *signature, size_t signatureLength) {
    const EVP_MD *md = digestName != NULL ? C.EVP_get_digestbyname(digestName) : NULL;
    int result = -1;
    if (pkey == NULL)
        return -1;
    if (isDigest) {
        EVP_PKEY_CTX *ctx = C.EVP_PKEY_CTX_new(pkey, NULL);
        if (ctx != NULL && C.EVP_PKEY_verify_init(ctx) > 0 && (md == NULL || C.EVP_PKEY_CTX_set_signature_md(ctx, md) > 0)) {
            if (C.EVP_PKEY_get_id(pkey) == EVP_PKEY_RSA)
                C.EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING);
            result = C.EVP_PKEY_verify(ctx, signature, signatureLength, data, length) == 1 ? 1 : 0;
        }
        if (ctx != NULL)
            C.EVP_PKEY_CTX_free(ctx);
        return result;
    }
    EVP_MD_CTX *mctx = C.EVP_MD_CTX_new();
    if (mctx != NULL && C.EVP_DigestVerifyInit(mctx, NULL, md, NULL, pkey) > 0)
        result = C.EVP_DigestVerify(mctx, signature, signatureLength, data, length) == 1 ? 1 : 0;
    if (mctx != NULL)
        C.EVP_MD_CTX_free(mctx);
    return result;
}

Boolean SecKeyVerifySignature(SecKeyRef key, SecKeyAlgorithm algorithm, CFDataRef signedData, CFDataRef signature, CFErrorRef *error) {
    Boolean isDigest = algorithm != NULL && CFStringFind(algorithm, CFSTR(":digest-"), 0).location != kCFNotFound;
    if (key == NULL || signedData == NULL || signature == NULL || algorithm == NULL) {
        setError(error, errSecParam);
        return false;
    }
    int ok = _SecCryptoVerify(_SecKeyGetPKey(key), "SHA256", CFDataGetBytePtr(signedData), (size_t)CFDataGetLength(signedData),
                              isDigest, CFDataGetBytePtr(signature), (size_t)CFDataGetLength(signature));
    if (ok != 1)
        setError(error, errSecVerifyFailed);
    return ok == 1;
}

Boolean SecKeyIsAlgorithmSupported(SecKeyRef key, int operation, SecKeyAlgorithm algorithm) {
    return key != NULL && algorithm != NULL &&
           (paddingFor(algorithm) >= 0 || CFStringFind(algorithm, CFSTR("SHA256"), 0).location != kCFNotFound);
}

/* ---- Certificates ---- */

struct __SecCertificate {
    CFRuntimeBase base;
    X509 *x509;
    CFDataRef der;
};

static void certificateFinalize(CFTypeRef cf) {
    struct __SecCertificate *c = (struct __SecCertificate *)cf;
    if (c->x509 != NULL) C.X509_free(c->x509);
    if (c->der != NULL) CFRelease(c->der);
}

SEC_TYPE(SecCertificate, "SecCertificate", certificateFinalize)

static SecCertificateRef createCertificate(X509 *x509) {
    unsigned char *der = NULL;
    int length = C.i2d_X509(x509, &der);
    struct __SecCertificate *c;
    if (length <= 0) {
        C.X509_free(x509);
        return NULL;
    }
    c = _SecCreateInstance(SecCertificateGetTypeID(), sizeof(struct __SecCertificate));
    c->x509 = x509;
    c->der = CFDataCreate(NULL, der, length);
    free(der);
    return c;
}

SecCertificateRef SecCertificateCreateWithData(CFAllocatorRef allocator, CFDataRef data) {
    const unsigned char *p;
    X509 *x509;
    if (data == NULL || !_SecCryptoAvailable())
        return NULL;
    p = CFDataGetBytePtr(data);
    x509 = C.d2i_X509(NULL, &p, CFDataGetLength(data));
    return x509 != NULL ? createCertificate(x509) : NULL;
}

OSStatus SecCertificateCreateFromData(const CSSM_DATA *data, CSSM_CERT_TYPE type, CSSM_CERT_ENCODING encoding,
                                      SecCertificateRef *certificate) {
    if (data == NULL || certificate == NULL)
        return errSecParam;
    CFDataRef bytes = CFDataCreate(NULL, data->Data, (CFIndex)data->Length);
    *certificate = SecCertificateCreateWithData(NULL, bytes);
    CFRelease(bytes);
    return *certificate != NULL ? errSecSuccess : errSecDecode;
}

CFDataRef SecCertificateCopyData(SecCertificateRef certificate) {
    return certificate != NULL ? CFRetain(((struct __SecCertificate *)certificate)->der) : NULL;
}

OSStatus SecCertificateCopyCommonName(SecCertificateRef certificate, CFStringRef *commonName) {
    char name[256];
    if (certificate == NULL || commonName == NULL)
        return errSecParam;
    X509_NAME *subject = C.X509_get_subject_name(((struct __SecCertificate *)certificate)->x509);
    int length = subject != NULL ? C.X509_NAME_get_text_by_NID(subject, NID_commonName, name, sizeof(name)) : -1;
    *commonName = length >= 0 ? CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8) : NULL;
    return errSecSuccess;
}

CFStringRef SecCertificateCopySubjectSummary(SecCertificateRef certificate) {
    CFStringRef name = NULL;
    SecCertificateCopyCommonName(certificate, &name);
    return name;
}

SecKeyRef SecCertificateCopyKey(SecCertificateRef certificate) {
    EVP_PKEY *pkey = certificate != NULL ? C.X509_get_pubkey(((struct __SecCertificate *)certificate)->x509) : NULL;
    return pkey != NULL ? _SecKeyCreateWithPKey(pkey, false) : NULL;
}

OSStatus SecCertificateCopyPublicKey(SecCertificateRef certificate, SecKeyRef *key) {
    if (key == NULL)
        return errSecParam;
    *key = SecCertificateCopyKey(certificate);
    return *key != NULL ? errSecSuccess : errSecDecode;
}

/* ---- Import ---- */

static Boolean looksLikePEM(CFDataRef data) {
    const char *bytes = (const char *)CFDataGetBytePtr(data);
    CFIndex length = CFDataGetLength(data);
    for (CFIndex i = 0; i + 10 < length && i < 4096; i++)
        if (memcmp(bytes + i, "-----BEGIN", 10) == 0)
            return true;
    return false;
}

/* PEM (keys and certificates, several in sequence) or DER (a public key,
   a private key or a certificate). */
OSStatus SecItemImport(CFDataRef importedData, CFStringRef fileNameOrExtension, SecExternalFormat *inputFormat,
                       SecExternalItemType *itemType, SecItemImportExportFlags flags,
                       const SecItemImportExportKeyParameters *keyParams, SecKeychainRef importKeychain, CFArrayRef *outItems) {
    CFMutableArrayRef items;
    SecExternalItemType type = kSecItemTypeUnknown;

    if (importedData == NULL)
        return errSecParam;
    if (!_SecCryptoAvailable())
        return errSecUnimplemented;
    items = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (looksLikePEM(importedData)) {
        const char *text = (const char *)CFDataGetBytePtr(importedData);
        const char *end = text + CFDataGetLength(importedData), *p = text;
        while ((p = strstr(p, "-----BEGIN ")) != NULL && p < end) {
            const char *close = strstr(p, "-----END ");
            const char *stop = close != NULL ? strstr(close + 9, "-----") : NULL;
            if (stop == NULL)
                break;
            stop += 5;
            BIO *bio = C.BIO_new_mem_buf(p, (int)(stop - p));
            CFTypeRef item = NULL;
            if (strncmp(p, "-----BEGIN CERTIFICATE", 22) == 0) {
                X509 *x509 = C.PEM_read_bio_X509(bio, NULL, NULL, NULL);
                if (x509 != NULL) { item = createCertificate(x509); type = kSecItemTypeCertificate; }
            } else if (strncmp(p, "-----BEGIN PUBLIC KEY", 21) == 0) {
                EVP_PKEY *pkey = C.PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
                if (pkey != NULL) { item = _SecKeyCreateWithPKey(pkey, false); type = kSecItemTypePublicKey; }
            } else {
                EVP_PKEY *pkey = C.PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
                if (pkey != NULL) { item = _SecKeyCreateWithPKey(pkey, true); type = kSecItemTypePrivateKey; }
            }
            C.BIO_free(bio);
            if (item != NULL) {
                CFArrayAppendValue(items, item);
                CFRelease(item);
            }
            p = stop;
        }
        if (inputFormat != NULL)
            *inputFormat = CFArrayGetCount(items) > 1 ? kSecFormatPEMSequence : kSecFormatOpenSSL;
    } else {
        const unsigned char *der = CFDataGetBytePtr(importedData);
        long length = (long)CFDataGetLength(importedData);
        const unsigned char *p = der;
        EVP_PKEY *pkey = C.d2i_PUBKEY(NULL, &p, length);
        CFTypeRef item = NULL;
        if (pkey != NULL) {
            item = _SecKeyCreateWithPKey(pkey, false);
            type = kSecItemTypePublicKey;
        } else if ((p = der, pkey = C.d2i_AutoPrivateKey(NULL, &p, length)) != NULL) {
            item = _SecKeyCreateWithPKey(pkey, true);
            type = kSecItemTypePrivateKey;
        } else {
            X509 *x509 = (p = der, C.d2i_X509(NULL, &p, length));
            if (x509 != NULL) {
                item = createCertificate(x509);
                type = kSecItemTypeCertificate;
            }
        }
        if (item != NULL) {
            CFArrayAppendValue(items, item);
            CFRelease(item);
        }
        if (inputFormat != NULL)
            *inputFormat = type == kSecItemTypeCertificate ? kSecFormatX509Cert : kSecFormatOpenSSL;
    }
    if (CFArrayGetCount(items) == 0) {
        CFRelease(items);
        return errSecUnknownFormat;
    }
    if (itemType != NULL)
        *itemType = CFArrayGetCount(items) > 1 ? kSecItemTypeAggregate : type;
    if (outItems != NULL)
        *outItems = items;
    else
        CFRelease(items);
    return errSecSuccess;
}
