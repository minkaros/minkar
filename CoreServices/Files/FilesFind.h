/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Find (File > Find, Command-F): items whose names contain the text
 * typed, in a folder and every folder in it, listed as they are found
 * (off the main thread; hidden ones left out unless shown). A double
 * click reveals one in a Files window. Searching the metadata index
 * (mdfind) is K4's, later.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface FilesFind : NSObject
/* A find window on folder; reveal is how an item found is shown. */
+ (void)showInFolder:(NSURL *)folder reveal:(void (^)(NSURL *url))reveal;
@end

NS_ASSUME_NONNULL_END
