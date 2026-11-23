/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#ifndef __CORESERVICES_METADATA__
#define __CORESERVICES_METADATA__

#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

/* The names of file attributes in metadata queries (NSMetadataQuery,
 * NSMetadataItem's valueForAttribute:). */
CF_EXPORT const CFStringRef kMDItemPath;
/* Where a downloaded file came from: its URLs (the page, the file). */
CF_EXPORT const CFStringRef kMDItemWhereFroms;
CF_EXPORT const CFStringRef kMDItemFSName;
CF_EXPORT const CFStringRef kMDItemDisplayName;
CF_EXPORT const CFStringRef kMDItemFSSize;
CF_EXPORT const CFStringRef kMDItemFSCreationDate;
CF_EXPORT const CFStringRef kMDItemFSContentChangeDate;
CF_EXPORT const CFStringRef kMDItemContentModificationDate;
CF_EXPORT const CFStringRef kMDItemContentType;
CF_EXPORT const CFStringRef kMDItemContentTypeTree;
CF_EXPORT const CFStringRef kMDItemKind;
CF_EXPORT const CFStringRef kMDItemContentCreationDate;
CF_EXPORT const CFStringRef kMDItemLastUsedDate;
CF_EXPORT const CFStringRef kMDItemFSOwnerUserID;
CF_EXPORT const CFStringRef kMDItemFSOwnerGroupID;
CF_EXPORT const CFStringRef kMDItemFSIsExtensionHidden;
CF_EXPORT const CFStringRef kMDItemFSInvisible;
CF_EXPORT const CFStringRef kMDItemCFBundleIdentifier;
CF_EXPORT const CFStringRef kMDItemVersion;
/* Sizes (a package's: its files'), when it was put where it is, the
   Finder fields (none here), a bundle's architectures and copyright. */
CF_EXPORT const CFStringRef kMDItemLogicalSize;
CF_EXPORT const CFStringRef kMDItemPhysicalSize;
CF_EXPORT const CFStringRef kMDItemDateAdded;
CF_EXPORT const CFStringRef kMDItemFSNodeCount;
CF_EXPORT const CFStringRef kMDItemFSLabel;
CF_EXPORT const CFStringRef kMDItemFSFinderFlags;
CF_EXPORT const CFStringRef kMDItemFSCreatorCode;
CF_EXPORT const CFStringRef kMDItemFSTypeCode;
CF_EXPORT const CFStringRef kMDItemExecutableArchitectures;
CF_EXPORT const CFStringRef kMDItemCopyright;
CF_EXPORT const CFStringRef kMDItemAlternateNames;
CF_EXPORT const CFStringRef kMDItemDocumentIdentifier;

/* A file's metadata, read from the file system and its type when asked
   (there is no index: mdfind searches the disk). */
typedef struct __MDItem *MDItemRef;

CF_EXPORT CFTypeID MDItemGetTypeID(void);
CF_EXPORT MDItemRef MDItemCreate(CFAllocatorRef allocator, CFStringRef path);
CF_EXPORT MDItemRef MDItemCreateWithURL(CFAllocatorRef allocator, CFURLRef url);
CF_EXPORT CFTypeRef MDItemCopyAttribute(MDItemRef item, CFStringRef name);
CF_EXPORT CFDictionaryRef MDItemCopyAttributes(MDItemRef item, CFArrayRef names);
CF_EXPORT CFArrayRef MDItemCopyAttributeNames(MDItemRef item);
CF_EXPORT CFDictionaryRef MDItemCopyAttributeList(MDItemRef item, ...);

CF_EXTERN_C_END

#endif
