/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The file manager's file operations (Atlas.md, K2): copy, move,
 * duplicate, rename, new folder, move to the Trash and put back. Copies
 * and moves run off the main thread, with a progress window when they
 * take longer than a moment; each operation can be undone.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface FilesOperations : NSObject

+ (instancetype)shared;

/* One for the app: the Edit menu's Undo and Redo. */
@property (nonatomic, readonly) NSUndoManager *undoManager;

/* A name not taken in folder: name, then "name 2", "name 3"… (before the
   extension). With copy, "name copy", "name copy 2"… */
+ (NSString *)freeNameFor:(NSString *)name inFolder:(NSURL *)folder copy:(BOOL)copy;

/* Each calls done on the main thread with the items made (or moved),
   or nil when it failed (after telling the user why). */
- (void)copyItems:(NSArray<NSURL *> *)items toFolder:(NSURL *)folder
             done:(nullable void (^)(NSArray<NSURL *> *_Nullable made))done;
- (void)moveItems:(NSArray<NSURL *> *)items toFolder:(NSURL *)folder
             done:(nullable void (^)(NSArray<NSURL *> *_Nullable moved))done;
- (void)duplicateItems:(NSArray<NSURL *> *)items
                  done:(nullable void (^)(NSArray<NSURL *> *_Nullable made))done;
- (nullable NSURL *)renameItem:(NSURL *)item to:(NSString *)name;
- (nullable NSURL *)newFolderIn:(NSURL *)folder;
- (void)trashItems:(NSArray<NSURL *> *)items;

/* A disk image attached (hdiutil attach), off the main thread; done gets
   its first mounted volume, or nil after telling the user why. */
- (void)attachDiskImage:(NSURL *)image done:(void (^)(NSURL *_Nullable volume))done;

/* Items in the Trash go back where they were (Put Back). */
+ (nullable NSURL *)putBackLocationOf:(NSURL *)trashed;
- (void)putBackItems:(NSArray<NSURL *> *)items;

/* The user's Trash; whether url is in it. */
+ (NSURL *)trashURL;
+ (BOOL)isInTrash:(NSURL *)url;

@end

/* The files a drag carries: its file names, else its file URL (an app
   that writes the file as the drag starts, as Firefox does with an image,
   gives only that). Existing files only. */
NSArray<NSURL *> *FilesDraggedFileURLs(NSPasteboard *pasteboard);


NS_ASSUME_NONNULL_END
