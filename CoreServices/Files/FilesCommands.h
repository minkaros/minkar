/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The file manager's commands beyond K2's operations, as the reference
 * platform's file manager has them in its menus: empty the Trash, delete
 * immediately, compress, make an alias and show its original, a new folder
 * with the selection, copy pathnames, recent folders, Go to Folder, and
 * keep an app in the Dock.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

/* The Trash emptied; asked first unless ask is NO (Option held). */
void FilesEmptyTrash(BOOL ask);
BOOL FilesTrashIsEmpty(void);

/* Items erased for good, after asking. YES when they were. */
BOOL FilesDeleteImmediately(NSArray<NSURL *> *items);

/* A zip archive beside the items: "name.zip" for one, "Archive.zip" for
   several (a free name either way), made off the main thread; done gets
   it, or nil after telling the user why. */
void FilesCompress(NSArray<NSURL *> *items, void (^done)(NSURL *_Nullable archive));

/* An alias of each item beside it, "name alias" (Minkar's aliases are
   symbolic links). The aliases made. */
NSArray<NSURL *> *FilesMakeAliases(NSArray<NSURL *> *items);

/* What an alias points to, or nil when url is not one or its original is
   gone. */
NSURL *_Nullable FilesOriginalOf(NSURL *url);

/* A new folder in folder, "New Folder With Items", the items moved into
   it. The folder, or nil. */
NSURL *_Nullable FilesNewFolderWithItems(NSArray<NSURL *> *items, NSURL *folder);

/* The items' paths on the general pasteboard, one a line. */
void FilesCopyPathnames(NSArray<NSURL *> *items);

/* Recent folders (the Go menu's): the last ten shown, newest first. */
void FilesNoteRecentFolder(NSURL *folder);
NSArray<NSURL *> *FilesRecentFolders(void);
void FilesClearRecentFolders(void);

/* Go to Folder: the path asked (a "~" is the home folder); nil when
   cancelled. A path that does not exist is said so and asked again. */
NSURL *_Nullable FilesAskFolder(NSWindow *_Nullable window);

/* The applications among items kept in the Dock. */
void FilesAddToDock(NSArray<NSURL *> *items);

NS_ASSUME_NONNULL_END
