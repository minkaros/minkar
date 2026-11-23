/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Network pane's Advanced sheet (NetworkAdvanced.m).
 */

#import <Cocoa/Cocoa.h>
#import <SystemConfiguration/SystemConfiguration.h>

@interface NetworkAdvanced : NSObject
- (instancetype)initWithPreferences:(SCPreferencesRef)prefs serviceID:(NSString *)serviceID bundle:(NSBundle *)bundle;
/* done(YES) once OK wrote into the preferences. */
- (void)beginSheetForWindow:(NSWindow *)window completion:(void (^)(BOOL changed))done;
@end
