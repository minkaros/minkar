/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The font manager: the fonts the system finds (fontconfig's, shared by
 * CoreText and AppKit) and the ones an app registers for itself.
 */

#ifndef __CTFONTMANAGER__
#define __CTFONTMANAGER__

#include <CoreText/CoreTextExport.h>
#include <CoreText/CTFontDescriptor.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdbool.h>

typedef CF_ENUM(uint32_t, CTFontManagerScope) {
    kCTFontManagerScopeNone = 0,
    kCTFontManagerScopeProcess = 1,
    kCTFontManagerScopePersistent = 2,
    kCTFontManagerScopeSession = 3,
    kCTFontManagerScopeUser = 2,
};

CORETEXT_EXPORT const CFStringRef kCTFontManagerErrorDomain;
CORETEXT_EXPORT const CFStringRef kCTFontManagerErrorFontURLsKey;

typedef CF_ENUM(CFIndex, CTFontManagerError) {
    kCTFontManagerErrorFileNotFound = 101,
    kCTFontManagerErrorInsufficientPermissions = 102,
    kCTFontManagerErrorUnrecognizedFormat = 103,
    kCTFontManagerErrorInvalidFontData = 104,
    kCTFontManagerErrorAlreadyRegistered = 105,
    kCTFontManagerErrorExceededResourceLimit = 106,
    kCTFontManagerErrorNotRegistered = 201,
    kCTFontManagerErrorInUse = 202,
    kCTFontManagerErrorSystemRequired = 203,
};

/* Registered for the process: the files' fonts are found by name and
   family as the system's are. */
CORETEXT_EXPORT bool CTFontManagerRegisterFontsForURL(CFURLRef fontURL, CTFontManagerScope scope, CFErrorRef *error);
CORETEXT_EXPORT bool CTFontManagerUnregisterFontsForURL(CFURLRef fontURL, CTFontManagerScope scope, CFErrorRef *error);
#ifdef __BLOCKS__
CORETEXT_EXPORT void CTFontManagerRegisterFontURLs(CFArrayRef fontURLs, CTFontManagerScope scope, bool enabled,
    bool (^registrationHandler)(CFArrayRef errors, bool done));
CORETEXT_EXPORT void CTFontManagerUnregisterFontURLs(CFArrayRef fontURLs, CTFontManagerScope scope,
    bool (^registrationHandler)(CFArrayRef errors, bool done));
#endif

CORETEXT_EXPORT CFArrayRef CTFontManagerCopyAvailableFontFamilyNames(void);
CORETEXT_EXPORT CFArrayRef CTFontManagerCopyAvailablePostScriptNames(void);

/* The faces of a font file, and a face in memory (a web font). */
CORETEXT_EXPORT CFArrayRef CTFontManagerCreateFontDescriptorsFromURL(CFURLRef fileURL);
CORETEXT_EXPORT CTFontDescriptorRef CTFontManagerCreateFontDescriptorFromData(CFDataRef data);

/* Minkar: the fontconfig configuration every font lookup uses (an
   FcConfig *), with the fonts registered for the process in it. */
CORETEXT_EXPORT void *_CTFontManagerSharedFontConfig(void);

#endif
