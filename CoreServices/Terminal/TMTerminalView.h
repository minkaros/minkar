/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>
#import "TMSession.h"

/* A session's screen, its scrollback above, in a fixed-pitch font (Agave):
 * drawn cell by cell grid, typed into, selected with the mouse. It sits
 * in a scroll view whose size sets the session's rows and columns. */
@interface TMTerminalView : NSView <TMSessionDelegate>
@property (readonly) TMSession *session;
@property (nonatomic) CGFloat fontSize;
@property (readonly) NSSize cellSize;
/* How many times the bell rang (whatever the preference made of it). */
@property (readonly) NSUInteger bellCount;
/* Called when the session's title changes or its program ends. */
@property (copy) void (^titleChanged)(TMTerminalView *view);
@property (copy) void (^exited)(TMTerminalView *view, int status);

- (instancetype)initWithFrame:(NSRect)frame arguments:(NSArray<NSString *> *)arguments;
/* The rows and columns that fit the scroll view now. */
- (void)fitToScrollView;
/* Takes the preferences again (posted on each change). */
- (void)applyPreferences;
/* The frame a scroll view needs for these rows and columns. */
- (NSSize)contentSizeForRows:(int)rows columns:(int)columns;
- (NSString *)selectedText;
- (IBAction)copy:(id)sender;
- (IBAction)paste:(id)sender;
- (IBAction)selectAll:(id)sender;
- (IBAction)clearScrollback:(id)sender;
@end
