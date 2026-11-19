/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * SecTrust on libcrypto. libcrypto builds the chain to an anchor (the
 * system's, /etc/ssl/cert.pem, or the app's), and only that; the policies'
 * checks are made here, each a failure of its own, reported in the
 * reference's order (measured on it): the chain not built, a TLS server
 * certificate valid for over 825 days, expired or not yet valid, no
 * trusted anchor, no serverAuth usage, the name not among the leaf's
 * subject alternative names (its common name is not taken).
 */

#include "Security.h"
#include "SecInternal.h"
#include <arpa/inet.h>
#include <dlfcn.h>
#include <time.h>

#define kLibCrypto "/usr/lib/libcrypto.3.dylib"
#define kSystemAnchors "/etc/ssl/cert.pem"

typedef struct x509_st X509;
typedef struct x509_store_st X509_STORE;
typedef struct x509_store_ctx_st X509_STORE_CTX;
typedef struct stack_st OPENSSL_STACK;
typedef struct asn1_string_st ASN1_TIME;
typedef struct evp_md_st EVP_MD;

enum {
    X509_V_OK = 0,
    X509_V_FLAG_PARTIAL_CHAIN = 0x80000,
    X509_V_FLAG_NO_CHECK_TIME = 0x200000,
    X509_CHECK_FLAG_NEVER_CHECK_SUBJECT = 0x20,
    X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT = 2,
    X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY = 20,
    EXFLAG_XKUSAGE = 0x4,
    XKU_SSL_SERVER = 0x1,
    XKU_ANYEKU = 0x100,
};

static struct {
    void *image;
    X509_STORE *(*X509_STORE_new)(void);
    void (*X509_STORE_free)(X509_STORE *);
    int (*X509_STORE_add_cert)(X509_STORE *, X509 *);
    int (*X509_STORE_load_file)(X509_STORE *, const char *);
    int (*X509_STORE_set_flags)(X509_STORE *, unsigned long);
    X509_STORE_CTX *(*X509_STORE_CTX_new)(void);
    void (*X509_STORE_CTX_free)(X509_STORE_CTX *);
    int (*X509_STORE_CTX_init)(X509_STORE_CTX *, X509_STORE *, X509 *, OPENSSL_STACK *);
    int (*X509_STORE_CTX_get_error)(const X509_STORE_CTX *);
    OPENSSL_STACK *(*X509_STORE_CTX_get1_chain)(const X509_STORE_CTX *);
    int (*X509_verify_cert)(X509_STORE_CTX *);
    OPENSSL_STACK *(*OPENSSL_sk_new_null)(void);
    int (*OPENSSL_sk_push)(OPENSSL_STACK *, const void *);
    int (*OPENSSL_sk_num)(const OPENSSL_STACK *);
    void *(*OPENSSL_sk_value)(const OPENSSL_STACK *, int);
    void (*OPENSSL_sk_free)(OPENSSL_STACK *);
    void (*OPENSSL_sk_pop_free)(OPENSSL_STACK *, void (*)(void *));
    int (*X509_check_issued)(X509 *, X509 *);
    int (*X509_check_host)(X509 *, const char *, size_t, unsigned int, char **);
    int (*X509_check_ip_asc)(X509 *, const char *, unsigned int);
    uint32_t (*X509_get_extension_flags)(X509 *);
    uint32_t (*X509_get_extended_key_usage)(X509 *);
    const ASN1_TIME *(*X509_get0_notBefore)(const X509 *);
    const ASN1_TIME *(*X509_get0_notAfter)(const X509 *);
    int (*ASN1_TIME_diff)(int *, int *, const ASN1_TIME *, const ASN1_TIME *);
    int (*X509_cmp_time)(const ASN1_TIME *, time_t *);
    void (*X509_free)(X509 *);
    int (*i2d_X509)(X509 *, unsigned char **);
    const EVP_MD *(*EVP_sha256)(void);
    int (*EVP_Digest)(const void *, size_t, unsigned char *, unsigned int *, const EVP_MD *, void *);
} T;

static pthread_once_t sTrustOnce = PTHREAD_ONCE_INIT;

#define LOAD(name) if ((*(void **)&T.name = dlsym(T.image, #name)) == NULL) ok = 0

static void loadTrust(void) {
    int ok = 1;
    T.image = dlopen(kLibCrypto, RTLD_LAZY | RTLD_LOCAL);
    if (T.image == NULL)
        return;
    LOAD(X509_STORE_new); LOAD(X509_STORE_free); LOAD(X509_STORE_add_cert); LOAD(X509_STORE_load_file);
    LOAD(X509_STORE_set_flags); LOAD(X509_STORE_CTX_new); LOAD(X509_STORE_CTX_free); LOAD(X509_STORE_CTX_init);
    LOAD(X509_STORE_CTX_get_error); LOAD(X509_STORE_CTX_get1_chain); LOAD(X509_verify_cert);
    LOAD(OPENSSL_sk_new_null); LOAD(OPENSSL_sk_push); LOAD(OPENSSL_sk_num); LOAD(OPENSSL_sk_value);
    LOAD(OPENSSL_sk_free); LOAD(OPENSSL_sk_pop_free); LOAD(X509_check_issued); LOAD(X509_check_host);
    LOAD(X509_check_ip_asc); LOAD(X509_get_extension_flags); LOAD(X509_get_extended_key_usage);
    LOAD(X509_get0_notBefore); LOAD(X509_get0_notAfter); LOAD(ASN1_TIME_diff); LOAD(X509_cmp_time);
    LOAD(X509_free); LOAD(i2d_X509); LOAD(EVP_sha256); LOAD(EVP_Digest);
    if (!ok) {
        dlclose(T.image);
        T.image = NULL;
    }
}

static Boolean trustAvailable(void) {
    pthread_once(&sTrustOnce, loadTrust);
    return T.image != NULL && _SecCryptoAvailable();
}

/* ---- The trust object ---- */

struct __SecTrust {
    CFRuntimeBase base;
    CFArrayRef certificates;          /* given, the leaf first */
    CFArrayRef policies;
    CFArrayRef anchors;               /* the app's, or NULL */
    Boolean anchorsOnly;
    CFDateRef verifyDate;             /* NULL: now */
    CFDataRef exceptions;
    Boolean evaluated;
    SecTrustResultType result;
    OSStatus failure;                 /* the first failure reported */
    CFArrayRef failures;              /* every failure, as CFNumbers */
    CFArrayRef chain;                 /* built and trusted, or NULL */
    pthread_mutex_t lock;
};

static void trustFinalize(CFTypeRef cf) {
    struct __SecTrust *t = (struct __SecTrust *)cf;
    CFTypeRef owned[] = { t->certificates, t->policies, t->anchors, t->verifyDate, t->exceptions, t->failures, t->chain };
    for (size_t i = 0; i < sizeof(owned) / sizeof(*owned); i++)
        if (owned[i])
            CFRelease(owned[i]);
    pthread_mutex_destroy(&t->lock);
}

SEC_TYPE(SecTrust, "SecTrust", trustFinalize)

static void replace(CFTypeRef *slot, CFTypeRef value) {
    if (value)
        CFRetain(value);
    if (*slot)
        CFRelease(*slot);
    *slot = value;
}

/* One or an array, as an array of that type's values. */
static CFArrayRef arrayOf(CFTypeRef value, CFTypeID type) {
    if (value == NULL)
        return NULL;
    if (CFGetTypeID(value) == type)
        return CFArrayCreate(NULL, &value, 1, &kCFTypeArrayCallBacks);
    if (CFGetTypeID(value) != CFArrayGetTypeID())
        return NULL;
    for (CFIndex i = 0; i < CFArrayGetCount(value); i++)
        if (CFGetTypeID(CFArrayGetValueAtIndex(value, i)) != type)
            return NULL;
    return CFArrayCreateCopy(NULL, value);
}

static void invalidate(struct __SecTrust *t) {
    t->evaluated = false;
    t->result = kSecTrustResultInvalid;
    t->failure = errSecSuccess;
    replace((CFTypeRef *)&t->failures, NULL);
    replace((CFTypeRef *)&t->chain, NULL);
}

OSStatus SecTrustCreateWithCertificates(CFTypeRef certificates, CFTypeRef policies, SecTrustRef *trust) {
    CFArrayRef certs, pols;
    struct __SecTrust *t;
    if (trust == NULL)
        return errSecParam;
    *trust = NULL;
    certs = arrayOf(certificates, SecCertificateGetTypeID());
    if (certs == NULL || CFArrayGetCount(certs) == 0) {
        if (certs) CFRelease(certs);
        return errSecParam;
    }
    if (policies != NULL) {
        pols = arrayOf(policies, SecPolicyGetTypeID());
        if (pols == NULL) {
            CFRelease(certs);
            return errSecParam;
        }
    } else {
        SecPolicyRef basic = SecPolicyCreateBasicX509();
        pols = CFArrayCreate(NULL, (const void **)&basic, 1, &kCFTypeArrayCallBacks);
        CFRelease(basic);
    }
    t = _SecCreateInstance(SecTrustGetTypeID(), sizeof(struct __SecTrust));
    if (t == NULL) {
        CFRelease(certs);
        CFRelease(pols);
        return errSecAllocate;
    }
    pthread_mutex_init(&t->lock, NULL);
    t->certificates = certs;
    t->policies = pols;
    *trust = t;
    return errSecSuccess;
}

OSStatus SecTrustSetPolicies(SecTrustRef trust, CFTypeRef policies) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    CFArrayRef pols = t != NULL ? arrayOf(policies, SecPolicyGetTypeID()) : NULL;
    if (pols == NULL)
        return errSecParam;
    pthread_mutex_lock(&t->lock);
    replace((CFTypeRef *)&t->policies, pols);
    invalidate(t);
    pthread_mutex_unlock(&t->lock);
    CFRelease(pols);
    return errSecSuccess;
}

OSStatus SecTrustCopyPolicies(SecTrustRef trust, CFArrayRef *policies) {
    if (trust == NULL || policies == NULL)
        return errSecParam;
    *policies = CFRetain(((struct __SecTrust *)trust)->policies);
    return errSecSuccess;
}

OSStatus SecTrustSetAnchorCertificates(SecTrustRef trust, CFArrayRef anchorCertificates) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    CFArrayRef anchors = NULL;
    if (t == NULL)
        return errSecParam;
    if (anchorCertificates != NULL && (anchors = arrayOf(anchorCertificates, SecCertificateGetTypeID())) == NULL)
        return errSecParam;
    pthread_mutex_lock(&t->lock);
    replace((CFTypeRef *)&t->anchors, anchors);
    t->anchorsOnly = anchors != NULL;
    invalidate(t);
    pthread_mutex_unlock(&t->lock);
    if (anchors)
        CFRelease(anchors);
    return errSecSuccess;
}

OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef trust, Boolean anchorCertificatesOnly) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    if (t == NULL)
        return errSecParam;
    pthread_mutex_lock(&t->lock);
    t->anchorsOnly = anchorCertificatesOnly;
    invalidate(t);
    pthread_mutex_unlock(&t->lock);
    return errSecSuccess;
}

OSStatus SecTrustCopyCustomAnchorCertificates(SecTrustRef trust, CFArrayRef *anchors) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    if (t == NULL || anchors == NULL)
        return errSecParam;
    *anchors = t->anchors ? CFRetain(t->anchors) : NULL;
    return errSecSuccess;
}

OSStatus SecTrustSetVerifyDate(SecTrustRef trust, CFDateRef verifyDate) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    if (t == NULL || verifyDate == NULL)
        return errSecParam;
    pthread_mutex_lock(&t->lock);
    replace((CFTypeRef *)&t->verifyDate, verifyDate);
    invalidate(t);
    pthread_mutex_unlock(&t->lock);
    return errSecSuccess;
}

CFAbsoluteTime SecTrustGetVerifyTime(SecTrustRef trust) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    return (t != NULL && t->verifyDate != NULL) ? CFDateGetAbsoluteTime(t->verifyDate) : CFAbsoluteTimeGetCurrent();
}

/* ---- Evaluation ---- */

static X509 *x509Of(CFArrayRef certificates, CFIndex i) {
    return _SecCertificateGetX509((SecCertificateRef)CFArrayGetValueAtIndex(certificates, i));
}

static void addFailure(CFMutableArrayRef failures, OSStatus status) {
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &status);
    if (!CFArrayContainsValue(failures, CFRangeMake(0, CFArrayGetCount(failures)), n))
        CFArrayAppendValue(failures, n);
    CFRelease(n);
}

/* The leaf's SHA-256, which exceptions are for. */
static CFDataRef leafDigest(CFArrayRef certificates) {
    unsigned char *der = NULL, digest[32];
    unsigned int length = 0;
    int n = T.i2d_X509(x509Of(certificates, 0), &der);
    CFDataRef result = NULL;
    if (n > 0 && T.EVP_Digest(der, (size_t)n, digest, &length, T.EVP_sha256(), NULL) == 1)
        result = CFDataCreate(NULL, digest, length);
    free(der);
    return result;
}

/* Issued since 1 July 2019 (the reference's TLS rules apply). */
static Boolean issuedSinceJuly2019(X509 *x) {
    time_t rule = 1561939200;   /* 2019-07-01T00:00:00Z */
    return T.X509_cmp_time(T.X509_get0_notBefore(x), &rule) >= 0;
}

static Boolean nameMatches(X509 *leaf, CFStringRef hostname) {
    char name[256];
    unsigned char address[16];
    if (!CFStringGetCString(hostname, name, sizeof(name), kCFStringEncodingUTF8))
        return false;
    if (inet_pton(AF_INET, name, address) == 1 || inet_pton(AF_INET6, name, address) == 1)
        return T.X509_check_ip_asc(leaf, name, 0) == 1;
    return T.X509_check_host(leaf, name, strlen(name), X509_CHECK_FLAG_NEVER_CHECK_SUBJECT, NULL) == 1;
}

static void evaluateLocked(struct __SecTrust *t) {
    CFMutableArrayRef failures = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    X509 *leaf = x509Of(t->certificates, 0);
    X509_STORE *store = T.X509_STORE_new();
    X509_STORE_CTX *ctx = T.X509_STORE_CTX_new();
    OPENSSL_STACK *untrusted = T.OPENSSL_sk_new_null();
    OPENSSL_STACK *built = NULL;
    Boolean trusted = false, chainFailed = false;
    time_t now = (time_t)(SecTrustGetVerifyTime(t) + kCFAbsoluteTimeIntervalSince1970);
    CFIndex certificateCount = CFArrayGetCount(t->certificates);

    /* The anchors: the app's, and the system's unless only the app's. */
    if (t->anchors)
        for (CFIndex i = 0; i < CFArrayGetCount(t->anchors); i++)
            T.X509_STORE_add_cert(store, x509Of(t->anchors, i));
    if (t->anchors == NULL || !t->anchorsOnly)
        T.X509_STORE_load_file(store, kSystemAnchors);
    /* An anchor need not be a root; time is checked here, not by libcrypto. */
    T.X509_STORE_set_flags(store, X509_V_FLAG_PARTIAL_CHAIN | X509_V_FLAG_NO_CHECK_TIME);
    for (CFIndex i = 1; i < certificateCount; i++)
        T.OPENSSL_sk_push(untrusted, x509Of(t->certificates, i));
    if (store && ctx && T.X509_STORE_CTX_init(ctx, store, leaf, untrusted) == 1) {
        trusted = T.X509_verify_cert(ctx) == 1;
        if (trusted)
            built = T.X509_STORE_CTX_get1_chain(ctx);
        else {
            int error = T.X509_STORE_CTX_get_error(ctx);
            /* No issuer to go on with, for a certificate not its own issuer. */
            chainFailed = (error == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT ||
                           error == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) &&
                          T.X509_check_issued(leaf, leaf) != X509_V_OK;
        }
    }

    for (CFIndex p = 0; p < CFArrayGetCount(t->policies); p++) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(t->policies, p);
        Boolean tlsServer = _SecPolicyIsSSL(policy) && _SecPolicyIsServer(policy);
        if (chainFailed)
            addFailure(failures, errSecCreateChainFailed);
        if (tlsServer && issuedSinceJuly2019(leaf)) {
            int days = 0, seconds = 0;
            if (T.ASN1_TIME_diff(&days, &seconds, T.X509_get0_notBefore(leaf), T.X509_get0_notAfter(leaf)) == 1 &&
                (days > 825 || (days == 825 && seconds > 0)))
                addFailure(failures, errSecCertificateValidityPeriodTooLong);
        }
        for (CFIndex i = 0; i < certificateCount; i++) {
            X509 *x = x509Of(t->certificates, i);
            if (T.X509_cmp_time(T.X509_get0_notAfter(x), &now) < 0)
                addFailure(failures, errSecCertificateExpired);
            else if (T.X509_cmp_time(T.X509_get0_notBefore(x), &now) > 0)
                addFailure(failures, errSecCertificateNotValidYet);
        }
        if (!trusted && !chainFailed)
            addFailure(failures, errSecNotTrusted);
        if (tlsServer && issuedSinceJuly2019(leaf)) {
            uint32_t usage = (T.X509_get_extension_flags(leaf) & EXFLAG_XKUSAGE) ? T.X509_get_extended_key_usage(leaf) : 0;
            if (!(usage & (XKU_SSL_SERVER | XKU_ANYEKU)))
                addFailure(failures, errSecInvalidExtendedKeyUsage);
        }
        if (tlsServer && _SecPolicyGetHostname(policy) != NULL && !nameMatches(leaf, _SecPolicyGetHostname(policy)))
            addFailure(failures, errSecHostNameMismatch);
    }

    /* What the user accepted for this certificate: the failure reported is
       then the first one not accepted. */
    Boolean excepted = false;
    CFArrayRef accepted = NULL;
    if (CFArrayGetCount(failures) > 0 && t->exceptions != NULL) {
        CFPropertyListRef list = CFPropertyListCreateWithData(NULL, t->exceptions, kCFPropertyListImmutable, NULL, NULL);
        CFDataRef digest = leafDigest(t->certificates);
        if (list && CFGetTypeID(list) == CFDictionaryGetTypeID() && digest &&
            CFEqual(digest, CFDictionaryGetValue(list, CFSTR("SHA256")) ?: (CFTypeRef)kCFNull)) {
            accepted = CFDictionaryGetValue(list, CFSTR("Failures"));
            if (accepted != NULL && CFGetTypeID(accepted) != CFArrayGetTypeID())
                accepted = NULL;
            if (accepted)
                CFRetain(accepted);
            excepted = accepted != NULL;
            for (CFIndex i = 0; excepted && i < CFArrayGetCount(failures); i++)
                if (!CFArrayContainsValue(accepted, CFRangeMake(0, CFArrayGetCount(accepted)), CFArrayGetValueAtIndex(failures, i)))
                    excepted = false;
        }
        if (list) CFRelease(list);
        if (digest) CFRelease(digest);
    }

    replace((CFTypeRef *)&t->failures, failures);
    if (CFArrayGetCount(failures) == 0) {
        t->result = kSecTrustResultUnspecified;
        t->failure = errSecSuccess;
    } else if (excepted) {
        t->result = kSecTrustResultProceed;
        t->failure = errSecSuccess;
    } else {
        int32_t first = 0;
        for (CFIndex i = 0; i < CFArrayGetCount(failures); i++) {
            CFNumberRef f = CFArrayGetValueAtIndex(failures, i);
            if (accepted == NULL || !CFArrayContainsValue(accepted, CFRangeMake(0, CFArrayGetCount(accepted)), f)) {
                CFNumberGetValue(f, kCFNumberSInt32Type, &first);
                break;
            }
        }
        t->result = kSecTrustResultRecoverableTrustFailure;
        t->failure = first;
    }
    if (accepted)
        CFRelease(accepted);
    CFRelease(failures);

    /* The chain built to a trusted anchor, as certificates. */
    if (built != NULL) {
        CFMutableArrayRef chain = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        for (int i = 0; i < T.OPENSSL_sk_num(built); i++) {
            unsigned char *der = NULL;
            int n = T.i2d_X509(T.OPENSSL_sk_value(built, i), &der);
            if (n > 0) {
                CFDataRef data = CFDataCreate(NULL, der, n);
                SecCertificateRef c = SecCertificateCreateWithData(NULL, data);
                if (c) {
                    CFArrayAppendValue(chain, c);
                    CFRelease(c);
                }
                CFRelease(data);
            }
            free(der);
        }
        replace((CFTypeRef *)&t->chain, chain);
        CFRelease(chain);
        T.OPENSSL_sk_pop_free(built, (void (*)(void *))T.X509_free);
    }
    if (untrusted) T.OPENSSL_sk_free(untrusted);
    if (ctx) T.X509_STORE_CTX_free(ctx);
    if (store) T.X509_STORE_free(store);
    t->evaluated = true;
}

static void ensureEvaluated(struct __SecTrust *t) {
    pthread_mutex_lock(&t->lock);
    if (!t->evaluated) {
        if (trustAvailable())
            evaluateLocked(t);
        else {
            t->evaluated = true;
            t->result = kSecTrustResultOtherError;
            t->failure = errSecNotAvailable;
        }
    }
    pthread_mutex_unlock(&t->lock);
}

static Boolean resultTrusted(SecTrustResultType result) {
    return result == kSecTrustResultProceed || result == kSecTrustResultUnspecified;
}

bool SecTrustEvaluateWithError(SecTrustRef trust, CFErrorRef *error) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    if (error)
        *error = NULL;
    if (t == NULL) {
        if (error)
            *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecParam, NULL);
        return false;
    }
    pthread_mutex_lock(&t->lock);
    invalidate(t);                      /* evaluated again, as asked */
    pthread_mutex_unlock(&t->lock);
    ensureEvaluated(t);
    if (resultTrusted(t->result))
        return true;
    if (error) {
        CFStringRef description = CFStringCreateWithFormat(NULL, NULL, CFSTR("The certificate is not trusted (%d)."), (int)t->failure);
        const void *keys[] = { kCFErrorDescriptionKey }, *values[] = { description };
        CFDictionaryRef info = CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, t->failure, info);
        CFRelease(info);
        CFRelease(description);
    }
    return false;
}

OSStatus SecTrustEvaluate(SecTrustRef trust, SecTrustResultType *result) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    if (t == NULL || result == NULL)
        return errSecParam;
    pthread_mutex_lock(&t->lock);
    invalidate(t);
    pthread_mutex_unlock(&t->lock);
    ensureEvaluated(t);
    *result = t->result;
    return errSecSuccess;
}

OSStatus SecTrustGetTrustResult(SecTrustRef trust, SecTrustResultType *result) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    if (t == NULL || result == NULL)
        return errSecParam;
    ensureEvaluated(t);
    *result = t->result;
    return errSecSuccess;
}

static CFArrayRef certificatesOf(struct __SecTrust *t) {
    return (t->evaluated && t->chain != NULL) ? t->chain : t->certificates;
}

CFIndex SecTrustGetCertificateCount(SecTrustRef trust) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    return t != NULL ? CFArrayGetCount(certificatesOf(t)) : 0;
}

SecCertificateRef SecTrustGetCertificateAtIndex(SecTrustRef trust, CFIndex ix) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    CFArrayRef certificates = t != NULL ? certificatesOf(t) : NULL;
    if (certificates == NULL || ix < 0 || ix >= CFArrayGetCount(certificates))
        return NULL;
    return (SecCertificateRef)CFArrayGetValueAtIndex(certificates, ix);
}

SecKeyRef SecTrustCopyPublicKey(SecTrustRef trust) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    return t != NULL ? SecCertificateCopyKey((SecCertificateRef)CFArrayGetValueAtIndex(t->certificates, 0)) : NULL;
}

/* The leaf's digest and what failed, as a property list (opaque to apps). */
CFDataRef SecTrustCopyExceptions(SecTrustRef trust) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    CFDataRef digest, data;
    CFMutableDictionaryRef list;
    if (t == NULL || !trustAvailable())
        return NULL;
    ensureEvaluated(t);
    digest = leafDigest(t->certificates);
    if (digest == NULL)
        return NULL;
    list = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(list, CFSTR("SHA256"), digest);
    if (t->failures)
        CFDictionarySetValue(list, CFSTR("Failures"), t->failures);
    else {
        CFArrayRef none = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
        CFDictionarySetValue(list, CFSTR("Failures"), none);
        CFRelease(none);
    }
    data = CFPropertyListCreateData(NULL, list, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    CFRelease(list);
    CFRelease(digest);
    return data;
}

bool SecTrustSetExceptions(SecTrustRef trust, CFDataRef exceptions) {
    struct __SecTrust *t = (struct __SecTrust *)trust;
    Boolean matches = false;
    if (t == NULL)
        return false;
    if (exceptions != NULL && trustAvailable()) {
        CFPropertyListRef list = CFPropertyListCreateWithData(NULL, exceptions, kCFPropertyListImmutable, NULL, NULL);
        CFDataRef digest = leafDigest(t->certificates);
        matches = list && CFGetTypeID(list) == CFDictionaryGetTypeID() && digest &&
                  CFEqual(digest, CFDictionaryGetValue(list, CFSTR("SHA256")) ?: (CFTypeRef)kCFNull);
        if (list) CFRelease(list);
        if (digest) CFRelease(digest);
    }
    pthread_mutex_lock(&t->lock);
    replace((CFTypeRef *)&t->exceptions, exceptions);
    invalidate(t);
    pthread_mutex_unlock(&t->lock);
    /* As the reference: true when they are for this certificate. */
    return exceptions == NULL || matches;
}
