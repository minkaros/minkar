/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/NSObject.h>

@class NSString, NSAttributedString, NSView, NSImage;

/* A window's tab, as its tab bar shows it (API 23). */
@interface NSWindowTab : NSObject
@property (copy) NSString *title;
@property (copy) NSAttributedString *attributedTitle;
@property (copy) NSString *toolTip;
@property (strong) NSView *accessoryView;
@end
