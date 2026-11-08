/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The window AppKit shows an app's help book in (private to AppKit).
 */

#import <Foundation/NSObject.h>

@class NSString, NSURL, NSWindow;

@interface NSHelpViewer : NSObject
+ (NSHelpViewer *)sharedViewer;
- (void)showBookAtURL:(NSURL *)index title:(NSString *)title;
/* A page of the book whose start page is home, at its anchor (or nil). */
- (void)showPage:(NSURL *)page anchor:(NSString *)anchor home:(NSURL *)home title:(NSString *)title;
- (NSWindow *)window;
- (NSString *)pageText;
@end
