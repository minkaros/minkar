/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

/* Terminal's Preferences window (⌘,): General (shell, what happens when
 * it exits, the window's size, scrollback, bell) and Text (size, scheme,
 * colors). Each control writes its preference as it changes. */
@interface TMPreferencesController : NSWindowController
+ (TMPreferencesController *)sharedController;
/* The controls, for tests. */
@property (readonly) NSTabView *tabView;
@property (readonly) NSTextField *shellField, *columnsField, *rowsField, *scrollbackField, *fontSizeField;
@property (readonly) NSPopUpButton *exitPopUp, *bellPopUp, *schemePopUp;
@property (readonly) NSColorWell *backgroundWell, *textWell, *cursorWell;
@property (readonly) NSButton *accentCursorButton;
@property (readonly) NSArray<NSColorWell *> *paletteWells;
/* Shows the values the preferences have now. */
- (void)updateControls;
- (IBAction)restoreDefaults:(id)sender;
@end
