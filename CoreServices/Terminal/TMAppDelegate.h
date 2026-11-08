/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>
#import "TMWindowController.h"

@interface TMAppDelegate : NSObject <NSApplicationDelegate>
@property (readonly) NSArray<TMWindowController *> *windowControllers;
- (TMWindowController *)newWindowWithArguments:(NSArray<NSString *> *)arguments;
- (void)buildMenus;
@end
