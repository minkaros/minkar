/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

/* The generic icons: Atlas's (/System/Library/Icons, blue folders), or
   drawn ones when those are missing. */
NSImage *FilesFolderIcon(void);
NSImage *FilesDocumentIcon(void);
/* A folder's icon: its own for the home folder and the known folders in
   it (Documents, Downloads, Music...), else the generic one. */
NSImage *FilesFolderIconForURL(NSURL *url);

NS_ASSUME_NONNULL_END
