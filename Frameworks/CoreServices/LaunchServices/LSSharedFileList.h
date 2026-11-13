/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Shared file lists: the user's lists of items kept by the system, such
 * as the apps opened at login.
 */

#ifndef __LSSHAREDFILELIST__
#define __LSSHAREDFILELIST__

#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN

typedef struct OpaqueLSSharedFileListRef *LSSharedFileListRef;
typedef struct OpaqueLSSharedFileListItemRef *LSSharedFileListItemRef;

/* The lists. */
CF_EXPORT const CFStringRef kLSSharedFileListSessionLoginItems;
CF_EXPORT const CFStringRef kLSSharedFileListGlobalLoginItems;
CF_EXPORT const CFStringRef kLSSharedFileListRecentApplicationItems;
CF_EXPORT const CFStringRef kLSSharedFileListRecentDocumentItems;

/* Where to insert an item: first or last. */
CF_EXPORT const LSSharedFileListItemRef kLSSharedFileListItemBeforeFirst;
CF_EXPORT const LSSharedFileListItemRef kLSSharedFileListItemLast;

/* An item's properties. */
CF_EXPORT const CFStringRef kLSSharedFileListLoginItemHidden;

typedef struct OpaqueIconRef *IconRef;

typedef CF_OPTIONS(UInt32, LSSharedFileListResolutionFlags) {
	kLSSharedFileListNoUserInteraction = 1 << 0,
	kLSSharedFileListDoNotMountVolumes = 1 << 1,
};

CF_EXPORT CFTypeID LSSharedFileListGetTypeID(void);
CF_EXPORT CFTypeID LSSharedFileListItemGetTypeID(void);

/* A list, its items now (the seed changes with every change), and
   adding, moving or taking out items. An item added that is already in
   the list moves there. */
CF_EXPORT LSSharedFileListRef LSSharedFileListCreate(CFAllocatorRef inAllocator, CFStringRef inListType,
    CFTypeRef listOptions) CF_RETURNS_RETAINED;
CF_EXPORT CFArrayRef LSSharedFileListCopySnapshot(LSSharedFileListRef inList, UInt32 *outSnapshotSeed)
    CF_RETURNS_RETAINED;
CF_EXPORT LSSharedFileListItemRef LSSharedFileListInsertItemURL(LSSharedFileListRef inList,
    LSSharedFileListItemRef insertAfterThisItem, CFStringRef inDisplayName, IconRef inIconRef, CFURLRef inURL,
    CFDictionaryRef inPropertiesToSet, CFArrayRef inPropertiesToClear) CF_RETURNS_RETAINED;
CF_EXPORT OSStatus LSSharedFileListItemRemove(LSSharedFileListRef inList, LSSharedFileListItemRef inItem);
CF_EXPORT OSStatus LSSharedFileListRemoveAllItems(LSSharedFileListRef inList);

/* An item's URL (NULL for a file that is gone), name, ID and properties. */
CF_EXPORT CFURLRef LSSharedFileListItemCopyResolvedURL(LSSharedFileListItemRef inItem,
    LSSharedFileListResolutionFlags inFlags, CFErrorRef *outError) CF_RETURNS_RETAINED;
CF_EXPORT CFStringRef LSSharedFileListItemCopyDisplayName(LSSharedFileListItemRef inItem) CF_RETURNS_RETAINED;
CF_EXPORT UInt32 LSSharedFileListItemGetID(LSSharedFileListItemRef inItem);
CF_EXPORT CFTypeRef LSSharedFileListItemCopyProperty(LSSharedFileListItemRef inItem, CFStringRef inPropertyName)
    CF_RETURNS_RETAINED;

CF_EXTERN_C_END

#endif
