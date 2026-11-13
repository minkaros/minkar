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

CF_EXTERN_C_END

#endif
