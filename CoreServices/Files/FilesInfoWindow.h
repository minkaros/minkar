/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "FilesItem.h"

NS_ASSUME_NONNULL_BEGIN

/* Get Info: a window with an item's icon, name, kind, size, place and
   dates (Atlas.md, K1: basic Get Info). One per item. */
@interface FilesInfoWindow : NSWindow
+ (void)showForItem:(FilesItem *)item;
@end

NS_ASSUME_NONNULL_END
