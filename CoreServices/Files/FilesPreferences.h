/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Files' preferences (Files > Preferences…), in the reference platform's
 * file manager's keys, in Files' domain: what the desktop shows, where a
 * new window opens, the Trash's warning, folders on top, hidden files.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

/* Posted when one changes: windows and the desktop read their folders
   again. */
extern NSString *const FilesPreferencesChangedNotification;

/* The folder a new window shows (NewWindowTarget). */
NSURL *FilesNewWindowFolder(void);
/* WarnOnEmptyTrash (default YES). */
BOOL FilesWarnsOnEmptyTrash(void);
/* _FXSortFoldersFirst (default YES). */
BOOL FilesSortsFoldersFirst(void);
/* AppleShowAllFiles (default NO): names starting with a dot shown. */
BOOL FilesShowsHiddenFiles(void);
void FilesSetShowsHiddenFiles(BOOL shown);
/* ShowHardDrivesOnDesktop (default NO): the startup disk on the desktop;
   ShowExternalHardDrivesOnDesktop (default YES): the mounted volumes. */
BOOL FilesDesktopShowsStartupDisk(void);
BOOL FilesDesktopShowsVolumes(void);

@interface FilesPreferences : NSObject
+ (void)show;
@end

NS_ASSUME_NONNULL_END
