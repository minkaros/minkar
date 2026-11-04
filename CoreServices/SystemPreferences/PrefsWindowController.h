/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

/* A pane found on disk, not yet loaded. */
@interface PrefsPaneInfo : NSObject
@property (nonatomic, readonly) NSBundle *bundle;
@property (nonatomic, readonly, copy) NSString *identifier;
@property (nonatomic, readonly, copy) NSString *label;
@property (nonatomic, readonly) NSImage *icon;
@end

/* The window: the icon grid, or one pane. */
@interface PrefsWindowController : NSObject
@property (nonatomic, readonly) NSWindow *window;
- (instancetype)init;
- (IBAction)showAll:(nullable id)sender;
- (IBAction)goBack:(nullable id)sender;
- (IBAction)goForward:(nullable id)sender;
/* Shows the pane with this bundle identifier (NO if there is none). */
- (BOOL)showPaneWithIdentifier:(NSString *)identifier;
@end

NS_ASSUME_NONNULL_END
