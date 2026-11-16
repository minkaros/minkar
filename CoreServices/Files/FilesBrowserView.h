/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "FilesItem.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, FilesViewMode) {
    FilesViewModeIcons,
    FilesViewModeList,
    FilesViewModeColumns,   /* shown by FilesColumnView */
};

@class FilesBrowserView;

@protocol FilesBrowserViewDelegate <NSObject>
/* Double click, Command-Down or Command-O on an item. */
- (void)browserView:(FilesBrowserView *)view openItem:(FilesItem *)item;
- (void)browserViewSelectionChanged:(FilesBrowserView *)view;
/* Drag and drop (K2): the folder the view shows, and items dropped into
   a folder (copying, or moving). */
- (NSURL *)folderOfBrowserView:(FilesBrowserView *)view;
/* A name typed over an item's (Return, or File > Rename). */
- (void)browserView:(FilesBrowserView *)view renameItem:(FilesItem *)item to:(NSString *)name;
- (void)browserView:(FilesBrowserView *)view dropURLs:(NSArray<NSURL *> *)urls
           intoFolder:(NSURL *)folder copying:(BOOL)copying;
@end

/* A folder's items as icons in a grid or as a list with columns; the
   document view of a scroll view. */
@interface FilesBrowserView : NSView
@property (nonatomic, copy) NSArray<FilesItem *> *items;
@property (nonatomic) FilesViewMode mode;
@property (nonatomic, readonly, nullable) FilesItem *selectedItem;
/* Every item selected (Command-click adds, Shift-click a range), in order. */
@property (nonatomic, readonly) NSArray<FilesItem *> *selectedItems;
- (void)selectItemsNamed:(NSArray<NSString *> *)names;
- (IBAction)selectAll:(nullable id)sender;
/* The selected item's name edited in place; NO when not one item is selected. */
- (BOOL)beginRenaming;
@property (nonatomic, weak, nullable) id<FilesBrowserViewDelegate> delegate;
- (void)selectItemNamed:(NSString *)name;
/* Sized again to its scroll view's width and its items. */
- (void)relayout;
@end

NS_ASSUME_NONNULL_END
