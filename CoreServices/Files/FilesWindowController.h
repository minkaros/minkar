/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "FilesBrowserView.h"

NS_ASSUME_NONNULL_BEGIN

/* One browser window: a bar with Back, Forward, the folder's name and
   the view buttons; the sidebar; the folder's items; a status line. */
@interface FilesWindowController : NSObject
@property (nonatomic, readonly) NSWindow *window;
@property (nonatomic, readonly) NSURL *folder;
@property (nonatomic, readonly, nullable) FilesItem *selectedItem;
- (instancetype)initWithFolder:(NSURL *)folder;
- (void)goToFolder:(NSURL *)folder;
/* Selects an item of the folder shown, in any view. */
- (void)selectItemNamed:(NSString *)name;
- (IBAction)goBack:(nullable id)sender;
- (IBAction)goForward:(nullable id)sender;
- (IBAction)goToEnclosingFolder:(nullable id)sender;
- (IBAction)openSelection:(nullable id)sender;
- (IBAction)showAsIcons:(nullable id)sender;
- (IBAction)showAsList:(nullable id)sender;
- (IBAction)showAsColumns:(nullable id)sender;

/* File operations (K2), on the selection or the folder shown. */
@property (nonatomic, readonly) NSArray<NSURL *> *selectedURLs;
- (IBAction)copy:(nullable id)sender;
- (IBAction)selectAll:(nullable id)sender;
- (IBAction)paste:(nullable id)sender;
- (IBAction)moveItemsHere:(nullable id)sender;   /* paste, moving */
- (IBAction)duplicate:(nullable id)sender;
- (IBAction)newFolder:(nullable id)sender;
- (IBAction)rename:(nullable id)sender;
- (IBAction)moveToTrash:(nullable id)sender;
- (IBAction)putBack:(nullable id)sender;
/* The file URLs on the general pasteboard. */
+ (NSArray<NSURL *> *)pasteboardURLs;
/* The folder shown read again now, then name selected. */
- (void)reloadSelecting:(nullable NSString *)name;
@end

NS_ASSUME_NONNULL_END
