/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "Security.h"
#include "SecInternal.h"

static const struct { OSStatus status; const char *message; } kMessages[] = {
    { errSecSuccess, "No error." },
    { errSecUnimplemented, "Function or operation not implemented." },
    { errSecIO, "I/O error." },
    { errSecParam, "One or more parameters passed to a function were not valid." },
    { errSecWrPerm, "Write permissions error." },
    { errSecAllocate, "Failed to allocate memory." },
    { errSecUserCanceled, "User canceled the operation." },
    { errSecNotAvailable, "No keychain is available." },
    { errSecAuthFailed, "The user name or passphrase you entered is not correct." },
    { errSecNoSuchKeychain, "The specified keychain could not be found." },
    { errSecDuplicateItem, "The specified item already exists in the keychain." },
    { errSecItemNotFound, "The specified item could not be found in the keychain." },
    { errSecInteractionNotAllowed, "User interaction is not allowed." },
    { errSecDecode, "Unable to decode the provided data." },
    { errSecVerifyFailed, "A cryptographic verification failure has occurred." },
    { errSecUnknownFormat, "The item you are trying to import has an unknown format." },
    { errSecUnsupportedFormat, "The specified import or export format is not supported." },
    { errSecInvalidAlgorithm, "An invalid algorithm was encountered." },
    { errSecCSUnsigned, "The code object is not signed at all." },
    { errSecCSSignatureFailed, "Invalid signature (code or signature have been modified)." },
    { errSecCSReqFailed, "The code failed to satisfy one of the code requirements." },
    { errSecCSStaticCodeNotFound, "The code object could not be found." },
    { -60005, "The authorization was denied." },
    { -60006, "The authorization was canceled by the user." },
    { -60007, "The authorization required interaction, which was not allowed." },
};

CFStringRef SecCopyErrorMessageString(OSStatus status, void *reserved) {
    for (size_t i = 0; i < sizeof(kMessages) / sizeof(kMessages[0]); i++)
        if (kMessages[i].status == status)
            return CFStringCreateWithCString(NULL, kMessages[i].message, kCFStringEncodingUTF8);
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("OSStatus %d"), (int)status);
}

/* One CF class per type; the instance's finalizer is its own. */
CFTypeID _SecRegisterClass(CFTypeID *storage, const char *name, void (*finalize)(CFTypeRef)) {
    if (*storage == 0) {
        CFRuntimeClass *cls = calloc(1, sizeof(CFRuntimeClass));
        cls->className = name;
        cls->finalize = finalize;
        *storage = _CFRuntimeRegisterClass(cls);
    }
    return *storage;
}

void *_SecCreateInstance(CFTypeID type, size_t size) {
    return (void *)_CFRuntimeCreateInstance(NULL, type, (CFIndex)(size - sizeof(CFRuntimeBase)), NULL);
}
