/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The desktop's icons (Atlas.md, K3): the volumes mounted under /Volumes
 * and the items of ~/Desktop, in a window at the desktop icon level over
 * the Dock's desktop picture, in columns from the top right. They are
 * opened, dragged, dropped on, put in the Trash and ejected as in Files'
 * windows.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface FilesDesktop : NSObject

/* How the app opens what is double-clicked (a folder in a Files window). */
- (instancetype)initWithOpener:(void (^)(NSURL *url))opener;
- (void)show;
- (void)reload;

/* What the desktop shows, in order (for the tests: its log). */
@property (nonatomic, readonly) NSArray<NSURL *> *itemURLs;

@end

NS_ASSUME_NONNULL_END
