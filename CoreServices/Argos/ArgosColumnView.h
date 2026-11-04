/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "ArgosItem.h"

NS_ASSUME_NONNULL_BEGIN

@class ArgosColumnView;

@protocol ArgosColumnViewDelegate <NSObject>
/* Double click or Command-Down on an item. */
- (void)columnView:(ArgosColumnView *)view openItem:(ArgosItem *)item;
/* The deepest column now shows another folder. */
- (void)columnView:(ArgosColumnView *)view showsFolder:(NSURL *)folder;
@end

/* Folders side by side, from a root folder: an item chosen in a column
   is shown in the next one when it is a folder. The document view of a
   scroll view. */
@interface ArgosColumnView : NSView
@property (nonatomic, readonly, copy) NSURL *rootFolder;
/* The folder of the last column. */
@property (nonatomic, readonly, copy) NSURL *deepestFolder;
@property (nonatomic, readonly) NSUInteger deepestItemCount;
/* The item chosen in the column the keyboard acts in. */
@property (nonatomic, readonly, nullable) ArgosItem *selectedItem;
@property (nonatomic, weak, nullable) id<ArgosColumnViewDelegate> delegate;
/* One column, the folder's; NO when the folder cannot be read. */
- (BOOL)showRootFolder:(NSURL *)folder;
/* The folders read again, keeping what is chosen in each. */
- (void)reload;
/* In the active column. */
- (void)selectItemNamed:(NSString *)name;
- (void)relayout;
@end

NS_ASSUME_NONNULL_END
