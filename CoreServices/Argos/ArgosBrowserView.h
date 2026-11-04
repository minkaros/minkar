/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "ArgosItem.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ArgosViewMode) {
    ArgosViewModeIcons,
    ArgosViewModeList,
};

@class ArgosBrowserView;

@protocol ArgosBrowserViewDelegate <NSObject>
/* Double click, Command-Down or Command-O on an item. */
- (void)browserView:(ArgosBrowserView *)view openItem:(ArgosItem *)item;
- (void)browserViewSelectionChanged:(ArgosBrowserView *)view;
@end

/* A folder's items as icons in a grid or as a list with columns; the
   document view of a scroll view. */
@interface ArgosBrowserView : NSView
@property (nonatomic, copy) NSArray<ArgosItem *> *items;
@property (nonatomic) ArgosViewMode mode;
@property (nonatomic, readonly, nullable) ArgosItem *selectedItem;
@property (nonatomic, weak, nullable) id<ArgosBrowserViewDelegate> delegate;
- (void)selectItemNamed:(NSString *)name;
/* Sized again to its scroll view's width and its items. */
- (void)relayout;
@end

NS_ASSUME_NONNULL_END
