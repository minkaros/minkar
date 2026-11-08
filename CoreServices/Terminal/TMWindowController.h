/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>
#import "TMTerminalView.h"

/* A Terminal window: its tabs, each a session (the tab bar shows from the
 * second one). A tab closes when its program ends; the window with its
 * last tab. */
@interface TMWindowController : NSWindowController <NSWindowDelegate, NSTabViewDelegate>
@property (copy) void (^closed)(TMWindowController *controller);
- (instancetype)initWithArguments:(NSArray<NSString *> *)arguments;
- (TMTerminalView *)addTabWithArguments:(NSArray<NSString *> *)arguments;
@property (readonly) TMTerminalView *currentTerminal;
@property (readonly) NSArray<TMTerminalView *> *terminals;
- (IBAction)newTab:(id)sender;
- (IBAction)closeTab:(id)sender;
- (IBAction)selectNextTab:(id)sender;
- (IBAction)selectPreviousTab:(id)sender;
- (IBAction)biggerText:(id)sender;
- (IBAction)smallerText:(id)sender;
- (IBAction)defaultTextSize:(id)sender;
@end
