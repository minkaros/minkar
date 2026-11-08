/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>
#import "LGWindowController.h"

@interface LGAppDelegate : NSObject <NSApplicationDelegate>
@property (readonly) LGWindowController *windowController;
- (void)buildMenus;
@end
