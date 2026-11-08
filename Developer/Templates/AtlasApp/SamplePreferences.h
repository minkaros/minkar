/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

/* The app's Preferences window, one for the app. */
@interface SamplePreferences : NSWindowController
+ (instancetype)sharedPreferences;
@end
