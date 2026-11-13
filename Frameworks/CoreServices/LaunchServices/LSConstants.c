/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A key's value is its name without "k" and "Key" (the quarantine keys are
 * the ones Foundation keeps in a file's quarantine properties).
 */

#include "LSQuarantine.h"
#include "LSSharedFileList.h"
#include "LSInfo.h"

#define KEY(name, value) const CFStringRef name = CFSTR(value)

KEY(kLSQuarantineAgentNameKey, "LSQuarantineAgentName");
KEY(kLSQuarantineAgentBundleIdentifierKey, "LSQuarantineAgentBundleIdentifier");
KEY(kLSQuarantineTimeStampKey, "LSQuarantineTimeStamp");
KEY(kLSQuarantineTypeKey, "LSQuarantineType");
KEY(kLSQuarantineDataURLKey, "LSQuarantineDataURL");
KEY(kLSQuarantineOriginURLKey, "LSQuarantineOriginURL");
KEY(kLSQuarantineTypeWebDownload, "LSQuarantineTypeWebDownload");
KEY(kLSQuarantineTypeOtherDownload, "LSQuarantineTypeOtherDownload");
KEY(kLSQuarantineTypeEmailAttachment, "LSQuarantineTypeEmailAttachment");
KEY(kLSQuarantineTypeInstantMessageAttachment, "LSQuarantineTypeInstantMessageAttachment");
KEY(kLSQuarantineTypeCalendarEventAttachment, "LSQuarantineTypeCalendarEventAttachment");
KEY(kLSQuarantineTypeOtherAttachment, "LSQuarantineTypeOtherAttachment");

KEY(kLSItemContentType, "LSItemContentType");
KEY(kLSItemFileType, "LSItemFileType");
KEY(kLSItemFileCreator, "LSItemFileCreator");
KEY(kLSItemExtension, "LSItemExtension");
KEY(kLSItemDisplayName, "LSItemDisplayName");
KEY(kLSItemDisplayKind, "LSItemDisplayKind");
KEY(kLSItemRoleHandlerDisplayName, "LSItemRoleHandlerDisplayName");
KEY(kLSItemIsInvisible, "LSItemIsInvisible");
KEY(kLSItemExtensionIsHidden, "LSItemExtensionIsHidden");
KEY(kLSItemQuarantineProperties, "LSItemQuarantineProperties");

KEY(kLSSharedFileListSessionLoginItems, "LSSharedFileListSessionLoginItems");
KEY(kLSSharedFileListGlobalLoginItems, "LSSharedFileListGlobalLoginItems");
KEY(kLSSharedFileListRecentApplicationItems, "LSSharedFileListRecentApplicationItems");
KEY(kLSSharedFileListRecentDocumentItems, "LSSharedFileListRecentDocumentItems");
KEY(kLSSharedFileListLoginItemHidden, "LSSharedFileListLoginItemHidden");

/* Positions, not items: two distinct addresses no item can have. */
static const char beforeFirst, last;
const LSSharedFileListItemRef kLSSharedFileListItemBeforeFirst = (LSSharedFileListItemRef)&beforeFirst;
const LSSharedFileListItemRef kLSSharedFileListItemLast = (LSSharedFileListItemRef)&last;
