/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The LaunchServices calls that take file system references and Apple
 * event parameters: what an item is (LSCopyItemInfoForURL), and opening
 * items with a role or an application (LSOpenURLsWithRole).
 */

#ifndef __LSITEMINFO__
#define __LSITEMINFO__

#include <CoreServices/Files.h>
#include <CoreServices/MacMemory.h>
#if __has_include(<AE/AE.h>)
#include <AE/AE.h>
#include <LaunchServices/LaunchServices.h>
#else
#include "AE/AE.h"
#include "LaunchServices/LaunchServices.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct ProcessSerialNumber;

typedef CF_OPTIONS(OptionBits, LSRequestedInfo) {
    kLSRequestExtension = 0x00000001,
    kLSRequestTypeCreator = 0x00000002,
    kLSRequestBasicFlagsOnly = 0x00000004,
    kLSRequestAppTypeFlags = 0x00000008,
    kLSRequestAllFlags = 0x00000010,
    kLSRequestIconAndKind = 0x00000020,
    kLSRequestExtensionFlagsOnly = 0x00000040,
    kLSRequestAllInfo = (LSRequestedInfo)0xFFFFFFFF
};

typedef CF_OPTIONS(OptionBits, LSItemInfoFlags) {
    kLSItemInfoIsPlainFile = 0x00000001,
    kLSItemInfoIsPackage = 0x00000002,
    kLSItemInfoIsApplication = 0x00000004,
    kLSItemInfoIsContainer = 0x00000008,
    kLSItemInfoIsAliasFile = 0x00000010,
    kLSItemInfoIsSymlink = 0x00000020,
    kLSItemInfoIsInvisible = 0x00000040,
    kLSItemInfoIsNativeApp = 0x00000080,
    kLSItemInfoIsClassicApp = 0x00000100,
    kLSItemInfoAppPrefersNative = 0x00000200,
    kLSItemInfoAppPrefersClassic = 0x00000400,
    kLSItemInfoAppIsScriptable = 0x00000800,
    kLSItemInfoIsVolume = 0x00001000,
    kLSItemInfoExtensionIsHidden = 0x00100000
};

typedef struct LSItemInfoRecord {
    LSItemInfoFlags flags;
    OSType filetype;
    OSType creator;
    CFStringRef extension;
} LSItemInfoRecord;

typedef CF_OPTIONS(OptionBits, LSLaunchFlagsOptions) {
    kLSLaunchInhibitBGOnly = 0x00000080,
    kLSLaunchNoParams = 0x00000800,
    kLSLaunchStartClassic = 0x00020000,
    kLSLaunchInClassic = 0x00040000
};

typedef struct LSApplicationParameters {
    CFIndex version;
    LSLaunchFlags flags;
    const FSRef *application;
    void *asyncLaunchRefCon;
    CFDictionaryRef environment;
    CFArrayRef argv;
    AppleEvent *initialEvent;
} LSApplicationParameters;

/* The item's flags, type and creator (from its Finder information) and
   extension; the caller releases the extension. */
OSStatus LSCopyItemInfoForURL(CFURLRef inURL, LSRequestedInfo inWhichInfo, LSItemInfoRecord *outItemInfo);
OSStatus LSCopyItemInfoForRef(const FSRef *inItemRef, LSRequestedInfo inWhichInfo, LSItemInfoRecord *outItemInfo);
/* Opens the URLs, with inAppParams' application when it names one, else
   each with its own. outPSNs gets nothing back (it is cleared). */
OSStatus LSOpenURLsWithRole(CFArrayRef inURLs, LSRolesMask inRole, const AEKeyDesc *inAEParam,
                            const LSApplicationParameters *inAppParams, struct ProcessSerialNumber *outPSNs,
                            CFIndex inMaxPSNCount);
OSStatus LSOpenApplication(const LSApplicationParameters *appParams, struct ProcessSerialNumber *outPSN);
OSStatus LSOpenFSRef(const FSRef *inRef, FSRef *outLaunchedRef);

#ifdef __cplusplus
}
#endif

#endif
