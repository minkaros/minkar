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
CF_EXPORT const CFStringRef kMDItemFSName;
CF_EXPORT const CFStringRef kMDItemDisplayName;
CF_EXPORT const CFStringRef kMDItemFSSize;
CF_EXPORT const CFStringRef kMDItemFSCreationDate;
CF_EXPORT const CFStringRef kMDItemFSContentChangeDate;
CF_EXPORT const CFStringRef kMDItemContentModificationDate;
CF_EXPORT const CFStringRef kMDItemContentType;
CF_EXPORT const CFStringRef kMDItemContentTypeTree;
CF_EXPORT const CFStringRef kMDItemKind;

CF_EXTERN_C_END

#endif
