/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Transforms run synchronously: executing one first gets its input (its
 * input attribute, or the output of the transform connected to it), then
 * makes its output. A group executes its last transform (the one whose
 * output goes nowhere).
 */

#include "Security.h"
#include "SecInternal.h"
#include <CommonCrypto/CommonDigest.h>

const CFStringRef kSecTransformErrorDomain = CFSTR("com.apple.security.transforms.error");
const CFStringRef kSecTransformInputAttributeName = CFSTR("INPUT");
const CFStringRef kSecTransformOutputAttributeName = CFSTR("OUTPUT");
const CFStringRef kSecTransformDebugAttributeName = CFSTR("DEBUG");
const CFStringRef kSecTransformTransformName = CFSTR("NAME");
const CFStringRef kSecTransformAbortAttributeName = CFSTR("ABORT");
const CFStringRef kSecDigestMD5 = CFSTR("MD5");
const CFStringRef kSecDigestSHA1 = CFSTR("SHA1");
const CFStringRef kSecDigestSHA2 = CFSTR("SHA2");
const CFStringRef kSecDigestTypeAttribute = CFSTR("DigestType");
const CFStringRef kSecDigestLengthAttribute = CFSTR("DigestLength");
const CFStringRef kSecKeyAttributeName = CFSTR("KEY");
const CFStringRef kSecSignatureAttributeName = CFSTR("Signature");
const CFStringRef kSecInputIsAttributeName = CFSTR("InputIs");
const CFStringRef kSecInputIsPlainText = CFSTR("PlainText");
const CFStringRef kSecInputIsDigest = CFSTR("Digest");
const CFStringRef kSecInputIsRaw = CFSTR("Raw");

typedef enum { kindRead, kindDigest, kindVerify, kindGroup } Kind;

struct __SecTransform {
    CFRuntimeBase base;
    Kind kind;
    CFMutableDictionaryRef attributes;
    CFTypeRef source;            /* the transform whose output is the input */
    CFMutableArrayRef members;   /* a group's transforms */
    Boolean consumed;            /* its output goes to another transform */
};
typedef struct __SecTransform Transform;

static void transformFinalize(CFTypeRef cf) {
    Transform *t = (Transform *)cf;
    if (t->attributes != NULL) CFRelease(t->attributes);
    if (t->source != NULL) CFRelease(t->source);
    if (t->members != NULL) CFRelease(t->members);
}

SEC_TYPE(SecTransform, "SecTransform", transformFinalize)

CFTypeID SecGroupTransformGetTypeID(void) {
    return SecTransformGetTypeID();
}

static Transform *create(Kind kind) {
    Transform *t = _SecCreateInstance(SecTransformGetTypeID(), sizeof(Transform));
    if (t != NULL) {
        t->kind = kind;
        t->attributes = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    return t;
}

static void setError(CFErrorRef *error, CFIndex code) {
    if (error != NULL)
        *error = CFErrorCreate(NULL, kSecTransformErrorDomain, code, NULL);
}

SecGroupTransformRef SecTransformCreateGroupTransform(void) {
    Transform *g = create(kindGroup);
    g->members = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    return g;
}

SecTransformRef SecTransformCreateReadTransformWithReadStream(CFReadStreamRef inputStream) {
    Transform *t;
    if (inputStream == NULL)
        return NULL;
    t = create(kindRead);
    CFDictionarySetValue(t->attributes, kSecTransformInputAttributeName, inputStream);
    return t;
}

SecTransformRef SecDigestTransformCreate(CFTypeRef digestType, CFIndex digestLength, CFErrorRef *error) {
    Transform *t;
    CFIndex length = digestLength;
    if (digestType == NULL)
        digestType = kSecDigestSHA2;
    if (!CFEqual(digestType, kSecDigestMD5) && !CFEqual(digestType, kSecDigestSHA1) && !CFEqual(digestType, kSecDigestSHA2)) {
        setError(error, kSecTransformErrorInvalidAlgorithm);
        return NULL;
    }
    if (CFEqual(digestType, kSecDigestSHA2)) {
        if (length == 0)
            length = 512;
        if (length != 224 && length != 256 && length != 384 && length != 512) {
            setError(error, kSecTransformErrorInvalidLength);
            return NULL;
        }
    }
    t = create(kindDigest);
    CFDictionarySetValue(t->attributes, kSecDigestTypeAttribute, digestType);
    CFNumberRef n = CFNumberCreate(NULL, kCFNumberCFIndexType, &length);
    CFDictionarySetValue(t->attributes, kSecDigestLengthAttribute, n);
    CFRelease(n);
    return t;
}

SecTransformRef SecVerifyTransformCreate(SecKeyRef key, CFDataRef signature, CFErrorRef *error) {
    Transform *t;
    if (key == NULL || signature == NULL) {
        setError(error, kSecTransformErrorMissingParameter);
        return NULL;
    }
    t = create(kindVerify);
    CFDictionarySetValue(t->attributes, kSecKeyAttributeName, key);
    CFDictionarySetValue(t->attributes, kSecSignatureAttributeName, signature);
    return t;
}

Boolean SecTransformSetAttribute(SecTransformRef transformRef, CFStringRef key, CFTypeRef value, CFErrorRef *error) {
    Transform *t = (Transform *)transformRef;
    if (t == NULL || key == NULL) {
        setError(error, kSecTransformInvalidArgument);
        return false;
    }
    if (value == NULL)
        CFDictionaryRemoveValue(t->attributes, key);
    else
        CFDictionarySetValue(t->attributes, key, value);
    return true;
}

CFTypeRef SecTransformGetAttribute(SecTransformRef transformRef, CFStringRef key) {
    Transform *t = (Transform *)transformRef;
    return (t != NULL && key != NULL) ? CFDictionaryGetValue(t->attributes, key) : NULL;
}

static void addMember(Transform *group, CFTypeRef member) {
    if (group != NULL && !CFArrayContainsValue(group->members, CFRangeMake(0, CFArrayGetCount(group->members)), member))
        CFArrayAppendValue(group->members, member);
}

SecGroupTransformRef SecTransformConnectTransforms(SecTransformRef sourceTransformRef, CFStringRef sourceAttributeName,
                                                   SecTransformRef destinationTransformRef, CFStringRef destinationAttributeName,
                                                   SecGroupTransformRef group, CFErrorRef *error) {
    Transform *source = (Transform *)sourceTransformRef, *destination = (Transform *)destinationTransformRef;
    if (source == NULL || destination == NULL || group == NULL ||
        (sourceAttributeName != NULL && !CFEqual(sourceAttributeName, kSecTransformOutputAttributeName)) ||
        (destinationAttributeName != NULL && !CFEqual(destinationAttributeName, kSecTransformInputAttributeName))) {
        setError(error, kSecTransformErrorInvalidConnection);
        return NULL;
    }
    if (destination->source != NULL)
        CFRelease(destination->source);
    destination->source = CFRetain(source);
    source->consumed = true;
    addMember((Transform *)group, source);
    addMember((Transform *)group, destination);
    return group;
}

static CFDataRef readAll(CFReadStreamRef stream) {
    CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
    UInt8 buffer[65536];
    CFIndex n;
    if (CFReadStreamGetStatus(stream) == kCFStreamStatusNotOpen && !CFReadStreamOpen(stream)) {
        CFRelease(data);
        return NULL;
    }
    while ((n = CFReadStreamRead(stream, buffer, sizeof(buffer))) > 0)
        CFDataAppendBytes(data, buffer, n);
    CFReadStreamClose(stream);
    if (n < 0) {
        CFRelease(data);
        return NULL;
    }
    return data;
}

static CFDataRef digest(CFStringRef type, CFIndex length, CFDataRef input) {
    uint8_t out[CC_SHA512_DIGEST_LENGTH];
    const void *bytes = CFDataGetBytePtr(input);
    CC_LONG n = (CC_LONG)CFDataGetLength(input);
    CFIndex size;
    if (CFEqual(type, kSecDigestMD5)) { CC_MD5(bytes, n, out); size = CC_MD5_DIGEST_LENGTH; }
    else if (CFEqual(type, kSecDigestSHA1)) { CC_SHA1(bytes, n, out); size = CC_SHA1_DIGEST_LENGTH; }
    else if (length == 224) { CC_SHA224(bytes, n, out); size = CC_SHA224_DIGEST_LENGTH; }
    else if (length == 256) { CC_SHA256(bytes, n, out); size = CC_SHA256_DIGEST_LENGTH; }
    else if (length == 384) { CC_SHA384(bytes, n, out); size = CC_SHA384_DIGEST_LENGTH; }
    else { CC_SHA512(bytes, n, out); size = CC_SHA512_DIGEST_LENGTH; }
    return CFDataCreate(NULL, out, size);
}

/* The digest name libcrypto knows for the verify transform's digest. */
static const char *digestName(Transform *t) {
    CFTypeRef type = CFDictionaryGetValue(t->attributes, kSecDigestTypeAttribute);
    CFNumberRef lengthNumber = CFDictionaryGetValue(t->attributes, kSecDigestLengthAttribute);
    CFIndex length = 0;
    if (lengthNumber != NULL)
        CFNumberGetValue(lengthNumber, kCFNumberCFIndexType, &length);
    if (type == NULL || CFEqual(type, kSecDigestSHA1)) return "SHA1";
    if (CFEqual(type, kSecDigestMD5)) return "MD5";
    switch (length) {
    case 224: return "SHA224";
    case 384: return "SHA384";
    case 512: return "SHA512";
    default: return "SHA256";
    }
}

static CFTypeRef execute(Transform *t, CFErrorRef *error);

static CFTypeRef inputOf(Transform *t, CFErrorRef *error) {
    if (t->source != NULL)
        return execute((Transform *)t->source, error);
    CFTypeRef input = CFDictionaryGetValue(t->attributes, kSecTransformInputAttributeName);
    if (input == NULL) {
        setError(error, kSecTransformErrorMissingParameter);
        return NULL;
    }
    return CFRetain(input);
}

static CFTypeRef execute(Transform *t, CFErrorRef *error) {
    CFTypeRef input, output = NULL;

    if (t->kind == kindGroup) {
        CFIndex count = CFArrayGetCount(t->members);
        for (CFIndex i = 0; i < count; i++) {
            Transform *member = (Transform *)CFArrayGetValueAtIndex(t->members, i);
            if (!member->consumed)
                return execute(member, error);
        }
        setError(error, kSecTransformErrorInvalidOperation);
        return NULL;
    }
    input = inputOf(t, error);
    if (input == NULL)
        return NULL;
    if (t->kind == kindRead) {
        if (CFGetTypeID(input) == CFReadStreamGetTypeID())
            output = readAll((CFReadStreamRef)input);
        else if (CFGetTypeID(input) == CFDataGetTypeID())
            output = CFRetain(input);
        if (output == NULL)
            setError(error, kSecTransformErrorInvalidInput);
    } else if (CFGetTypeID(input) != CFDataGetTypeID())
        setError(error, kSecTransformErrorInvalidType);
    else if (t->kind == kindDigest) {
        CFIndex length = 0;
        CFNumberGetValue(CFDictionaryGetValue(t->attributes, kSecDigestLengthAttribute), kCFNumberCFIndexType, &length);
        output = digest(CFDictionaryGetValue(t->attributes, kSecDigestTypeAttribute), length, input);
    } else if (t->kind == kindVerify) {
        CFDataRef signature = CFDictionaryGetValue(t->attributes, kSecSignatureAttributeName);
        CFTypeRef inputIs = CFDictionaryGetValue(t->attributes, kSecInputIsAttributeName);
        Boolean isDigest = inputIs != NULL && CFEqual(inputIs, kSecInputIsDigest);
        int ok = _SecCryptoVerify(_SecKeyGetPKey((SecKeyRef)CFDictionaryGetValue(t->attributes, kSecKeyAttributeName)),
                                  digestName(t), CFDataGetBytePtr(input), (size_t)CFDataGetLength(input), isDigest,
                                  CFDataGetBytePtr(signature), (size_t)CFDataGetLength(signature));
        if (ok < 0)
            setError(error, kSecTransformErrorInvalidAlgorithm);
        else
            output = CFRetain(ok ? kCFBooleanTrue : kCFBooleanFalse);
    }
    CFRelease(input);
    return output;
}

CFTypeRef SecTransformExecute(SecTransformRef transformRef, CFErrorRef *errorRef) {
    if (errorRef != NULL)
        *errorRef = NULL;
    if (transformRef == NULL) {
        setError(errorRef, kSecTransformInvalidArgument);
        return NULL;
    }
    return execute((Transform *)transformRef, errorRef);
}
