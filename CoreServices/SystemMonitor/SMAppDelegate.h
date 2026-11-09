/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>
#import "SMWindowController.h"

@interface SMAppDelegate : NSObject <NSApplicationDelegate>
@property (readonly) SMWindowController *windowController;
- (void)buildMenus;
@end
