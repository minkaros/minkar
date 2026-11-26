/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The path bar (View > Show Path Bar): the folder shown and the folders
 * that hold it, from the computer down, each with its icon; a click on
 * one goes to it, a double click on the last opens it.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface FilesPathBar : NSView
@property (nonatomic, copy, nullable) NSURL *URL;
/* Called with the folder clicked. */
@property (nonatomic, copy, nullable) void (^chooser)(NSURL *folder);
@end

NS_ASSUME_NONNULL_END
