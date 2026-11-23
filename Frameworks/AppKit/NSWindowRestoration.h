/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSWindow.h>
#import <AppKit/NSApplication.h>

@class NSCoder;

/* Window restoration (API 10.7): a class that makes a window again, at
   launch, from what was saved when the app quit keeping its windows. */
@protocol NSWindowRestoration <NSObject>
+ (void)restoreWindowWithIdentifier:(NSUserInterfaceItemIdentifier)identifier
                              state:(NSCoder *)state
                  completionHandler:(void (^)(NSWindow *window, NSError *error))completionHandler;
@end

@interface NSApplication (NSWindowRestoration)
/* Restores a window from the saved state, when it was saved (YES): its
   restoration class is asked for it. */
- (BOOL)restoreWindowWithIdentifier:(NSUserInterfaceItemIdentifier)identifier
                              state:(NSCoder *)state
                  completionHandler:(void (^)(NSWindow *window, NSError *error))completionHandler;
@end

/* A window's identifier: which of its restoration class's windows it is. */
@interface NSWindow (NSUserInterfaceItemIdentification) <NSUserInterfaceItemIdentification>
@property (copy) NSUserInterfaceItemIdentifier identifier;
@end

@interface NSWindow (NSRestorableState)
- (void)encodeRestorableStateWithCoder:(NSCoder *)coder;
- (void)restoreStateWithCoder:(NSCoder *)coder;
@end
