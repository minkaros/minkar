/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

/* A plain text document in a window: replace what it holds and how it
 * reads and writes it with the app's own. */
@interface SampleDocument : NSDocument
@property (copy) NSString *text;
@end
