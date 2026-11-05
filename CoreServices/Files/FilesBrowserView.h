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
@end

/* A folder's items as icons in a grid or as a list with columns; the
   document view of a scroll view. */
@interface FilesBrowserView : NSView
@property (nonatomic, copy) NSArray<FilesItem *> *items;
@property (nonatomic) FilesViewMode mode;
@property (nonatomic, readonly, nullable) FilesItem *selectedItem;
@property (nonatomic, weak, nullable) id<FilesBrowserViewDelegate> delegate;
- (void)selectItemNamed:(NSString *)name;
/* Sized again to its scroll view's width and its items. */
- (void)relayout;
@end

NS_ASSUME_NONNULL_END
