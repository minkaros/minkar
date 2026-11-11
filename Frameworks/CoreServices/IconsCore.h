/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Icon references: a counted reference to the icon of a type (creator and
 * type code, or a file name extension or content type). AppKit turns one
 * into an image with -[NSImage initWithIconRef:].
 */

#ifndef __ICONSCORE__
#define __ICONSCORE__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OpaqueIconRef *IconRef;
typedef UInt32 IconServicesUsageFlags;

enum {
    kOnSystemDisk = -32768,
    kSystemIconsCreator = 'macs'
};

enum {
    kGenericDocumentIcon = 'docu',
    kGenericApplicationIcon = 'APPL',
    kGenericFolderIcon = 'fldr',
    kAliasBadgeIcon = 'abdg',
    kLockedBadgeIcon = 'lbdg',
    kMountedBadgeIcon = 'mbdg',
    kSharedBadgeIcon = 'sbdg',
    kGenericHardDiskIcon = 'hdsk',
    kGenericRemovableMediaIcon = 'rmov',
    kGenericNetworkIcon = 'gnet',
    kGenericURLIcon = 'gurl',
    kGenericFileServerIcon = 'srvr',
    kGenericQueryDocumentIcon = 'qery',
    kGenericStationeryIcon = 'sdoc',
    kGenericExtensionIcon = 'extn',
    kGenericPreferencesIcon = 'pref',
    kTrashIcon = 'trsh',
    kFullTrashIcon = 'ftrh',
    kDesktopIcon = 'desk',
    kOpenFolderIcon = 'ofld',
    kSharedFolderIcon = 'shfl',
    kDropFolderIcon = 'dbox',
    kApplicationsFolderIcon = 'apps',
    kDocumentsFolderIcon = 'docs',
    kSystemFolderIcon = 'macs',
    kAlertNoteIcon = 'note',
    kAlertCautionIcon = 'caut',
    kAlertStopIcon = 'stop',
    kHelpIcon = 'help',
    kToolbarDeleteIcon = 'tdel',
    kToolbarFavoritesIcon = 'tfav',
    kToolbarHomeIcon = 'thom',
    kUnknownFSObjectIcon = 'unfs',
    kLockedIcon = 'lock',
    kErasingIcon = 'eras'
};

OSErr GetIconRef(SInt16 vRefNum, OSType creator, OSType iconType, IconRef *theIconRef);
OSErr GetIconRefFromTypeInfo(OSType inCreator, OSType inType, CFStringRef inExtension, CFStringRef inMIMEType,
                             IconServicesUsageFlags inUsageFlags, IconRef *outIconRef);
OSErr AcquireIconRef(IconRef theIconRef);
OSErr ReleaseIconRef(IconRef theIconRef);
OSErr GetIconRefOwners(IconRef theIconRef, UInt16 *owners);
Boolean IsValidIconRef(IconRef theIconRef);

/* What an icon reference stands for: its type code and, when it was made
   from one, its extension (NULL otherwise). */
OSType _IconRefGetType(IconRef theIconRef);
CFStringRef _IconRefGetExtension(IconRef theIconRef);

#ifdef __cplusplus
}
#endif

#endif
