/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Find panel text views share (NSFindPanel.m); private.
 */

#import <AppKit/NSTextView.h>

@interface NSFindPanel : NSObject
+ (NSFindPanel *)sharedFindPanel;
- (void)perform:(NSFindPanelAction)action for:(NSTextView *)view;
- (BOOL)validateAction:(NSFindPanelAction)action for:(NSTextView *)view;
@end

@interface NSTextView (NSFindPanelPrivate)
- (BOOL)_validateFindPanelAction:(id)sender;
@end
