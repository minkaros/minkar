/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Items left out of backups (CarbonCore's BackupCore).
 */
#ifndef __BACKUPCORE__
#define __BACKUPCORE__

#include <CoreFoundation/CoreFoundation.h>
#include <MacTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

OSStatus CSBackupSetItemExcluded(CFURLRef item, Boolean exclude, Boolean excludeByPath);
Boolean CSBackupIsItemExcluded(CFURLRef item, Boolean *excludeByPath);

#ifdef __cplusplus
}
#endif

#endif
