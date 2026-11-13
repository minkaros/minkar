/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The standard folders (Folders.h): a folder type in a domain (the user's,
 * the machine's, the system's), as a path and as an FSRef.
 */

#ifndef __FOLDERS__
#define __FOLDERS__

#include <CoreServices/Files.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Domains (the vRefNum argument). */
enum {
    kOnSystemDisk = -32768,
    kOnAppropriateDisk = -32767,
    kSystemDomain = -32766,
    kLocalDomain = -32765,
    kNetworkDomain = -32764,
    kUserDomain = -32763,
    kClassicDomain = -32762,
    kFolderManagerLastDomain = -32760
};

enum {
    kCreateFolder = true,
    kDontCreateFolder = false
};

/* Folder types. */
enum {
    kSystemFolderType = 'macs',
    kDesktopFolderType = 'desk',
    kTrashFolderType = 'trsh',
    kFontsFolderType = 'font',
    kPreferencesFolderType = 'pref',
    kTemporaryFolderType = 'temp',
    kChewableItemsFolderType = 'flnt',
    kApplicationSupportFolderType = 'asup',
    kApplicationsFolderType = 'apps',
    kDocumentsFolderType = 'docs',
    kDownloadsFolderType = 'down',
    kMovieDocumentsFolderType = 'mdoc',
    kMusicDocumentsFolderType = 0xB5646F63, /* 'µdoc' */
    kPictureDocumentsFolderType = 'pdoc',
    kPublicFolderType = 'pubb',
    kCachedDataFolderType = 'cach',
    kLogsFolderType = 'logs',
    kDomainTopLevelFolderType = 'dtop',
    kDomainLibraryFolderType = 'dlib',
    kCurrentUserFolderType = 'cusr',
    kUsersFolderType = 'usrs',
    kFrameworksFolderType = 'fram',
    kExtensionFolderType = 'extn',
    kVolumeRootFolderType = 'root'
};

OSErr FSFindFolder(FSVolumeRefNum vRefNum, OSType folderType, Boolean createFolder, FSRef *foundRef);
/* Minkar: the folder's path, without an FSRef. */
OSErr _FSFindFolderPath(FSVolumeRefNum vRefNum, OSType folderType, Boolean createFolder, char *path, size_t size);

#ifdef __cplusplus
}
#endif

#endif
