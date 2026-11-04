/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "ArgosBrowserView.h"

NS_ASSUME_NONNULL_BEGIN

/* One browser window: a bar with Back, Forward, the folder's name and
   the view buttons; the sidebar; the folder's items; a status line. */
@interface ArgosWindowController : NSObject
@property (nonatomic, readonly) NSWindow *window;
@property (nonatomic, readonly) NSURL *folder;
@property (nonatomic, readonly, nullable) ArgosItem *selectedItem;
- (instancetype)initWithFolder:(NSURL *)folder;
- (void)goToFolder:(NSURL *)folder;
- (IBAction)goBack:(nullable id)sender;
- (IBAction)goForward:(nullable id)sender;
- (IBAction)goToEnclosingFolder:(nullable id)sender;
- (IBAction)openSelection:(nullable id)sender;
- (IBAction)showAsIcons:(nullable id)sender;
- (IBAction)showAsList:(nullable id)sender;
@end

NS_ASSUME_NONNULL_END
