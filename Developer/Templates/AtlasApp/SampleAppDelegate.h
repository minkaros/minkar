/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

@interface SampleAppDelegate : NSObject <NSApplicationDelegate>
/* The standard menus (the app menu, File, Edit, Window, Help). */
- (void)buildMenus;
- (IBAction)showPreferences:(id)sender;
@end
