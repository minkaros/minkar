/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An item left out of backups carries the extended attribute the reference
 * marks it with (com.apple.metadata:com_apple_backup_excludeItem, a binary
 * property list of the string "com.apple.backupd"), so it stays left out
 * wherever it is moved or copied, as on the reference. An exclusion by path
 * is the backup tool's own list, which Minkar does not have yet: such a
 * request is answered as not allowed.
 */

#include "BackupCore.h"
#include "MacErrors.h"
#include <sys/xattr.h>
#include <limits.h>
#include <errno.h>

static const char kBackupExcludeAttribute[] = "com.apple.metadata:com_apple_backup_excludeItem";

static Boolean pathOf(CFURLRef item, char *path) {
	return item != NULL && CFURLGetFileSystemRepresentation(item, true, (UInt8 *)path, PATH_MAX);
}

OSStatus CSBackupSetItemExcluded(CFURLRef item, Boolean exclude, Boolean excludeByPath) {
	char path[PATH_MAX];

	if (!pathOf(item, path))
		return paramErr;
	if (excludeByPath)
		return permErr;
	if (!exclude) {
		if (removexattr(path, kBackupExcludeAttribute, 0) != 0 && errno != ENOATTR)
			return (errno == ENOENT) ? fnfErr : ioErr;
		return noErr;
	}

	CFDataRef value = CFPropertyListCreateData(kCFAllocatorDefault, CFSTR("com.apple.backupd"),
	    kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	int rc;

	if (value == NULL)
		return memFullErr;
	rc = setxattr(path, kBackupExcludeAttribute, CFDataGetBytePtr(value), (size_t)CFDataGetLength(value), 0, 0);
	CFRelease(value);
	if (rc != 0)
		return (errno == ENOENT) ? fnfErr : (errno == EACCES || errno == EPERM) ? permErr : ioErr;
	return noErr;
}

Boolean CSBackupIsItemExcluded(CFURLRef item, Boolean *excludeByPath) {
	char path[PATH_MAX];

	if (excludeByPath != NULL)
		*excludeByPath = false;
	if (!pathOf(item, path))
		return false;
	return getxattr(path, kBackupExcludeAttribute, NULL, 0, 0, 0) >= 0;
}
