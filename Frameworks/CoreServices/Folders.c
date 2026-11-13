/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The folder of a type is a path in its domain: the user's home, / for the
 * machine (local) and /System for the system. A folder that does not exist
 * is made when asked, except in the system's domain.
 */

#include "Folders.h"
#include "MacErrors.h"
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *homeDirectory(void) {
    const char *home = getenv("HOME");
    if (home != NULL && home[0] == '/')
        return home;
    struct passwd *pw = getpwuid(getuid());
    return pw != NULL ? pw->pw_dir : "/var/empty";
}

/* The folder's path inside its domain's top ("" for the top itself), or
   NULL when the domain has no such folder. */
static const char *subpath(int domain, OSType type) {
    switch (type) {
    case kDomainTopLevelFolderType: return "";
    case kDomainLibraryFolderType: return "Library";
    case kApplicationSupportFolderType: return "Library/Application Support";
    case kCachedDataFolderType: return "Library/Caches";
    case kPreferencesFolderType: return "Library/Preferences";
    case kFontsFolderType: return "Library/Fonts";
    case kLogsFolderType: return "Library/Logs";
    case kFrameworksFolderType: return "Library/Frameworks";
    case kExtensionFolderType: return domain == kSystemDomain ? "Library/Extensions" : NULL;
    case kApplicationsFolderType: return domain == kSystemDomain ? "Applications" : "Applications";
    }
    if (domain != kUserDomain)
        return NULL;
    switch (type) {
    case kDesktopFolderType: return "Desktop";
    case kDocumentsFolderType: return "Documents";
    case kDownloadsFolderType: return "Downloads";
    case kMovieDocumentsFolderType: return "Movies";
    case kMusicDocumentsFolderType: return "Music";
    case kPictureDocumentsFolderType: return "Pictures";
    case kPublicFolderType: return "Public";
    case kTrashFolderType: return ".Trash";
    case kCurrentUserFolderType: return "";
    }
    return NULL;
}

static int makePath(const char *path) {
    char buf[PATH_MAX];
    strlcpy(buf, path, sizeof(buf));
    for (char *p = buf + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    return mkdir(buf, 0755) != 0 && errno != EEXIST ? -1 : 0;
}

OSErr _FSFindFolderPath(FSVolumeRefNum vRefNum, OSType folderType, Boolean createFolder, char *path, size_t size) {
    int domain = vRefNum;
    if (domain == kOnSystemDisk || domain == kOnAppropriateDisk || domain >= 0)
        domain = kUserDomain;

    /* The folders that are the same in every domain. */
    switch (folderType) {
    case kSystemFolderType:
        snprintf(path, size, "/System");
        return noErr;
    case kVolumeRootFolderType:
        snprintf(path, size, "/");
        return noErr;
    case kUsersFolderType:
        snprintf(path, size, "/Users");
        return noErr;
    case kTemporaryFolderType:
    case kChewableItemsFolderType: {
        const char *tmp = getenv("TMPDIR");
        snprintf(path, size, "%s", tmp != NULL && tmp[0] == '/' ? tmp : "/tmp");
        size_t n = strlen(path);
        if (n > 1 && path[n - 1] == '/')
            path[n - 1] = '\0';
        return noErr;
    }
    }

    const char *top;
    switch (domain) {
    case kUserDomain: top = homeDirectory(); break;
    case kLocalDomain: top = ""; break;
    case kSystemDomain: top = "/System"; break;
    case kNetworkDomain: top = "/Network"; break;
    default: return paramErr;
    }
    const char *sub = subpath(domain, folderType);
    if (sub == NULL)
        return fnfErr;
    if (sub[0] == '\0')
        snprintf(path, size, "%s", top[0] != '\0' ? top : "/");
    else
        snprintf(path, size, "%s/%s", top, sub);

    struct stat st;
    if (stat(path, &st) == 0)
        return S_ISDIR(st.st_mode) ? noErr : dirNFErr;
    if (!createFolder || domain == kSystemDomain)
        return fnfErr;
    if (makePath(path) != 0)
        return errno == EACCES || errno == EPERM ? permErr : ioErr;
    return noErr;
}

OSErr FSFindFolder(FSVolumeRefNum vRefNum, OSType folderType, Boolean createFolder, FSRef *foundRef) {
    char path[PATH_MAX];
    if (foundRef == NULL)
        return paramErr;
    OSErr err = _FSFindFolderPath(vRefNum, folderType, createFolder, path, sizeof(path));
    if (err != noErr)
        return err;
    return (OSErr)FSPathMakeRef((const UInt8 *)path, foundRef, NULL);
}
