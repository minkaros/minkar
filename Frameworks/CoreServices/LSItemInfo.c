/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#include "CoreServices.h"
#include <limits.h>
#include <string.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

/* The type and creator in the Finder information (zero without). */
static void finderTypeCreator(const char *path, OSType *type, OSType *creator) {
    struct attrlist list = { ATTR_BIT_MAP_COUNT, 0, ATTR_CMN_FNDRINFO, 0, 0, 0, 0 };
    struct { u_int32_t length; char info[32]; } __attribute__((aligned(4), packed)) buffer;

    *type = *creator = 0;
    if (getattrlist(path, &list, &buffer, sizeof(buffer), FSOPT_NOFOLLOW) == 0) {
        const UInt8 *p = (const UInt8 *)buffer.info;
        *type = ((OSType)p[0] << 24) | ((OSType)p[1] << 16) | ((OSType)p[2] << 8) | p[3];
        *creator = ((OSType)p[4] << 24) | ((OSType)p[5] << 16) | ((OSType)p[6] << 8) | p[7];
    }
}

static Boolean isMountPoint(const char *path) {
    struct statfs fs;
    return statfs(path, &fs) == 0 && strcmp(fs.f_mntonname, path) == 0;
}

static OSStatus itemInfo(const char *path, LSRequestedInfo which, LSItemInfoRecord *out) {
    struct stat st, target;
    CFURLRef url;

    memset(out, 0, sizeof(*out));
    if (lstat(path, &st) != 0)
        return fnfErr;
    Boolean link = S_ISLNK(st.st_mode);
    if (link && stat(path, &target) == 0)
        st = target;
    Boolean directory = S_ISDIR(st.st_mode);
    url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), directory);
    CFStringRef extension = url != NULL ? CFURLCopyPathExtension(url) : NULL;
    const char *name = strrchr(path, '/');
    name = name != NULL ? name + 1 : path;

    if (link)
        out->flags |= kLSItemInfoIsSymlink;
    if (name[0] == '.' || (st.st_flags & UF_HIDDEN))
        out->flags |= kLSItemInfoIsInvisible;
    if (directory) {
        Boolean app = extension != NULL && CFStringCompare(extension, CFSTR("app"), kCFCompareCaseInsensitive) == kCFCompareEqualTo;
        Boolean package = app;
        if (!package && extension != NULL && CFStringGetLength(extension) > 0) {
            CFStringRef type = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, extension, CFSTR("public.directory"));
            package = type != NULL && UTTypeConformsTo(type, CFSTR("com.apple.package"));
            if (type != NULL)
                CFRelease(type);
        }
        if (isMountPoint(path))
            out->flags |= kLSItemInfoIsVolume;
        if (package)
            out->flags |= kLSItemInfoIsPackage;
        else
            out->flags |= kLSItemInfoIsContainer;
        if (app)
            out->flags |= kLSItemInfoIsApplication | kLSItemInfoIsNativeApp;
    } else {
        out->flags |= kLSItemInfoIsPlainFile;
        if ((st.st_mode & S_IXUSR) && extension == NULL)
            out->flags |= kLSItemInfoIsApplication | kLSItemInfoIsNativeApp;
    }
    if (which & kLSRequestTypeCreator)
        finderTypeCreator(path, &out->filetype, &out->creator);
    if ((which & kLSRequestExtension) && extension != NULL && CFStringGetLength(extension) > 0)
        out->extension = CFRetain(extension);
    if (extension != NULL)
        CFRelease(extension);
    if (url != NULL)
        CFRelease(url);
    if ((which & (kLSRequestAllFlags | kLSRequestAppTypeFlags | kLSRequestBasicFlagsOnly | kLSRequestIconAndKind |
                  kLSRequestExtensionFlagsOnly)) == 0)
        out->flags = 0;
    return noErr;
}

OSStatus LSCopyItemInfoForURL(CFURLRef inURL, LSRequestedInfo inWhichInfo, LSItemInfoRecord *outItemInfo) {
    char path[PATH_MAX];
    if (inURL == NULL || outItemInfo == NULL)
        return paramErr;
    if (!CFURLGetFileSystemRepresentation(inURL, true, (UInt8 *)path, sizeof(path)))
        return kLSDataUnavailableErr;
    return itemInfo(path, inWhichInfo, outItemInfo);
}

OSStatus LSCopyItemInfoForRef(const FSRef *inItemRef, LSRequestedInfo inWhichInfo, LSItemInfoRecord *outItemInfo) {
    char path[PATH_MAX];
    OSStatus status;
    if (outItemInfo == NULL)
        return paramErr;
    status = FSRefMakePath(inItemRef, (UInt8 *)path, sizeof(path));
    return status != noErr ? status : itemInfo(path, inWhichInfo, outItemInfo);
}

static CFURLRef urlOfRef(const FSRef *ref) {
    char path[PATH_MAX];
    struct stat st;
    if (ref == NULL || FSRefMakePath(ref, (UInt8 *)path, sizeof(path)) != noErr)
        return NULL;
    return CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path),
                                                   stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

OSStatus LSOpenURLsWithRole(CFArrayRef inURLs, LSRolesMask inRole, const AEKeyDesc *inAEParam,
                            const LSApplicationParameters *inAppParams, struct ProcessSerialNumber *outPSNs,
                            CFIndex inMaxPSNCount) {
    LSLaunchURLSpec spec = { NULL, inURLs, NULL, kLSLaunchDefaults, NULL };
    OSStatus status;

    if (outPSNs != NULL && inMaxPSNCount > 0)
        memset(outPSNs, 0, (size_t)inMaxPSNCount * 8);
    if (inAppParams != NULL && inAppParams->application != NULL) {
        spec.appURL = urlOfRef(inAppParams->application);
        if (spec.appURL == NULL)
            return fnfErr;
        spec.launchFlags = inAppParams->flags != 0 ? inAppParams->flags : kLSLaunchDefaults;
    }
    if (inURLs == NULL && spec.appURL == NULL)
        return paramErr;
    status = LSOpenFromURLSpecWithArguments(&spec, inAppParams != NULL ? inAppParams->argv : NULL, NULL);
    if (spec.appURL != NULL)
        CFRelease(spec.appURL);
    return status;
}

OSStatus LSOpenApplication(const LSApplicationParameters *appParams, struct ProcessSerialNumber *outPSN) {
    if (appParams == NULL || appParams->application == NULL)
        return paramErr;
    return LSOpenURLsWithRole(NULL, kLSRolesAll, NULL, appParams, outPSN, outPSN != NULL ? 1 : 0);
}

OSStatus LSOpenFSRef(const FSRef *inRef, FSRef *outLaunchedRef) {
    CFURLRef url = urlOfRef(inRef), launched = NULL;
    OSStatus status;
    if (url == NULL)
        return fnfErr;
    status = LSOpenCFURLRef(url, outLaunchedRef != NULL ? &launched : NULL);
    if (launched != NULL) {
        char path[PATH_MAX];
        if (CFURLGetFileSystemRepresentation(launched, true, (UInt8 *)path, sizeof(path)))
            FSPathMakeRef((const UInt8 *)path, outLaunchedRef, NULL);
        CFRelease(launched);
    }
    CFRelease(url);
    return status;
}
